#ifndef RESPONSE_H
#define RESPONSE_H
#pragma once

#include <stdint.h>

#include "constraints.h"

/**
 * @brief Declaration of functionality to send responses from server to client
 */

enum response_status {
	OK = 0,
	DATA = 1,
	FAIL = 2
};

struct Response {
	uint8_t status; /* (uint8_t)response_status::* */

	uint32_t extra_data_len; /* sizeof(*extra_data_content); 0 to UINT32_MAX bytes */

	/* optional - check extra_data_len is > 0 before reading this in */

	void *extra_data_content; /* either char* or struct msghdr* */
};

/**
 * @brief response_send - encodes response packet and sends to client
 * @param const struct Response *const server_response - populated response struct to be sent
 * @param const int client_sock - endpoint to send packet contents to
 * @return int - non-zero exit code is success, else failure
 * 1 is error encoding, 2 is error sending
 */
int response_send(const struct Response *const server_response, const int client_sock);

/**
 * @brief response_recv - decodes response packet from server
 * @param const struct Response *const server_response - empty response struct to be filled
 * @param const int client_sock - endpoint to get packet contents
 * @return int - non-zero exit code is success, else failure
 * 1 is error receiving, 2 is error decoding
 */
int response_recv(struct Response *const server_response, const int client_sock);

#endif /* RESPONSE_H */
