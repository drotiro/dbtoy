#include "dbtoy.h"
#include "largetext.h"
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>

#include <mysql.h>

struct my_conn
{
	MYSQL *	_conn;
	char **	_schemas;
	int		_sch_cnt;
};


DBTOY_CONN	mydb_connect(const char* host, const char *login, const char *password);
void 		mysql_disconnect(DBTOY_CONN conn);
int 		mysql_count_dbs(DBTOY_CONN conn);
int 		mysql_count_tables(DBTOY_CONN conn, const char * db);
DBTOY_NAME	mysql_what_schema(DBTOY_CONN conn, int schema_num);
DBTOY_NAME	mysql_what_table(DBTOY_CONN conn, const char * db, int table_num);
int			mysql_search_schema	(DBTOY_CONN, const char * db);
int			mysql_search_table	(DBTOY_CONN, const char * db, const char * table);
char**		mysql_read_types	(DBTOY_CONN, const char * db, const char * table,
									int * len);
DBTOY_RES	mysql_read_data_start	(DBTOY_CONN, const char * db, const char * table, const char * q,
									int * len, int * ncols);
DBTOY_RES	mysql_read_data_start_from	(DBTOY_CONN, const char * db, const char * table, const char * q,
									int * len, int * ncols, int from);
void		mysql_read_data_end	(DBTOY_RES res);
DBTOY_ROW	mysql_read_data_next(DBTOY_RES res);
void		mysql_read_data_seek(DBTOY_RES res, int row);
DBTOY_NAME	mysql_error_msg(DBTOY_CONN conn);


#define BIGNUMBER 9999999

static void update_dbs_cache(struct my_conn * c, MYSQL_RES * res);

struct dbtoy_driver mysql_driver = 
{
	connect:		mydb_connect,
	disconnect: 	mysql_disconnect,
	count_dbs: 		mysql_count_dbs,
	count_tables:	mysql_count_tables,
	what_schema:	mysql_what_schema,
	what_table:		mysql_what_table,
	search_schema:	mysql_search_schema,
	search_table:	mysql_search_table,
	read_types:		mysql_read_types,
	read_data_start:		mysql_read_data_start,
	read_data_start_from:	mysql_read_data_start_from,
	read_data_end:	mysql_read_data_end,
	read_data_next: mysql_read_data_next,
	read_data_seek: mysql_read_data_seek,
	error_msg:		mysql_error_msg
};


#define GET_CONN(conn) (((struct my_conn*)conn)->_conn)

DBTOY_CONN mydb_connect(const char* host, const char *login, const char *password)
{
	struct my_conn * conn;
	MYSQL * db_conn = NULL;
	MYSQL_RES * res;

	db_conn = mysql_init(db_conn);
	db_conn = mysql_real_connect(db_conn, host, login,password,NULL,0, NULL,0);
	if(!db_conn) return NULL;

	conn = (struct my_conn *) malloc(sizeof( struct my_conn));
	conn->_conn=db_conn;
	conn->_schemas=NULL;
	res = mysql_list_dbs(db_conn,NULL);
	update_dbs_cache(conn,res);
	mysql_free_result(res);
	return conn;
}      
void mysql_disconnect(DBTOY_CONN conn)
{
	int i;
	struct my_conn * c = (struct my_conn *) conn;
	mysql_close(GET_CONN(conn));

	for( i=0; i< c->_sch_cnt; ++i) free(c->_schemas[i]);
	free(c->_schemas);
	free(c);
}

int mysql_count_dbs(DBTOY_CONN conn){
	return ((struct my_conn *)conn)->_sch_cnt;
}

int mysql_count_tables(DBTOY_CONN conn, const char * db){
	int nTab;
	MYSQL_RES * res;
	if(mysql_select_db(GET_CONN(conn),db)) return 0;
	res = mysql_list_tables(GET_CONN(conn),NULL);
	nTab = mysql_num_rows(res);
	mysql_free_result(res);
	return nTab;
}

DBTOY_NAME mysql_what_schema(DBTOY_CONN conn, int schema_num)
{
	char * name;
	MYSQL_RES * res;
	MYSQL_ROW row;
	struct my_conn * c = (struct my_conn *)conn;

	if(schema_num>=c->_sch_cnt || schema_num < 0) return NULL;
	name = (char*) malloc(strlen(c->_schemas[schema_num])+1);
	strcpy(name,c->_schemas[schema_num]);
	return name;
}

DBTOY_NAME mysql_what_table(DBTOY_CONN conn, const char * db, int table_num)
{
	char * name;
	MYSQL_RES * res;
	MYSQL_ROW row;
	mysql_select_db(GET_CONN(conn),db);
	res = mysql_list_tables(GET_CONN(conn),NULL);
	if(table_num>=mysql_num_rows(res) || table_num < 0) return NULL;
	mysql_data_seek(res,table_num);
	row = mysql_fetch_row(res);
	if(strlen(row[0])==0) {row[0]="null_record_0"; row[12]+=table_num;}
	name = (char*) malloc(strlen(row[0])+1);
	strcpy(name,row[0]);
	mysql_free_result(res);
	return name;
}

