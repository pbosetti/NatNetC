//
//  main.c
//  NatNetCLI
//
//  Created by Paolo Bosetti on 19/12/15.
//  Copyright © 2015 UniTN. All rights reserved.
//

//=============================================================================
// Copyright © 2014 NaturalPoint, Inc. All Rights Reserved.
//
// This software is provided by the copyright holders and contributors "as is"
// and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are
// disclaimed.
// In no event shall NaturalPoint, Inc. or contributors be liable for any
// direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//=============================================================================

/*
 
 PacketClient.cpp
 
 Decodes NatNet packets directly.
 
 Usage [optional]:
 
 PacketClient [ServerIP] [LocalIP]
 
 [ServerIP]			IP address of server ( defaults to
 local machine)
 [LocalIP]			IP address of client ( defaults to
 local machine)
 
 */

#include "NatNetC.h"

#include <stdio.h>
#pragma warning(disable : 4996)
#pragma GCC diagnostic ignored "-Wunused"





#define MULTICAST_ADDRESS "239.255.42.99" // IANA, local network
#define PORT_COMMAND 1510
#define PORT_DATA 1511


#pragma mark -
#pragma mark GLOBALS

//SOCKET CommandSocket;
//SOCKET DataSocket;
in_addr ServerAddress;
//sockaddr_in HostAddr;

int NatNetVersion[4] = {0, 0, 0, 0};
int ServerVersion[4] = {0, 0, 0, 0};

int gCommandResponse = 0;
int gCommandResponseSize = 0;
unsigned char gCommandResponseString[MAX_PATH];
int gCommandResponseCode = 0;

#pragma mark -
#pragma mark Function Declarations

DWORD WINAPI CommandListenThread(void *dummy);
DWORD WINAPI DataListenThread(void *dummy);
void UnpackDebug(char *pData);
int SendCommand(NatNet *nn, char *szCommand);



#pragma mark -
#pragma mark Function Definitions

