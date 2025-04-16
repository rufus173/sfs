
# About

This is a userspace filesystem, utilising `fuse`, of my own design.
It is named scrapfs because its useless and should probably be removed, like that cup sitting on your desk.
It is slow and clumsy, and not recommended to ever be uses practically. It is just more for me to try fuse out and experiment

# Using and installing

Use the `-h` / `--help` options on the tools to see how to use them
`make all` to make all the tools
There is an `mkscrapfs` command which will create a file containing an empty filesystem of a requested size.
There is the mounting tool `mountscrapfs`

# Design of the filesystem

The filesystem is split into 1024 byte pages:
 - header page
 this stores all the information about the filesystem
 - inode page
 this stores all the information about a file/directory
 - data page
 contains the data of a file/other object. Linked list style where it has a previous and a next pointer to any relevant continuation pages
 - free page
 the first one is pointed to in the header page, and they each point to the next free page.
