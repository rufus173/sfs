#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "libtable.h"

int main(){
	TABLE *table = table_new(10);
	for (int i = 0; i < 10; i++){
		printf("%d\n",table_allocate_index(table));
	}
	for (int i = 0; i < 10; i++){
		table_free_index(table,i);
	}
	for (int i = 0; i < 10; i++){
		printf("%d\n",table_allocate_index(table));
	}
	table_delete(table);
}
