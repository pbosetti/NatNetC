//
//  main.c
//  fuse_yaml
//
//  Created by Mirko Brentari on 14/01/16.
//  Copyright © 2016 UniTN. All rights reserved.
//
/*
 Decodes NatNet packets directly.

 Usage [optional]:

 PacketClient [ServerIP] [LocalIP]

 [ServerIP]			IP address of server ( defaults to
 local machine)
 [LocalIP]			IP address of client ( defaults to
 local machine)

 */

#define FUSE_USE_VERSION 30
#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_MAX_LEN 64
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <regex.h>
#include <fuse.h>
#include <fuse/fuse_common.h>

#include <NatNetC.h>




#pragma warning(disable : 4996)
#pragma GCC diagnostic ignored "-Wunused"

#pragma mark -
#pragma mark Function Declarations

void *DataListenThread(void *dummy);
long get_version(NatNet *nn);

#pragma mark -
#pragma mark Function Definitions

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

static NatNet *nn = NULL;

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
    filler(buf, "frame.yaml", NULL, 0);
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
    else if (strcmp(path_components[2], "frame.yaml") == 0) {
      len = strlen(nn->yaml);
      if (offset < len) {
        if (offset + size > len)
          size = len - offset;
        memcpy(buf, nn->yaml + offset, size);
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

int main(int argc, char *argv[]) {
  int retval, nargs = 0;
  char szMyIPAddress[128] = "";
  char szServerIPAddress[128] = "";
  struct in_addr ServerAddress, MyAddress;
  
  // server address
  if (argc > 1) {
    strcpy(szServerIPAddress, argv[1]); // specified on command line
    retval = IPAddress_StringToAddr(szServerIPAddress, &ServerAddress);
    nargs = 1;
  } else {
    GetLocalIPAddresses((unsigned long *)&ServerAddress, 1);
    strcpy(szServerIPAddress, inet_ntoa(ServerAddress));
  }

  // client address
  if (argc > 2) {
    strcpy(szMyIPAddress, argv[2]); // specified on command line
    retval = IPAddress_StringToAddr(szMyIPAddress, &MyAddress);
    nargs = 2;
  } else {
    GetLocalIPAddresses((unsigned long *)&MyAddress, 1);
    strcpy(szMyIPAddress, inet_ntoa(MyAddress));
  }
  argv[2] = argv[0];
  argc -= nargs;
  argv += nargs;

  nn = NatNet_new(szMyIPAddress, szServerIPAddress, MULTICAST_ADDRESS,
                          PORT_COMMAND, PORT_DATA);
  
  if (NatNet_bind(nn)) {
    perror("Error binding NetNat");
    return -1;
  }

  printf("Client (this machine): %s\n", nn->my_addr);
  printf("Server:                %s\n", nn->their_addr);
  printf("Multicast Group:       %s\n", nn->multicast_addr);
  printf("Data port:             %d\n", nn->data_port);
  printf("Command port:          %d\n", nn->command_port);
  

  long version_response = get_version(nn);

  // startup our "Data Listener" thread
  pthread_t data_listener;
  if (pthread_create(&data_listener, NULL, DataListenThread, nn) != 0) {
    perror("[PacketClient] Error creating data listener thread");
    return 1;
  }

  printf("Packet Client started\n\n");

  
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  asprintf(&version_str, "NatNet fuse client Version: %d.%d\n", VERSION_MAJOR, VERSION_MINOR);
  
  fuse_opt_parse(&args, NULL, NULL, NULL);
  fuse_opt_add_arg(&args, "-o direct_io");
  
  retval = fuse_main(argc, argv, &NatNet_oper, NULL);

  NatNet_free(nn);
  
  return retval;
  
  
  
}

// get version Mirko
long get_version(NatNet *nn) {
  // send initial ping command
  NatNet_packet PacketOut;
  PacketOut.iMessage = NAT_PING;
  PacketOut.nDataBytes = 0;
  NatNet_send_pkt(nn, PacketOut, 3);
  
  // create blocking socket
  long nDataBytesReceived;
  NatNet_packet PacketIn;
  nDataBytesReceived =
  NatNet_recv_cmd(nn, (char *)&PacketIn, sizeof(NatNet_packet));
  if ((nDataBytesReceived == 0) || (nDataBytesReceived == SOCKET_ERROR)) {
    return 0;
  }
  else if (PacketIn.iMessage == NAT_PINGRESPONSE) {
    for (int i = 0; i < 4; i++) {
      nn->NatNet_ver[i] = (int)PacketIn.Data.Sender.NatNetVersion[i];
      nn->server_ver[i] = (int)PacketIn.Data.Sender.Version[i];
    }
    printf("[CommandThread] Ping echo form server version %d.%d.%d.%d\n",
           nn->server_ver[0], nn->server_ver[1], nn->server_ver[2],
           nn->server_ver[3]);
    printf("[CommandThread]                NatNet version %d.%d.%d.%d\n",
           nn->NatNet_ver[0], nn->NatNet_ver[1], nn->NatNet_ver[2],
           nn->NatNet_ver[3]);
    return nDataBytesReceived;
  } else {
    return -1;
  }

}


// Data listener thread
void *DataListenThread(void *dummy) {
  NatNet *nn = (NatNet *)dummy;
  char szData[20000];
  size_t len;
  long nDataBytesReceived;

  while (1) {
    // Block until we receive a datagram from the network (from anyone including
    // ourselves)
    nDataBytesReceived = NatNet_recv_data(nn, szData, sizeof(szData));
    if (nDataBytesReceived > 0 && errno != EINVAL) {
//      NatNet_unpack_all(nn, szData, &len);
      NatNet_unpack_yaml(nn, szData, &len);
//      printf("---\nBodies: %zu\n", nn->last_frame->n_bodies);
//      for (int i = 0; i < nn->last_frame->n_bodies; i++) {
//        printf("  Body %d L[%11.6f %11.6f %11.6f]  O[%11.6f %11.6f %11.6f "
//               "%11.6f]\n",
//               i, nn->last_frame->bodies[i]->loc.x,
//               nn->last_frame->bodies[i]->loc.y,
//               nn->last_frame->bodies[i]->loc.z,
//               nn->last_frame->bodies[i]->ori.qx,
//               nn->last_frame->bodies[i]->ori.qy,
//               nn->last_frame->bodies[i]->ori.qz,
//               nn->last_frame->bodies[i]->ori.qw);
//      }
//      printf("Latency: %f\n", nn->last_frame->latency);
    }
  }

  return 0;
}
