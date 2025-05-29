#ifndef _LIBBST_H
#define _LIBBST_H

//====== data types ======
struct bst {
	struct bst_node *root;
	struct bst_user_functions *user_functions;
};
typedef struct bst BST;

struct bst_node {
	void *data;
	struct bst_node *parent;
	struct bst_node *left;
	struct bst_node *right;
};
struct bst_user_functions {
	//should return less then 0 for a is < b, 0 for a == b, and > 0 for a > b
	int (*datacmp)(void *,void *);
	void (*free_data)(void *);
	void (*print_data)(void *);
};

//====== functions ======
BST *bst_new(struct bst_user_functions *user_functions);
int bst_delete(BST *bst);
int bst_delete_all_nodes(BST *bst);
int bst_new_node(BST *bst,void *data);
void bst_print_nodes_inorder(BST *bst);
int bst_delete_node(BST *bst,struct bst_node *node);
struct bst_node *bst_find_node(BST *bst,void *data);

#endif
