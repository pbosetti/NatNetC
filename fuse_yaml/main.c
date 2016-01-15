//
//  main.c
//  fuse_yaml
//
//  Created by Mirko Brentari on 14/01/16.
//  Copyright © 2016 UniTN. All rights reserved.
//
/*
 Decodes NatNet packets directly.

 Usage [optional]:

 PacketClient [ServerIP] [LocalIP]

 [ServerIP]			IP address of server ( defaults to
 local machine)
 [LocalIP]			IP address of client ( defaults to
 local machine)

 */

#include <NatNetC.h>

#include <stdio.h>

#pragma warning(disable : 4996)
#pragma GCC diagnostic ignored "-Wunused"

#pragma mark -
#pragma mark Function Declarations

void *CommandListenThread(void *dummy);
void *DataListenThread(void *dummy);
void UnpackDebug(NatNet *nn, char *pData);

#pragma mark -
#pragma mark Function Definitions

int main(int argc, char *argv[]) {
  int retval;
  char szMyIPAddress[128] = "";
  char szServerIPAddress[128] = "";
  struct in_addr ServerAddress, MyAddress;

  // server address
  if (argc > 1) {
    strcpy(szServerIPAddress, argv[1]); // specified on command line
    retval = IPAddress_StringToAddr(szServerIPAddress, &ServerAddress);
  } else {
    GetLocalIPAddresses((unsigned long *)&ServerAddress, 1);
    strcpy(szServerIPAddress, inet_ntoa(ServerAddress));
  }

  // client address
  if (argc > 2) {
    strcpy(szMyIPAddress, argv[2]); // specified on command line
    retval = IPAddress_StringToAddr(szMyIPAddress, &MyAddress);
  } else {
    GetLocalIPAddresses((unsigned long *)&MyAddress, 1);
    strcpy(szMyIPAddress, inet_ntoa(MyAddress));
  }

  NatNet *nn = NatNet_new(szMyIPAddress, szServerIPAddress, MULTICAST_ADDRESS,
                          PORT_COMMAND, PORT_DATA);
  if (NatNet_bind(nn)) {
    perror("Error binding NetNat");
    return -1;
  }

  printf("Client (this machine): %s\n", nn->my_addr);
  printf("Server:                %s\n", nn->their_addr);
  printf("Multicast Group:       %s\n", nn->multicast_addr);
  printf("Data port:             %d\n", nn->data_port);
  printf("Command port:          %d\n", nn->command_port);

  // send initial ping command
  NatNet_packet PacketOut;
  PacketOut.iMessage = NAT_PING;
  PacketOut.nDataBytes = 0;
  NatNet_send_pkt(nn, PacketOut, 3);

  // startup our "Command Listener" thread
  pthread_t command_listener;
  if (pthread_create(&command_listener, NULL, CommandListenThread, nn) != 0) {
    perror("[PacketClient] Error creating command listener thread");
    return 1;
  }

  // startup our "Data Listener" thread
  pthread_t data_listener;
  if (pthread_create(&data_listener, NULL, DataListenThread, nn) != 0) {
    perror("[PacketClient] Error creating data listener thread");
    return 1;
  }

  printf("Packet Client started\n\n");
  printf("Commands:q\tquit\n\n");

  int c;
  c = getchar();
  if (c == 'q') {
    NatNet_free(nn);
    return 0;
  }

  NatNet_free(nn);
  return 0;
}

// get version Mirko
void get_version(NatNet *nn) {
  // send initial ping command
  NatNet_packet PacketOut;
  PacketOut.iMessage = NAT_PING;
  PacketOut.nDataBytes = 0;
  NatNet_send_pkt(nn, PacketOut, 3);
  
  // create blocking socket
  
}

