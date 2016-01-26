//
//  NatNetYAML.c
//  NatNetCLI
//
//  Created by Paolo Bosetti on 11/01/16.
//  Copyright © 2016 UniTN. All rights reserved.
//

#ifdef NATNET_YAML
#include <stdio.h>
#include <yaml.h>
#define BUFFER_SIZE 64

#include "NatNetC.h"

#define YAML_ADD_STRING(emitter, event, s, event_error, emitter_error)         \
  if (!yaml_scalar_event_initialize(event, NULL, NULL, (yaml_char_t *)s,       \
                                    (int)strlen(s), 1, 1,                      \
                                    YAML_ANY_SCALAR_STYLE))                    \
    goto event_error;                                                          \
  if (!yaml_emitter_emit(emitter, event))                                      \
    goto emitter_error;

#define YAML_ADD_PAIR(emitter, event, k, v, event_error, emitter_error)        \
  YAML_ADD_STRING(emitter, event, k, event_error, emitter_error)               \
  YAML_ADD_STRING(emitter, event, v, event_error, emitter_error)

#define YAML_SEQUENCE_START(emitter, event, event_error, emitter_error)        \
  if (!yaml_sequence_start_event_initialize(event, NULL, NULL, 1,              \
                                            YAML_ANY_SEQUENCE_STYLE))          \
    goto event_error;                                                          \
  if (!yaml_emitter_emit(emitter, event))                                      \
    goto emitter_error;

#define YAML_SEQUENCE_END(emitter, event)                                      \
  if (!yaml_sequence_end_event_initialize(event))                              \
    goto event_error;                                                          \
  if (!yaml_emitter_emit(emitter, event))                                      \
    goto emitter_error;

#define YAML_MAPPING_START(emitter, event, event_error, emitter_error)         \
  if (!yaml_mapping_start_event_initialize(event, NULL, NULL, 1,               \
                                           YAML_ANY_MAPPING_STYLE))            \
    goto event_error;                                                          \
  if (!yaml_emitter_emit(emitter, event))                                      \
    goto emitter_error;

#define YAML_MAPPING_END(emitter, event)                                       \
  if (!yaml_mapping_end_event_initialize(event))                               \
    goto event_error;                                                          \
  if (!yaml_emitter_emit(emitter, event))                                      \
    goto emitter_error;

static int yaml_write_handler(void *data, unsigned char *buffer, size_t size) {
  NatNet *nn = (NatNet *)data;
  size_t current_len;
  if (nn->yaml == NULL) {
    if (!(nn->yaml = (char *)calloc(size + 1, sizeof(char)))) {
      return 0;
    }
  }
  current_len = strlen(nn->yaml);
  if (current_len > 0) {
    if (!(nn->yaml = (char *)realloc(nn->yaml, current_len + size + 1))) {
      return 0;
    }
  }
  memcpy(nn->yaml + current_len, buffer, size);
  return 1;
}


