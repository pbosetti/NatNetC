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

#include <stdio.h>



#ifdef _MSC_VER
#pragma warning(disable : 4996)
#include <tchar.h>
#include <conio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#else
#pragma GCC diagnostic ignored "-Wunused"
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

typedef struct in_addr in_addr;
typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;
typedef enum {
  false = 0,
  true,
} bool;

#define DWORD void
#define WINAPI *
#define SOCKET int
#define MAX_PATH PATH_MAX
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define closesocket close
#define sprintf_s snprintf
#define strcpy_s(dst, src) strcpy(dst, src)
#define _getch getchar

#endif

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

#define MAX_PACKETSIZE                                                         \
  100000 // max size of packet (actual packet size is dynamic)

#define MULTICAST_ADDRESS "239.255.42.99" // IANA, local network
#define PORT_COMMAND 1510
#define PORT_DATA 1511

#pragma mark -
#pragma mark Types

// sender
typedef struct {
  char szName[MAX_NAMELENGTH]; // sending app's name
  unsigned char
      Version[4]; // sending app's version [major.minor.build.revision]
  unsigned char NatNetVersion[4]; // sending app's NatNet version
                                  // [major.minor.build.revision]

} sSender;

typedef struct {
  unsigned short iMessage;   // message ID (e.g. NAT_FRAMEOFDATA)
  unsigned short nDataBytes; // Num bytes in payload
  union {
    unsigned char cData[MAX_PACKETSIZE];
    char szData[MAX_PACKETSIZE];
    unsigned long lData[MAX_PACKETSIZE / 4];
    float fData[MAX_PACKETSIZE / 4];
    sSender Sender;
  } Data; // Payload

} sPacket;

#pragma mark -
#pragma mark GLOBALS

SOCKET CommandSocket;
SOCKET DataSocket;
in_addr ServerAddress;
sockaddr_in HostAddr;

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
SOCKET CreateCommandSocket(in_addr_t IP_Address, unsigned short uPort);
bool IPAddress_StringToAddr(char *szNameOrAddress, struct in_addr *Address);
void Unpack(char *pData);
int GetLocalIPAddresses(unsigned long Addresses[], int nMax);
int SendCommand(char *szCOmmand);

#pragma mark -
#pragma mark Function Definitions

