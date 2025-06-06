
# About

This is a userspace filesystem, utilising `fuse`, of my own design.
The s in sfs stands for small, simple or any other word starting with s you can thing of.
It is slow and clumsy, and not recommended to ever be uses practically. It is just more for me to try fuse out and experiment

# Using and installing

Use the `-h` / `--help` options on the tools to see how to use them
`make all` to make all the tools
There is an `mksfs` command which will create a file containing an empty filesystem of a requested size.
There is the mounting tool `mountsfs`

## Mountsfs

### Arguments

Usage: `mountsfs [options] <filesystem image> <mountpoint>`

Here is a list of arguments:
 - `-h / --help` : display help text
 - `-fh` : display fuse help
 - `-f<fuse argument (without '-')>` : pass argument to fuse (more information in `Passing fuse arguments` section)

### Passing fuse arguments

This can be done through using `-f<fuse argument>`, e.g. passing `-omodules=subdir` would become `-fomodules=subdir`

# Design of the filesystem

## To-do:

```
verify inode_get_pointer and inode_set_pointer work
fix save/restore of next free page being one page behind

implement inodes being able to point to continuation pages
implement inodes storing data
	test sfs_inode_add_pointer
	create sfs_inode_create
	create sfs_inode_destroy
implement the readdir for the fuse fs
implement the mkdir for the fuse fs
implement the rmdir for the fuse fs
```

The filesystem is split into 1024 byte pages:

## general information

All information stored in the page headers is stored in big endian (network byte order).
All functions that take in structs to write to a block header or other such thing will deal with correcting endianness for you, likewise reading a struct from a page header will automatically convert to your machines endianness.

## Superblock

This stores all the information about the filesystem. It is a 256 byte region at the start of the filesystem that contains in the order shown. The rest of the space is padded with 0s.
4 bytes of `uint32_t magic_number`
8 bytes of `uint64_t page_count`
8 bytes of `uint64_t first_free_page`
8 bytes of `uint64_t current_generation_number`

## Inode page

This stores all the information about a file/directory. Inodes can be spread across multiple different pages.
The way they work is they have a header region describing them, then the rest of the page is `uint64_t` pointers to relevant pages. In the case of a file, data pages, in order for storing the contents of the file. In the case of a directory, other inodes, that are in that directory.
When the inode spans across multiple pages, the header region is a duplicate, except for the next and previous pointers. You SHOULD NOT rely on the duplicate information on continuation pages being up to date, or give a continuation page index as a replacement for an inode index. ALWAYS give the base inode page rather then a continuation page index. (except `sfs_read_inode_page` and its write counter part)
The next and previous pointers may be `0xFFFFFFFF / (uint64_t)-1` indicating there are no further nodes.
On the root node, the parent pointer points to itself.
Every time an inode is created, the current generation number is set as the generation number and then incremented by one.

The header region contains:
8 bytes of `uint64_t page` (current page where the inode resides (does not change for continuation nodes))
8 bytes of `uint64_t parent_inode_pointer`
8 bytes of `uint64_t pointer_count` (for storing the number of data pages or other inodes it points to)
8 bytes `uint64_t next_page`
8 bytes `uint64_t previous_page`
8 bytes of `uint64_t generation_number` (unique for every inode ever created, even if it shares an inode number with a deleted inode)
1 bytes of `uint8_t inode_type`
256 bytes of a null terminated name

The data region of the inode contains all the pointers to relevant pages. The size of this region depends on the page size
X bytes of padding to align to a multiple of 8
8 bytes of `uint64_t page_pointer` (e.g  on bytes 304-311)
...
8 bytes of `uint64_t page_pointer` 

### Root directory

Always stored on the second page (index 1).

## Data page

Contains the data of a file/other object. Linked list style where it has a previous and a next pointer to any relevant continuation pages.
 
## Free page

The first one is pointed to in the header page, and they each point to the next free page. The header of one of these pages is as such:
1 byte of `uint8_t page_type = 1`
8 bytes of `uint64_t next_free_page_index`

A `(uint64_t)-1` in the next free page index is like the NULL at the end of a linked list, specifying there are no more after this node.

# Design of the FUSE driver

## Inode lookup count implementation

This is stored in a binary search tree and sorted by inode.
```
struct referenced_inode {
	uint64_t inode;
	int lookup_count;
	void *destruction_data;
	//if null, no action will be taken, else this will be called when lookup count reaches 0
	//                           destruction_data  inode
	void (*destruction_function)(void *,           uint64_t);
};
```

Calling `int increase_inode_reference_count(uint64_t inode,uint64_t count)` will increase the reference count (and create the relevant structure and data).
Calling `int reduce_inode_referebce_count`

## Opening and reading directories implementation

Calling opendir simply caches the current contents of the directory at that instant, stored in an array of:
```
struct opendir {
	uint64_t opened_dir_inode;
	size_t inode_count;
	uint64_t inodes[];
};
```
All future calls to readdir using the same handle will read into the cache, in order to bypass race conditions where an inode in a directory is removed after some of the directory is read, causing it to skip over unrelated entries due to the nature of how inodes are deleted.
Closing the dir simply frees the allocated space.

## Open Inode tracker

This tracks all the open inodes. Stored in a binary search tree (sorted by inode)

# Binary search tree library

The library operates on the principles of data being pointed to in a void pointer in each node. It requires you to pass your own functions as parameters such as for comparing if a node is equal to a value or turning a data pointer into an integer value

## Creating a new tree with `bst_new()`

This function allocates and returns a `BST *` that can then be passed to other functions in the library. It takes one argument of user defined functions, for which the struct can be found in the relevant data structures section.

### `int datacmp(void *a,void *b)`

This function will be passed 2 data pointers, and is expected to return:
 - a value less than 0 if a < b
 - a value greater than 0 if a > b
 - 0 if a == b
It is up to the user how they want to decide how to classify if their data is less than, greater than, or equal to another piece of their given data. E.g. If the data is a struct, some sort of value in the struct can be compared, or if it is an integer pointer, a can simply be subtracted from b.

### `void free_data(void *data)`

This is called when the tree is destroyed, and given a piece of data, should free it if necessary. You may provide NULL instead of a function, in which case it will not be called.

## Destroying a tree

You can call `bst_delete_all_nodes()` to delete all the nodes in a tree but keep the tree itself, or `bst_delete()` to free any data associated with the tree. `bst_delete()` calls `bst_delete_all_nodes()`.

## Adding nodes with `bst_new_node()`

Given a data pointer, it will insert it into the correct position in the tree

## Relevant data structures


```
struct bst_node {
	void *data;
	struct bst_node *parent;
	struct bst_node *left;
	struct bst_node *right;
};
struct bst { //typedef'd to "BST"
	struct bst_node *root;
	struct bst_user_functions *user_functions;
};
struct bst_user_functions {
	//should return less then 0 for a is < b, 0 for a == b, and > 0 for a > b
	int (*datacmp)(void *,void *);
	void (*free_data)(void *);
	void (*print_data)(void *);
};
```

