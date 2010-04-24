/*
	DBTOY - A FUSE filesystem mapping an RDBMS
	Domenico Rotiroti <pinguino@thesaguaros.com>

    Licensed under the GPL License.
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <unistd.h>
#include <syslog.h>

#include "dbtoy.h"
#include "xml_format.h"
#include "largetext.h"

#define AUXROWLEN 3

/* available drivers */
#ifdef HAVE_MYSQL_DRV
extern struct dbtoy_driver mysql_driver;
#endif
#ifdef HAVE_POSTGRESQL_DRV
extern struct dbtoy_driver pgsql_driver;
#endif


/* communication with the driver */
struct dbtoy_driver * drv = NULL;
DBTOY_CONN conn = NULL;

char xname[1024];
char xtabname[1024];
char qs[1024];
char * nargv[] = {NULL,"-s","-o","direct_io",NULL};
int nargc = sizeof(nargv)/sizeof(nargv[0]);
char * dbfile[] = {"data","types"};
int ndbfile = sizeof(dbfile)/sizeof(dbfile[0]);

/* parsed options */
char *	hostname	= NULL;
char *	login	= NULL;
char *	passwd	= NULL;
char *	prolog	= NULL;
char *	dbms		= NULL;
char *	instance	= NULL;
int		verbose	= 0;

/* read hint */
char * rh_path = NULL;
off_t rh_off = 0;
int rh_row;
char * rh_buf = NULL;

struct dbtoy_driver * lookup_driver();

void set_read_hint(const char * path, off_t offset, int currow, char * buf)
{
	if(rh_path) free(rh_path);
	if(rh_buf) free(rh_buf);

	rh_path=strdup(path);
	rh_buf=strdup(buf);
	rh_off=offset;
	rh_row=currow;
}

int get_read_hint(const char * path, off_t offset)
{
    int res = 0;
	if(!rh_path || strcmp(rh_path,path)) {
        //do nothing f.n.
    } else if(offset==rh_off) {
        res = rh_row;
    }
	
    return res;
}

void free_rset(char ** data, int nr, int nc) {
	int i;
	for(i=0; i<nr*nc;++i)
		free(data[i]);
	free(data);
}

int count(const char * name,const char target) {
	int c = 0;
	char * n = strchr(name,target);
	while(n) {
		++c;
		n = strchr(n+1,target);
	}
	return c;
}

int dlevel(const char * path) {
	int l = count(path,'/');
	if(path[strlen(path)-1]!='/') ++l;
	return l;
}

const char * pbase(const char * path) {
	return strrchr(path+1,'/')+1;
}

char * pdir(const char * path) {
	int sz = (strrchr(path,'/')-path);
	char * buf = (char *)malloc(sz);
	strncpy(buf,path+1,sz-1);
	buf[sz-1]=0;
	return buf;
}

int table_exists(const char * path) {
	int res=1; //ottimismo :P
	char * b = pdir(path);
	if(drv->search_schema(conn,b)<0 || drv->search_table(conn,b,pbase(path))<0) { 
		res = 0; 
	}
	free(b);
	return res;
}

void split_path(const char * path, char * name, char * tabname) {
	int len;
	char * p1, *p2;
	p1 = strchr(path+1,'/');
	p2 = strchr(p1+1,'/');
	strncpy(name,path+1,p1-path);
	strncpy(tabname,p1+1,p2-p1);
	name[p1-path-1]=0; tabname[p2-p1-1]=0;
}

void parse_clause(const char * pars){
	qs[0]=0;
	if(pars[0]!='?') return;
	strcpy(qs," where ");
	strcpy(qs+7,pars+1);
	/*
	struct lt * lq;
	qs = NULL;
	if(pars[0]!='?') return;
	lq = lt_new_v(" where ");
	*/	
}

static int dbtoy_getattr(const char *path, struct stat *stbuf)
{
    int res = 0, lev;
	
    memset(stbuf, 0, sizeof(struct stat));
	lev = dlevel(path);
	switch(lev) {
		case 1:
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			return res;
			break;
		case 2:
			if(drv->search_schema(conn,path+1)<0) { return -ENOENT; }
    	    stbuf->st_mode = S_IFDIR | 0755;
	        stbuf->st_nlink = 2;
			return res;
			break;
		case 3:
			if(!table_exists(path)) return -ENOENT;
    	    stbuf->st_mode = S_IFDIR | 0755;
	        stbuf->st_nlink = 2;			
			return res;
			break;

		case 4:
			if(strncmp(pbase(path),"data",4) && strcmp(pbase(path),"types")) { return -ENOENT; }
			stbuf->st_mode = S_IFREG | 0444;
        	stbuf->st_nlink = 1;
			stbuf->st_size = 0;
			return res;
			break;	
			
		default:
			return -ENOENT;			
	}		
}