// command response listener thread
void *CommandListenThread(void *dummy) {
  NatNet *nn = (NatNet *)dummy;
  socklen_t addr_len;
  long nDataBytesReceived;
  NatNet_packet PacketIn;
  addr_len = sizeof(struct sockaddr);

  while (1) {
    // blocking
    nDataBytesReceived =
        NatNet_recv_cmd(nn, (char *)&PacketIn, sizeof(NatNet_packet));

    if ((nDataBytesReceived == 0) || (nDataBytesReceived == SOCKET_ERROR))
      continue;

    // debug - print message
    printf(
        "[CommandThread] Received command from %s: Command=%d, nDataBytes=%d\n",
        inet_ntoa(nn->host_sockaddr.sin_addr), (int)PacketIn.iMessage,
        (int)PacketIn.nDataBytes);

    // handle command
    switch (PacketIn.iMessage) {
    case NAT_MODELDEF:
      // UnpackDebug(nn, (char *)&PacketIn);
      break;
    case NAT_FRAMEOFDATA:
      // UnpackDebug(nn, (char *)&PacketIn);
      break;
    case NAT_PINGRESPONSE:
      for (int i = 0; i < 4; i++) {
        nn->NatNet_ver[i] = (int)PacketIn.Data.Sender.NatNetVersion[i];
        nn->server_ver[i] = (int)PacketIn.Data.Sender.Version[i];
      }
      printf("[CommandThread] Ping echo form server version %d.%d.%d.%d\n",
             nn->server_ver[0], nn->server_ver[1], nn->server_ver[2],
             nn->server_ver[3]);
      printf("[CommandThread]                NatNet version %d.%d.%d.%d\n",
             nn->NatNet_ver[0], nn->NatNet_ver[1], nn->NatNet_ver[2],
             nn->NatNet_ver[3]);
      break;
    case NAT_RESPONSE:
      nn->cmd_response.response_size = PacketIn.nDataBytes;
      if (nn->cmd_response.response_size == 4)
        memcpy(&nn->cmd_response.response, &PacketIn.Data.lData[0],
               nn->cmd_response.response_size);
      else {
        memcpy(&nn->cmd_response.response_string[0], &PacketIn.Data.cData[0],
               nn->cmd_response.response_size);
        printf("[CommandThread] Response : %s",
               nn->cmd_response.response_string);
        nn->cmd_response.response = 0; // ok
      }
      break;
    case NAT_UNRECOGNIZED_REQUEST:
      printf("[CommandThread] received 'unrecognized request'\n");
      nn->cmd_response.response_size = 0;
      nn->cmd_response.response = 1; // err
      break;
    case NAT_MESSAGESTRING:
      printf("[CommandThread] Received message: %s\n", PacketIn.Data.szData);
      break;
    }
  }
}

// Data listener thread
void *DataListenThread(void *dummy) {
  NatNet *nn = (NatNet *)dummy;
  char szData[20000];
  size_t len;
  long nDataBytesReceived;

  while (1) {
    // Block until we receive a datagram from the network (from anyone including
    // ourselves)
    nDataBytesReceived = NatNet_recv_data(nn, szData, sizeof(szData));
    if (nDataBytesReceived > 0 && errno != EINVAL) {
      NatNet_unpack_all(nn, szData, &len);
      printf("---\nBodies: %zu\n", nn->last_frame->n_bodies);
      for (int i = 0; i < nn->last_frame->n_bodies; i++) {
        printf("  Body %d L[%11.6f %11.6f %11.6f]  O[%11.6f %11.6f %11.6f "
               "%11.6f]\n",
               i, nn->last_frame->bodies[i]->loc.x,
               nn->last_frame->bodies[i]->loc.y,
               nn->last_frame->bodies[i]->loc.z,
               nn->last_frame->bodies[i]->ori.qx,
               nn->last_frame->bodies[i]->ori.qy,
               nn->last_frame->bodies[i]->ori.qz,
               nn->last_frame->bodies[i]->ori.qw);
      }
      printf("Latency: %f\n", nn->last_frame->latency);
    }
  }

  return 0;
}