//
//  NatNetTypes.c
//  NatNetCLI
//
//  Created by Paolo Bosetti on 29/12/15.
//  Copyright © 2015 UniTN. All rights reserved.
//

#include "NatNetC/NatNetTypes.h"


NatNet_markers_set * NatNet_markers_set_new(const char *name, uint n_markers) {
  NatNet_markers_set *ms = calloc(1, sizeof(NatNet_markers_set));
  NatNet_markers_set_alloc_markers(ms, n_markers);
  strcpy(ms->name, name);
  return ms;
}

void NatNet_markers_set_alloc_markers(NatNet_markers_set *ms, uint n_markers) {
  // Nothing to release when shrinking
  if (ms->n_markers == 0 || ms->markers == NULL) { // allocating
    ms->markers = calloc(n_markers, sizeof(NatNet_point));
  }
  else if (ms->n_markers != n_markers) {  // growing/shrinking
    ms->markers = realloc(ms->markers, n_markers * sizeof(NatNet_point));
  }
  ms->n_markers = n_markers;
}

void NatNet_markers_set_free(NatNet_markers_set *ms) {
  free(ms->markers);
  free(ms);
}



NatNet_rigid_body * NatNet_rigid_body_new(uint n_markers) {
  NatNet_rigid_body *rb = calloc(1, sizeof(NatNet_rigid_body));
  NatNet_rigid_body_alloc_markers(rb, n_markers);
  rb->error = 0.0;
  rb->tracking_valid = false;
  return rb;
}

void NatNet_rigid_body_alloc_markers(NatNet_rigid_body *rb, uint n_markers) {
  // Nothing to release when shrinking
  if (rb->n_markers == 0 || rb->markers == NULL) { // allocating
    rb->markers = calloc(n_markers, sizeof(NatNet_point));
  }
  else if (rb->n_markers != n_markers) {  // growing/shrinking
    rb->markers = realloc(rb->markers, n_markers * sizeof(NatNet_point));
  }
  rb->n_markers = n_markers;
}

void NatNet_rigid_body_free(NatNet_rigid_body *rb) {
  if (rb) {
    free(rb->markers);
    free(rb);
  }
}



NatNet_skeleton * NatNet_skeleton_new(uint n_bodies) {
  NatNet_skeleton *sk = calloc(1, sizeof(NatNet_skeleton));
  NatNet_skeleton_alloc_bodies(sk, n_bodies);
  return sk;
}

void NatNet_skeleton_alloc_bodies(NatNet_skeleton *sk, uint n_bodies) {
  if (sk->n_bodies > n_bodies) { // Only when shrinking
    size_t i = 0;
    for (i=n_bodies; i<sk->n_bodies; i++) {
      NatNet_rigid_body_free(sk->bodies[i]);
    }
  }
  if (sk->n_bodies == 0 || sk->bodies == NULL) { // Allocating
    sk->bodies = calloc(n_bodies, sizeof(NatNet_rigid_body *));
  }
  else if (sk->n_bodies != n_bodies) { // Resizing
    sk->bodies = realloc(sk->bodies, n_bodies * sizeof(NatNet_rigid_body *));
  }
  sk->n_bodies = n_bodies;
}

void NatNet_skeleton_free(NatNet_skeleton *sk) {
  int i = 0;
  for (i=0; i<sk->n_bodies; i++) {
    free(sk->bodies[i]->markers);
  }
  free(sk->bodies);
  free(sk);
}



NatNet_frame * NatNet_frame_new(uint ID, size_t bytes) {
  NatNet_frame *frame = calloc(1, sizeof(NatNet_frame));
  frame->ID = ID;
  frame->bytes = bytes;
  return frame;
}

