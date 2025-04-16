
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

The filesystem is split into 1024 byte pages:

## general information

All information stored in the page headers is stored in big endian (network byte order).
Each page has a header describing 

## Superblock page

This stores all the information about the filesystem. It is a 256 byte region at the start of the filesystem that contains in the order shown. The rest of the space is padded with 0s
4 bytes of `uint32_t magic_number`
8 bytes of `uint64_t page_count`
8 bytes of `uint64_t first_free_page`


## Inode page

This stores all the information about a file/directory.

## Data page

Contains the data of a file/other object. Linked list style where it has a previous and a next pointer to any relevant continuation pages
 
## Free page

 the first one is pointed to in the header page, and they each point to the next free page.
