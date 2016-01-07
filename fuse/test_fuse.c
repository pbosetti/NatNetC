//
//  test_fuse.c
//  NatNetCLI
//
//  Created by Paolo Bosetti on 04/01/16.
//  Copyright © 2016 UniTN. All rights reserved.
//

/*
 FUSE: Filesystem in Userspace
 Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
 
 This program can be distributed under the terms of the GNU GPL.
 See the file COPYING.
 */

/** @file
 *
 * hello.c - minimal FUSE example featuring fuse_main usage
 *
 * \section section_compile compiling this example
 *
 * gcc -Wall hello.c `pkg-config fuse3 --cflags --libs` -o hello
 *
 * \section section_usage usage
 \verbatim
 % mkdir mnt
 % ./hello mnt        # program will vanish into the background
 % ls -la mnt
 total 4
 drwxr-xr-x 2 root root      0 Jan  1  1970 ./
 drwxrwx--- 1 root vboxsf 4096 Jun 16 23:12 ../
 -r--r--r-- 1 root root     13 Jan  1  1970 hello
 % cat mnt/hello
 Hello World!
 % fusermount -u mnt
 \endverbatim
 *
 * \section section_source the complete source
 * \include hello.c
 */


#define FUSE_USE_VERSION 30

//#include <config.h>

#include <fuse.h>
#include <fuse/fuse_common.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>

typedef enum fuse_readdir_flags fuse_readdir_flags_t;

static const char *hello_str = "Hello World!\n";
static const char *hello_path = "/hello";
static const char *time_path = "/time";

static const char *frame_path = "/frame";
static const char *info_path = "/info";

struct fuse_status {
  size_t time_size, frame_size, info_size;
};
static struct fuse_status hello_status = {
  .time_size = 1024,
};


static int hello_getattr(const char *path, struct stat *stbuf)
{
  int res = 0;
  struct timeval tv;
  gettimeofday(&tv, NULL);

  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  }
  else if (strcmp(path, "/frame") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  }
  else if (strcmp(path, "/info") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  }
  else if (strcmp(path, "/data") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  }
  else if (strcmp(path, time_path) == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = 1024;
#ifndef __linux__
    stbuf->st_mtimespec.tv_sec = tv.tv_sec;
    stbuf->st_mtimespec.tv_nsec = tv.tv_usec * 1000;
    stbuf->st_atimespec.tv_sec = tv.tv_sec;
    stbuf->st_atimespec.tv_nsec = tv.tv_usec * 1000;
    stbuf->st_ctimespec.tv_sec = tv.tv_sec;
    stbuf->st_ctimespec.tv_nsec = tv.tv_usec * 1000;
#else
    stbuf->st_mtim.tv_sec = tv.tv_sec;
    stbuf->st_mtim.tv_nsec = tv.tv_usec * 1000;
    stbuf->st_atim.tv_sec = tv.tv_sec;
    stbuf->st_atim.tv_nsec = tv.tv_usec * 1000;
    stbuf->st_ctim.tv_sec = tv.tv_sec;
    stbuf->st_ctim.tv_nsec = tv.tv_usec * 1000;
#endif 
  }

  else if (strcmp(path, hello_path) == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = strlen(hello_str);
  }
  else {
    res = -ENOENT;
  }
  return res;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
  (void) offset;
  (void) fi;
  
  if (strcmp(path, "/") != 0)
    return -ENOENT;
  
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  filler(buf, "data", NULL, 0);
  filler(buf, frame_path + 1, NULL, 0);
  filler(buf, info_path + 1, NULL, 0);
  filler(buf, time_path + 1, NULL, 0);

  filler(buf, hello_path + 1, NULL, 0);
  
  return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{
  fi->direct_io = 1;
  if (strcmp(path, time_path) == 0) {

  }
  else if (strcmp(path, hello_path) == 0) {
    fi->direct_io = 0;
  }
  else {
    return -ENOENT;
  }
  
  if ((fi->flags & 3) != O_RDONLY)
    return -EACCES;
  
  return 0;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
  size_t len;
  (void) fi;


  if(strcmp(path, hello_path) == 0) {
    
    len = strlen(hello_str);
    if (offset < len) {
      if (offset + size > len)
        size = len - offset;
      memcpy(buf, hello_str + offset, size);
    } else
      size = 0;
    
    return (int)size;
  }
  else if (strcmp(path, time_path) == 0) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double t = (double)(tv.tv_sec) + ((double)(tv.tv_usec) / 1.0E6);
    char loc_buf[1024];
    size_t len;
    sprintf(loc_buf, "---\nfloat: %.6f\nsec: %ld\nusec: %ld\n", t, tv.tv_sec, (long)tv.tv_usec);
    len = strlen(loc_buf);
    if (offset < len) {
      if (offset + size > len)
        size = len - offset;
      memcpy(buf, loc_buf + offset, size);
      hello_status.time_size = size;
    }
    else {
      size = 0;
    }
    
    return (int)size;
  }
  return -ENOENT;
}


static struct fuse_operations hello_oper = {
  .getattr	= hello_getattr,
  .readdir	= hello_readdir,
  .open		= hello_open,
  .read		= hello_read
};


int main(int argc, char *argv[])
{
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  
  fuse_opt_parse(&args, NULL, NULL, NULL);
  fuse_opt_add_arg(&args, "-o direct_io");

  return fuse_main(argc, argv, &hello_oper, NULL);
}