int NatNet_unpack_yaml(NatNet *nn, char *pData, size_t *len) {
  int major = 2; //nn->NatNet_ver[0];
  int minor = 9; //nn->NatNet_ver[1];
  char *ptr = pData;
  
  yaml_emitter_t emitter;
  yaml_event_t event;
  memset(&emitter, 0, sizeof(emitter));
  memset(&event, 0, sizeof(event));
  
  if (nn->yaml) {
    free(nn->yaml);
    nn->yaml = NULL;
  }
  
  if (!yaml_emitter_initialize(&emitter)) {
    fprintf(stderr, "Could not inialize the emitter object\n");
    return 1;
  }

  /* Set the emitter parameters. */
  // yaml_emitter_set_output_string(&emitter, buffer, BUFFER_SIZE, &written);
  yaml_emitter_set_output(&emitter, yaml_write_handler, nn);
  yaml_emitter_set_canonical(&emitter, 0);
  yaml_emitter_set_unicode(&emitter, 1);

  /* Create and emit the STREAM-START event. */
  if (!yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING))
    goto event_error;
  if (!yaml_emitter_emit(&emitter, &event))
    goto emitter_error;

  /* Create and emit the DOCUMENT-START event. */
  if (!yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0))
    goto event_error;
  if (!yaml_emitter_emit(&emitter, &event))
    goto emitter_error;

  /* Create and emit the SEQUENCE-START event. */
  if (!yaml_mapping_start_event_initialize(&event, NULL, NULL, 1,
                                           YAML_ANY_MAPPING_STYLE))
    goto event_error;
  if (!yaml_emitter_emit(&emitter, &event))
    goto emitter_error;

  char key[128], value[1024];

  // message ID
  short MessageID = 0;
  memcpy(&MessageID, ptr, 2);
  IPLTOHS(MessageID);
  ptr += 2;

  // size
  short nBytes = 0;
  memcpy(&nBytes, ptr, 2);
  IPLTOHS(nBytes);
  ptr += 2;

  if (MessageID == 7) // FRAME OF MOCAP DATA packet
  {
    // frame number
    int frameNumber = 0;
    memcpy(&frameNumber, ptr, 4);
    IPLTOHL(frameNumber);
    ptr += 4;

    YAML_ADD_PAIR(&emitter, &event, "messageType", "mocapFrame", event_error,
                  emitter_error);
    sprintf(value, "%d", frameNumber);
    YAML_ADD_PAIR(&emitter, &event, "frameID", value, event_error,
                  emitter_error);

    // number of data sets (markersets, rigidbodies, etc)
    int nMarkerSets = 0;
    memcpy(&nMarkerSets, ptr, 4);
    IPLTOHL(nMarkerSets);
    ptr += 4;

    strcpy(key, "markersSet");
    YAML_ADD_STRING(&emitter, &event, key, event_error, emitter_error);

    YAML_SEQUENCE_START(&emitter, &event, event_error, emitter_error);
    for (int i = 0; i < nMarkerSets; i++) {
      // Markerset name
      char szName[256];
      strncpy(szName, ptr, 256);
      int nDataBytes = (int)strlen(szName) + 1;
      ptr += nDataBytes;

      // marker data
      int nMarkers = 0;
      memcpy(&nMarkers, ptr, 4);
      IPLTOHL(nMarkers);
      ptr += 4;

      YAML_MAPPING_START(&emitter, &event, event_error, emitter_error);
      strcpy(key, "modelName");
      YAML_ADD_PAIR(&emitter, &event, key, szName, event_error, emitter_error);

      strcpy(key, "markers");
      YAML_ADD_STRING(&emitter, &event, key, event_error, emitter_error);
      YAML_SEQUENCE_START(&emitter, &event, event_error, emitter_error);
      for (int j = 0; j < nMarkers; j++) {
        YAML_SEQUENCE_START(&emitter, &event, event_error, emitter_error);
        float x = 0;
        memcpy(&x, ptr, 4);
        ptr += 4;
        float y = 0;
        memcpy(&y, ptr, 4);
        ptr += 4;
        float z = 0;
        memcpy(&z, ptr, 4);
        ptr += 4;

        sprintf(value, "%f", x);
        YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
        sprintf(value, "%f", y);
        YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
        sprintf(value, "%f", z);
        YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
        sprintf(value, "%f", 0.0);
        YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
        YAML_SEQUENCE_END(&emitter, &event);
      }
      YAML_SEQUENCE_END(&emitter, &event);
      YAML_MAPPING_END(&emitter, &event)
    }
    YAML_SEQUENCE_END(&emitter, &event);

    // unidentified markers
    int nOtherMarkers = 0;
    memcpy(&nOtherMarkers, ptr, 4);
    IPLTOHL(nOtherMarkers);
    ptr += 4;

    YAML_ADD_STRING(&emitter, &event, "uiMarkers", event_error, emitter_error);
    YAML_SEQUENCE_START(&emitter, &event, event_error, emitter_error);
    for (int j = 0; j < nOtherMarkers; j++) {
      YAML_SEQUENCE_START(&emitter, &event, event_error, emitter_error);
      float x = 0.0f;
      memcpy(&x, ptr, 4);
      ptr += 4;
      float y = 0.0f;
      memcpy(&y, ptr, 4);
      ptr += 4;
      float z = 0.0f;
      memcpy(&z, ptr, 4);
      ptr += 4;

      sprintf(value, "%f", x);
      YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
      sprintf(value, "%f", y);
      YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
      sprintf(value, "%f", z);
      YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
      sprintf(value, "%f", 0.0);
      YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
      YAML_SEQUENCE_END(&emitter, &event);
    }
    YAML_SEQUENCE_END(&emitter, &event);

    // rigid bodies
    int nRigidBodies = 0;
    memcpy(&nRigidBodies, ptr, 4);
    IPLTOHL(nRigidBodies);
    ptr += 4;
    YAML_ADD_STRING(&emitter, &event, "rigidBodies", event_error,
                    emitter_error);
    YAML_SEQUENCE_START(&emitter, &event, event_error, emitter_error);
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

      YAML_MAPPING_START(&emitter, &event, event_error, emitter_error);
      sprintf(value, "%d", ID);
      YAML_ADD_PAIR(&emitter, &event, "ID", value, event_error, emitter_error);
      YAML_ADD_STRING(&emitter, &event, "pos", event_error, emitter_error);
      YAML_SEQUENCE_START(&emitter, &event, event_error, emitter_error);
      sprintf(value, "%f", x);
      YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
      sprintf(value, "%f", y);
      YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
      sprintf(value, "%f", z);
      YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
      sprintf(value, "%f", 0.0);
      YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
      YAML_SEQUENCE_END(&emitter, &event);
      YAML_ADD_STRING(&emitter, &event, "ori", event_error, emitter_error);
      YAML_SEQUENCE_START(&emitter, &event, event_error, emitter_error);
      sprintf(value, "%f", qx);
      YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
      sprintf(value, "%f", qy);
      YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
      sprintf(value, "%f", qz);
      YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
      sprintf(value, "%f", qw);
      YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
      YAML_SEQUENCE_END(&emitter, &event);

      // associated marker positions
      int nRigidMarkers = 0;
      memcpy(&nRigidMarkers, ptr, 4);
      IPLTOHL(nRigidMarkers);
      ptr += 4;

      int nBytes = nRigidMarkers * 3 * sizeof(float);
      float *markerData = (float *)malloc(nBytes);
      memcpy(markerData, ptr, nBytes);
      ptr += nBytes;

      YAML_ADD_STRING(&emitter, &event, "markers", event_error, emitter_error);
      YAML_SEQUENCE_START(&emitter, &event, event_error, emitter_error);
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
          YAML_MAPPING_START(&emitter, &event, event_error, emitter_error);
          sprintf(value, "%d", LTOHL(markerIDs[k]));
          YAML_ADD_PAIR(&emitter, &event, "ID", value, event_error,
                        emitter_error);
          YAML_ADD_STRING(&emitter, &event, "pos", event_error, emitter_error);
          YAML_SEQUENCE_START(&emitter, &event, event_error, emitter_error);
          sprintf(value, "%f", markerData[k * 3]);
          YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
          sprintf(value, "%f", markerData[k * 3 + 1]);
          YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
          sprintf(value, "%f", markerData[k * 3 + 2]);
          YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
          sprintf(value, "%f", markerSizes[k]);
          YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
          YAML_SEQUENCE_END(&emitter, &event);
          YAML_MAPPING_END(&emitter, &event);
        }

        if (markerIDs)
          free(markerIDs);
        if (markerSizes)
          free(markerSizes);

      } else {
        for (int k = 0; k < nRigidMarkers; k++) {
          YAML_MAPPING_START(&emitter, &event, event_error, emitter_error);
          sprintf(value, "%d", k);
          YAML_ADD_PAIR(&emitter, &event, "ID", value, event_error,
                        emitter_error);
          YAML_ADD_STRING(&emitter, &event, "pos", event_error, emitter_error);
          YAML_SEQUENCE_START(&emitter, &event, event_error, emitter_error);
          sprintf(value, "%f", markerData[k * 3]);
          YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
          sprintf(value, "%f", markerData[k * 3 + 1]);
          YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
          sprintf(value, "%f", markerData[k * 3 + 2]);
          YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
          sprintf(value, "%f", 0.0);
          YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
          YAML_SEQUENCE_END(&emitter, &event);
          YAML_MAPPING_END(&emitter, &event);
        }
      }
      YAML_SEQUENCE_END(&emitter, &event);
      if (markerData)
        free(markerData);

      if (major >= 2) {
        // Mean marker error
        float fError = 0.0f;
        memcpy(&fError, ptr, 4);
        ptr += 4;
        sprintf(value, "%f", fError);
        YAML_ADD_PAIR(&emitter, &event, "meanError", value, event_error,
                      emitter_error);
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
        // frame->bodies[j]->tracking_valid = bTrackingValid;
        YAML_ADD_PAIR(&emitter, &event, "trackingValid",
                      (bTrackingValid ? "true" : "false"), event_error,
                      emitter_error);
      }
      YAML_MAPPING_END(&emitter, &event);

    } // next rigid body
    YAML_SEQUENCE_END(&emitter, &event);

    // skeletons (version 2.1 and later)
    if (((major == 2) && (minor > 0)) || (major > 2)) {
      int nSkeletons = 0;
      memcpy(&nSkeletons, ptr, 4);
      IPLTOHL(nSkeletons);
      ptr += 4;

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
        printf("Rigid Body Count : %d\n", nRigidBodies);

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
          printf("ID : %d\n", ID);
          printf("pos: [%3.2f,%3.2f,%3.2f]\n", x, y, z);
          printf("ori: [%3.2f,%3.2f,%3.2f,%3.2f]\n", qx, qy, qz, qw);

          // associated marker positions
          int nRigidMarkers = 0;
          memcpy(&nRigidMarkers, ptr, 4);
          IPLTOHL(nRigidMarkers);
          ptr += 4;
          printf("Marker Count: %d\n", nRigidMarkers);
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

          for (int k = 0; k < nRigidMarkers; k++) {
            printf("\tMarker %d: id=%d\tsize=%3.1f\tpos=[%3.2f,%3.2f,%3.2f]\n",
                   k, LTOHL(markerIDs[k]), markerSizes[k], markerData[k * 3],
                   markerData[k * 3 + 1], markerData[k * 3 + 2]);
          }

          // Mean marker error (2.0 and later)
          if (major >= 2) {
            float fError = 0.0f;
            memcpy(&fError, ptr, 4);
            ptr += 4;
            printf("Mean marker error: %3.2f\n", fError);
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
      YAML_ADD_STRING(&emitter, &event, "labeledMarkers", event_error,
                      emitter_error);
      YAML_SEQUENCE_START(&emitter, &event, event_error, emitter_error);
      int nLabeledMarkers = 0;
      memcpy(&nLabeledMarkers, ptr, 4);
      IPLTOHL(nLabeledMarkers);
      ptr += 4;

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

        YAML_MAPPING_START(&emitter, &event, event_error, emitter_error);
        sprintf(value, "%d", ID);
        YAML_ADD_PAIR(&emitter, &event, "ID", value, event_error,
                      emitter_error);
        YAML_ADD_STRING(&emitter, &event, "pos", event_error, emitter_error);
        YAML_SEQUENCE_START(&emitter, &event, event_error, emitter_error);
        sprintf(value, "%f", x);
        YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
        sprintf(value, "%f", y);
        YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
        sprintf(value, "%f", z);
        YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
        sprintf(value, "%f", size);
        YAML_ADD_STRING(&emitter, &event, value, event_error, emitter_error);
        YAML_SEQUENCE_END(&emitter, &event);

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

          YAML_ADD_PAIR(&emitter, &event, "occluded",
                        (bOccluded ? "true" : "false"), event_error,
                        emitter_error);
          YAML_ADD_PAIR(&emitter, &event, "pcSolved",
                        (bPCSolved ? "true" : "false"), event_error,
                        emitter_error);
          YAML_ADD_PAIR(&emitter, &event, "mdlSolved",
                        (bModelSolved ? "true" : "false"), event_error,
                        emitter_error);
        }

        YAML_MAPPING_END(&emitter, &event);
      }
      YAML_SEQUENCE_END(&emitter, &event);
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
        printf("Force Plate : %d\n", ID);
        
        // Channel Count
        int nChannels = 0;
        memcpy(&nChannels, ptr, 4);
        IPLTOHL(nChannels);
        ptr += 4;
        
        // Channel Data
        for (int i = 0; i < nChannels; i++) {
          printf(" Channel %d : ", i);
          int nFrames = 0;
          memcpy(&nFrames, ptr, 4);
          IPLTOHL(nFrames);
          ptr += 4;
          for (int j = 0; j < nFrames; j++) {
            float val = 0.0f;
            memcpy(&val, ptr, 4);
            ptr += 4;
            printf("%3.2f   ", val);
          }
          printf("\n");
        }
      }
    }
    
    // latency
    float latency = 0.0f;
    memcpy(&latency, ptr, 4);
    ptr += 4;
    sprintf(value, "%f", latency);
    YAML_ADD_PAIR(&emitter, &event, "latency", value, event_error,
                  emitter_error);

    // timecode
    unsigned int timecode = 0;
    memcpy(&timecode, ptr, 4);
    IPLTOHL(timecode);
    ptr += 4;
    unsigned int timecodeSub = 0;
    memcpy(&timecodeSub, ptr, 4);
    IPLTOHL(timecodeSub);
    ptr += 4;

    sprintf(value, "%d", timecode);
    YAML_ADD_PAIR(&emitter, &event, "timecode", value, event_error,
                  emitter_error);
    sprintf(value, "%d", timecodeSub);
    YAML_ADD_PAIR(&emitter, &event, "subTimecode", value, event_error,
                  emitter_error);

    // char szTimecode[128] = "";
    // TimecodeStringify(timecode, timecodeSub, szTimecode, 128);

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
    sprintf(value, "%f", timestamp);
    YAML_ADD_PAIR(&emitter, &event, "timestamp", value, event_error,
                  emitter_error);

    // frame params
    short params = 0;
    memcpy(&params, ptr, 2);
    IPLTOHS(params);
    ptr += 2;
    bool bIsRecording = params & 0x01; // 0x01 Motive is recording
    bool bTrackedModelsChanged =
        params & 0x02; // 0x02 Actively tracked model list has changed

    sprintf(value, "%f", timestamp);
    YAML_ADD_PAIR(&emitter, &event, "isRecording",
                  (bIsRecording ? "true" : "false"), event_error,
                  emitter_error);
    sprintf(value, "%f", timestamp);
    YAML_ADD_PAIR(&emitter, &event, "modelChanged",
                  (bTrackedModelsChanged ? "true" : "false"), event_error,
                  emitter_error);

    // end of data tag
    int eod = 0;
    memcpy(&eod, ptr, 4);
    IPLTOHL(eod);
    ptr += 4;
  }
  else {
    printf("Unrecognized Packet Type.\n");
  }
  *len = (size_t)(ptr - pData);

  /* Create and emit the SEQUENCE-END event. */
  if (!yaml_mapping_end_event_initialize(&event))
    goto event_error;
  if (!yaml_emitter_emit(&emitter, &event))
    goto emitter_error;

  /* Create and emit the DOCUMENT-END event. */
  if (!yaml_document_end_event_initialize(&event, 0))
    goto event_error;
  if (!yaml_emitter_emit(&emitter, &event))
    goto emitter_error;

  /* Create and emit the STREAM-END event. */
  if (!yaml_stream_end_event_initialize(&event))
    goto event_error;
  if (!yaml_emitter_emit(&emitter, &event))
    goto emitter_error;

  yaml_event_delete(&event);
  yaml_emitter_delete(&emitter);

  return 0;

event_error:
  nn->yaml = calloc(1024, sizeof(char));
  strncpy(nn->yaml, "---\nmessageType: YAML Event Error", 1024);
  return -1;
emitter_error:
  nn->yaml = calloc(1024, sizeof(char));
  strncpy(nn->yaml, "---\nmessageType: YAML Emitter Error", 1024);
  return -2;
}




#endif