void NatNet_frame_alloc_marker_sets(NatNet_frame *frame, uint n_marker_sets) {
  if (frame->n_marker_sets > n_marker_sets) { // Only when shrinking
    size_t i = 0;
    for (i=n_marker_sets; i<frame->n_marker_sets; i++) {
      NatNet_markers_set_free(frame->marker_sets[i]);
    }
  }
  if (frame->n_marker_sets == 0 || frame->marker_sets == NULL) { // Allocating
    frame->marker_sets = calloc(n_marker_sets, sizeof(NatNet_markers_set *));
  }
  else if (n_marker_sets != frame->n_marker_sets) { // Resizing
    frame->marker_sets = realloc(frame->marker_sets, n_marker_sets * sizeof(NatNet_markers_set *));
  }
  frame->n_marker_sets = n_marker_sets;
}

void NatNet_frame_alloc_bodies(NatNet_frame *frame, uint n_bodies) {
  if (frame->n_bodies > n_bodies) { // Only when shrinking
    size_t i = 0;
    for (i=n_bodies; i<frame->n_bodies; i++) {
      NatNet_rigid_body_free(frame->bodies[i]);
    }
  }
  if (frame->n_bodies == 0 || frame->bodies == NULL) { // Allocating
    frame->bodies = calloc(n_bodies, sizeof(NatNet_rigid_body *));
  }
  else if (frame->n_bodies != n_bodies) { // Resizing
    frame->bodies = realloc(frame->bodies, n_bodies * sizeof(NatNet_rigid_body *));
  }
  frame->n_bodies = n_bodies;
}

void NatNet_frame_alloc_skeletons(NatNet_frame *frame, uint n_skeletons) {
  if (frame->n_skeletons > n_skeletons) { // Only when shrinking
    size_t i = 0;
    for (i=n_skeletons; i<frame->n_skeletons; i++) {
      NatNet_skeleton_free(frame->skeletons[i]);
    }
  }
  if (frame->n_skeletons == 0 || frame->skeletons == NULL) { // Allocating
    frame->skeletons = calloc(n_skeletons, sizeof(NatNet_skeleton *));
  }
  else if (n_skeletons != frame->n_skeletons) { //Resizing
    frame->skeletons = realloc(frame->skeletons, n_skeletons * sizeof(NatNet_skeleton *));
  }
  frame->n_skeletons = n_skeletons;
}

void NatNet_frame_alloc_ui_markers(NatNet_frame *frame, uint n_ui_markers) {
  // Nothing to release when shrinking
  if (frame->n_ui_markers == 0 || frame->ui_markers == NULL) {
    frame->ui_markers = calloc(n_ui_markers, sizeof(NatNet_point));
  }
  else if (n_ui_markers != frame->n_ui_markers) {
    frame->ui_markers = realloc(frame->ui_markers, n_ui_markers * sizeof(NatNet_point));
  }
  frame->n_ui_markers = n_ui_markers;
}

void NatNet_frame_alloc_labeled_markers(NatNet_frame *frame, uint n_labeled_markers) {
  // Nothing to release when shrinking
  if (frame->n_ui_markers == 0 || frame->ui_markers == NULL) {
    frame->labeled_markers = calloc(n_labeled_markers, sizeof(NatNet_labeled_marker));
  }
  else if (n_labeled_markers != frame->n_labeled_markers) {
    frame->labeled_markers = realloc(frame->labeled_markers, n_labeled_markers * sizeof(NatNet_labeled_marker));
  }
  frame->n_labeled_markers = n_labeled_markers;
}



void NatNet_frame_free(NatNet_frame *frame) {
  size_t i = 0;
  for (i=0; i<frame->n_bodies; i++) {
    NatNet_rigid_body_free(frame->bodies[i]);
  }
  free(frame->bodies);
  
  for (i=0; i<frame->n_marker_sets; i++) {
    NatNet_markers_set_free(frame->marker_sets[i]);
  }
  free(frame->marker_sets);
  
  for (i=0; i<frame->n_skeletons; i++) {
    NatNet_skeleton_free(frame->skeletons[i]);
  }
  free(frame->skeletons);
  
  free(frame->ui_markers);
  free(frame->labeled_markers);
  
  free(frame);
}






