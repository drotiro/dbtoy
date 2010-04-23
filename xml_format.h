#ifndef XML_FORMAT_H
#define XML_FORMAT_H

#include <unistd.h>
#include <fcntl.h>
#include "largetext.h"
#include "dbtoy.h"

char * format_types(char ** types, int n_types, const char * prolog, char * name, char * tabname);
/*char * format_data(char ** types, char ** aux, int n_rows, int n_cols, size_t size, off_t offset);*/
void format_data_start(struct lt *, const char * prolog, char * name, char * tabname);
void format_data_next(struct lt *, DBTOY_ROW row, int len, int cdata[], char * label[]);
void format_data_error(struct lt *, DBTOY_NAME msg);
void format_data_end(struct lt *);

#endif /*XML_FORMAT_H*/
