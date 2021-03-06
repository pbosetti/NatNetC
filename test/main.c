//
//  main.c
//  test
//
//  Created by Paolo Bosetti on 23/12/15.
//  Copyright © 2015 UniTN. All rights reserved.
//

#include <stdio.h>
#ifndef NATNET_YAML
#define NATNET_YAML
#endif
#include <NatNetC.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, const char * argv[]) {
#if 0
  int opt_value = 1, retval = 0;
  int bufsize = 20000;
  struct timeval data_timeout = {.tv_sec = 1, .tv_usec = 0};
  struct sockaddr_in data_sockaddr, host_sockaddr;
  SOCKET data_sock;

  
  data_sockaddr.sin_family = AF_INET;
  data_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  data_sockaddr.sin_port = htons(1234);
beginning:
  // Data socket configuration
  if ((data_sock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
    perror("Could not create socket");
    retval--;
  }
  if (setsockopt(data_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt_value,
                 sizeof(opt_value))) {
    perror("Setting data socket option");
    retval--;
  }
  if (setsockopt(data_sock, SOL_SOCKET, SO_RCVBUF, (char *)&bufsize,
                 sizeof(bufsize))) {
    perror("Setting data socket receive buffer");
    retval--;
  }
  if (setsockopt(data_sock, SOL_SOCKET, SO_RCVTIMEO, &data_timeout,
                 sizeof(data_timeout)) != 0) {
    perror("Setting data socket timeout");
    retval--;
  }
  // Optionally set to non-blocking behavior
//  if (fcntl(data_sock, F_SETFL, O_NONBLOCK) != 0) {
//    perror("Settin non-blocking behavior");
//    retval--;
//  }

  if (bind(data_sock, (struct sockaddr *)&data_sockaddr, sizeof(data_sockaddr))) {
    perror("Binding data socket");
    return -1;
  }
  
  if (retval != 0) {
    close(data_sock);
    return retval;
  }
  
  
  
  long bytes_received;
  socklen_t addr_len = sizeof(struct sockaddr);
  char *data = calloc(bufsize, sizeof(char));
  for (;;) {
    bytes_received = recvfrom(data_sock, data, bufsize, NULL,
                              (struct sockaddr *)&host_sockaddr, &addr_len);
    if (bytes_received < 0 && errno != EAGAIN)
      perror("Error: ");
    else if (bytes_received > 0)
      break;
  }
  data[bytes_received - 1] = '\0';
  printf("Got %ld bytes: '%s'\n", bytes_received, data);
  close(data_sock);
  if (strncmp(data, "stop", 4) != 0) {
    memset(data, 0, bufsize);
    goto beginning;
  }
#else
  
//  struct stat info;
//  char *data_file_name = (char *)argv[1];
//  stat(data_file_name, &info);
//  printf("FILE %s SIZE: %lld\n", data_file_name, info.st_size);
//  FILE *data_file = fopen(data_file_name, "r");
//  char *data = (char *)calloc(info.st_size, sizeof(char));
//  fread(data, info.st_size, sizeof(char), data_file);
//  fclose(data_file);
  
  size_t nsets = 10;
  size_t nmarkers = 10;
  size_t nbodies = 3;
  size_t nuimarkers = 2;
  size_t labmarkers = 3;
  size_t s, m, j;
  char name[128] = "";
  
  NatNet *nn = NatNet_new("127.0.0.1", "127.0.0.1", MULTICAST_ADDRESS,
                          PORT_COMMAND, PORT_DATA);
  

  
  
  NatNet_frame *frame = nn->last_frame;
  frame->ID = 123;
  
  NatNet_frame_alloc_marker_sets(frame, nsets);
  
  for (s = 0; s < frame->n_marker_sets; s++) {
    sprintf(name, "marker set %zu", s);
    frame->marker_sets[s] = NatNet_markers_set_new(name, nmarkers);
  }
  
  NatNet_frame_alloc_marker_sets(frame, nsets * 2);
  for (s = nsets; s < frame->n_marker_sets; s++) {
    sprintf(name, "New marker set %zu", s);
    frame->marker_sets[s] = NatNet_markers_set_new(name, nmarkers);
  }

  NatNet_frame_alloc_ui_markers(frame, nuimarkers);
  for (s = 0; s < frame->n_ui_markers; s++) {
    frame->ui_markers[s].x = 0.0f;
    frame->ui_markers[s].y = 1.0f;
    frame->ui_markers[s].z = 2.0f;
    frame->ui_markers[s].w = 0.0f;
  }

  NatNet_frame_alloc_labeled_markers(frame, labmarkers);
  for (s = 0; s < frame->n_labeled_markers; s++) {
    frame->labeled_markers[s].ID = s;
    frame->labeled_markers[s].loc.x = 0.0f;
    frame->labeled_markers[s].loc.y= 0.0f;
    frame->labeled_markers[s].loc.z = 0.0f;
    frame->labeled_markers[s].loc.w = (float)s;
    frame->labeled_markers[s].model_solved = false;
    frame->labeled_markers[s].occluded = false;
    frame->labeled_markers[s].pc_solved = false;
  }
  

  NatNet_frame_alloc_bodies(frame, nbodies);
  for (j=0; j < nbodies; j++) {
    if (!frame->bodies[j]) {
      frame->bodies[j] = NatNet_rigid_body_new(nmarkers);
    }
    frame->bodies[j]->ID = j;
    frame->bodies[j]->loc.x = j;
    frame->bodies[j]->loc.y = j*2;
    frame->bodies[j]->loc.z = j*3;
    frame->bodies[j]->loc.w = 0;
    frame->bodies[j]->ori.qx = j*4;
    frame->bodies[j]->ori.qy = j*3;
    frame->bodies[j]->ori.qz = j*2;
    frame->bodies[j]->ori.qw = j;
    for (m=0; m < nmarkers; m++) {
      frame->bodies[j]->markers[m].x = m * j;
      frame->bodies[j]->markers[m].y = m * j;
      frame->bodies[j]->markers[m].z = m * j;
    }
    frame->bodies[j]->error = 0.01f;
  }
  

  
  int messageID = 7;
  char *pData = calloc(2048, sizeof(char));
  memset(pData, 0, 2048);
  memcpy(pData, &messageID, 2);
  
  nn->NatNet_ver[0] = 2;
  nn->NatNet_ver[1] = 6;
  nn->printf = &NatNet_printf_std;
  
  size_t length = 0;

  NatNet_pack_struct(nn, nn->raw_data, &length);
  
  
  while (true) {
    NatNet_unpack_yaml(nn, &length);
  }
  
//  size_t len = 0;
  //NatNet_unpack_all(nn, data, &len);
  //NatNet_unpack_yaml(nn, data, &len);
  
   printf("YAML: \n%s\n", nn->yaml);
  
  NatNet_free(nn);
#endif
  
  return 0;
}
