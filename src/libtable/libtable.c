#include <stdlib.h>
#include <string.h>
#include "libtable.h"

//====== exported functions ======
TABLE *table_new(size_t size){
	//initialise everything
	TABLE *table = malloc(sizeof(TABLE));
	memset(table,0,sizeof(TABLE));
	table->size = size;
	table->array = malloc(sizeof(struct table_entry)*size);
	//setup all the empty pointers
	for (size_t i = 0; i < size; i++){
		memset(table->array+i,0,sizeof(struct table_entry));
		table->array[i].next_free_index = i+1;
	}
	table->array[size-1].next_free_index = -1;
	//return
	return table;
}
void table_delete(TABLE *table){
	free(table->array);
	free(table);
}
int table_allocate_index(TABLE *table){
	//check there is space
	if (table->first_free_index == -1) return -1;
	//find the next free index
	int next_free_index = table->first_free_index;
	//update the first free index to skip our allocated index
	table->first_free_index = table->array[next_free_index].next_free_index;
	return next_free_index;
}
void table_free_index(TABLE *table,int index){
	//add it to the chain of next pointers
	table->array[index].next_free_index = table->first_free_index;
	table->first_free_index = index;
}
void *table_get_data(TABLE *table,int index){
	return table->array[index].data;
}
void table_set_data(TABLE *table,int index,void *data){
	table->array[index].data = data;
}