int main(int argc, char *argv[]) {
  int retval;
  char szMyIPAddress[128] = "";
  char szServerIPAddress[128] = "";
  in_addr MyAddress, MultiCastAddress;
  int optval = 0x100000;
  unsigned int optval_size = 4;

#ifdef _MSC_VER
  WSADATA WsaData;
  if (WSAStartup(0x202, &WsaData) == SOCKET_ERROR) {
    printf("[PacketClient] WSAStartup failed (error: %d)\n", WSAGetLastError());
    WSACleanup();
    return 0;
  }
#endif

  // server address
  if (argc > 1) {
    strcpy_s(szServerIPAddress, argv[1]); // specified on command line
    retval = IPAddress_StringToAddr(szServerIPAddress, &ServerAddress);
  } else {
    GetLocalIPAddresses((unsigned long *)&ServerAddress, 1);
#ifdef _MSC_VER
    sprintf_s(szServerIPAddress, "%d.%d.%d.%d", ServerAddress.S_un.S_un_b.s_b1,
              ServerAddress.S_un.S_un_b.s_b2, ServerAddress.S_un.S_un_b.s_b3,
              ServerAddress.S_un.S_un_b.s_b4);
#else
    strcpy(szServerIPAddress, inet_ntoa(ServerAddress));
#endif
  }
  // client address
  if (argc > 2) {
    strcpy_s(szMyIPAddress, argv[2]); // specified on command line
    retval = IPAddress_StringToAddr(szMyIPAddress, &MyAddress);
  } else {
    GetLocalIPAddresses((unsigned long *)&MyAddress, 1);
#ifdef _MSC_VER
    sprintf_s(szMyIPAddress, "%d.%d.%d.%d", MyAddress.S_un.S_un_b.s_b1,
              MyAddress.S_un.S_un_b.s_b2, MyAddress.S_un.S_un_b.s_b3,
              MyAddress.S_un.S_un_b.s_b4);
#else
    strcpy(szMyIPAddress, inet_ntoa(MyAddress));
#endif
  }
#ifdef _MSC_VER
  MultiCastAddress.S_un.S_addr = inet_addr(MULTICAST_ADDRESS);
#else
  MultiCastAddress.s_addr = inet_addr(MULTICAST_ADDRESS);
#endif
  printf("Client (this machine): %s\n", szMyIPAddress);
  printf("Server:                %s\n", szServerIPAddress);
  printf("Multicast Group:       %s\n", MULTICAST_ADDRESS);
  printf("Data port:             %d\n", PORT_DATA);
  printf("Command port:          %d\n", PORT_COMMAND);
  // create "Command" socket
  int port = 0;
#ifdef _MSC_VER
  CommandSocket = CreateCommandSocket(MyAddress.S_un.S_addr, port);
#else
  CommandSocket = CreateCommandSocket(MyAddress.s_addr, port);
  pthread_t command_listener;
#endif
  if (CommandSocket == -1) {
    perror("Error creating command socket");
    return 1;
  } else {
    // [optional] set to non-blocking
    // u_long iMode=1;
    // ioctlsocket(CommandSocket,FIONBIO,&iMode);
    // set buffer
    setsockopt(CommandSocket, SOL_SOCKET, SO_RCVBUF, (char *)&optval, 4);
    getsockopt(CommandSocket, SOL_SOCKET, SO_RCVBUF, (char *)&optval,
               &optval_size);
    if (optval != 0x100000) {
      // err - actual size...
      perror("Error setting socket options");
    }
// startup our "Command Listener" thread
#ifdef _MSC_VER
    SECURITY_ATTRIBUTES security_attribs;
    security_attribs.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attribs.lpSecurityDescriptor = NULL;
    security_attribs.bInheritHandle = TRUE;
    DWORD CommandListenThread_ID;
    HANDLE CommandListenThread_Handle;
    CommandListenThread_Handle =
        CreateThread(&security_attribs, 0, CommandListenThread, NULL, 0,
                     &CommandListenThread_ID);
#else
    if (pthread_create(&command_listener, NULL, CommandListenThread, NULL) !=
        0) {
      perror("Error creating command listener thread");
      return 1;
    }
#endif
  }

  // create a "Data" socket
  DataSocket = socket(AF_INET, SOCK_DGRAM, 0);

  // allow multiple clients on same machine to use address/port
  int value = 1;
  retval = setsockopt(DataSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&value,
                      sizeof(value));
  if (retval == SOCKET_ERROR) {
    perror("Error setting options for data socket");
    closesocket(DataSocket);
    return -1;
  }

  struct sockaddr_in MySocketAddr;
  memset(&MySocketAddr, 0, sizeof(MySocketAddr));
  MySocketAddr.sin_family = AF_INET;
  MySocketAddr.sin_port = htons(PORT_DATA);
  MySocketAddr.sin_addr.s_addr = htonl(INADDR_ANY); // MyAddress;
  if (bind(DataSocket, (struct sockaddr *)&MySocketAddr,
           sizeof(struct sockaddr)) == SOCKET_ERROR) {
#ifdef _MSC_VER
    printf("[PacketClient] bind failed (error: %d)\n", WSAGetLastError());
    WSACleanup();
#else
    perror("[PacketClient] bind failed");
#endif
    return -1;
  }
  // join multicast group
  struct ip_mreq Mreq;
  Mreq.imr_multiaddr = MultiCastAddress;
  Mreq.imr_interface = MyAddress;
  retval = setsockopt(DataSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&Mreq,
                      sizeof(Mreq));
  if (retval == SOCKET_ERROR) {
#ifdef _MSC_VER
    printf("[PacketClient] join failed (error: %d)\n", WSAGetLastError());
    WSACleanup();
#else
    perror("[PacketClient] join failed");
#endif
    return -1;
  }
  // create a 1MB buffer
  setsockopt(DataSocket, SOL_SOCKET, SO_RCVBUF, (char *)&optval, 4);
  getsockopt(DataSocket, SOL_SOCKET, SO_RCVBUF, (char *)&optval, &optval_size);
  if (optval != 0x100000) {
    printf("[PacketClient] ReceiveBuffer size = %d", optval);
  }

  struct timeval timeout = {.tv_sec = 0, .tv_usec = 50000};
  if (setsockopt(DataSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                 sizeof(timeout)) != 0) {
    perror("[PacketClient] Setting DataSocket timeout");
  }
// startup our "Data Listener" thread
#ifdef _MSC_VER
  SECURITY_ATTRIBUTES security_attribs;
  security_attribs.nLength = sizeof(SECURITY_ATTRIBUTES);
  security_attribs.lpSecurityDescriptor = NULL;
  security_attribs.bInheritHandle = TRUE;
  DWORD DataListenThread_ID;
  HANDLE DataListenThread_Handle;
  DataListenThread_Handle = CreateThread(&security_attribs, 0, DataListenThread,
                                         NULL, 0, &DataListenThread_ID);
#else
  pthread_t data_listener;
  if (pthread_create(&data_listener, NULL, DataListenThread, NULL) != 0) {
    perror("[PacketClient] Error creating data listener thread");
    return 1;
  }
#endif

  // server address for commands
  memset(&HostAddr, 0, sizeof(HostAddr));
  HostAddr.sin_family = AF_INET;
  HostAddr.sin_port = htons(PORT_COMMAND);
  HostAddr.sin_addr = ServerAddress;

  // send initial ping command
  sPacket PacketOut;
  PacketOut.iMessage = NAT_PING;
  PacketOut.nDataBytes = 0;
  int nTries = 3;
  while (nTries--) {
    ssize_t iRet =
        sendto(CommandSocket, (char *)&PacketOut, 4 + PacketOut.nDataBytes, 0,
               (sockaddr *)&HostAddr, sizeof(HostAddr));
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
            sendto(CommandSocket, (char *)&PacketOut, 4 + PacketOut.nDataBytes,
                   0, (sockaddr *)&HostAddr, sizeof(HostAddr));
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
            sendto(CommandSocket, (char *)&PacketOut, 4 + PacketOut.nDataBytes,
                   0, (sockaddr *)&HostAddr, sizeof(HostAddr));
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
            sendto(CommandSocket, (char *)&PacketOut, 4 + PacketOut.nDataBytes,
                   0, (sockaddr *)&HostAddr, sizeof(HostAddr));
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
            sendto(CommandSocket, (char *)&PacketOut, 4 + PacketOut.nDataBytes,
                   0, (sockaddr *)&HostAddr, sizeof(HostAddr));
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
      returnCode = SendCommand(szCommand);

      testVal = 1500;
      sprintf(szCommand, "SetPlaybackStopFrame,%ld", testVal);
      returnCode = SendCommand(szCommand);

      testVal = 0;
      sprintf(szCommand, "SetPlaybackLooping,%ld", testVal);
      returnCode = SendCommand(szCommand);

      testVal = 100;
      sprintf(szCommand, "SetPlaybackCurrentFrame,%ld", testVal);
      returnCode = SendCommand(szCommand);

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
  socklen_t addr_len;
  long nDataBytesReceived;
  char str[256];
  sockaddr_in TheirAddress;
  sPacket PacketIn;
  addr_len = sizeof(struct sockaddr);

  while (1) {
    // blocking
    nDataBytesReceived =
        recvfrom(CommandSocket, (char *)&PacketIn, sizeof(sPacket), 0,
                 (sockaddr *)&TheirAddress, &addr_len);

    if ((nDataBytesReceived == 0) || (nDataBytesReceived == SOCKET_ERROR))
      continue;

// debug - print message
#ifdef _MSC_VER
    sprintf(
        str,
        "[Client] Received command from %d.%d.%d.%d: Command=%d, nDataBytes=%d",
        TheirAddress.sin_addr.S_un.S_un_b.s_b1,
        TheirAddress.sin_addr.S_un.S_un_b.s_b2,
        TheirAddress.sin_addr.S_un.S_un_b.s_b3,
        TheirAddress.sin_addr.S_un.S_un_b.s_b4, (int)PacketIn.iMessage,
        (int)PacketIn.nDataBytes);
#else
    sprintf(str,
            "[Client] Received command from %s: Command=%d, nDataBytes=%d\n",
            inet_ntoa(TheirAddress.sin_addr), (int)PacketIn.iMessage,
            (int)PacketIn.nDataBytes);
#endif

    // handle command
    switch (PacketIn.iMessage) {
    case NAT_MODELDEF:
      Unpack((char *)&PacketIn);
      break;
    case NAT_FRAMEOFDATA:
      Unpack((char *)&PacketIn);
      break;
    case NAT_PINGRESPONSE:
      for (int i = 0; i < 4; i++) {
        NatNetVersion[i] = (int)PacketIn.Data.Sender.NatNetVersion[i];
        ServerVersion[i] = (int)PacketIn.Data.Sender.Version[i];
      }
      printf("Ping echo form server version %d.%d.%d.%d\n", ServerVersion[0],
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
        printf("Response : %s", gCommandResponseString);
        gCommandResponse = 0; // ok
      }
      break;
    case NAT_UNRECOGNIZED_REQUEST:
      printf("[Client] received 'unrecognized request'\n");
      gCommandResponseSize = 0;
      gCommandResponse = 1; // err
      break;
    case NAT_MESSAGESTRING:
      printf("[Client] Received message: %s\n", PacketIn.Data.szData);
      break;
    }
  }
#ifdef _MSC_VER
  return 0;
#endif
}

// Data listener thread
DWORD WINAPI DataListenThread(void *dummy) {
  char szData[20000];
  long nDataBytesReceived;
  socklen_t addr_len = sizeof(struct sockaddr);
  sockaddr_in TheirAddress;

  while (1) {
    // Block until we receive a datagram from the network (from anyone including
    // ourselves)
    nDataBytesReceived = recvfrom(DataSocket, szData, sizeof(szData), 0,
                                  (sockaddr *)&TheirAddress, &addr_len);
    if (nDataBytesReceived > 0 && errno != EINVAL) {
      Unpack(szData);
    }
  }

  return 0;
}

SOCKET CreateCommandSocket(in_addr_t IP_Address, unsigned short uPort) {
  struct sockaddr_in my_addr;
  static unsigned long ivalue;
  static unsigned long bFlag;
  int nlengthofsztemp = 64;
  SOCKET sockfd;

  // Create a blocking, datagram socket
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
    return -1;
  }

  // bind socket
  memset(&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(uPort);
#ifdef _MSC_VER
  my_addr.sin_addr.S_un.S_addr = IP_Address;
#else
  my_addr.sin_addr.s_addr = IP_Address;
#endif
  if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) ==
      SOCKET_ERROR) {
    closesocket(sockfd);
    return -1;
  }

  // set to broadcast mode
  ivalue = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (char *)&ivalue,
                 sizeof(ivalue)) == SOCKET_ERROR) {
    closesocket(sockfd);
    return -1;
  }

  return sockfd;
}

