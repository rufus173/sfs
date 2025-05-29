#ifndef _LIBTABLE_H
#define _LIBTABLE_H

#include <stddef.h>

//====== types ======
struct table_entry {
	int next_free_index;
	void *data;
};
struct table {
	int first_free_index;
	size_t size;
	struct table_entry *array;
};
typedef struct table TABLE;

//====== functions ======

TABLE *table_new(size_t size);
void table_delete(TABLE *table);
int table_allocate_index(TABLE *table);
void *table_get_data(TABLE *table,int index);
void table_free_index(TABLE *table,int index);
void table_set_data(TABLE *table,int index,void *data);

#endif
