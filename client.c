#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h> /* open() */
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "tftp.h"
#include "utils.h"

int main(int argc, char **argv)
{
	int sock, port, result, fd, block, nblock, blocksize, length, errcode;
	struct sockaddr_in saddr_rx, saddr_tx;
	struct timeval tstart, tstop;
	char buffer[TFTP_BUFFERSIZE];
	char errmsg[32] = {0};
	char *filename, *ip;
	unsigned int opcode;

	if (argc != 4) {
		printf("Usage: %s HOST PORT FILE.\n", argv[0]);
		exit(1);
	}

	ip = argv[1];
	port = atoi(argv[2]);
	filename = argv[3];

	/* Set network address, port */
	saddr_rx.sin_family = AF_INET;
	saddr_rx.sin_port = htons(port);
	saddr_rx.sin_addr.s_addr = htonl(INADDR_ANY);

	saddr_tx.sin_family = AF_INET;
	saddr_tx.sin_port = htons(port);
	inet_pton(AF_INET, ip, &saddr_tx.sin_addr);

	/* Socket UDP */
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	/* bind socket to port */
	if (bind(sock, (struct sockaddr *)&saddr_rx, sizeof(saddr_rx)) == -1) {
		printf("bind error (%d: %s).\n", errno, strerror(errno));
		return EXIT_FAILURE;
	}

	gettimeofday(&tstart, NULL);

#ifdef DEBUG
	printf("request %s from tftp server (%s:%d).\n", filename, ip, port);
#endif

	result = tftp_rrq(sock, &saddr_tx, filename, buffer, sizeof(buffer));
	if (result == -1) {
		printf("tftp_rrq error");
		return EXIT_FAILURE;
	}

	block = (buffer[2] << 8) + buffer[3];
	blocksize = result - 4;

	if (block != 1) {
		tftp_error(sock, &saddr_tx, 1, "invalid block number");
		return EXIT_FAILURE;
	}

	tftp_ack(sock, &saddr_tx, block);
	nblock = (block + 1) % (TFTP_BLOCKMAX + 1);

	/* Create file */
	fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 644);
	if (fd == -1) {
		printf("failed to create %s.\n", filename);
		return EXIT_FAILURE;
	}

	/* store data */
	if (write(fd, &buffer[4], blocksize) == -1) {
		printf("failed to write in %s.\n", filename);
		result = EXIT_FAILURE;
		goto close_fd;
	}

	length = blocksize;

	while (blocksize == TFTP_BLOCKSIZE) {
		result = tftp_recv(sock, &saddr_tx, buffer, sizeof(buffer));
		if (result == -1) {
			printf("tftp_recv error");
			result = EXIT_FAILURE;
			goto close_fd;
		}

		opcode = (buffer[0] << 8) + buffer[1];

		switch(opcode) {
			case OPCODE_ERROR:
				errcode = (buffer[2] << 8) + buffer[3];
				strncpy(errmsg, &buffer[4], sizeof(errmsg) - 1);
				printf("tftp error %d (%s).\n", errcode, errmsg);
				result = EXIT_FAILURE;
				goto close_fd;

			case OPCODE_DATA:
				/* nothing to do */
				break;

			default:
				printf("unexpected opcode (%02x).\n", opcode);
				result = EXIT_FAILURE;
				goto close_fd;
		}

		block = (buffer[2] << 8) + buffer[3];
		blocksize = result - 4;

		if (block != nblock) {
			tftp_error(sock, &saddr_tx, 1, "invalid block number");
			return EXIT_FAILURE;
		}

		/* Store data */
		if (write(fd, &buffer[4], blocksize) == -1) {
			printf("failed to write in %s.\n", filename);
			result = EXIT_FAILURE;
			goto close_fd;
		}

		tftp_ack(sock, &saddr_tx, block);
		nblock = (block + 1) % (TFTP_BLOCKMAX + 1);

		length += blocksize;
	}

	gettimeofday(&tstop, NULL);
	printstats(&tstart, &tstop, length);

	result = EXIT_SUCCESS;

close_fd:
	close(fd);

#ifdef DEBUG
	printf("exit.\n");
#endif

	return result;
}
