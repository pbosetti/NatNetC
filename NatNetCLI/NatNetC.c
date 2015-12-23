//
//  NatNetC.c
//  NatNetCLI
//
//  Created by Paolo Bosetti on 21/12/15.
//  Copyright © 2015 UniTN. All rights reserved.
//

#include "NatNetC.h"


NatNet * NatNet_new(char *my_addr, char *their_addr, char *multicast_addr, u_short command_port, u_short data_port) {
  NatNet *nn = (NatNet*)malloc(sizeof(NatNet));
  if (NatNet_init(nn, my_addr, their_addr, multicast_addr, command_port, data_port)) {
    free(nn);
    return NULL;
  }
  return nn;
}

int NatNet_init(NatNet *nn, char *my_addr, char *their_addr, char *multicast_addr, u_short command_port, u_short data_port) {
  memset(nn, 0, sizeof(*nn));
  strcpy(nn->my_addr, my_addr);
  strcpy(nn->their_addr, their_addr);
  strcpy(nn->multicast_addr, multicast_addr);
  nn->command_port = command_port;
  nn->data_port = data_port;
  nn->receive_bufsize = RCV_BUFSIZE;
  nn->timeout.tv_sec = 0;
  nn->timeout.tv_usec = 100000;
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
  if (setsockopt(nn->data, SOL_SOCKET, SO_RCVTIMEO, &nn->timeout,
                 sizeof(nn->timeout)) != 0) {
    perror("Setting data socket timeout");
    retval--;
  }
  if (setsockopt(nn->data, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq,
                 sizeof(mreq))) {
    perror("Joining multicast group");
    retval--;
  }
  if (bind(nn->data, (struct sockaddr *)&data_sockaddr, sizeof(data_sockaddr))) {
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
  if (bind(nn->command, (struct sockaddr *)&cmd_sockaddr, sizeof(cmd_sockaddr))) {
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






SOCKET CreateCommandSocket(in_addr IP_Address, unsigned short uPort, int bufsize) {
  sockaddr_in my_addr;
  static unsigned long ivalue;
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
  my_addr.sin_addr.S_un.S_addr = IP_Address.S_un.S_addr;
#else
  my_addr.sin_addr.s_addr = IP_Address.s_addr;
#endif
  if (bind(sockfd, (struct sockaddr *)&my_addr,
           sizeof(struct sockaddr)) == SOCKET_ERROR) {
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
  
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufsize,
                 sizeof(bufsize)) == SOCKET_ERROR) {
    closesocket(sockfd);
    return -1;
  }
  
  return sockfd;
}

SOCKET CreateDataSocket(in_addr my_addr, in_addr multicast_addr,
                        unsigned short uPort, int bufsize) {
  SOCKET sockfd;
  int value = 1;
  struct timeval timeout = {.tv_sec = 0, .tv_usec = 50000};
  sockaddr_in MySocketAddr;
  
  // create a "Data" socket
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  
  // allow multiple clients on same machine to use address/port
  value = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&value,
                 sizeof(value)) == SOCKET_ERROR) {
    closesocket(sockfd);
    return -1;
  }
  
  memset(&MySocketAddr, 0, sizeof(MySocketAddr));
  MySocketAddr.sin_family = AF_INET;
  MySocketAddr.sin_port = htons(uPort);
#ifdef _MSC_VER
  MySocketAddr.sin_addr.U_un.S_addr = htonl(INADDR_ANY);
#else
  MySocketAddr.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
  if (bind(sockfd, (struct sockaddr *)&MySocketAddr,
           sizeof(struct sockaddr)) == SOCKET_ERROR) {
#ifdef _MSC_VER
    printf("[PacketClient] bind failed (error: %d)\n", WSAGetLastError());
    WSACleanup();
#else
    closesocket(sockfd);
    perror("[PacketClient] bind failed");
#endif
    return -1;
  }
  
  // join multicast group
  struct ip_mreq Mreq;
  Mreq.imr_multiaddr = multicast_addr;
  Mreq.imr_interface = my_addr;
  if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&Mreq,
                 sizeof(Mreq)) == SOCKET_ERROR) {
#ifdef _MSC_VER
    printf("[PacketClient] join failed (error: %d)\n", WSAGetLastError());
    WSACleanup();
#else
    perror("[PacketClient] join failed");
#endif
    return -1;
  }

  // create a 1MB buffer
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufsize,
                 sizeof(bufsize)) == SOCKET_ERROR) {
    return -1;
  }
  
  // Set timeout
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                 sizeof(timeout)) != 0) {
    return -1;
  }
  
  return sockfd;
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

