#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "libbst.h"
int node_comparison(void *a, void *b){
	return *(int *)b-*(int *)a;
}
void print_data(void *data){
	printf("%d\n",*(int *)data);
}
int main(int argc, char **argv){
	struct bst_user_functions functions = {
		.datacmp = node_comparison,
		.free_data = free,
		.print_data = print_data
	};
	BST *bst = bst_new(&functions);
	for (int i = 0; i < 10; i++){
		int *data = malloc(sizeof(int));
		*data = random()%100;
		printf("%d\n",*data);
		bst_new_node(bst,data);
	}
	int *data = malloc(sizeof(int));
	*data = 22;
	bst_new_node(bst,data);
	printf("\n");
	bst_delete_node(bst,bst->root->left->left);
	bst_print_nodes_inorder(bst);
	bst_delete(bst);
}
