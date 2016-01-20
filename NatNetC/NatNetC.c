//
//  NatNetC.c
//  NatNetCLI
//
//  Created by Paolo Bosetti on 21/12/15.
//  Copyright © 2015 UniTN. All rights reserved.
//

#include "NatNetC.h"
#include <arpa/inet.h>

#pragma mark -
#pragma mark Class-like NatNet functions

NatNet *NatNet_new(char *my_addr, char *their_addr, char *multicast_addr,
                   u_short command_port, u_short data_port) {
  NatNet *nn = (NatNet *)malloc(sizeof(NatNet));
  if (NatNet_init(nn, my_addr, their_addr, multicast_addr, command_port,
                  data_port)) {
    free(nn);
    return NULL;
  }
  return nn;
}

void NatNet_free(NatNet *nn) {
  NatNet_frame_free(nn->last_frame);
#ifdef NATNET_YAML
  if (nn->yaml) {
    free(nn->yaml);
  }
#endif
  free(nn);
}

int NatNet_printf_noop(const char * restrict format, ...) {return 0;}
int NatNet_printf_std(const char * restrict format, ...) {
  va_list ap;
  va_start(ap, format);
  return vprintf(format, ap);
}


int NatNet_init(NatNet *nn, char *my_addr, char *their_addr,
                char *multicast_addr, u_short command_port, u_short data_port) {
  memset(nn, 0, sizeof(*nn));
  strcpy(nn->my_addr, my_addr);
  strcpy(nn->their_addr, their_addr);
  strcpy(nn->multicast_addr, multicast_addr);
  nn->command_port = command_port;
  nn->data_port = data_port;
  nn->receive_bufsize = RCV_BUFSIZE;
  nn->data_timeout.tv_sec = 0;
  nn->data_timeout.tv_usec = 100000;
  nn->cmd_timeout.tv_sec = 0;
  nn->cmd_timeout.tv_usec = 500000;
  if ((nn->last_frame = NatNet_frame_new(0, 0)) == NULL) {
    return -1;
  }
#ifdef NATNET_YAML
  nn->yaml = NULL;
#endif
  nn->printf = &NatNet_printf_noop;
  return 0;
}