int	mysql_search_schema	(DBTOY_CONN conn, const char * db)
{
	struct my_conn * c = (struct my_conn *)conn;
	int pos, found=0, nrows=c->_sch_cnt;
	for(pos=0; pos < nrows && !found; ++pos)
	{
		if(!strcmp(c->_schemas[pos],db)) found=1;
	}
	return (found ? pos : -1);
}

int	mysql_search_table	(DBTOY_CONN conn, const char * db, const char * table)
{
	MYSQL_RES * res;
	MYSQL_ROW row;
	int pos, found=0, nrows;
	if(mysql_select_db(GET_CONN(conn),db)) return -1;
	res = mysql_list_tables(GET_CONN(conn),NULL);
	nrows = mysql_num_rows(res);
	for(pos=0; pos<nrows && !found; ++pos)
	{
		row = mysql_fetch_row(res);
		if(!strcmp(row[0],table)) found=1;
	}
	mysql_free_result(res);
	return (found ? pos : -1);
}

DBTOY_NAME	mysql_error_msg(DBTOY_CONN conn)
{
	return (DBTOY_NAME)mysql_error(GET_CONN(conn));
}

char**	mysql_read_types (DBTOY_CONN conn, const char * db, const char * table,
									int * len)
{
	struct my_conn * c = (struct my_conn *) conn;
	int pos, nrows;
	MYSQL_RES * res;
	MYSQL_ROW row;
	char q[1024]="show columns from ", **types;
	mysql_select_db(GET_CONN(conn),db);
	strcat(q,table);
	//res = mysql_list_fields(GET_CONN(conn),table,"%");
	mysql_query(GET_CONN(conn),q);
	res = mysql_store_result(GET_CONN(conn));
	nrows = mysql_num_rows(res);
	*len=nrows;
	types = (char**)malloc(3*nrows*sizeof(char*));
	for (pos=0; pos<nrows; ++pos)
	{
		row = mysql_fetch_row(res);
		types[pos*3] =(char*)strdup(row[0]);
		types[pos*3+1] =(char*)strdup(row[1]);
		types[pos*3+2] =(char*)strdup(row[3]);
	}
	mysql_free_result(res);
	return types;
}

DBTOY_RES	mysql_read_data_start(DBTOY_CONN conn, const char * db, const char * table,
									const char * pars, int * len, int * n_cols)
{
	int pos, nrows = 0 , ncols = 0, j, rc;
	MYSQL_RES * res;
	MYSQL_ROW row;
	struct lt * l;
	char **types = NULL;
	mysql_select_db(GET_CONN(conn),db);
	l = lt_new_v("select * from ");
	lt_cat(l,table);

	if(pars) lt_cat(l,pars);

	rc = mysql_query(GET_CONN(conn),l->lt_text);
	if(!rc) {
		res = mysql_store_result(GET_CONN(conn));
		nrows = mysql_num_rows(res);
		ncols = mysql_num_fields(res);
	} else {
		res = NULL;
	}
	lt_free(l);

	*len=nrows;
	*n_cols=ncols;
	return res;
}

DBTOY_RES	mysql_read_data_start_from(DBTOY_CONN conn, const char * db, const char * table,
									const char * pars, int * len, int * n_cols, int from)
{
	int pos, nrows = 0 , ncols = 0, j, rc;
	MYSQL_RES * res;
	MYSQL_ROW row;
	struct lt * l;
	char **types = NULL;
	char limitbuf[64]="";

	mysql_select_db(GET_CONN(conn),db);
	l = lt_new_v("select * from ");
	lt_cat(l,table);
	if(pars) lt_cat(l,pars);
	sprintf(limitbuf," limit %d,%d",from,BIGNUMBER);
	lt_cat(l,limitbuf);

	rc = mysql_query(GET_CONN(conn),l->lt_text);
	if(!rc) {
		res = mysql_store_result(GET_CONN(conn));
		nrows = mysql_num_rows(res);
		ncols = mysql_num_fields(res);
	} else {
		res = NULL;
	}
	lt_free(l);
	
	*len=nrows;
	*n_cols=ncols;
	return res;
}

DBTOY_ROW mysql_read_data_next(DBTOY_RES res)
{
	return mysql_fetch_row(res);
}

void	mysql_read_data_end(DBTOY_RES res)
{
	if(res) mysql_free_result((MYSQL_RES *)res);
}

void    mysql_read_data_seek(DBTOY_RES res, int row)
{
	mysql_data_seek(res,row);
}


/* private functions */
static void update_dbs_cache(struct my_conn * c, MYSQL_RES * res)
{
	int i;
	MYSQL_ROW row;
	c->_sch_cnt=mysql_num_rows(res);
	c->_schemas= (char ** ) malloc(sizeof(char*)*c->_sch_cnt);
	for ( i=0; i<c->_sch_cnt; ++i)
	{
		row = mysql_fetch_row(res);
		c->_schemas[i] = (char *) malloc(strlen(row[0])+1);
		strcpy(c->_schemas[i],row[0]);
	}
}