int main(int argc, char *argv[]) {
  int retval;
  char szMyIPAddress[128] = "";
  char szServerIPAddress[128] = "";
  in_addr MyAddress;
  int rcv_bufsize = 0x100000;
  unsigned int optval_size = 4;
  
  // server address
  if (argc > 1) {
    strcpy_s(szServerIPAddress, argv[1]); // specified on command line
    retval = IPAddress_StringToAddr(szServerIPAddress, &ServerAddress);
  } else {
    GetLocalIPAddresses((unsigned long *)&ServerAddress, 1);
    strcpy(szServerIPAddress, inet_ntoa(ServerAddress));
  }
  
  // client address
  if (argc > 2) {
    strcpy_s(szMyIPAddress, argv[2]); // specified on command line
    retval = IPAddress_StringToAddr(szMyIPAddress, &MyAddress);
  } else {
    GetLocalIPAddresses((unsigned long *)&MyAddress, 1);
    strcpy(szMyIPAddress, inet_ntoa(MyAddress));
  }
  
  
  NatNet *nn = NatNet_new(szMyIPAddress, szServerIPAddress, MULTICAST_ADDRESS, PORT_COMMAND, PORT_DATA);
  if (NatNet_bind(nn)) {
    perror("Error binding NetNat");
    return -1;
  }

  printf("Client (this machine): %s\n", nn->my_addr);
  printf("Server:                %s\n", nn->their_addr);
  printf("Multicast Group:       %s\n", nn->multicast_addr);
  printf("Data port:             %d\n", nn->data_port);
  printf("Command port:          %d\n", nn->command_port);
  
  // create "Command" socket
//  int port = 0;
//  CommandSocket = CreateCommandSocket(MyAddress, port, rcv_bufsize);
//  if (CommandSocket == -1) {
//    perror("Error creating command socket");
//    return 1;
//  }
//CommandSocket = nn->command;
  // startup our "Command Listener" thread

  pthread_t command_listener;
  if (pthread_create(&command_listener, NULL, CommandListenThread, nn) !=
      0) {
    perror("[PacketClient] Error creating command listener thread");
    return 1;
  }

  
  // create "Data" socket
//  DataSocket = CreateDataSocket(MyAddress, MultiCastAddress, PORT_DATA, rcv_bufsize);
//  if (DataSocket == -1) {
//    perror("[PacketClient] Error creating data socket");
//    return 1;
//  }
//DataSocket = nn->data;
  // startup our "Data Listener" thread

  pthread_t data_listener;
  if (pthread_create(&data_listener, NULL, DataListenThread, nn) != 0) {
    perror("[PacketClient] Error creating data listener thread");
    return 1;
  }

  
  // server address for commands
//  memset(&HostAddr, 0, sizeof(HostAddr));
//  HostAddr.sin_family = AF_INET;
//  HostAddr.sin_port = htons(PORT_COMMAND);
//  HostAddr.sin_addr = ServerAddress;
  
  // send initial ping command
  sPacket PacketOut;
  PacketOut.iMessage = NAT_PING;
  PacketOut.nDataBytes = 0;
  int nTries = 3;
  while (nTries--) {
    ssize_t iRet =
    sendto(nn->command, (char *)&PacketOut, 4 + PacketOut.nDataBytes, 0,
           (sockaddr *)&nn->host_sockaddr, sizeof(nn->host_sockaddr));
    if (iRet != SOCKET_ERROR)
      break;
  }
  
  printf("Packet Client started\n\n");
  printf("Commands:\ns\tsend data descriptions\nf\tsend frame of data\nt\tsend "
         "test request\nq\tquit\n\n");
  
  int c;
  char szRequest[512];
  bool bExit = false;
  nTries = 3;
  while (!bExit) {
    c = _getch();
    switch (c) {
      case 's':
        // send NAT_REQUEST_MODELDEF command to server (will respond on the
        // "Command Listener" thread)
        PacketOut.iMessage = NAT_REQUEST_MODELDEF;
        PacketOut.nDataBytes = 0;
        nTries = 3;
        while (nTries--) {
          ssize_t iRet =
          sendto(nn->command, (char *)&PacketOut, 4 + PacketOut.nDataBytes,
                 0, (sockaddr *)&nn->host_sockaddr, sizeof(nn->host_sockaddr));
          if (iRet != SOCKET_ERROR)
            break;
        }
        break;
      case 'f':
        // send NAT_REQUEST_FRAMEOFDATA (will respond on the "Command Listener"
        // thread)
        PacketOut.iMessage = NAT_REQUEST_FRAMEOFDATA;
        PacketOut.nDataBytes = 0;
        nTries = 3;
        while (nTries--) {
          ssize_t iRet =
          sendto(nn->command, (char *)&PacketOut, 4 + PacketOut.nDataBytes,
                 0, (sockaddr *)&nn->host_sockaddr, sizeof(nn->host_sockaddr));
          if (iRet != SOCKET_ERROR)
            break;
        }
        break;
      case 't':
        // send NAT_MESSAGESTRING (will respond on the "Command Listener" thread)
        strcpy(szRequest, "TestRequest");
        PacketOut.iMessage = NAT_REQUEST;
        PacketOut.nDataBytes = (int)strlen(szRequest) + 1;
        strcpy(PacketOut.Data.szData, szRequest);
        nTries = 3;
        while (nTries--) {
          ssize_t iRet =
          sendto(nn->command, (char *)&PacketOut, 4 + PacketOut.nDataBytes,
                 0, (sockaddr *)&nn->host_sockaddr, sizeof(nn->host_sockaddr));
          if (iRet != SOCKET_ERROR)
            break;
        }
        break;
      case 'p':
        // send NAT_MESSAGESTRING (will respond on the "Command Listener" thread)
        strcpy(szRequest, "Ping");
        PacketOut.iMessage = NAT_PING;
        PacketOut.nDataBytes = (int)strlen(szRequest) + 1;
        strcpy(PacketOut.Data.szData, szRequest);
        nTries = 3;
        while (nTries--) {
          ssize_t iRet =
          sendto(nn->command, (char *)&PacketOut, 4 + PacketOut.nDataBytes,
                 0, (sockaddr *)&nn->host_sockaddr, sizeof(nn->host_sockaddr));
          if (iRet != SOCKET_ERROR) {
            printf("Sent ping command\n");
            break;
          }
        }
        break;
      case 'w': {
        char szCommand[512];
        long testVal;
        int returnCode;
        
        testVal = -50;
        sprintf(szCommand, "SetPlaybackStartFrame,%ld", testVal);
        returnCode = SendCommand(nn, szCommand);
        
        testVal = 1500;
        sprintf(szCommand, "SetPlaybackStopFrame,%ld", testVal);
        returnCode = SendCommand(nn, szCommand);
        
        testVal = 0;
        sprintf(szCommand, "SetPlaybackLooping,%ld", testVal);
        returnCode = SendCommand(nn, szCommand);
        
        testVal = 100;
        sprintf(szCommand, "SetPlaybackCurrentFrame,%ld", testVal);
        returnCode = SendCommand(nn, szCommand);
        
      } break;
      case 'q':
        bExit = true;
        break;
      default:
        break;
    }
  }
  
  return 0;
}

