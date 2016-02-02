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
#include <stdarg.h>


#include "NatNetC/NatNetTypes.h"


#define MULTICAST_ADDRESS "239.255.42.99" // IANA, local network
#define PORT_COMMAND 1510
#define PORT_DATA 1511

#define SOCKET_ERROR -1
#define INVALID_SOCKET -1

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


#pragma mark -
#pragma mark Function Declarations

/*!
 Create a new NatNet object, which is an instance of NatNet struct.
 \author Paolo
 \return a \p NatNet instance
 \param my_addr This host address (e.g. "128.0.0.1")
 \param their_addr host address of host running Motive
 \param multicast_addr multicast address, default to \p MULTICAST_ADDRESS
 \param command_port local port listening for commands
 \param data_port local port listening for data
 */
NatNet *NatNet_new(char *my_addr, char *their_addr, char *multicast_addr,
                   u_short command_port, u_short data_port);

/*!
 Free NatNet allocated memory.
 \author Paolo
 \return void
 \param nn the NatNet instance to be freed
 */
void NatNet_free(NatNet *nn);

/*!
 Initialize a newly allocated  NatNet object.
 
 \b Warning: the function initializes the buffer \p NatNet.raw_data to a size of 
             NatNet.receive_bufsize. If you want to change this size, you have
            to manually call realloc() or to free() and calloc() it again.
 \b Warning: the function does not allocate the NatNet.yaml field, which is
             expected to be automatically allocated by NatNet_unpack_yaml().
 \author Paolo
 \return 0 on success
 \param nn theNatNet struct to be initialized
 \param my_addr This host address (e.g. "128.0.0.1")
 \param their_addr host address of host running Motive
 \param multicast_addr multicast address, default to \p MULTICAST_ADDRESS
 \param command_port local port listening for commands
 \param data_port local port listening for data
 */
int NatNet_init(NatNet *nn, char *my_addr, char *their_addr,
                char *multicast_addr, u_short command_port, u_short data_port);


/*!
 Bind both command and data socket.
 \brief Bind both sockets
 \author Paolo
 \return 0 on success
 \params nn a NatNet struct
 */
int NatNet_bind(NatNet *nn);

/*!
 Bind Command socket only.
 \brief Bind command socket
 \return 0 on success
 \params nn a NatNet struct
 */
int NatNet_bind_command(NatNet *nn);

/*!
 Bind Data socket only
 \brief Bind data socket
 \author Paolo
 \return 0 on success
 \params nn a NatNet struct
 */
int NatNet_bind_data(NatNet *nn);

/*!
 Send a packet to the Motive server, trying to do it for a given amount of times.
 \brief Send a packet to Motive
 \author Paolo
 \return 0 on success, -1 on error
 \params nn NatNet struct
 \params packet a NatNet packet
 \params tries the number of times to repeat the sending
 */
int NatNet_send_pkt(NatNet *nn, NatNet_packet packet, uint tries);

/*!
 Close both command and data sockets.
 \brief Close sockets
 \author Paolo
 \return 0 on success, <0 on error (check \p errno!)
 \params nn NatNet struct
 */
int NatNet_close(NatNet *nn);

/*!
 Send a command to the Motive server.
 \brief Send a command
 \author Paolo
 \return 0 on success, -1 on timeout, >0 is an error
 \params nn NatNet struct
 \params cmd   the command to be sent
 \params tries the number of times to repeat the sending
 */
int NatNet_send_cmd(NatNet *nn, char *cmd, uint tries);

/*!
 Receive a command from Motive
 \brief Receive a command
 \author Paolo
 \return Number of bytes received
 \params nn NatNet struct
 \params cmd the (externally allocated) buffer where received data will be stored
 \params len the size of the received data (in bytes)
 */
long NatNet_recv_cmd(NatNet *nn, char *cmd, size_t len);

/*!
 Receive data from Motive into a passed buffer
 \brief Receive data
 \author Paolo
 \return Number of bytes received
 \params nn NatNet struct
 \params cmd the (externally allocated) buffer where received data will be stored
 \params len the size of the received data (in bytes)
 */
long NatNet_recv_data_into(NatNet *nn, char *cmd, size_t len);

/*!
 Receive data from Motive into NatNet.raw_data
 \brief Receive data
 \author Paolo
 \return Number of bytes received (also in NatNet.raw_data_len)
 \params nn NatNet struct
 */
size_t NatNet_recv_data(NatNet *nn);

/*!
 Unpack a whole frame of data (in NatNet.raw_data) and put the result into 
 \p nn->last_frame
 \brief Unpack into \p NatNet.last_frame
 \author Paolo
 \params nn NatNet struct
 \params len on exit is the size of data
 */
void NatNet_unpack_all(NatNet *nn, size_t *len);

/*!
 Pack existing frame struct into \p NatNet.last_frame into a binary string in \p data.
 \brief Pack NatNet.last_frame into char array
 \author Paolo
 \params nn NatNet struct
 \params data char array that will be internally allocated
 \params len length of the char array containing packed data, written on exit
 */
void NatNet_pack_struct(NatNet *nn, char *data, size_t *len);

#ifdef NATNET_YAML
/*!
 Unpack a whole frame of data and put the result into \p NatNet.yaml
 \brief Unpack into YAML format
 \author Paolo
 \params nn NatNet struct
 \params pData pointer to data buffer (externally allocated)
 \params len on exit is the size of data
 */
int NatNet_unpack_yaml(NatNet *nn, size_t *len);
#endif

#pragma mark -
#pragma mark Custom printf templates
/*!
 Custom print function, no-op (prints nothing)
 \brief No-op function
 \author Paolo
 \return 0
 \params format a format string
 \params ... optional arguments
 */
int NatNet_printf_noop(const char * restrict format, ...);

/*!
 Custom print function, same behavior of \p printf()
 \brief Standard \p printf()
 \author Paolo
 \return Same as \p printf()
 \params format a format string
 \params ... optional arguments
 */
int NatNet_printf_std(const char * restrict format, ...);



#pragma mark -
#pragma mark Utilities
// Byte swapping functions for 2 (short) and 4 byte integers
// Remember: Use system ntohl/htonl family of functions for converting from/to
//           big-endian (network) to host byte order (little endian for i386,
//           big endian for ARM)
//           Use these functions for converting from little to big and viceversa
// Functions beginning with 'ip' work In-Place on the passed pointer.
// NatNet protocol uses little-endian order (i386).
// Corresponding macros are made available for
#ifdef __BIG_ENDIAN
#define IPLTOHS(i) ipswap2(i)
#define LTOHS(i) swap2(i)
#define IPLTOHL(i) ipswap4(i)
#define LTOHL(i) swap4(i)
#else
#define IPLTOHS(i)
#define LTOHS(i) i
#define IPLTOHL(i)
#define LTOHL(i) i
#endif
int swap4(int i);
void ipswap4(void *a);
short swap2(short i);
void ipswap2(void *a);




#pragma mark -
#pragma mark Fossile functions (to be removed later)
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

#endif /* NatNetC_h */