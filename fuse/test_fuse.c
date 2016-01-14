//
//  test_fuse.c
//  NatNetCLI
//
//  Created by Paolo Bosetti on 04/01/16.
//  Copyright © 2016 UniTN. All rights reserved.
//

#define FUSE_USE_VERSION 30
#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_MAX_LEN 64

#include <stdlib.h>
#include <fuse.h>
#include <fuse/fuse_common.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <lua.h>
#include <regex.h>

typedef enum fuse_readdir_flags fuse_readdir_flags_t;

static char *version_str;
static const char *version_path = "/info/version.txt";
static const char *time_path = "/time.yml";

static const char *frame_path = "/frame";
static const char *info_path = "/info";

struct fuse_status {
  size_t time_size, frame_size, info_size;
};
static struct fuse_status NatNet_status = {
  .time_size = 0,
  .frame_size = 0,
  .info_size = 0
};


static int NatNet_getattr(const char *path, struct stat *stbuf)
{
  int res = 0;
  memset(stbuf, 0, sizeof(struct stat));

#ifdef __linux__
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  stbuf->st_mtim.tv_sec = tv.tv_sec;
  stbuf->st_mtim.tv_nsec = tv.tv_nsec;
  stbuf->st_atim.tv_sec = tv.tv_sec;
  stbuf->st_atim.tv_nsec = tv.tv_nsec;
  stbuf->st_ctim.tv_sec = tv.tv_sec;
  stbuf->st_ctim.tv_nsec = tv.tv_nsec;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  stbuf->st_mtimespec.tv_sec = tv.tv_sec;
  stbuf->st_mtimespec.tv_nsec = tv.tv_usec * 1000;
  stbuf->st_atimespec.tv_sec = tv.tv_sec;
  stbuf->st_atimespec.tv_nsec = tv.tv_usec * 1000;
  stbuf->st_ctimespec.tv_sec = tv.tv_sec;
  stbuf->st_ctimespec.tv_nsec = tv.tv_usec * 1000;
  stbuf->st_birthtimespec.tv_sec = tv.tv_sec;
  stbuf->st_birthtimespec.tv_nsec = tv.tv_usec * 1000;
#endif

  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0555;
    stbuf->st_nlink = 2;
  }
  else if (strcmp(path, "/frame") == 0) {
    stbuf->st_mode = S_IFDIR | 0555;
    stbuf->st_nlink = 2;
  }
  else if (strcmp(path, "/info") == 0) {
    stbuf->st_mode = S_IFDIR | 0555;
    stbuf->st_nlink = 2;
  }
  else if (strcmp(path, "/data") == 0) {
    stbuf->st_mode = S_IFDIR | 0555;
    stbuf->st_nlink = 2;
  }
  else if (strcmp(path, time_path) == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = NatNet_status.time_size;
  }

  else if (strcmp(path, version_path) == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = strlen(version_str);
  }
  else {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = 0;
  }
  return res;
}

static int NatNet_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
  (void) offset;
  (void) fi;
  
  if (strcmp(path, "/") == 0) {
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, "data", NULL, 0);
    filler(buf, frame_path + 1, NULL, 0);
    filler(buf, info_path + 1, NULL, 0);
    filler(buf, time_path + 1, NULL, 0);
  }
  else if (strcmp(path, "/data") == 0) {
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
  }
  else if (strcmp(path, "/frame") == 0) {
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, "test.txt", NULL, 0);
  }
  else if (strcmp(path, "/info") == 0) {
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, version_path + 6, NULL, 0);
  }
  else {
    return -ENOENT;
  }
  return 0;
}

static int NatNet_open(const char *path, struct fuse_file_info *fi)
{
  fi->direct_io = 1;
  if (strcmp(path, time_path) == 0) {

  }
  else if (strcmp(path, version_path) == 0) {

  }
  else if (strcmp(path, frame_path) == 0) {
    
  }
  else {
    //return -ENOENT;
  }
  
  if ((fi->flags & 3) != O_RDONLY)
    return -EACCES;
  
  return 0;
}


static int NatNet_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
  size_t len;
  (void) fi;
  size_t nlevels = 3;
  size_t i = 0;
  int ret = 0;
  
  char **path_components = calloc(nlevels, sizeof(char *));
  char *tmp_path = calloc(strlen(path), sizeof(char));
  strncpy(tmp_path, path, strlen(path));
  
  for (i = 0; i < nlevels; i++) {
    if ((path_components[i] = strsep(&tmp_path, "/")) == NULL)
      break;
  }
  
  if(strcmp(path, version_path) == 0) {
    
    len = strlen(version_str);
    if (offset < len) {
      if (offset + size > len)
        size = len - offset;
      memcpy(buf, version_str + offset, size);
    } else
      size = 0;
    
    ret = (int)size;
  }
  else if (strcmp(path, time_path) == 0) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double t = (double)(tv.tv_sec) + ((double)(tv.tv_usec) / 1.0E6);
    char * loc_buf;
    size_t len;
    asprintf(&loc_buf, "---\nfloat: %.6f\nsec: %ld\nusec: %ld\n", t, tv.tv_sec, (long)tv.tv_usec);
    len = strlen(loc_buf);
    if (offset < len) {
      if (offset + size > len)
        size = len - offset;
      memcpy(buf, loc_buf + offset, size);
      NatNet_status.time_size = size;
    }
    else {
      size = 0;
    }
    free(loc_buf);
    ret = (int)size;
  }
  else if (strcmp(path_components[1], "frame") == 0) {
    if (strcmp(path_components[2], "test.txt") == 0) {
      char *test_str = "This is a demo string\n";
      len = strlen(test_str);
      if (offset < len) {
        if (offset + size > len)
          size = len - offset;
        memcpy(buf, test_str + offset, size);
      } else
        size = 0;
      
      ret = (int)size;
    }
  }
  else {
    ret = -ENOENT;
  }

  free(tmp_path);
  free(path_components[0]);
  free(path_components);

  return ret;
}


static struct fuse_operations NatNet_oper = {
  .getattr	= NatNet_getattr,
  .readdir	= NatNet_readdir,
  .open		= NatNet_open,
  .read		= NatNet_read
};


int main(int argc, char *argv[])
{
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  asprintf(&version_str, "NatNet fuse client Version: %d.%d\n", VERSION_MAJOR, VERSION_MINOR);
  
  fuse_opt_parse(&args, NULL, NULL, NULL);
  fuse_opt_add_arg(&args, "-o direct_io");

  return fuse_main(argc, argv, &NatNet_oper, NULL);
}
