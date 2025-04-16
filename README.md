
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

Fix superblock reading on opening filesystem.
Finnish code for freeing pages so they are properly added to the linked list.

The filesystem is split into 1024 byte pages:

## general information

All information stored in the page headers is stored in big endian (network byte order).

## Superblock

This stores all the information about the filesystem. It is a 256 byte region at the start of the filesystem that contains in the order shown. The rest of the space is padded with 0s.
4 bytes of `uint32_t magic_number`
8 bytes of `uint64_t page_count`
8 bytes of `uint64_t first_free_page`

## Inode page

This stores all the information about a file/directory.

## Data page

Contains the data of a file/other object. Linked list style where it has a previous and a next pointer to any relevant continuation pages.
 
## Free page

the first one is pointed to in the header page, and they each point to the next free page. the header of one of these pages is as such:
1 byte of `uint8_t page_type = 1`
8 bytes of `uint64_t next_free_page_index`

A `(uint64_t)-1` in the next free page index is like the NULL at the end of a linked list, specifying there are no more after this node.

