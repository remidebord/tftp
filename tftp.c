#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TFTP_BLOCKSIZE 512 /* see https://www.ietf.org/rfc/rfc1350.txt */
#define TFTP_BUFFERSIZE (2 * TFTP_BLOCKSIZE)

typedef enum {
	OPCODE_RRQ = 1,
	OPCODE_WRQ,
	OPCODE_DATA,
	OPCODE_ACK,
	OPCODE_ERROR
} TFPT_OPCODES;

int main(int argc, char **argv) {

	int s, port, length, result, fd;
	int opcode, block, blocksize, error;
	char buffer[TFTP_BUFFERSIZE];
	char *filename, *ip, *mode;
	struct sockaddr_in saddr;
	socklen_t addrlen;

	if (argc != 4) {
		printf("Usage: %s <ip> <port> <file>.\n", argv[0]);
		exit(1);
	}
	
	ip = argv[1];
	port = atoi(argv[2]);
	filename = argv[3];

	/* Set network address, port */
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &saddr.sin_addr);

	/* Socket UDP */
	s = socket(AF_INET, SOCK_DGRAM, 0);

	printf("request %s from tftp server (%s:%d).\n", filename, ip, port);

	/* Send Read request (RRQ) */
	opcode = OPCODE_RRQ;
	mode = "octet";
	length = 0;

	buffer[length++] = opcode >> 8;
	buffer[length++] = opcode & 0xff;

	length += sprintf(&buffer[length], "%s", filename);

	buffer[length++] = 0x00;

	length += sprintf(&buffer[length], "%s", mode);

	result = sendto(s, buffer, length, MSG_CONFIRM, (const struct sockaddr *)&saddr, sizeof(saddr));
	
	if (result == -1) {
		printf("socket: sendto error (RRQ).\n");
		return result;
	}

	/* Check reply */
	addrlen = sizeof(saddr);
	result = recvfrom(s, buffer, TFTP_BUFFERSIZE, MSG_WAITALL, (struct sockaddr *)&saddr, &addrlen);

	if (result == -1) {
		printf("socket: recvfrom error.\n");
		return result;
	}

	opcode = (buffer[0] << 8) + buffer[1];

#ifdef DEBUG
	printf("length: %d, data: ");
	for (int i = 0; i < 32; i++)
		printf("0x%02x ");
	printf("\n");
#endif

	if (opcode == OPCODE_ERROR) {
		error = (buffer[2] << 8) + buffer[3];
		printf("tftp: error %d (%s).\n", error, &buffer[4]);
		return -1;
	} else if (opcode == OPCODE_DATA) {
		block = (buffer[2] << 8) + buffer[3];
		blocksize = result - 4;

		if (block != 1)
			printf("tftp: first block is not 1 (%d).\n", block);

		/* Create file */
		fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 644);

		if (fd == -1) {
			printf("failed to create %s.\n", filename);
			return -1;
		}

		/* Store data */
		if (write(fd, &buffer[4], blocksize) == -1) {
			printf("failed to write in %s.\n", filename);
			goto close_fd;
		}

#ifndef DEBUG
		printf("*");
#endif
		/* Send ACK */
		opcode = OPCODE_ACK;
		length = 0;

		buffer[length++] = opcode >> 8;
		buffer[length++] = opcode & 0xff;

		buffer[length++] = block >> 8;
		buffer[length++] = block & 0xff;

		result = sendto(s, buffer, length, MSG_CONFIRM, (const struct sockaddr *)&saddr, sizeof(saddr));

		if (result == -1) {
			printf("socket: sendto error (ACK).\n");
			goto close_fd;
		}

		while (blocksize == TFTP_BLOCKSIZE) {
			result = recvfrom(s, buffer, TFTP_BUFFERSIZE, MSG_WAITALL, (struct sockaddr *)&saddr, &addrlen);

			if (result == -1) {
				printf("socket: recvfrom error.\n");
				goto close_fd;
			}

			opcode = (buffer[0] << 8) + buffer[1];

#ifdef DEBUG
			printf("length: %d, data: ");
			for (int i = 0; i < 32; i++)
				printf("0x%02x ");
			printf("\n");
#endif

			if (opcode == OPCODE_ERROR) {
				error = (buffer[2] << 8) + buffer[3];
				printf("tftp: error %d (%s).\n", error, &buffer[4]);
				goto close_fd;
			} else if (opcode == OPCODE_DATA) {
				block = (buffer[2] << 8) + buffer[3];
				blocksize = result - 4;

				/* Store data */
				if (write(fd, &buffer[4], blocksize) == -1) {
					printf("failed to write in %s.\n", filename);
					goto close_fd;
				}
#ifndef DEBUG
				printf("*");
#endif
				/* Send ACK */
				opcode = OPCODE_ACK;
				length = 0;

				buffer[length++] = opcode >> 8;
				buffer[length++] = opcode & 0xff;

				buffer[length++] = block >> 8;
				buffer[length++] = block & 0xff;

				result = sendto(s, buffer, length, MSG_CONFIRM, (const struct sockaddr *)&saddr, sizeof(saddr));
			} else {
				printf("tftp: unknown opcode (%d).\n", opcode);
				goto close_fd;
			}
		}

close_fd:
		close(fd);

	} else {
		printf("tftp: unknown opcode (%d).\n", opcode);
	}

	printf("exit.\n");
	return 0;
}