static int dbtoy_readdir(const char *path, void * h, fuse_fill_dir_t filler, off_t oi,
				struct fuse_file_info * fi)
{
	int i, lev;
	DBTOY_NAME name;
	int num_schemas = -1;
	int num_tables  = -1;
	
	lev = dlevel(path);
	switch(lev) {
		case 1: //root path
		    filler(h, ".", 0, 0);
    		filler(h, "..", 0, 0);
			num_schemas = drv->count_dbs(conn);
			for(i=0; i<num_schemas; ++i) {
				name = drv->what_schema(conn,i);
				filler(h,name,0,0);
			}
    		return 0;
			break; //superfluo

		case 2: // schema dir
			if(drv->search_schema(conn,path+1)<0) { return -ENOENT; }

    		filler(h, ".", 0, 0);
   			filler(h, "..", 0, 0);		
			num_tables = drv->count_tables(conn,path+1);
			for(i=0; i<num_tables; ++i) {
				name = drv->what_table(conn,path+1,i);
				filler(h,name,0,0);
			}

			return 0;
			break;

		case 3: // table directory

			if(!table_exists(path)) return -ENOENT;
			filler(h, ".", 0, 0);
   			filler(h, "..", 0, 0);
			filler(h, "data", 0, 0);
			filler(h, "types", 0, 0);

			return 0;
			break;

		default:
			return -ENOENT;
	}
    	
}


static int dbtoy_open(const char *path, struct fuse_file_info *fi)
{
	char * lpath;
	int res=0, len;
	
	if(strncmp(pbase(path),"data",4) && strcmp(pbase(path),"types")) { return -ENOENT; }
	len = strrchr(path,'/')-path;
	lpath = (char*) malloc(len+1);
	strncpy(lpath,path,len);
	lpath[len]=0;
	if(!table_exists(lpath)) res = -ENOENT;
	free(lpath);
	if(!res) return res;
		
    if((fi->flags & 3) != O_RDONLY)
        return -EACCES;

    return 0;
	
}

int fill(char * buf, size_t size, off_t offset, char * xml) {
	int len = strlen(xml), nsize = 0;
    if (offset < len) {
        nsize = len - offset;
		if(nsize > size) {
			nsize = size;
		}
		memcpy(buf, xml + offset, nsize);
    } else
        nsize = 0;

    return nsize;
	
}

static int dbtoy_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
    (void) fi;

	const char * fname = pbase(path);
	char ** data, ** auxdata, **label, *stype, *xml;
	int res, currow;
	int len, ncols, alen, *cdata, i;
	off_t fill_off=offset;
	int error=0;
	DBTOY_RES results;
	DBTOY_ROW row;
	struct lt * lxml;

	
	if(strncmp(fname,"data",4) && strcmp(fname,"types")) { return -ENOENT; }
	split_path(path,xname,xtabname);
	
	if(!strncmp(fname,"data",4)) {
		
		lxml = lt_new();
		currow=get_read_hint(path,offset);

		if(currow >=0) {
			parse_clause(fname+4);
			if(currow == 0) {
                results = drv->read_data_start(conn,xname,xtabname,qs,&len,&ncols);
                if(verbose) syslog(LOG_DEBUG,"dbtoy_read: got %d rows for path %s",len,path);

			} else {
                results = drv->read_data_start_from(conn,xname,xtabname,qs,&len,&ncols,currow);
                if(verbose) syslog(LOG_DEBUG,"dbtoy_read: got %d rows for path %s (starting from %d)",len,path,currow);
            }
			
			/* check for errors */
			if(!results) {
				currow = -1;
				error = 1;
				syslog(LOG_ERR,"dbtoy_read: error while reading file %s",path);
				format_data_start(lxml,prolog,xname,xtabname);
				format_data_error(lxml,drv->error_msg(conn));
				format_data_end(lxml);				
				drv->read_data_end(results);
			} else {
				/* read type info */
				auxdata = drv->read_types(conn,xname,xtabname,&alen);
				cdata = (int*) malloc(alen*sizeof(int));
				label = (char**) malloc(alen*sizeof(char*));
				for ( i=0; i<alen; ++i)
				{
					label[i] = auxdata[i*AUXROWLEN];
					stype = auxdata[i*AUXROWLEN+1];
					if(strstr(stype,"char") || strstr(stype,"CHAR") ||
					   strstr(stype,"text") || strstr(stype,"TEXT")) {
						   cdata[i]=1;
					} else {
						cdata[i]=0;
					}
				}
			}
		}
		
		if(currow != 0) {
			if(rh_buf){
				lt_cat(lxml,rh_buf);
			}
			fill_off = 0;
		}
		
		if( currow == 0) {
			format_data_start(lxml,prolog,xname,xtabname);
		}
		
		if(currow >= 0) {
			while((row = drv->read_data_next(results)) && (lxml->lt_len < offset+size ))
			{
				format_data_next(lxml,row,ncols,cdata,label);
				++currow;
			}
			if(!row ) {
				currow = -1;
				format_data_end(lxml);
			}
		}
		
		res = fill(buf,size,fill_off,lxml->lt_text);

		set_read_hint(path, offset+res, currow, lxml->lt_text+fill_off+res);
		
		lt_free(lxml);
		if(currow>=0) {
			drv->read_data_end(results);
			free(cdata);
			free(label);
			free_rset(auxdata,alen,AUXROWLEN);
		}
		
		return res;
	}
	
	// reading types
	// ddl is read "one shot" as is assumed to be not so large...
	data = drv->read_types(conn,xname,xtabname,&len);
	xml = format_types(data,len,prolog,xname,xtabname);
	res = fill(buf,size,offset,xml);
	free_rset(data,len,3);
	free(xml);
	return res;
}