// Send a command to Motive.
int SendCommand(char *szCommand) {
  // reset global result
  gCommandResponse = -1;

  // format command packet
  sPacket commandPacket;
  strcpy(commandPacket.Data.szData, szCommand);
  commandPacket.iMessage = NAT_REQUEST;
  commandPacket.nDataBytes = (int)strlen(commandPacket.Data.szData) + 1;

  // send command, and wait (a bit) for command response to set global response
  // var in CommandListenThread
  ssize_t iRet = sendto(CommandSocket, (char *)&commandPacket,
                        4 + commandPacket.nDataBytes, 0, (sockaddr *)&HostAddr,
                        sizeof(HostAddr));
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
  if ((retVal = getnameinfo((sockaddr *)&saGNI, sizeof(sockaddr), hostName, 256,
                            servInfo, 256, NI_NUMERICSERV)) != 0) {
#ifdef _MSC_VER
    printf("[PacketClient] GetHostByAddr failed. Error #: %ld\n",
           WSAGetLastError());
#else
    perror("[PacketClient] GetHostByAddr failed");
#endif
    return false;
  }

#ifdef _MSC_VER
  Address->S_un.S_addr = saGNI.sin_addr.S_un.S_addr;
#else
  Address->s_addr = saGNI.sin_addr.s_addr;
#endif

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

#ifdef _MSC_VER
  if (GetComputerName(szMyName, &NameLength) != true) {
    printf("[PacketClient] get computer name  failed. Error #: %ld\n",
           WSAGetLastError());
    return 0;
  }
#else
  if (gethostname(szMyName, NameLength) != 0) {
    perror("[PacketClient] get computer name  failed");
    return 0;
  }
#endif

  memset(&aiHints, 0, sizeof(aiHints));
  aiHints.ai_family = AF_INET;
  aiHints.ai_socktype = SOCK_DGRAM;
  aiHints.ai_protocol = IPPROTO_UDP;
  if ((retVal = getaddrinfo(szMyName, port, &aiHints, &aiList)) != 0) {
#ifdef _MSC_VER
    printf("[PacketClient] getaddrinfo failed. Error #: %ld\n",
           WSAGetLastError());
#else
    perror("[PacketClient] getaddrinfo failed");
#endif
    return 0;
  }

  memcpy(&addr, aiList->ai_addr, aiList->ai_addrlen);
  freeaddrinfo(aiList);
#ifdef _MSC_VER
  Addresses[0] = addr.sin_addr.S_un.S_addr;
#else
  Addresses[0] = addr.sin_addr.s_addr;
#endif

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

  sprintf_s(Buffer, BufferSize, "%2d:%2d:%2d:%2d.%d", hour, minute, second,
            frame, subframe);
  for (unsigned int i = 0; i < strlen(Buffer); i++)
    if (Buffer[i] == ' ')
      Buffer[i] = '0';

  return bValid;
}

void Unpack(char *pData) {
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