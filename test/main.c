//
//  main.c
//  test
//
//  Created by Paolo Bosetti on 23/12/15.
//  Copyright © 2015 UniTN. All rights reserved.
//

#include <stdio.h>
#include <NatNetC.h>

int main(int argc, const char * argv[]) {
  // insert code here...
  
  size_t nsets = 3;
  size_t nmarkers = 10;
  size_t nbodies = 10;
  size_t s, m, j;
  
  NatNet *nn = NatNet_new("127.0.0.1", "127.0.0.1", MULTICAST_ADDRESS,
                          PORT_COMMAND, PORT_DATA);
  

  
  
  NatNet_frame *frame = nn->last_frame;
  
  NatNet_frame_alloc_marker_sets(frame, nsets);
  
  for (s = 0; s < frame->n_marker_sets; s++) {
    frame->marker_sets[s] = NatNet_markers_set_new("test", nmarkers);
  }
  
  NatNet_frame_alloc_marker_sets(frame, nsets * 2);
  for (s = 3; s < frame->n_marker_sets; s++) {
    frame->marker_sets[s] = NatNet_markers_set_new("test2", nmarkers);
  }

  NatNet_frame_alloc_ui_markers(frame, 10);
  NatNet_frame_alloc_labeled_markers(frame, 3);
  

  NatNet_frame_alloc_bodies(frame, nbodies);
  for (j=0; j<nbodies; j++) {
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
    for (m=0; m<nmarkers; m++) {
      frame->bodies[j]->markers[m].x = m * j;
      frame->bodies[j]->markers[m].y = m * j;
      frame->bodies[j]->markers[m].z = m * j;
    }
  }
  
  NatNet_frame_alloc_bodies(frame, nbodies*2);
  for (j=0; j<nbodies*2; j++) {
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
    for (m=0; m<nmarkers; m++) {
      frame->bodies[j]->markers[m].x = m * j;
      frame->bodies[j]->markers[m].y = m * j;
      frame->bodies[j]->markers[m].z = m * j;
    }
  }
  
  int messageID = 7;
  char *pData = calloc(2048, sizeof(char));
  size_t len = 0;
  memset(pData, 0, 2048);
  memcpy(pData, &messageID, 2);
  NatNet_unpack_all(nn, pData, &len);
  
  
  NatNet_free(nn);
  
  
  return 0;
}
