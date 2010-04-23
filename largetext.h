#ifndef LARGETEXT_H
#define LARGETEXT_H

struct lt {
	char * lt_text;
	long lt_size;
	long lt_len;
};

struct lt * lt_new();
struct lt * lt_new_v(const char *);
void lt_free(struct lt* t);
char * lt_text(struct lt * l);
void lt_cat(struct lt* t, const char * text);
void lt_merge(struct lt* t, struct lt * tm);

#endif /*LARGETEXT_H*/
