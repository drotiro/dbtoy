#include "largetext.h"
#include "stdio.h"
#include <syslog.h>
#include <stdlib.h>
#include <string.h>

/* private stuff ...*/
void lt_docat(struct lt * l,const char * text, long len);
static const long START_SIZE = 8192;
/* -----------------*/

struct lt * lt_new() {
	struct lt * l = (struct lt *)malloc(sizeof(struct lt));
	l->lt_text=(char *)malloc(START_SIZE);
	l->lt_text[0]=0;
	l->lt_len=0;
	l->lt_size=START_SIZE;
	return l;
}

struct lt * lt_new_v(const char * text) {
	struct lt * l = lt_new();
	lt_cat(l,text);
	return l;
}

void lt_free(struct lt* t) {
	if(t->lt_text) free(t->lt_text);
	free(t);
}

char * lt_text(struct lt * l) {
	char * res = (char*)malloc(l->lt_len+1);
	if(!res) return NULL;
	strncpy(res,l->lt_text,l->lt_len);
	res[l->lt_len]=0;
	return res;
}

void lt_cat(struct lt* t, const char * text) {
	if(!text) return;
	lt_docat(t, text, strlen(text));
}

void lt_merge(struct lt* t, struct lt * tm) {
	lt_docat(t,tm->lt_text, tm->lt_len);
}


void lt_docat(struct lt * l,const char * text, long len) {
	if(!len) return;
	if(l->lt_len+len>=l->lt_size) {
		/* devo allargare il buffer */
		long incr = (len >= START_SIZE ? len : START_SIZE);
		l->lt_size+=incr;
		l->lt_text = (char *) realloc(l->lt_text,l->lt_size);
		if(!l->lt_text) {
			syslog(LOG_ERR,"Out of memory allocating %ld bytes",l->lt_size);
		}
	}
	strcat(l->lt_text+l->lt_len,text);
 	l->lt_len+=len;
	l->lt_text[l->lt_len]=0;
}
