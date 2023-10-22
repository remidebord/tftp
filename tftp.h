#ifndef __TFTP_H__
#define __TFTP_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TFTP_BLOCKSIZE 512 /* see https://www.ietf.org/rfc/rfc1350.txt */
#define TFTP_BUFFERSIZE (2 * TFTP_BLOCKSIZE)

#define TFTP_BLOCKMAX 65535

typedef enum {
	OPCODE_RRQ = 1,
	OPCODE_WRQ,
	OPCODE_DATA,
	OPCODE_ACK,
	OPCODE_ERROR
} TFPT_OPCODES;

int tftp_send(int sock, struct sockaddr_in *saddr, char *buffer, int length);
int tftp_recv(int sock, struct sockaddr_in *saddr, char *buffer, int size);

int tftp_ack(int sock, struct sockaddr_in *saddr, int block);
int tftp_error(int sock, struct sockaddr_in *saddr, int errcode, char *errmsg);
int tftp_rrq(int sock, struct sockaddr_in *saddr, char *filename, char* buffer, int size);
int tftp_wrq(int sock, struct sockaddr_in *saddr, char *filename);
int tftp_data(int sock, struct sockaddr_in *saddr, int block, char *data, int datalen);

#endif /* __TFTP_H__ */