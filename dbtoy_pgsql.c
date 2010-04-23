#include "dbtoy.h"
#include "largetext.h"
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>

#include <libpq-fe.h>

struct pg_conn
{
	PGconn *	_conn;
	char **		_schemas;
	int			_sch_cnt;
	char **		_types;
	int			_typ_cnt;
	Oid *		_oids;
};

struct pg_res {
	PGresult *	_res;
	long	 	_offset;
	long		_nrows;
	int			_ncols;
};


DBTOY_CONN	pgsql_connect(const char* host, const char *login, const char *password);
DBTOY_CONN	pgsql_connect_xtra(const char* host, const char *login, 
								 const char *password, void * xtra_info);
void 		pgsql_disconnect(DBTOY_CONN conn);
int 		pgsql_count_dbs(DBTOY_CONN conn);
int 		pgsql_count_tables(DBTOY_CONN conn, const char * db);
DBTOY_NAME	pgsql_what_schema(DBTOY_CONN conn, int schema_num);
DBTOY_NAME	pgsql_what_table(DBTOY_CONN conn, const char * db, int table_num);
int			pgsql_search_schema	(DBTOY_CONN, const char * db);
int			pgsql_search_table	(DBTOY_CONN, const char * db, const char * table);
char**		pgsql_read_types	(DBTOY_CONN, const char * db, const char * table,
									int * len);
DBTOY_RES	pgsql_read_data_start	(DBTOY_CONN, const char * db, const char * table, const char * q,
									int * len, int * ncols);
DBTOY_RES	pgsql_read_data_start_from	(DBTOY_CONN, const char * db, const char * table, const char * q,
									int * len, int * ncols, int from);
void		pgsql_read_data_end	(DBTOY_RES res);
DBTOY_ROW	pgsql_read_data_next(DBTOY_RES res);
void		pgsql_read_data_seek(DBTOY_RES res, int row);
DBTOY_NAME	pgsql_error_msg(DBTOY_CONN conn);


#define BIGNUMBER 9999999
#define PKINFO	"pkPStm"

/* private funcs */
static void gen_cache(struct pg_conn * c);
static struct pg_res * make_res(PGresult * res);
static char * type_name(struct pg_conn * conn, Oid toid);

struct dbtoy_driver pgsql_driver = 
{
	connect:		pgsql_connect,
	connect_xtra:	pgsql_connect_xtra,
	disconnect: 	pgsql_disconnect,
	count_dbs: 		pgsql_count_dbs,
	count_tables:	pgsql_count_tables,
	what_schema:	pgsql_what_schema,
	what_table:		pgsql_what_table,
	search_schema:	pgsql_search_schema,
	search_table:	pgsql_search_table,
	read_types:		pgsql_read_types,
	read_data_start:		pgsql_read_data_start,
	read_data_start_from:	pgsql_read_data_start_from,
	read_data_end:	pgsql_read_data_end,
	read_data_next: pgsql_read_data_next,
	read_data_seek: pgsql_read_data_seek,
	error_msg:		pgsql_error_msg
};


#define GET_CONN(conn) (((struct pg_conn*)conn)->_conn)

DBTOY_CONN pgsql_connect(const char* host, const char *login, const char *password)
{
	struct pg_conn * conn;
	char loginbuff[1024]="";
	PGconn * db_conn;
	sprintf(loginbuff,"host='%s' user='%s' password='%s' dbname=postgres",
			host,login,password);
	db_conn = PQconnectdb(loginbuff);
    if(PQstatus(db_conn)==CONNECTION_BAD) return NULL;

	conn = (struct pg_conn *) malloc(sizeof( struct pg_conn));
	conn->_conn=db_conn;
	conn->_schemas=NULL;
	gen_cache(conn);
	return conn;
}      

DBTOY_CONN pgsql_connect_xtra(const char* host, const char *login, 
							  const char *password, void * xtra_info)
{
	struct pg_conn * conn;
	char loginbuff[1024]="";
	PGconn * db_conn;
	sprintf(loginbuff,"host='%s' user='%s' password='%s' dbname='%s'",
			host, login, password, (char*)xtra_info);
	db_conn = PQconnectdb(loginbuff);
    if(PQstatus(db_conn)==CONNECTION_BAD) return NULL;

	conn = (struct pg_conn *) malloc(sizeof( struct pg_conn));
	conn->_conn=db_conn;
	conn->_schemas=NULL;
	gen_cache(conn);
	return conn;
}      