int NatNet_bind(NatNet *nn) {
  struct sockaddr_in cmd_sockaddr, data_sockaddr;
  struct in_addr multicast_in_addr;
  struct ip_mreq mreq;
  int opt_value = 0;
  int retval = 0;

  // Command socket: socket receiving incoming command replies, broadcast mode
  cmd_sockaddr.sin_family = AF_INET;
  cmd_sockaddr.sin_addr.s_addr = inet_addr(nn->my_addr);
  cmd_sockaddr.sin_port = htons(0);

  // Data socket: socket that accepts incoming data packets on nn->data_port
  data_sockaddr.sin_family = AF_INET;
  data_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  data_sockaddr.sin_port = htons(nn->data_port);
  // Multicast group to which data socket is added
  multicast_in_addr.s_addr = inet_addr(nn->multicast_addr);
  mreq.imr_multiaddr = multicast_in_addr;
  mreq.imr_interface = cmd_sockaddr.sin_addr;

  // Host socket address, to which commands will be sent
  nn->host_sockaddr.sin_family = AF_INET;
  nn->host_sockaddr.sin_addr.s_addr = inet_addr(nn->their_addr);
  nn->host_sockaddr.sin_port = htons(nn->command_port);

  // Data socket configuration
  if ((nn->data = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
    perror("Could not create socket");
    retval--;
  }
  opt_value = 1;
  if (setsockopt(nn->data, SOL_SOCKET, SO_REUSEADDR, (char *)&opt_value,
                 sizeof(opt_value))) {
    perror("Setting data socket option");
    retval--;
  }
  if (setsockopt(nn->data, SOL_SOCKET, SO_RCVBUF, (char *)&nn->receive_bufsize,
                 sizeof(nn->receive_bufsize))) {
    perror("Setting data socket receive buffer");
    retval--;
  }
  if (setsockopt(nn->data, SOL_SOCKET, SO_RCVTIMEO, &nn->data_timeout,
                 sizeof(nn->data_timeout)) != 0) {
    perror("Setting data socket timeout");
    retval--;
  }
  if (setsockopt(nn->data, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq,
                 sizeof(mreq))) {
    perror("Joining multicast group");
    retval--;
  }
  if (bind(nn->data, (struct sockaddr *)&data_sockaddr,
           sizeof(data_sockaddr))) {
    perror("Binding data socket");
    retval--;
  }

  if (retval != 0) {
    close(nn->data);
    return retval;
  }

  // Command socket configuration
  if ((nn->command = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
    perror("Could not create command socket");
    retval--;
  }
  opt_value = 1;
  if (setsockopt(nn->command, SOL_SOCKET, SO_BROADCAST, (char *)&opt_value,
                 sizeof(opt_value))) {
    perror("Setting command socket option");
    retval--;
  }
  if (setsockopt(nn->command, SOL_SOCKET, SO_RCVTIMEO, &nn->cmd_timeout,
                 sizeof(nn->cmd_timeout)) != 0) {
    perror("Setting command socket timeout");
    retval--;
  }
  if (bind(nn->command, (struct sockaddr *)&cmd_sockaddr,
           sizeof(cmd_sockaddr))) {
    perror("Binding command socket");
    retval--;
  }

  if (retval != 0) {
    close(nn->data);
    close(nn->command);
    return retval;
  }

  return 0;
}

uint NatNet_send_pkt(NatNet *nn, NatNet_packet packet, uint tries) {
  ssize_t sent = 0;
  if (tries < 1) {
    tries = 1;
  }
  while (tries--) {
    sent = sendto(nn->command, (char *)&packet, 4 + packet.nDataBytes, 0,
                  (struct sockaddr *)&nn->host_sockaddr,
                  sizeof(nn->host_sockaddr));
    if (sent != SOCKET_ERROR) {
      return 0;
    }
  }
  return -1;
}

uint NatNet_send_cmd(NatNet *nn, char *cmd, uint tries) {
  // reset global result
  nn->cmd_response.response = -1;
  // format command packet
  NatNet_packet commandPacket;
  strcpy(commandPacket.Data.szData, cmd);
  commandPacket.iMessage = NAT_REQUEST;
  commandPacket.nDataBytes = (int)strlen(commandPacket.Data.szData) + 1;

  // send command, and wait (a bit) for command response to set global response
  // var in CommandListenThread
  ssize_t iRet = sendto(
      nn->command, (char *)&commandPacket, 4 + commandPacket.nDataBytes, 0,
      (struct sockaddr *)&nn->host_sockaddr, sizeof(nn->host_sockaddr));
  if (iRet == SOCKET_ERROR) {
    perror("Socket error sending command");
  } else {
    int waitTries = tries;
    while (waitTries--) {
      if (nn->cmd_response.response != -1)
        break;
      sleep(1);
    }

    if (nn->cmd_response.response == -1) {
      printf("Command response not received (timeout)\n");
    } else if (nn->cmd_response.response == 0) {
      printf("Command response received with success\n");
    } else if (nn->cmd_response.response > 0) {
      printf("Command response received with errors\n");
    }
  }

  return nn->cmd_response.response;
}

long NatNet_recv_cmd(NatNet *nn, char *data, size_t len) {
  long bytes_received;
  socklen_t addr_len = sizeof(struct sockaddr);
  bytes_received = recvfrom(nn->command, data, len, 0,
                            (struct sockaddr *)&nn->host_sockaddr, &addr_len);
  return bytes_received;
}

long NatNet_recv_data(NatNet *nn, char *data, size_t len) {
  long bytes_received;
  socklen_t addr_len = sizeof(struct sockaddr);
  bytes_received = recvfrom(nn->data, data, len, 0,
                            (struct sockaddr *)&nn->host_sockaddr, &addr_len);
  return bytes_received;
}

#define READ_AND_ADVANCE(dst, src, len, FILE)                                  \
  memcpy(dst, src, 2);                                                         \
  if (FILE) {                                                                  \
    fwrite(src, sizeof(char), len, FILE);                                      \
  }                                                                            \
  ptr += len;




void NatNet_unpack_all(NatNet *nn, char *pData, size_t *len) {
  // Check where's the problem here (on windows, version goes here)
//  int major = nn->NatNet_ver[0];
//  int minor = nn->NatNet_ver[1];
  int major = nn->server_ver[0];
  int minor = nn->server_ver[1];
  char *ptr = pData;
  
  nn->printf("Begin Packet\n-------\n");
  
  // message ID
  short MessageID = 0;
  memcpy(&MessageID, ptr, 2);
  IPLTOHS(MessageID);
  ptr += 2;
  nn->printf("Message ID : %d\n", MessageID);
  
  // size
  short nBytes = 0;
  memcpy(&nBytes, ptr, 2);
  IPLTOHS(nBytes);
  ptr += 2;
  nn->printf("Byte count : %d\n", nBytes);
  
  if (MessageID == 7) // FRAME OF MOCAP DATA packet
  {
    NatNet_frame *frame = nn->last_frame;
    // frame number
    int frameNumber = 0;
    memcpy(&frameNumber, ptr, 4);
    IPLTOHL(frameNumber);
    ptr += 4;
    nn->printf("Frame # : %d\n", frameNumber);
    frame->ID = frameNumber;
    frame->bytes = nBytes;
    
    // number of data sets (markersets, rigidbodies, etc)
    int nMarkerSets = 0;
    memcpy(&nMarkerSets, ptr, 4);
    IPLTOHL(nMarkerSets);
    ptr += 4;
    nn->printf("Marker Set Count : %d\n", nMarkerSets);
    NatNet_frame_alloc_marker_sets(frame, nMarkerSets);
    
    for (int i = 0; i < nMarkerSets; i++) {
      // Markerset name
      char szName[256];
      strncpy(szName, ptr, 256);
      int nDataBytes = (int)strlen(szName) + 1;
      ptr += nDataBytes;
      nn->printf("Model Name: %s\n", szName);
      
      // marker data
      int nMarkers = 0;
      memcpy(&nMarkers, ptr, 4);
      IPLTOHL(nMarkers);
      ptr += 4;
      nn->printf("Marker Count : %d\n", nMarkers);
      
      if (!frame->marker_sets[i]) {
        frame->marker_sets[i] = NatNet_markers_set_new(szName, nMarkers);
      }
      else {
        NatNet_markers_set_alloc_markers(frame->marker_sets[i], nMarkers);
      }
      for (int j = 0; j < nMarkers; j++) {
        float x = 0;
        memcpy(&x, ptr, 4);
        ptr += 4;
        float y = 0;
        memcpy(&y, ptr, 4);
        ptr += 4;
        float z = 0;
        memcpy(&z, ptr, 4);
        ptr += 4;
        nn->printf("\tMarker %d : [x=%3.2f,y=%3.2f,z=%3.2f]\n", j, x, y, z);
        frame->marker_sets[i]->markers[j].x = x;
        frame->marker_sets[i]->markers[j].y = y;
        frame->marker_sets[i]->markers[j].z = z;
        frame->marker_sets[i]->markers[j].w = 0;
      }
    }
    
    // unidentified markers
    int nOtherMarkers = 0;
    memcpy(&nOtherMarkers, ptr, 4);
    IPLTOHL(nOtherMarkers);
    ptr += 4;
    nn->printf("Unidentified Marker Count : %d\n", nOtherMarkers);
    NatNet_frame_alloc_ui_markers(frame, nOtherMarkers);
    
    for (int j = 0; j < nOtherMarkers; j++) {
      float x = 0.0f;
      memcpy(&x, ptr, 4);
      ptr += 4;
      float y = 0.0f;
      memcpy(&y, ptr, 4);
      ptr += 4;
      float z = 0.0f;
      memcpy(&z, ptr, 4);
      ptr += 4;
      nn->printf("\tMarker %d : pos = [%3.2f,%3.2f,%3.2f]\n", j, x, y, z);
      frame->ui_markers[j].x = x;
      frame->ui_markers[j].y = y;
      frame->ui_markers[j].z = z;
      frame->ui_markers[j].w = 0;
    }
    
    // rigid bodies
    int nRigidBodies = 0;
    memcpy(&nRigidBodies, ptr, 4);
    IPLTOHL(nRigidBodies);
    ptr += 4;
    nn->printf("Rigid Body Count : %d\n", nRigidBodies);
    NatNet_frame_alloc_bodies(frame, nRigidBodies);
    
    for (int j = 0; j < nRigidBodies; j++) {
      // rigid body pos/ori
      int ID = 0;
      memcpy(&ID, ptr, 4);
      IPLTOHL(ID);
      ptr += 4;
      float x = 0.0f;
      memcpy(&x, ptr, 4);
      ptr += 4;
      float y = 0.0f;
      memcpy(&y, ptr, 4);
      ptr += 4;
      float z = 0.0f;
      memcpy(&z, ptr, 4);
      ptr += 4;
      float qx = 0;
      memcpy(&qx, ptr, 4);
      ptr += 4;
      float qy = 0;
      memcpy(&qy, ptr, 4);
      ptr += 4;
      float qz = 0;
      memcpy(&qz, ptr, 4);
      ptr += 4;
      float qw = 0;
      memcpy(&qw, ptr, 4);
      ptr += 4;
      nn->printf("ID : %d\n", ID);
      nn->printf("pos: [%3.2f,%3.2f,%3.2f]\n", x, y, z);
      nn->printf("ori: [%3.2f,%3.2f,%3.2f,%3.2f]\n", qx, qy, qz, qw);
      
      // associated marker positions
      int nRigidMarkers = 0;
      memcpy(&nRigidMarkers, ptr, 4);
      IPLTOHL(nRigidMarkers);
      ptr += 4;
      nn->printf("Marker Count: %d\n", nRigidMarkers);
      int nBytes = nRigidMarkers * 3 * sizeof(float);
      float *markerData = (float *)malloc(nBytes);
      memcpy(markerData, ptr, nBytes);
      ptr += nBytes;
      
      if (!frame->bodies[j]) {
        frame->bodies[j] = NatNet_rigid_body_new(nRigidMarkers);
      }
      else {
        NatNet_rigid_body_alloc_markers(frame->bodies[j], nRigidMarkers);
      }
      frame->bodies[j]->ID = ID;
      frame->bodies[j]->loc.x = x;
      frame->bodies[j]->loc.y = y;
      frame->bodies[j]->loc.z = z;
      frame->bodies[j]->loc.w = 0;
      frame->bodies[j]->ori.qx = qx;
      frame->bodies[j]->ori.qy = qy;
      frame->bodies[j]->ori.qz = qz;
      frame->bodies[j]->ori.qw = qw;
      
      if (major >= 2) {
        // associated marker IDs
        nBytes = nRigidMarkers * sizeof(int);
        int *markerIDs = (int *)malloc(nBytes);
        memcpy(markerIDs, ptr, nBytes);
        ptr += nBytes;
        
        // associated marker sizes
        nBytes = nRigidMarkers * sizeof(float);
        float *markerSizes = (float *)malloc(nBytes);
        memcpy(markerSizes, ptr, nBytes);
        ptr += nBytes;
        
        for (int k = 0; k < nRigidMarkers; k++) {
          nn->printf("\tMarker %d: id=%d\tsize=%3.1f\tpos=[%3.2f,%3.2f,%3.2f]\n", k,
                 LTOHL(markerIDs[k]), markerSizes[k], markerData[k * 3],
                 markerData[k * 3 + 1], markerData[k * 3 + 2]);
          frame->bodies[j]->markers[k].x = markerData[k * 3];
          frame->bodies[j]->markers[k].y = markerData[k * 3 + 1];
          frame->bodies[j]->markers[k].z = markerData[k * 3 + 2];
          frame->bodies[j]->markers[k].w = markerSizes[k];
        }
        
        if (markerIDs)
          free(markerIDs);
        if (markerSizes)
          free(markerSizes);
        
      } else {
        for (int k = 0; k < nRigidMarkers; k++) {
          nn->printf("\tMarker %d: pos = [%3.2f,%3.2f,%3.2f]\n", k,
                 markerData[k * 3], markerData[k * 3 + 1],
                 markerData[k * 3 + 2]);
          frame->bodies[j]->markers[k].x = markerData[k * 3];
          frame->bodies[j]->markers[k].y = markerData[k * 3 + 1];
          frame->bodies[j]->markers[k].z = markerData[k * 3 + 2];
          frame->bodies[j]->markers[k].w = 0;
        }
      }
      if (markerData)
        free(markerData);
      
      if (major >= 2) {
        // Mean marker error
        float fError = 0.0f;
        memcpy(&fError, ptr, 4);
        ptr += 4;
        nn->printf("Mean marker error: %3.2f\n", fError);
        frame->bodies[j]->error = fError;
      }
      
      // 2.6 and later
      if (((major == 2) && (minor >= 6)) || (major > 2) || (major == 0)) {
        // params
        short params = 0;
        memcpy(&params, ptr, 2);
        IPLTOHS(params);
        ptr += 2;
        bool bTrackingValid =
        params &
        0x01; // 0x01 : rigid body was successfully tracked in this frame
        frame->bodies[j]->tracking_valid = bTrackingValid;
      }
      
    } // next rigid body
    
    // skeletons (version 2.1 and later)
    if (((major == 2) && (minor > 0)) || (major > 2)) {
      int nSkeletons = 0;
      memcpy(&nSkeletons, ptr, 4);
      IPLTOHL(nSkeletons);
      ptr += 4;
      nn->printf("Skeleton Count : %d\n", nSkeletons);
      NatNet_frame_alloc_skeletons(frame, nSkeletons);
      for (int j = 0; j < nSkeletons; j++) {
        // skeleton id
        int skeletonID = 0;
        memcpy(&skeletonID, ptr, 4);
        IPLTOHL(skeletonID);
        ptr += 4;
        
        // # of rigid bodies (bones) in skeleton
        int nRigidBodies = 0;
        memcpy(&nRigidBodies, ptr, 4);
        IPLTOHL(nRigidBodies);
        ptr += 4;
        nn->printf("Rigid Body Count : %d\n", nRigidBodies);
        
        if (!frame->skeletons[j]) {
          frame->skeletons[j] = NatNet_skeleton_new(nRigidBodies);
        }
        else {
          NatNet_skeleton_alloc_bodies(frame->skeletons[j], nRigidBodies);
        }
        frame->skeletons[j]->ID = skeletonID;
        
        for (int i = 0; i < nRigidBodies; i++) {
          // rigid body pos/ori
          int ID = 0;
          memcpy(&ID, ptr, 4);
          IPLTOHL(ID);
          ptr += 4;
          float x = 0.0f;
          memcpy(&x, ptr, 4);
          ptr += 4;
          float y = 0.0f;
          memcpy(&y, ptr, 4);
          ptr += 4;
          float z = 0.0f;
          memcpy(&z, ptr, 4);
          ptr += 4;
          float qx = 0;
          memcpy(&qx, ptr, 4);
          ptr += 4;
          float qy = 0;
          memcpy(&qy, ptr, 4);
          ptr += 4;
          float qz = 0;
          memcpy(&qz, ptr, 4);
          ptr += 4;
          float qw = 0;
          memcpy(&qw, ptr, 4);
          ptr += 4;
          nn->printf("ID : %d\n", ID);
          nn->printf("pos: [%3.2f,%3.2f,%3.2f]\n", x, y, z);
          nn->printf("ori: [%3.2f,%3.2f,%3.2f,%3.2f]\n", qx, qy, qz, qw);
          
          // associated marker positions
          int nRigidMarkers = 0;
          memcpy(&nRigidMarkers, ptr, 4);
          IPLTOHL(nRigidMarkers);
          ptr += 4;
          nn->printf("Marker Count: %d\n", nRigidMarkers);
          int nBytes = nRigidMarkers * 3 * sizeof(float);
          float *markerData = (float *)malloc(nBytes);
          memcpy(markerData, ptr, nBytes);
          ptr += nBytes;
          
          // associated marker IDs
          nBytes = nRigidMarkers * sizeof(int);
          int *markerIDs = (int *)malloc(nBytes);
          memcpy(markerIDs, ptr, nBytes);
          ptr += nBytes;
          
          // associated marker sizes
          nBytes = nRigidMarkers * sizeof(float);
          float *markerSizes = (float *)malloc(nBytes);
          memcpy(markerSizes, ptr, nBytes);
          ptr += nBytes;
          
          if (!frame->skeletons[j]->bodies[i]) {
            frame->skeletons[j]->bodies[i] = NatNet_rigid_body_new(nRigidMarkers);
          }
          frame->skeletons[j]->bodies[i]->ID = ID;
          frame->skeletons[j]->bodies[i]->loc.x = x;
          frame->skeletons[j]->bodies[i]->loc.y = y;
          frame->skeletons[j]->bodies[i]->loc.z = z;
          frame->skeletons[j]->bodies[i]->loc.w = 0;
          frame->skeletons[j]->bodies[i]->ori.qx = qx;
          frame->skeletons[j]->bodies[i]->ori.qy = qy;
          frame->skeletons[j]->bodies[i]->ori.qz = qz;
          frame->skeletons[j]->bodies[i]->ori.qw = qw;
          
          
          
          for (int k = 0; k < nRigidMarkers; k++) {
            nn->printf("\tMarker %d: id=%d\tsize=%3.1f\tpos=[%3.2f,%3.2f,%3.2f]\n",
                   k, LTOHL(markerIDs[k]), markerSizes[k], markerData[k * 3],
                   markerData[k * 3 + 1], markerData[k * 3 + 2]);
            frame->skeletons[j]->bodies[i]->markers[k].x = markerData[k * 3];
            frame->skeletons[j]->bodies[i]->markers[k].y = markerData[k * 3 + 1];
            frame->skeletons[j]->bodies[i]->markers[k].z = markerData[k * 3 + 2];
            frame->skeletons[j]->bodies[i]->markers[k].w = markerSizes[k];
          }
          
          // Mean marker error (2.0 and later)
          if (major >= 2) {
            float fError = 0.0f;
            memcpy(&fError, ptr, 4);
            ptr += 4;
            nn->printf("Mean marker error: %3.2f\n", fError);
            frame->skeletons[j]->bodies[i]->error = fError;
          }
          
          // Tracking flags (2.6 and later)
          if (((major == 2) && (minor >= 6)) || (major > 2) || (major == 0)) {
            // params
            short params = 0;
            memcpy(&params, ptr, 2);
            IPLTOHS(params);
            ptr += 2;
            bool bTrackingValid = params & 0x01; // 0x01 : rigid body was
                                                 // successfully tracked in this
                                                 // frame
            frame->skeletons[j]->bodies[i]->tracking_valid = bTrackingValid;
          }
          
          // release resources
          if (markerIDs)
            free(markerIDs);
          if (markerSizes)
            free(markerSizes);
          if (markerData)
            free(markerData);
          
        } // next rigid body
        
      } // next skeleton
    }
    
    // labeled markers (version 2.3 and later)
    if (((major == 2) && (minor >= 3)) || (major > 2)) {
      int nLabeledMarkers = 0;
      memcpy(&nLabeledMarkers, ptr, 4);
      IPLTOHL(nLabeledMarkers);
      ptr += 4;
      nn->printf("Labeled Marker Count : %d\n", nLabeledMarkers);
      NatNet_frame_alloc_labeled_markers(frame, nLabeledMarkers);
      
      for (int j = 0; j < nLabeledMarkers; j++) {
        // id
        int ID = 0;
        memcpy(&ID, ptr, 4);
        IPLTOHL(ID);
        ptr += 4;
        // x
        float x = 0.0f;
        memcpy(&x, ptr, 4);
        ptr += 4;
        // y
        float y = 0.0f;
        memcpy(&y, ptr, 4);
        ptr += 4;
        // z
        float z = 0.0f;
        memcpy(&z, ptr, 4);
        ptr += 4;
        // size
        float size = 0.0f;
        memcpy(&size, ptr, 4);
        ptr += 4;
        
        // 2.6 and later
        if (((major == 2) && (minor >= 6)) || (major > 2) || (major == 0)) {
          // marker params
          short params = 0;
          memcpy(&params, ptr, 2);
          IPLTOHS(params);
          ptr += 2;
          bool bOccluded =
          params & 0x01; // marker was not visible (occluded) in this frame
          bool bPCSolved =
          params & 0x02; // position provided by point cloud solve
          bool bModelSolved = params & 0x04; // position provided by model solve
          
          frame->labeled_markers[j].occluded = bOccluded;
          frame->labeled_markers[j].pc_solved = bPCSolved;
          frame->labeled_markers[j].model_solved = bModelSolved;
        }
        
        nn->printf("ID  : %d\n", ID);
        nn->printf("pos : [%3.2f,%3.2f,%3.2f]\n", x, y, z);
        nn->printf("size: [%3.2f]\n", size);
        
        frame->labeled_markers[j].ID = ID;
        frame->labeled_markers[j].loc.x = x;
        frame->labeled_markers[j].loc.y = y;
        frame->labeled_markers[j].loc.z = z;
        frame->labeled_markers[j].loc.w = size;
      }
    }
    
    // Force Plate data (version 2.9 and later)
    if (((major == 2) && (minor >= 9)) || (major > 2)) {
      int nForcePlates;
      memcpy(&nForcePlates, ptr, 4);
      IPLTOHL(nForcePlates);
      ptr += 4;
      for (int iForcePlate = 0; iForcePlate < nForcePlates; iForcePlate++) {
        // ID
        int ID = 0;
        memcpy(&ID, ptr, 4);
        IPLTOHL(ID);
        ptr += 4;
        nn->printf("Force Plate : %d\n", ID);
        
        // Channel Count
        int nChannels = 0;
        memcpy(&nChannels, ptr, 4);
        IPLTOHL(nChannels);
        ptr += 4;
        
        // Channel Data
        for (int i = 0; i < nChannels; i++) {
          nn->printf(" Channel %d : ", i);
          int nFrames = 0;
          memcpy(&nFrames, ptr, 4);
          IPLTOHL(nFrames);
          ptr += 4;
          for (int j = 0; j < nFrames; j++) {
            float val = 0.0f;
            memcpy(&val, ptr, 4);
            ptr += 4;
            nn->printf("%3.2f   ", val);
          }
          nn->printf("\n");
        }
      }
    }
    
    // latency
    float latency = 0.0f;
    memcpy(&latency, ptr, 4);
    ptr += 4;
    nn->printf("latency : %3.3f\n", latency);
    frame->latency = latency;
    
    // timecode
    unsigned int timecode = 0;
    memcpy(&timecode, ptr, 4);
    IPLTOHL(timecode);
    ptr += 4;
    unsigned int timecodeSub = 0;
    memcpy(&timecodeSub, ptr, 4);
    IPLTOHL(timecodeSub);
    ptr += 4;
    frame->timecode = timecode;
    frame->sub_timecode = timecodeSub;
    
    //char szTimecode[128] = "";
    //TimecodeStringify(timecode, timecodeSub, szTimecode, 128);
    
    // timestamp
    double timestamp = 0.0f;
    // 2.7 and later - increased from single to double precision
    if (((major == 2) && (minor >= 7)) || (major > 2)) {
      memcpy(&timestamp, ptr, 8);
      ptr += 8;
    } else {
      float fTemp = 0.0f;
      memcpy(&fTemp, ptr, 4);
      ptr += 4;
      timestamp = (double)fTemp;
    }
    frame->timestamp = timestamp;
    
    // frame params
    short params = 0;
    memcpy(&params, ptr, 2);
    IPLTOHS(params);
    ptr += 2;
    bool bIsRecording = params & 0x01; // 0x01 Motive is recording
    bool bTrackedModelsChanged =
    params & 0x02; // 0x02 Actively tracked model list has changed
    
    frame->is_recording = bIsRecording;
    frame->tracked_models_changed = bTrackedModelsChanged;
    
    // end of data tag
    int eod = 0;
    memcpy(&eod, ptr, 4);
    IPLTOHL(eod);
    ptr += 4;
    nn->printf("End Packet\n-------------\n");
    
  } else if (MessageID == 5) // Data Descriptions
  {
    // number of datasets
    int nDatasets = 0;
    memcpy(&nDatasets, ptr, 4);
    IPLTOHL(nDatasets);
    ptr += 4;
    nn->printf("Dataset Count : %d\n", nDatasets);
    
    for (int i = 0; i < nDatasets; i++) {
      nn->printf("Dataset %d\n", i);
      
      int type = 0;
      memcpy(&type, ptr, 4);
      IPLTOHL(type);
      ptr += 4;
      nn->printf("Type %d: %d\n", i, type);
      
      if (type == 0) // markerset
      {
        // name
        char szName[256];
        strcpy(szName, ptr);
        int nDataBytes = (int)strlen(szName) + 1;
        ptr += nDataBytes;
        nn->printf("Markerset Name: %s\n", szName);
        
        // marker data
        int nMarkers = 0;
        memcpy(&nMarkers, ptr, 4);
        IPLTOHL(nMarkers);
        ptr += 4;
        nn->printf("Marker Count : %d\n", nMarkers);
        
        for (int j = 0; j < nMarkers; j++) {
          char szName[256];
          strcpy(szName, ptr);
          int nDataBytes = (int)strlen(szName) + 1;
          ptr += nDataBytes;
          nn->printf("Marker Name: %s\n", szName);
        }
      } else if (type == 1) // rigid body
      {
        if (major >= 2) {
          // name
          char szName[MAX_NAMELENGTH];
          strcpy(szName, ptr);
          ptr += strlen(ptr) + 1;
          nn->printf("Name: %s\n", szName);
        }
        
        int ID = 0;
        memcpy(&ID, ptr, 4);
        IPLTOHL(ID);
        ptr += 4;
        nn->printf("ID : %d\n", ID);
        
        int parentID = 0;
        memcpy(&parentID, ptr, 4);
        IPLTOHL(parentID);
        ptr += 4;
        nn->printf("Parent ID : %d\n", parentID);
        
        float xoffset = 0;
        memcpy(&xoffset, ptr, 4);
        ptr += 4;
        nn->printf("X Offset : %3.2f\n", xoffset);
        
        float yoffset = 0;
        memcpy(&yoffset, ptr, 4);
        ptr += 4;
        nn->printf("Y Offset : %3.2f\n", yoffset);
        
        float zoffset = 0;
        memcpy(&zoffset, ptr, 4);
        ptr += 4;
        nn->printf("Z Offset : %3.2f\n", zoffset);
        
      } else if (type == 2) // skeleton
      {
        char szName[MAX_NAMELENGTH];
        strcpy(szName, ptr);
        ptr += strlen(ptr) + 1;
        nn->printf("Name: %s\n", szName);
        
        int ID = 0;
        memcpy(&ID, ptr, 4);
        IPLTOHL(ID);
        ptr += 4;
        nn->printf("ID : %d\n", ID);
        
        int nRigidBodies = 0;
        memcpy(&nRigidBodies, ptr, 4);
        IPLTOHL(nRigidBodies);
        ptr += 4;
        nn->printf("RigidBody (Bone) Count : %d\n", nRigidBodies);
        
        for (int i = 0; i < nRigidBodies; i++) {
          if (major >= 2) {
            // RB name
            char szName[MAX_NAMELENGTH];
            strcpy(szName, ptr);
            ptr += strlen(ptr) + 1;
            nn->printf("Rigid Body Name: %s\n", szName);
          }
          
          int ID = 0;
          memcpy(&ID, ptr, 4);
          IPLTOHL(ID);
          ptr += 4;
          nn->printf("RigidBody ID : %d\n", ID);
          
          int parentID = 0;
          memcpy(&parentID, ptr, 4);
          IPLTOHL(parentID);
          ptr += 4;
          nn->printf("Parent ID : %d\n", parentID);
          
          float xoffset = 0;
          memcpy(&xoffset, ptr, 4);
          ptr += 4;
          nn->printf("X Offset : %3.2f\n", xoffset);
          
          float yoffset = 0;
          memcpy(&yoffset, ptr, 4);
          ptr += 4;
          nn->printf("Y Offset : %3.2f\n", yoffset);
          
          float zoffset = 0;
          memcpy(&zoffset, ptr, 4);
          ptr += 4;
          nn->printf("Z Offset : %3.2f\n", zoffset);
        }
      }
      
    } // next dataset
    
    nn->printf("End Packet\n-------------\n");
  } else {
    nn->printf("Unrecognized Packet Type.\n");
  }
  *len = (size_t)(ptr - pData);

}



#pragma mark -
#pragma mark Utilities

int swap4(int i) {
  i = ((i & (0x0000FFFF)) << 16) | ((i & (0xFFFF0000)) >> 16);
  i = ((i & (0x00FF00FF)) << 8) | ((i & (0xFF00FF00)) >>8);
  return i;
}

void ipswap4(void *a) {
  int *i = (int *)a;
  *i = ((*i & (0x0000FFFF)) << 16) | ((*i & (0xFFFF0000)) >> 16);
  *i = ((*i & (0x00FF00FF)) << 8) | ((*i & (0xFF00FF00)) >>8);
}

short swap2(short i) {
  i = ((i & (0x00FF00FF)) << 8) | ((i & (0xFF00FF00)) >>8);
  return i;
}

void ipswap2(void *a) {
  short *i = (short *)a;
  *i = ((*i & (0x00FF00FF)) << 8) | ((*i & (0xFF00FF00)) >>8);
}

#pragma mark -
#pragma mark Top Be Removed
// Very Badly written functions (from original OptiTrack examples)
// To be rewritten!

// convert ip address string to addr
bool IPAddress_StringToAddr(char *szNameOrAddress, struct in_addr *Address) {
  int retVal;
  struct sockaddr_in saGNI;
  char hostName[256];
  char servInfo[256];
  u_short port;
  port = 0;

  // Set up sockaddr_in structure which is passed to the getnameinfo function
  saGNI.sin_family = AF_INET;
  saGNI.sin_addr.s_addr = inet_addr(szNameOrAddress);
  saGNI.sin_port = htons(port);

  // getnameinfo
  if ((retVal = getnameinfo((struct sockaddr *)&saGNI, sizeof(struct sockaddr),
                            hostName, 256, servInfo, 256, NI_NUMERICSERV)) !=
      0) {
    perror("[PacketClient] GetHostByAddr failed");
    return false;
  }

  Address->s_addr = saGNI.sin_addr.s_addr;

  return true;
}

// get ip addresses on local host
int GetLocalIPAddresses(unsigned long Addresses[], int nMax) {
  unsigned long NameLength = 128;
  char szMyName[1024];
  struct addrinfo aiHints;
  struct addrinfo *aiList = NULL;
  struct sockaddr_in addr;
  int retVal = 0;
  char *port = "0";

  if (gethostname(szMyName, NameLength) != 0) {
    perror("[PacketClient] get computer name  failed");
    return 0;
  }

  memset(&aiHints, 0, sizeof(aiHints));
  aiHints.ai_family = AF_INET;
  aiHints.ai_socktype = SOCK_DGRAM;
  aiHints.ai_protocol = IPPROTO_UDP;
  if ((retVal = getaddrinfo(szMyName, port, &aiHints, &aiList)) != 0) {
    perror("[PacketClient] getaddrinfo failed");
    return 0;
  }

  memcpy(&addr, aiList->ai_addr, aiList->ai_addrlen);
  freeaddrinfo(aiList);
  Addresses[0] = addr.sin_addr.s_addr;

  return 1;
}

bool DecodeTimecode(unsigned int inTimecode, unsigned int inTimecodeSubframe,
                    int *hour, int *minute, int *second, int *frame,
                    int *subframe) {
  bool bValid = true;

  *hour = (inTimecode >> 24) & 255;
  *minute = (inTimecode >> 16) & 255;
  *second = (inTimecode >> 8) & 255;
  *frame = inTimecode & 255;
  *subframe = inTimecodeSubframe;

  return bValid;
}

bool TimecodeStringify(unsigned int inTimecode, unsigned int inTimecodeSubframe,
                       char *Buffer, int BufferSize) {
  bool bValid;
  int hour, minute, second, frame, subframe;
  bValid = DecodeTimecode(inTimecode, inTimecodeSubframe, &hour, &minute,
                          &second, &frame, &subframe);

  snprintf(Buffer, BufferSize, "%2d:%2d:%2d:%2d.%d", hour, minute, second,
           frame, subframe);
  for (unsigned int i = 0; i < strlen(Buffer); i++)
    if (Buffer[i] == ' ')
      Buffer[i] = '0';

  return bValid;
}


