//
//  NatNetTypes.h
//  NatNetCLI
//
//  Created by Paolo Bosetti on 29/12/15.
//  Copyright © 2015 UniTN. All rights reserved.
//

#ifndef NatNetTypes_h
#define NatNetTypes_h

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <limits.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>



// max size of packet (actual packet size is dynamic)
#define MAX_PACKETSIZE 100000
#define RCV_BUFSIZE 0x100000
#define MAX_NAMELENGTH 256


#pragma mark -
#pragma mark Types

typedef enum {
  false = 0,
  true,
} bool;

typedef int SOCKET;


// sender
typedef struct {
  char szName[MAX_NAMELENGTH]; // sending app's name
  unsigned char
  Version[4]; // sending app's version [major.minor.build.revision]
  unsigned char NatNetVersion[4]; // sending app's NatNet version
                                  // [major.minor.build.revision]
  
} NatNet_sender;

typedef struct {
  unsigned short iMessage;   // message ID (e.g. NAT_FRAMEOFDATA)
  unsigned short nDataBytes; // Num bytes in payload
  union {
    unsigned char cData[MAX_PACKETSIZE];
    char szData[MAX_PACKETSIZE];
    unsigned long lData[MAX_PACKETSIZE / 4];
    float fData[MAX_PACKETSIZE / 4];
    NatNet_sender Sender;
  } Data; // Payload
} NatNet_packet;


typedef struct {
  float x, y, z, w;
} NatNet_point;


typedef struct {
  float qx, qy, qz, qw;
} NatNet_quaternion;


typedef struct {
  size_t ID;
  NatNet_point loc;
  NatNet_quaternion ori;
  NatNet_point *markers;
  size_t n_markers;
  float error;
  bool tracking_valid;
} NatNet_rigid_body;
NatNet_rigid_body * NatNet_rigid_body_new(size_t n_markers);
void NatNet_rigid_body_alloc_markers(NatNet_rigid_body *rb, size_t n_markers);
void NatNet_rigid_body_free(NatNet_rigid_body *rb);

typedef struct {
  size_t ID;
  NatNet_point loc;
  bool occluded;
  bool pc_solved;
  bool model_solved;
} NatNet_labeled_marker;


typedef struct {
  size_t ID;
  NatNet_rigid_body **bodies;
  size_t n_bodies;
} NatNet_skeleton;
NatNet_skeleton * NatNet_skeleton_new(size_t n_bodies);
void NatNet_skeleton_alloc_bodies(NatNet_skeleton *sk, size_t n_bodies);
void NatNet_skeleton_free(NatNet_skeleton *sk);

typedef struct {
  char name[64];
  NatNet_point *markers;
  size_t n_markers;
} NatNet_markers_set;
NatNet_markers_set * NatNet_markers_set_new(const char *name, size_t n_markers);
void NatNet_markers_set_alloc_markers(NatNet_markers_set *ms, size_t n_markers);
void NatNet_markers_set_free(NatNet_markers_set *ms);


typedef struct {
  size_t ID;
  size_t bytes;
  float latency;
  uint32_t timecode;
  uint32_t sub_timecode;
  double timestamp;
  bool is_recording;
  bool tracked_models_changed;
  
  NatNet_markers_set **marker_sets;
  size_t n_marker_sets;
  NatNet_rigid_body **bodies;
  size_t n_bodies;
  NatNet_skeleton **skeletons;
  size_t n_skeletons;
  NatNet_point *ui_markers;
  size_t n_ui_markers;
  NatNet_labeled_marker *labeled_markers;
  size_t n_labeled_markers;
} NatNet_frame;
NatNet_frame * NatNet_frame_new(size_t ID, size_t bytes);
void NatNet_frame_alloc_marker_sets(NatNet_frame *frame, size_t n_marker_sets);
void NatNet_frame_alloc_bodies(NatNet_frame *frame, size_t n_bodies);
void NatNet_frame_alloc_skeletons(NatNet_frame *frame, size_t n_skeletons);
void NatNet_frame_alloc_ui_markers(NatNet_frame *frame, size_t n_ui_markers);
void NatNet_frame_alloc_labeled_markers(NatNet_frame *frame, size_t n_labeled_markers);
void NatNet_frame_free(NatNet_frame *frame);


typedef struct {
  char my_addr[128], their_addr[128], multicast_addr[128];
  u_short command_port, data_port;
  int receive_bufsize;
  struct timeval timeout;
  SOCKET command;
  SOCKET data;
  struct sockaddr_in host_sockaddr;
  struct {
    int response;
    int response_size;
    unsigned char response_string[PATH_MAX];
    int response_ode;
  } cmd_response;
  int NatNet_ver[4];
  int server_ver[4];
  NatNet_frame *last_frame;
} NatNet;



#endif /* NatNetTypes_h */