static struct fuse_operations dbtoy_oper = {
    .getattr	= dbtoy_getattr,
    .readdir = dbtoy_readdir,
    .open	= dbtoy_open,
    .read	= dbtoy_read,
};

int parse_args(int argc , char* argv[])
{
	int opt;
	int res = 0;
	while (1) 
    {
		opt=getopt(argc,argv,"h:u:p:x:d:i:v");
		if(opt==EOF) break;
		switch((char)opt) {
		case 'h':
			hostname=optarg;
			break;
		case 'u':
			login=optarg;
			break;	
		case 'p':
			passwd=optarg;
			break;
		case 'x':
			prolog = optarg;
			break;
		case 'd':
			dbms = optarg;
			break;
		case 'i':
			instance = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			res = 1;
		}
	}
	if(login==NULL || passwd==NULL || dbms==NULL) res=1;
	if(res) {
		fprintf(stderr,"Usage: %s -u user -p passwd -d driver [ optional_params ] mountpoint\n",argv[0]);
		fprintf(stderr,"\nValid drivers:\n");
#ifdef HAVE_POSTGRESQL_DRV
		fprintf(stderr,"\tpostgresql\n");
#endif
#ifdef HAVE_MYSQL_DRV
		fprintf(stderr,"\tmysql\n");
#endif		
		fprintf(stderr,"\nOptional parameters:\n");
		fprintf(stderr,"\t-h host      (where the database is running)\n");
		fprintf(stderr,"\t-i instance  (to choose a PostgreSQL database, the default is the same as username)\n");
		fprintf(stderr,"\t-v           (enable verbose syslogging)\n");
		fprintf(stderr,"\t-x 'xml prolog'\n\n");
	}
	return res;
}

struct dbtoy_driver * lookup_driver()
{
#ifdef HAVE_POSTGRESQL_DRV
	if(!strcmp(dbms,"postgresql")) return &pgsql_driver;
#endif
#ifdef HAVE_MYSQL_DRV
	if(!strcmp(dbms,"mysql")) return &mysql_driver;
#endif
	return NULL;
}

void initDbtoy(int argc, char * argv[]) {
	nargv[0]=argv[0]; //keep program name
	nargv[nargc-1]=argv[argc-1]; //keep mountpoint
	if(parse_args(argc,argv)) {
		exit(1);
	}

	/* choose driver and connect */
	drv = lookup_driver();
	if(drv==NULL) {
		fprintf(stderr,"%s error: no suitable driver found for dbms type %s\n", argv[0], dbms);		
		exit(1);
	}

	if(instance && drv->connect_xtra) {
		conn = drv->connect_xtra(hostname, login, passwd, instance);
	} else {
		conn = drv->connect(hostname, login, passwd);
	}
	if(conn==NULL) {
		fprintf(stderr,"%s error: connection to the database failed\n", argv[0]);
		fprintf(stderr,"please check these parameters: username=%s, driver=%s\n", login, dbms);
		exit(1);
	}
	
	openlog(argv[0],0,LOG_USER);
	syslog(LOG_INFO,"Started successfully");
}

void cleanDbtoy() {
	drv->disconnect(conn);
	syslog(LOG_INFO,"Exiting");
	closelog();
}

int main(int argc, char *argv[])
{
	int mres;
	initDbtoy(argc,argv);
    mres = fuse_main(nargc, nargv, &dbtoy_oper,NULL);
	cleanDbtoy();
	return mres;
}
