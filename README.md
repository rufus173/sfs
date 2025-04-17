
# About

This is a userspace filesystem, utilising `fuse`, of my own design.
The s in sfs stands for small, simple or any other word starting with s you can thing of.
It is slow and clumsy, and not recommended to ever be uses practically. It is just more for me to try fuse out and experiment

# Using and installing

Use the `-h` / `--help` options on the tools to see how to use them
`make all` to make all the tools
There is an `mksfs` command which will create a file containing an empty filesystem of a requested size.
There is the mounting tool `mountsfs`

# Design of the filesystem

## todo:

Add the creation of a root inode to the mkfs.sfs tool
finnish the update_inode_header function and make a corresponding read_inode_header function

The filesystem is split into 1024 byte pages:

## general information

All information stored in the page headers is stored in big endian (network byte order).

## Superblock

This stores all the information about the filesystem. It is a 256 byte region at the start of the filesystem that contains in the order shown. The rest of the space is padded with 0s.
4 bytes of `uint32_t magic_number`
8 bytes of `uint64_t page_count`
8 bytes of `uint64_t first_free_page`

## Inode page

This stores all the information about a file/directory. Inodes can be spread across multiple different pages.
The way they work is they have a header region describing them, then the rest of the page is `uint64_t` pointers to relevant pages. In the case of a file, data pages, in order for storing the contents of the file. In the case of a directory, other inodes, that are in that directory.
When the inode spans across multiple pages, the header region is a duplicate, except for the next and previous pointers.
The next and previous pointers may be `0xFFFFFFFF / (uint64_t)-1` indicating there are no further nodes.
On the root node, the parent pointer points to itself.

The header region contains:
1 byte of `uint8_t page_type = 2`
1 bytes of `uint8_t inode_type`
8 bytes of `uint64_t page` (current page where the inode resides (does not change for continuation nodes))
8 bytes of `uint64_t parent_inode_pointer`
8 bytes of `uint64_t pointer_count` (for storing the number of data pages or other inodes it points to)
8 bytes `uint64_t next_page`
8 bytes `uint64_t previous_page`
256 bytes of a null terminated name

The data region of the inode contains all the pointers to relevant pages. The size of this region depends on the page size
X bytes of padding to align to a multiple of 8
8 bytes of `uint64_t page_pointer` (e.g  on bytes 304-311)
...
8 bytes of `uint64_t page_pointer` 

## Data page

Contains the data of a file/other object. Linked list style where it has a previous and a next pointer to any relevant continuation pages.
 
## Free page

The first one is pointed to in the header page, and they each point to the next free page. The header of one of these pages is as such:
1 byte of `uint8_t page_type = 1`
8 bytes of `uint64_t next_free_page_index`

A `(uint64_t)-1` in the next free page index is like the NULL at the end of a linked list, specifying there are no more after this node.