void pgsql_disconnect(DBTOY_CONN conn)
{
	int i;
	struct pg_conn * c = (struct pg_conn *) conn;
	PQfinish(GET_CONN(conn));
	for( i=0; i< c->_sch_cnt; ++i) free(c->_schemas[i]);
	for( i=0; i< c->_typ_cnt; ++i) free(c->_types[i]);
	free(c->_schemas);
	free(c->_types);
	free(c->_oids);
	free(c);
}

int pgsql_count_dbs(DBTOY_CONN conn){
	return ((struct pg_conn *)conn)->_sch_cnt;
}

int pgsql_count_tables(DBTOY_CONN conn, const char * db){
	int nTab;
	char q[128]="";
	PGresult * res;
	
	sprintf(q,"select count(tablename) from pg_tables where schemaname='%s'",db);
	res = PQexec(GET_CONN(conn), q);
	nTab = strtol(PQgetvalue(res, 0, 0), (char**)NULL, 10);
	PQclear(res);
	
	return nTab;
}

DBTOY_NAME pgsql_what_schema(DBTOY_CONN conn, int schema_num)
{
	char * name;
	struct pg_conn * c = (struct pg_conn *)conn;

	if(schema_num>=c->_sch_cnt || schema_num < 0) return NULL;

	name = (char*) malloc(strlen(c->_schemas[schema_num])+1);
	strcpy(name,c->_schemas[schema_num]);

	return name;
}

DBTOY_NAME pgsql_what_table(DBTOY_CONN conn, const char * db, int table_num)
{
	char * name;
	char q[128]="";
	PGresult * res;

	sprintf(q,"select tablename from pg_tables where schemaname='%s'",db);
	res = PQexec(GET_CONN(conn), q);
	if(table_num>=PQntuples(res) || table_num < 0) return NULL;
	name = (char*) malloc(PQgetlength(res,table_num,0)+1);
	strcpy(name,PQgetvalue(res,table_num,0));
	PQclear(res);
	
	return name;
}

int	pgsql_search_schema	(DBTOY_CONN conn, const char * db)
{
	struct pg_conn * c = (struct pg_conn *)conn;
	int pos, found=0, nrows=c->_sch_cnt;
	for(pos=0; pos < nrows && !found; ++pos)
	{
		if(!strcmp(c->_schemas[pos],db)) found=1;
	}
	return (found ? pos : -1);
}

int	pgsql_search_table	(DBTOY_CONN conn, const char * db, const char * table)
{
	int pos, found=0, nrows;
	char q[128]="";
	PGresult * res;

	sprintf(q,"select tablename from pg_tables where schemaname='%s'",db);
	res = PQexec(GET_CONN(conn), q);
	nrows = PQntuples(res);
	for(pos=0; pos<nrows && !found; ++pos)
	{
		if(!strcmp(PQgetvalue(res,pos,0),table)) found=1;
	}
	PQclear(res);
	return (found ? pos : -1);
}

DBTOY_NAME	pgsql_error_msg(DBTOY_CONN conn)
{
	return (DBTOY_NAME)PQerrorMessage(GET_CONN(conn));
}

char**	pgsql_read_types (DBTOY_CONN conn, const char * db, const char * table,
									int * len)
{
	struct pg_conn * c = (struct pg_conn *) conn;
	int pos, ncols;
	long colnum;
	PGresult * res;
	int rfmt=0;

	char * cur, * nxt, q[1024], **types;
	sprintf(q,"select * from %s.%s limit 0", db, table);
	res = PQexec(GET_CONN(conn), q);
	ncols = PQnfields(res);
	*len=ncols;
	types = (char**)malloc(3*ncols*sizeof(char*));
	for (pos=0; pos<ncols; ++pos)
	{
		types[pos*3] =(char*)strdup(PQfname(res, pos));
		types[pos*3+1] =(char*)strdup(type_name(c,PQftype(res, pos)));
		types[pos*3+2] =(char*)strdup("");
	}
	PQclear(res);
	
	/* lookup pk infos */
	res = PQexecPrepared(GET_CONN(conn),
						 PKINFO, 1,
                         &table,
                         &rfmt, /* ignored */
                         &rfmt, 0);

	cur = PQgetvalue(res, 0, 0);
	while(cur) {
		nxt = strstr(cur," ");
		colnum = strtol(cur, nxt, 10);
		if(colnum > 0 && colnum <= ncols) {
			colnum-=1; /* starting from zero to access types */
			free(types[colnum*3+2]);
			types[colnum*3+2] = (char*)strdup("yes");
		}
		cur = nxt;
	}
	PQclear(res);
	
	return types;
}

