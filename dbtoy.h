#ifndef DBTOYD_H
#define DBTOYD_H

#include <stddef.h>

typedef void * DBTOY_CONN;
typedef char * DBTOY_NAME;
typedef void * DBTOY_RES;
typedef char ** DBTOY_ROW;

struct dbtoy_driver {
	/* connection funcs */
	DBTOY_CONN	(*connect)(const char* host, const char *login, const char *password);
	void		(*disconnect)	(DBTOY_CONN conn);

	/* count commands */
	int			(*count_dbs)	(DBTOY_CONN);
	int			(*count_tables)	(DBTOY_CONN,const char * db);

	/* search name by pos */
	DBTOY_NAME	(*what_schema)	(DBTOY_CONN,int schema_num);
	DBTOY_NAME	(*what_table)	(DBTOY_CONN,const char * db, int table_num);
	/* search pos by name */
	int			(*search_schema)	(DBTOY_CONN, const char * db);
	int			(*search_table)	(DBTOY_CONN, const char * db, const char * table);

	/* read commands */
	char**		(*read_types)	(DBTOY_CONN, const char * db, const char * table,
									int * len);
	DBTOY_RES	(*read_data_start)	(DBTOY_CONN, const char * db, const char * table,const char * q,
									int * len, int * n_cols);
	DBTOY_RES	(*read_data_start_from)	(DBTOY_CONN, const char * db, const char * table,const char * q,
									int * len, int * n_cols, int from);
	DBTOY_ROW	(*read_data_next)	(DBTOY_RES);
	void		(*read_data_end)	(DBTOY_RES);
	DBTOY_NAME	(*error_msg)		(DBTOY_CONN);
	
	/* optional funcs */
	DBTOY_CONN	(*connect_xtra)(const char* host, const char *login, const char *password,
								void * xtra_info);
	void    	(*read_data_seek)	(DBTOY_RES, int);
};


#endif
/* DBTOYD_H */
