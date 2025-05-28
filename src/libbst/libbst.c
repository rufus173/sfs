#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libbst.h"

//====== static functions ======

static int _delete_all_nodes_recursive(BST *bst,struct bst_node *node){
	//base case
	if (node == NULL) return 0;
	//recursive step
	_delete_all_nodes_recursive(bst,node->left);
	_delete_all_nodes_recursive(bst,node->right);
	//free the node and user data
	if (bst->user_functions->free_data != NULL) bst->user_functions->free_data(node->data);
	free(node);
	
	//always return success
	return 0;
}

static void _recursive_print_inorder(BST *bst,struct bst_node *node){
	//base case
	if (node == NULL) return;
	//inorder traversal
	_recursive_print_inorder(bst,node->left);
	bst->user_functions->print_data(node->data);
	_recursive_print_inorder(bst,node->right);
}

//====== exported functions ======

BST *bst_new(struct bst_user_functions *user_functions){
	//create the bst struct
	BST *bst = malloc(sizeof(BST));
	memset(bst,0,sizeof(BST));
	bst->root = NULL;
	//copy the user functions in
	bst->user_functions = malloc(sizeof(struct bst_user_functions));
	memcpy(bst->user_functions,user_functions,sizeof(struct bst_user_functions));
	return bst;
}

int bst_delete(BST *bst){
	//traverse tree and free any remaining nodes
	bst_delete_all_nodes(bst);

	//free the user functions
	free(bst->user_functions);
	//delete the struct itself
	free(bst);
}

int bst_delete_all_nodes(BST *bst){
	_delete_all_nodes_recursive(bst,bst->root);
	bst->root = NULL;
	return 0;
}
int bst_new_node(BST *bst,void *data){
	//allocate the node
	struct bst_node *node = malloc(sizeof(struct bst_node));
	memset(node,0,sizeof(struct bst_node));
	node->data = data;
	//memset already does this but just to be explicit
	node->left = NULL;
	node->right = NULL;

	//====== if there is no root node just set it as the new root node ======
	if (bst->root == NULL){
		bst->root = node;
	}
	//====== fit it in the correct place ======
	else {
		for (struct bst_node *current_node = bst->root;;){
			if (bst->user_functions->datacmp(current_node->data,data) <= 0){
				if (current_node->left == NULL){
					current_node->left = node;
					break;
				}
				else current_node = current_node->left;
			}
			else if (bst->user_functions->datacmp(current_node->data,data) > 0){
				if (current_node->right == NULL){
					current_node->right = node;
					break;
				}
				else current_node = current_node->right;
			}
		}
	}
}
void bst_print_nodes_inorder(BST *bst){
	_recursive_print_inorder(bst,bst->root);
}
