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
	int sock, port, result, fd, block, length, errcode;
	struct sockaddr_in saddr_rx, saddr_tx;
	char buffer[TFTP_BUFFERSIZE] = {0};
	char data[TFTP_BLOCKSIZE] = {0};
	char filename[128] = {0};
	char errmsg[32] = {0};
	unsigned int opcode;
	struct stat sb;
	off_t st_size;

	if (argc != 2) {
		printf("Usage: %s PORT.\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[1]);

	/* set network address, port */
	saddr_rx.sin_family = AF_INET;
	saddr_rx.sin_port = htons(port);
	saddr_rx.sin_addr.s_addr = htonl(INADDR_ANY);

	/* socket UDP */
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	/* bind socket to port */
	if (bind(sock, (struct sockaddr *)&saddr_rx, sizeof(saddr_rx)) == -1) {
		printf("bind error (%d: %s).\n", errno, strerror(errno));
		return EXIT_FAILURE;
	}

	printf("Waiting client...\n");

	fd = 0;

	while (1) {
		result = tftp_recv(sock, &saddr_tx, buffer, sizeof(buffer));
		if (result == -1) {
			printf("tftp_recv error");
			result = EXIT_FAILURE;
		}

		opcode = (buffer[0] << 8) + buffer[1];

		switch(opcode) {
			case OPCODE_WRQ:
				/* TODO */
				tftp_error(sock, &saddr_tx, EBADRQC, strerror(EBADRQC));
				break;

			case OPCODE_RRQ:
				if (fd > 0) {
#ifdef DEBUG
					printf("file %s already opened.\n", filename);
#endif
					tftp_error(sock, &saddr_tx, ETXTBSY, strerror(ETXTBSY));
				}

				memset(filename, 0, sizeof(filename));
				strncpy(filename, &buffer[2], sizeof(filename) - 1);
#ifdef DEBUG
				printf("client %s request to read %s.\n", inet_ntoa(saddr_tx.sin_addr), filename);
#endif
				if (stat(filename, &sb) == -1) {
					tftp_error(sock, &saddr_tx, errno, strerror(errno));
					break;
				}

#ifdef DEBUG
				printf("file: %s, size: %ld\n", filename, sb.st_size);
#endif
				fd = open(filename, O_RDONLY);
				if (fd == -1) {
					tftp_error(sock, &saddr_tx, errno, strerror(errno));
					break;
				}

				length = read(fd, data, sizeof(data));
				if (length == -1) {
					tftp_error(sock, &saddr_tx, errno, strerror(errno));
					close(fd);
					break;
				}

				block = 1;

				result = tftp_data(sock, &saddr_tx, block, data, length);
				if (result == -1) {
					printf("tftp_data error.\n");
					close(fd);
					break;
				}

				st_size = length;
				break;

			case OPCODE_DATA:
				/* TODO */
				tftp_error(sock, &saddr_tx, EBADRQC, strerror(EBADRQC));
				break;

			case OPCODE_ACK:
				if (fd <= 0) {
#ifdef DEBUG
					printf("unexpected ack (no file opened).\n");
#endif
					break;
				}

				if (length < TFTP_BLOCKSIZE) {
#ifdef DEBUG
					printf("file %s sent", filename);
#endif
					close(fd);
					break;
				}

				if (st_size < sb.st_size) {
					length = read(fd, data, sizeof(data));
					if (length == -1) {
						tftp_error(sock, &saddr_tx, errno, strerror(errno));
						close(fd);
						break;
					}
				} else {
					/* send final DATA packet with a null length */
					length = 0;
				}

				block = (block + 1) % (TFTP_BLOCKMAX + 1);

				result = tftp_data(sock, &saddr_tx, block, data, length);
				if (result == -1) {
					printf("tftp_data error.\n");
					close(fd);
					break;
				}

				st_size += length;
				break;

			case OPCODE_ERROR:
				errcode = (buffer[2] << 8) + buffer[3];
				memset(errmsg, 0, sizeof(errmsg));
				strncpy(errmsg, &buffer[4], sizeof(errmsg) - 1);
				printf("tftp error %d (%s).\n", errcode, errmsg);

				if (fd > 0)
					close(fd);
				break;

			default:
				printf("unsupported opcode (%02x).\n", opcode);
				break;
		}
	}

	if (fd > 0)
		close(fd);

#ifdef DEBUG
	printf("exit.\n");
#endif

	return EXIT_SUCCESS;
}
