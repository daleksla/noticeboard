#ifndef REQUEST_H
#define REQUEST_H
#pragma once

#include <stdint.h>
#include <limits.h>

#include "constraints.h"

/**
 * @brief Declaration of functionality to send requests from client to server
 */

enum request_command {
	ADD = 0,
	GET = 1,
	REMOVE = 2
};

/**
 * @brief Request (struct) - struct to store details to send to server
 */
struct Request {
	uint8_t cmd; /* (uint8_t)request_command::* */

	uint32_t sbj_len; /* 1 to MAX_SBJ_LEN; for sbj_content */

	uint8_t sbj_content[MAX_SBJ_LEN]; /* for subject (i.e. filename / title). set as stack buffer as..., well its mandatory and small */

	uint32_t extra_data_len; /* sizeof(*extra_data_content); 0 to UINT32_MAX bytes */

	/* optional - check extra_data_len is > 0 before reading this in */

	void *extra_data_content; /* for now it's just a char*
				  * it would be nice if you could just exchange the fildes but not enough time to implement this feature
				  * limited to MAX_EXTRA_DATA_LEN for both
				  * unlike sbj_content, this requires a pointer
				  */
};

/**
 * @brief request_send - encodes request packet and sends to server
 * This function DOES NOT CARE about invalid requests, but that data can actually be sent - seperation of concerns
 * @param const struct Request *const client_request - populated request struct to be sent
 * @param const int server_sock - endpoint to send packet contents to
 * @return int - non-zero exit code is success, else failure
 * 1 is error encoding, 2 is error sending
 */
int request_send(const struct Request *const client_request, const int server_sock);

/**
 * @brief request_send - decodes request packet from client
 * @param const struct Request *const client_request - empty request struct to be filled
 * @param const int client_sock - endpoint to get packet contents
 * @return int - non-zero exit code is success, else failure
 * 1 is error receiving, 2 is error decoding
 */
int request_recv(struct Request *const client_request, const int client_sock);

#endif /* REQUEST_H */