DBTOY_RES	pgsql_read_data_start(DBTOY_CONN conn, const char * db, const char * table,
									const char * pars, int * len, int * n_cols)
{
	PGresult * res;
	struct pg_res * r;
	struct lt * l;

	l = lt_new_v("select * from ");
	lt_cat(l,db);
	lt_cat(l,".");
	lt_cat(l,table);
	if(pars) lt_cat(l,pars);

	res = PQexec(GET_CONN(conn),l->lt_text);
	r = make_res(res);
	lt_free(l);

	*len=r->_nrows;
	*n_cols=r->_ncols;
	return (DBTOY_RES)r;
}

DBTOY_RES	pgsql_read_data_start_from(DBTOY_CONN conn, const char * db, const char * table,
									const char * pars, int * len, int * n_cols, int from)
{
	PGresult * res;
	struct pg_res * r;
	struct lt * l;
	char limitbuf[64]="";

	l = lt_new_v("select * from ");
	lt_cat(l,db);
	lt_cat(l,".");
	lt_cat(l,table);
	if(pars) lt_cat(l,pars);
	sprintf(limitbuf," limit %d offset %d", BIGNUMBER, from);
	lt_cat(l,limitbuf);


	res = PQexec(GET_CONN(conn),l->lt_text);
	r = make_res(res);
	lt_free(l);

	*len=r->_nrows;
	*n_cols=r->_ncols;
	return (DBTOY_RES)r;
}

DBTOY_ROW pgsql_read_data_next(DBTOY_RES res)
{
	DBTOY_ROW row;
	int i;
	struct pg_res * r = (struct pg_res *) res;
	
	if(r->_offset >= r->_nrows) return NULL;
	
	row = (DBTOY_ROW) malloc(r->_ncols*sizeof(char*));
	for(i=0; i<r->_ncols; ++i) {
		row[i] = strdup(PQgetvalue(r->_res, r->_offset, i));
	}
	++r->_offset;
	
	return row;
}

void	pgsql_read_data_end(DBTOY_RES res)
{
	struct pg_res * r = (struct pg_res *) res;
	if(r) {
		PQclear(r->_res);
		free(r);
	}
}

void    pgsql_read_data_seek(DBTOY_RES res, int row)
{
	struct pg_res * r = (struct pg_res *) res;
	r->_offset=row;
}


/* private functions */
static void gen_cache(struct pg_conn * c)
{
	int i;
	PGresult * res;
	Oid textoid = 19;
	
	res = PQexec(GET_CONN(c), "select distinct schemaname from pg_tables");
	c->_sch_cnt=PQntuples(res);
	c->_schemas= (char ** ) malloc(sizeof(char*)*c->_sch_cnt);
	for ( i=0; i<c->_sch_cnt; ++i)
	{
		c->_schemas[i] = strdup(PQgetvalue(res, i, 0));
	}
	PQclear(res);
	
	res = PQexec(GET_CONN(c), "select typname, oid from pg_type");
	c->_typ_cnt=PQntuples(res);
	c->_types = (char ** ) malloc(sizeof(char*)*c->_typ_cnt);
	c->_oids = (Oid *) malloc(sizeof(Oid)*c->_typ_cnt); 
	for ( i=0; i<c->_typ_cnt; ++i)
	{
		c->_types[i] = strdup(PQgetvalue(res, i, 0));
		c->_oids[i] = strtol(PQgetvalue(res, i, 1),(char**)NULL,10);
	}	
	PQclear(res);
	
	res = PQprepare(GET_CONN(c),
                    PKINFO,
                    " select i.indkey from pg_class c, pg_index i where "
			"i.indisprimary=true and i.indrelid=c.oid and c.relname= $1 ",
                    1,
					&textoid);
	PQclear(res);
}

static struct pg_res * make_res(PGresult * res)
{
	struct pg_res * r = (struct pg_res *)malloc(sizeof(struct pg_res));
	r->_res = res;
	r->_offset = 0;
	r->_nrows = PQntuples(res);
	r->_ncols = PQnfields(res);
	return r;
}

static char * type_name(struct pg_conn * conn, Oid toid)
{
	int i;

	for( i=0; i<conn->_typ_cnt; ++i) {
		if(conn->_oids[i]==toid) return conn->_types[i];
	}
	return "unknown_oid";
}