// command response listener thread
DWORD WINAPI CommandListenThread(void *dummy) {
  NatNet *nn = (NatNet *)dummy;
  socklen_t addr_len;
  long nDataBytesReceived;
  char str[256];
  sPacket PacketIn;
  addr_len = sizeof(struct sockaddr);
  
  while (1) {
    // blocking
    nDataBytesReceived =
    recvfrom(nn->command, (char *)&PacketIn, sizeof(sPacket), 0,
             (sockaddr *)&nn->host_sockaddr, &addr_len);
    
    if ((nDataBytesReceived == 0) || (nDataBytesReceived == SOCKET_ERROR))
      continue;
    
    // debug - print message
    sprintf(str,
            "[CommandThread] Received command from %s: Command=%d, nDataBytes=%d\n",
            inet_ntoa(nn->host_sockaddr.sin_addr), (int)PacketIn.iMessage,
            (int)PacketIn.nDataBytes);

    
    // handle command
    switch (PacketIn.iMessage) {
      case NAT_MODELDEF:
        UnpackDebug((char *)&PacketIn);
        break;
      case NAT_FRAMEOFDATA:
        UnpackDebug((char *)&PacketIn);
        break;
      case NAT_PINGRESPONSE:
        for (int i = 0; i < 4; i++) {
          NatNetVersion[i] = (int)PacketIn.Data.Sender.NatNetVersion[i];
          ServerVersion[i] = (int)PacketIn.Data.Sender.Version[i];
        }
        printf("[CommandThread] Ping echo form server version %d.%d.%d.%d\n", ServerVersion[0],
               ServerVersion[1], ServerVersion[2], ServerVersion[3]);
        break;
      case NAT_RESPONSE:
        gCommandResponseSize = PacketIn.nDataBytes;
        if (gCommandResponseSize == 4)
          memcpy(&gCommandResponse, &PacketIn.Data.lData[0],
                 gCommandResponseSize);
        else {
          memcpy(&gCommandResponseString[0], &PacketIn.Data.cData[0],
                 gCommandResponseSize);
          printf("[CommandThread] Response : %s", gCommandResponseString);
          gCommandResponse = 0; // ok
        }
        break;
      case NAT_UNRECOGNIZED_REQUEST:
        printf("[CommandThread] received 'unrecognized request'\n");
        gCommandResponseSize = 0;
        gCommandResponse = 1; // err
        break;
      case NAT_MESSAGESTRING:
        printf("[CommandThread] Received message: %s\n", PacketIn.Data.szData);
        break;
    }
  }
}

// Data listener thread
DWORD WINAPI DataListenThread(void *dummy) {
  NatNet *nn = (NatNet *)dummy;
  char szData[20000];
  long nDataBytesReceived;
  socklen_t addr_len = sizeof(struct sockaddr);
  
  while (1) {
    // Block until we receive a datagram from the network (from anyone including
    // ourselves)
    nDataBytesReceived = recvfrom(nn->data, szData, sizeof(szData), 0,
                                  (sockaddr *)&nn->host_sockaddr, &addr_len);
    if (nDataBytesReceived > 0 && errno != EINVAL) {
      UnpackDebug(szData);
    }
  }
  
  return 0;
}



