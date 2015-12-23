//
//  NatNetC.h
//  NatNetCLI
//
//  Created by Paolo Bosetti on 21/12/15.
//  Copyright © 2015 UniTN. All rights reserved.
//

#ifndef NatNetC_h
#define NatNetC_h

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

#define MULTICAST_ADDRESS "239.255.42.99" // IANA, local network
#define PORT_COMMAND 1510
#define PORT_DATA 1511

#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define MAX_NAMELENGTH 256

// NATNET message ids
#define NAT_PING 0
#define NAT_PINGRESPONSE 1
#define NAT_REQUEST 2
#define NAT_RESPONSE 3
#define NAT_REQUEST_MODELDEF 4
#define NAT_MODELDEF 5
#define NAT_REQUEST_FRAMEOFDATA 6
#define NAT_FRAMEOFDATA 7
#define NAT_MESSAGESTRING 8
#define NAT_UNRECOGNIZED_REQUEST 100
#define UNDEFINED 999999.9999

// max size of packet (actual packet size is dynamic)
#define MAX_PACKETSIZE 100000
#define RCV_BUFSIZE 0x100000

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
} NatNet;

#pragma mark -
#pragma mark Function Declarations

NatNet *NatNet_new(char *my_addr, char *their_addr, char *multicast_addr,
                   u_short command_port, u_short data_port);

void NatNet_free(NatNet *nn);

int NatNet_init(NatNet *nn, char *my_addr, char *their_addr,
                char *multicast_addr, u_short command_port, u_short data_port);

int NatNet_bind(NatNet *nn);

uint NatNet_send_pkt(NatNet *nn, NatNet_packet packet, uint tries);
uint NatNet_send_cmd(NatNet *nn, char *cmd, uint tries);
long NatNet_recv_cmd(NatNet *nn, char *cmd, size_t len);


SOCKET CreateCommandSocket(struct in_addr IP_Address, unsigned short uPort,
                           int bufsize);
SOCKET CreateDataSocket(struct in_addr MyAddress, struct in_addr multicast_addr,
                        unsigned short uPort, int bufsize);

bool IPAddress_StringToAddr(char *szNameOrAddress, struct in_addr *Address);
int GetLocalIPAddresses(unsigned long Addresses[], int nMax);
bool DecodeTimecode(unsigned int inTimecode, unsigned int inTimecodeSubframe,
                    int *hour, int *minute, int *second, int *frame,
                    int *subframe);
bool TimecodeStringify(unsigned int inTimecode, unsigned int inTimecodeSubframe,
                       char *Buffer, int BufferSize);

void UnpackDebug(NatNet *nn, char *pData);

#endif /* NatNetC_h */