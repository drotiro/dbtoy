#include "xml_format.h"

#include <stdio.h>
#include <syslog.h>

#define VAL 1
#define ONOFF 2

static char * XMLSTART = "<?xml version=\"1.0\" ?>\n"
			"<!-- Created by DBToy http://freshmeat.net/projects/dbtoy -->\n";
/* types */
static char * TYPES_PROLOGUE = "<columns ";
static char * TYPES_EPILOGUE = "</columns>\n";
static char * COL_START = "\t<column ";
static char * ATTRS[] = {"sqlType","primaryKey"};
static int ATTR_TYPES[] = {VAL, ONOFF};
static char * COL_END = "</column>\n";
#define NATTRS (sizeof(ATTRS)/sizeof(ATTRS[0]))
#define ROWLEN (NATTRS+1)
/* data */
static char * DATA_PROLOGUE = "<rows ";
static char * DATA_EPILOGUE = "\n</rows>\n";
static char * DATA_ERROR_START = "\n\t<error>";
static char * DATA_ERROR_END = "</error>";
static char * ROW_START = "\n\t<row>";
static char * ROW_END = "\n\t</row>";
static char * DATA_START = "\n\t\t<";
static char * DATA_END = "</";
static char * TAG_CLOSE=">";
static char * CDATA_START = "<![CDATA[";
static char * CDATA_END = "]]>";

char * format_types(char ** types, int n_types, const char * prolog, char * name, char * tabname)
{
		int i, j;
		char * res;
		struct lt * l = lt_new();

		lt_cat(l,XMLSTART);
		if(prolog) {
			lt_cat(l,prolog);
			lt_cat(l,"\n");
		}		
		lt_cat(l,TYPES_PROLOGUE);
		lt_cat(l,"schema=\"");
		lt_cat(l,name);
		lt_cat(l,"\" table=\"");
		lt_cat(l,tabname);
		lt_cat(l,"\">\n");
		for(i = 0; i < n_types; ++i)
		{
		  lt_cat(l,COL_START);
			/*attrs...*/
			for(j =0; j< NATTRS; ++j)
			{
				switch(ATTR_TYPES[j]) {
					case VAL:
						lt_cat(l,ATTRS[j]);
						lt_cat(l,"=\"");
						lt_cat(l,types[i*ROWLEN+j+1]);
						lt_cat(l,"\" ");
						break;
					case ONOFF:
						if(*types[i*ROWLEN+j+1]) {
							lt_cat(l,ATTRS[j]);
							lt_cat(l,"=\"yes\"");
						}
						break;
				}
			}
			lt_cat(l,">");
			lt_cat(l,types[i*ROWLEN]);
			lt_cat(l,COL_END);
		}
		lt_cat(l,TYPES_EPILOGUE);
		res = lt_text(l);
		lt_free(l);
		return res;
}

void format_data_start(struct lt * l, const char * prolog, char * xname, char * xtabname)
{
		lt_cat(l,XMLSTART);
		if(prolog) {
			lt_cat(l,prolog);
			lt_cat(l,"\n");
		}
		lt_cat(l,DATA_PROLOGUE);
		lt_cat(l,"schema=\"");
		lt_cat(l,xname);
		lt_cat(l,"\" table=\"");
		lt_cat(l,xtabname);
		lt_cat(l,"\">");
}

void format_data_end(struct lt * l)
{
                lt_cat(l,DATA_EPILOGUE);
}

void format_data_error(struct lt * l, DBTOY_NAME msg)
{
	lt_cat(l, DATA_ERROR_START);
	lt_cat(l, msg);
	lt_cat(l, DATA_ERROR_END);
}

void format_data_next(struct lt * l, DBTOY_ROW row, int len, int cdata[], char * label[])
{
	int j;
	
	lt_cat(l,ROW_START);
	for(j=0; j<len; ++j)
	{
		lt_cat(l,DATA_START); lt_cat(l,label[j]); lt_cat(l,TAG_CLOSE);
		if(cdata[j]) lt_cat(l,CDATA_START);
		lt_cat(l,row[j]);
		if(cdata[j]) lt_cat(l,CDATA_END);
		lt_cat(l,DATA_END); lt_cat(l,label[j]); lt_cat(l,TAG_CLOSE);
	}
        lt_cat(l,ROW_END);
}