// Send a command to Motive.
int SendCommand(NatNet *nn, char *szCommand) {
  // reset global result
  gCommandResponse = -1;
  
  // format command packet
  sPacket commandPacket;
  strcpy(commandPacket.Data.szData, szCommand);
  commandPacket.iMessage = NAT_REQUEST;
  commandPacket.nDataBytes = (int)strlen(commandPacket.Data.szData) + 1;
  
  // send command, and wait (a bit) for command response to set global response
  // var in CommandListenThread
  ssize_t iRet = sendto(nn->command, (char *)&commandPacket,
                        4 + commandPacket.nDataBytes, 0, (sockaddr *)&nn->host_sockaddr,
                        sizeof(nn->host_sockaddr));
  if (iRet == SOCKET_ERROR) {
    perror("Socket error sending command");
  } else {
    int waitTries = 5;
    while (waitTries--) {
      if (gCommandResponse != -1)
        break;
      sleep(30);
    }
    
    if (gCommandResponse == -1) {
      printf("Command response not received (timeout)");
    } else if (gCommandResponse == 0) {
      printf("Command response received with success");
    } else if (gCommandResponse > 0) {
      printf("Command response received with errors");
    }
  }
  
  return gCommandResponse;
}


void UnpackDebug(char *pData) {
  int major = NatNetVersion[0];
  int minor = NatNetVersion[1];
  
  char *ptr = pData;
  
  printf("Begin Packet\n-------\n");
  
  // message ID
  int MessageID = 0;
  memcpy(&MessageID, ptr, 2);
  ptr += 2;
  printf("Message ID : %d\n", MessageID);
  
  // size
  int nBytes = 0;
  memcpy(&nBytes, ptr, 2);
  ptr += 2;
  printf("Byte count : %d\n", nBytes);
  
  if (MessageID == 7) // FRAME OF MOCAP DATA packet
  {
    // frame number
    int frameNumber = 0;
    memcpy(&frameNumber, ptr, 4);
    ptr += 4;
    printf("Frame # : %d\n", frameNumber);
    
    // number of data sets (markersets, rigidbodies, etc)
    int nMarkerSets = 0;
    memcpy(&nMarkerSets, ptr, 4);
    ptr += 4;
    printf("Marker Set Count : %d\n", nMarkerSets);
    
    for (int i = 0; i < nMarkerSets; i++) {
      // Markerset name
      char szName[256];
      strcpy_s(szName, ptr);
      int nDataBytes = (int)strlen(szName) + 1;
      ptr += nDataBytes;
      printf("Model Name: %s\n", szName);
      
      // marker data
      int nMarkers = 0;
      memcpy(&nMarkers, ptr, 4);
      ptr += 4;
      printf("Marker Count : %d\n", nMarkers);
      
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
        printf("\tMarker %d : [x=%3.2f,y=%3.2f,z=%3.2f]\n", j, x, y, z);
      }
    }
    
    // unidentified markers
    int nOtherMarkers = 0;
    memcpy(&nOtherMarkers, ptr, 4);
    ptr += 4;
    printf("Unidentified Marker Count : %d\n", nOtherMarkers);
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
      printf("\tMarker %d : pos = [%3.2f,%3.2f,%3.2f]\n", j, x, y, z);
    }
    
    // rigid bodies
    int nRigidBodies = 0;
    memcpy(&nRigidBodies, ptr, 4);
    ptr += 4;
    printf("Rigid Body Count : %d\n", nRigidBodies);
    for (int j = 0; j < nRigidBodies; j++) {
      // rigid body pos/ori
      int ID = 0;
      memcpy(&ID, ptr, 4);
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
      ptr += 4;
      printf("Marker Count: %d\n", nRigidMarkers);
      int nBytes = nRigidMarkers * 3 * sizeof(float);
      float *markerData = (float *)malloc(nBytes);
      memcpy(markerData, ptr, nBytes);
      ptr += nBytes;
      
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
          printf("\tMarker %d: id=%d\tsize=%3.1f\tpos=[%3.2f,%3.2f,%3.2f]\n", k,
                 markerIDs[k], markerSizes[k], markerData[k * 3],
                 markerData[k * 3 + 1], markerData[k * 3 + 2]);
        }
        
        if (markerIDs)
          free(markerIDs);
        if (markerSizes)
          free(markerSizes);
        
      } else {
        for (int k = 0; k < nRigidMarkers; k++) {
          printf("\tMarker %d: pos = [%3.2f,%3.2f,%3.2f]\n", k,
                 markerData[k * 3], markerData[k * 3 + 1],
                 markerData[k * 3 + 2]);
        }
      }
      if (markerData)
        free(markerData);
      
      if (major >= 2) {
        // Mean marker error
        float fError = 0.0f;
        memcpy(&fError, ptr, 4);
        ptr += 4;
        printf("Mean marker error: %3.2f\n", fError);
      }
      
      // 2.6 and later
      if (((major == 2) && (minor >= 6)) || (major > 2) || (major == 0)) {
        // params
        short params = 0;
        memcpy(&params, ptr, 2);
        ptr += 2;
        bool bTrackingValid =
        params &
        0x01; // 0x01 : rigid body was successfully tracked in this frame
      }
      
    } // next rigid body
    
    // skeletons (version 2.1 and later)
    if (((major == 2) && (minor > 0)) || (major > 2)) {
      int nSkeletons = 0;
      memcpy(&nSkeletons, ptr, 4);
      ptr += 4;
      printf("Skeleton Count : %d\n", nSkeletons);
      for (int j = 0; j < nSkeletons; j++) {
        // skeleton id
        int skeletonID = 0;
        memcpy(&skeletonID, ptr, 4);
        ptr += 4;
        // # of rigid bodies (bones) in skeleton
        int nRigidBodies = 0;
        memcpy(&nRigidBodies, ptr, 4);
        ptr += 4;
        printf("Rigid Body Count : %d\n", nRigidBodies);
        for (int j = 0; j < nRigidBodies; j++) {
          // rigid body pos/ori
          int ID = 0;
          memcpy(&ID, ptr, 4);
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
                   k, markerIDs[k], markerSizes[k], markerData[k * 3],
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
      int nLabeledMarkers = 0;
      memcpy(&nLabeledMarkers, ptr, 4);
      ptr += 4;
      printf("Labeled Marker Count : %d\n", nLabeledMarkers);
      for (int j = 0; j < nLabeledMarkers; j++) {
        // id
        int ID = 0;
        memcpy(&ID, ptr, 4);
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
          ptr += 2;
          bool bOccluded =
          params & 0x01; // marker was not visible (occluded) in this frame
          bool bPCSolved =
          params & 0x02; // position provided by point cloud solve
          bool bModelSolved = params & 0x04; // position provided by model solve
        }
        
        printf("ID  : %d\n", ID);
        printf("pos : [%3.2f,%3.2f,%3.2f]\n", x, y, z);
        printf("size: [%3.2f]\n", size);
      }
    }
    
    // Force Plate data (version 2.9 and later)
    if (((major == 2) && (minor >= 9)) || (major > 2)) {
      int nForcePlates;
      memcpy(&nForcePlates, ptr, 4);
      ptr += 4;
      for (int iForcePlate = 0; iForcePlate < nForcePlates; iForcePlate++) {
        // ID
        int ID = 0;
        memcpy(&ID, ptr, 4);
        ptr += 4;
        printf("Force Plate : %d\n", ID);
        
        // Channel Count
        int nChannels = 0;
        memcpy(&nChannels, ptr, 4);
        ptr += 4;
        
        // Channel Data
        for (int i = 0; i < nChannels; i++) {
          printf(" Channel %d : ", i);
          int nFrames = 0;
          memcpy(&nFrames, ptr, 4);
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
    printf("latency : %3.3f\n", latency);
    
    // timecode
    unsigned int timecode = 0;
    memcpy(&timecode, ptr, 4);
    ptr += 4;
    unsigned int timecodeSub = 0;
    memcpy(&timecodeSub, ptr, 4);
    ptr += 4;
    char szTimecode[128] = "";
    TimecodeStringify(timecode, timecodeSub, szTimecode, 128);
    
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
    
    // frame params
    short params = 0;
    memcpy(&params, ptr, 2);
    ptr += 2;
    bool bIsRecording = params & 0x01; // 0x01 Motive is recording
    bool bTrackedModelsChanged =
    params & 0x02; // 0x02 Actively tracked model list has changed
    
    // end of data tag
    int eod = 0;
    memcpy(&eod, ptr, 4);
    ptr += 4;
    printf("End Packet\n-------------\n");
    
  } else if (MessageID == 5) // Data Descriptions
  {
    // number of datasets
    int nDatasets = 0;
    memcpy(&nDatasets, ptr, 4);
    ptr += 4;
    printf("Dataset Count : %d\n", nDatasets);
    
    for (int i = 0; i < nDatasets; i++) {
      printf("Dataset %d\n", i);
      
      int type = 0;
      memcpy(&type, ptr, 4);
      ptr += 4;
      printf("Type %d: %d\n", i, type);
      
      if (type == 0) // markerset
      {
        // name
        char szName[256];
        strcpy_s(szName, ptr);
        int nDataBytes = (int)strlen(szName) + 1;
        ptr += nDataBytes;
        printf("Markerset Name: %s\n", szName);
        
        // marker data
        int nMarkers = 0;
        memcpy(&nMarkers, ptr, 4);
        ptr += 4;
        printf("Marker Count : %d\n", nMarkers);
        
        for (int j = 0; j < nMarkers; j++) {
          char szName[256];
          strcpy_s(szName, ptr);
          int nDataBytes = (int)strlen(szName) + 1;
          ptr += nDataBytes;
          printf("Marker Name: %s\n", szName);
        }
      } else if (type == 1) // rigid body
      {
        if (major >= 2) {
          // name
          char szName[MAX_NAMELENGTH];
          strcpy(szName, ptr);
          ptr += strlen(ptr) + 1;
          printf("Name: %s\n", szName);
        }
        
        int ID = 0;
        memcpy(&ID, ptr, 4);
        ptr += 4;
        printf("ID : %d\n", ID);
        
        int parentID = 0;
        memcpy(&parentID, ptr, 4);
        ptr += 4;
        printf("Parent ID : %d\n", parentID);
        
        float xoffset = 0;
        memcpy(&xoffset, ptr, 4);
        ptr += 4;
        printf("X Offset : %3.2f\n", xoffset);
        
        float yoffset = 0;
        memcpy(&yoffset, ptr, 4);
        ptr += 4;
        printf("Y Offset : %3.2f\n", yoffset);
        
        float zoffset = 0;
        memcpy(&zoffset, ptr, 4);
        ptr += 4;
        printf("Z Offset : %3.2f\n", zoffset);
        
      } else if (type == 2) // skeleton
      {
        char szName[MAX_NAMELENGTH];
        strcpy(szName, ptr);
        ptr += strlen(ptr) + 1;
        printf("Name: %s\n", szName);
        
        int ID = 0;
        memcpy(&ID, ptr, 4);
        ptr += 4;
        printf("ID : %d\n", ID);
        
        int nRigidBodies = 0;
        memcpy(&nRigidBodies, ptr, 4);
        ptr += 4;
        printf("RigidBody (Bone) Count : %d\n", nRigidBodies);
        
        for (int i = 0; i < nRigidBodies; i++) {
          if (major >= 2) {
            // RB name
            char szName[MAX_NAMELENGTH];
            strcpy(szName, ptr);
            ptr += strlen(ptr) + 1;
            printf("Rigid Body Name: %s\n", szName);
          }
          
          int ID = 0;
          memcpy(&ID, ptr, 4);
          ptr += 4;
          printf("RigidBody ID : %d\n", ID);
          
          int parentID = 0;
          memcpy(&parentID, ptr, 4);
          ptr += 4;
          printf("Parent ID : %d\n", parentID);
          
          float xoffset = 0;
          memcpy(&xoffset, ptr, 4);
          ptr += 4;
          printf("X Offset : %3.2f\n", xoffset);
          
          float yoffset = 0;
          memcpy(&yoffset, ptr, 4);
          ptr += 4;
          printf("Y Offset : %3.2f\n", yoffset);
          
          float zoffset = 0;
          memcpy(&zoffset, ptr, 4);
          ptr += 4;
          printf("Z Offset : %3.2f\n", zoffset);
        }
      }
      
    } // next dataset
    
    printf("End Packet\n-------------\n");
    
  } else {
    printf("Unrecognized Packet Type.\n");
  }
}
