#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "tftp.h"

int tftp_send(int sock, struct sockaddr_in *saddr, char *buffer, int length)
{
#ifdef DEBUG
	printf("(%d) ", length);
	for (size_t i = 0; i < length; i++) {
		if ((buffer[i] < 0x20) || (buffer[i] > 0x7E))
			printf("\\%02x", buffer[i]);
		else
			printf("%c", buffer[i]);
	}
	printf("\n");
#endif

	return sendto(sock, buffer, length, MSG_CONFIRM, (const struct sockaddr *)saddr, sizeof(*saddr));
}

int tftp_recv(int sock, struct sockaddr_in *saddr, char *buffer, int size)
{
	volatile socklen_t addrlen = sizeof(*saddr);
	int result;

	result = recvfrom(sock, buffer, size, MSG_WAITALL, (struct sockaddr *)saddr, (socklen_t *)&addrlen);

#ifdef DEBUG
	if (result != -1) {
		printf("(%d) ", result);
		for (size_t i = 0; i < result; i++) {
			if ((buffer[i] < 0x20) || (buffer[i] > 0x7E))
				printf("\\%02x", buffer[i]);
			else
				printf("%c", buffer[i]);
		}
		printf("\n");
	}
#endif

	return result;
}

int tftp_ack(int sock, struct sockaddr_in *saddr, int block)
{
	char buffer[TFTP_BUFFERSIZE] = {0};
	unsigned int opcode;
	int length;

	/*       ACK packet
	 *
	 *  2 bytes     2 bytes
	 *  ---------------------
	 * | Opcode |   Block #  |
	 *  ---------------------
	 */

	opcode = OPCODE_ACK;
	length = 0;

	buffer[length++] = opcode >> 8;
	buffer[length++] = opcode & 0xff;

	buffer[length++] = block >> 8;
	buffer[length++] = block & 0xff;

	return tftp_send(sock, saddr, buffer, length);
}

int tftp_error(int sock, struct sockaddr_in *saddr, int errcode, char *errmsg)
{
	char buffer[TFTP_BUFFERSIZE] = {0};
	unsigned int opcode;
	int length;

	/*              ERROR packet
	 *
	 *   2 bytes     2 bytes      string    1 byte
	 *  -----------------------------------------
	 * | Opcode |  ErrorCode |   ErrMsg   |   0  |
	 *  -----------------------------------------
	 */

	opcode = OPCODE_ERROR;
	length = 0;

	buffer[length++] = opcode >> 8;
	buffer[length++] = opcode & 0xff;

	buffer[length++] = errcode >> 8;
	buffer[length++] = errcode & 0xff;

	length += snprintf(&buffer[length], sizeof(buffer) - length, "%s", errmsg);

	buffer[length++] = 0x00;

	return tftp_send(sock, saddr, buffer, length);
}

int tftp_rrq(int sock, struct sockaddr_in *saddr, char *filename, char* buffer, int size)
{
	int length, result, errcode;
	char errmsg[32] = {0};
	unsigned int opcode;
	char *mode;

	/*                Read request (WRQ)
	 *
	 *  2 bytes     string    1 byte     string   1 byte
	 *  ------------------------------------------------
	 * | Opcode |  Filename  |   0  |    Mode    |   0  |
	 *  ------------------------------------------------
	 */

	opcode = OPCODE_RRQ;
	mode = "octet";
	length = 0;

	buffer[length++] = opcode >> 8;
	buffer[length++] = opcode & 0xff;

	length += snprintf(&buffer[length], size - length, "%s", filename);

	buffer[length++] = 0x00;

	length += snprintf(&buffer[length], size - length, "%s", mode);

	buffer[length++] = 0x00;

	result = tftp_send(sock, saddr, buffer, length);
	if (result == -1) {
		printf("sendto error (%d: %s).\n", errno, strerror(errno));
		return result;
	}

	result = tftp_recv(sock, saddr, buffer, size);
	if (result == -1) {
		printf("recvfrom error (%d: %s).\n", errno, strerror(errno));
		return result;
	}

	opcode = (buffer[0] << 8) + buffer[1];

	switch(opcode) {
		case OPCODE_ERROR:
			errcode = (buffer[2] << 8) + buffer[3];
			snprintf(errmsg, sizeof(errmsg), "%s", &buffer[4]);
			printf("tftp error %d (%s).\n", errcode, errmsg);
			return -1;

		case OPCODE_DATA:
			/* nothing to do */
			break;

		default:
			printf("unexpected opcode (%02x).\n", opcode);
			return -1;
	}

	return result;
}

int tftp_wrq(int sock, struct sockaddr_in *saddr, char *filename)
{
	char buffer[TFTP_BUFFERSIZE] = {0};
	int length, errcode, result;
	char errmsg[32] = {0};
	unsigned int opcode;
	char *mode;

	/*                Write request (WRQ)
	 *
	 *  2 bytes     string    1 byte     string   1 byte
	 *  ------------------------------------------------
	 * | Opcode |  Filename  |   0  |    Mode    |   0  |
	 *  ------------------------------------------------
	 */

	opcode = OPCODE_WRQ;
	mode = "octet";
	length = 0;

	buffer[length++] = opcode >> 8;
	buffer[length++] = opcode & 0xff;

	length += snprintf(&buffer[length], sizeof(buffer) - length, "%s", filename);

	buffer[length++] = 0x00;

	length += snprintf(&buffer[length], sizeof(buffer) - length, "%s", mode);

	buffer[length++] = 0x00;

	result = tftp_send(sock, saddr, buffer, length);
	if (result == -1) {
		printf("sendto error (%d: %s).\n", errno, strerror(errno));
		return result;
	}

	result = tftp_recv(sock, saddr, buffer, sizeof(buffer));
	if (result == -1) {
		printf("recvfrom error (%d: %s).\n", errno, strerror(errno));
		return result;
	}

	opcode = (buffer[0] << 8) + buffer[1];

	switch(opcode) {
		case OPCODE_ERROR:
			errcode = (buffer[2] << 8) + buffer[3];
			strncpy(errmsg, &buffer[4], sizeof(errmsg) - 1);
			printf("tftp error %d (%s).\n", errcode, errmsg);
			return -1;

		case OPCODE_ACK:
			/* nothing to do */
			break;

		default:
			printf("unexpected opcode (%02x).\n", opcode);
			return -1;
	}

	return 0;
}

int tftp_data(int sock, struct sockaddr_in *saddr, int block, char *data, int datalen)
{
	char buffer[TFTP_BUFFERSIZE] = {0};
	unsigned int opcode;
	int length, result;

	if (block > TFTP_BLOCKMAX) {
		printf("invalid block number (%d).\n", block);
		return -1;
	}

	if (datalen > TFTP_BLOCKSIZE) {
		printf("too much data to send (%d).\n", datalen);
		return -1;
	}

	/*           DATA packet
	 *
	 *  2 bytes     2 bytes      n bytes
	 *  ----------------------------------
	 * | Opcode |   Block #  |   Data     |
	 *  ----------------------------------
	 */

	opcode = OPCODE_DATA;
	length = 0;

	buffer[length++] = opcode >> 8;
	buffer[length++] = opcode & 0xff;

	buffer[length++] = block >> 8;
	buffer[length++] = block & 0xff;

	memcpy(&buffer[length], data, datalen);
	length += datalen;

	result = tftp_send(sock, saddr, buffer, length);
	if (result == -1) {
		printf("sendto error (%d: %s).\n", errno, strerror(errno));
		return result;
	}

	return 0;
}
