#ifndef CLIENT_HANDLING_H
#define CLIENT_HANDLING_H
#pragma once

#include "request.h"

/**
 * @brief Declarations of functionality to manage each server-client relationship
 */

/**
 * @brief client_connection - function to actually handle data coming to and from process behind socket handle
 * @param const int client_sock - IPC socket / file handle to communicate with
 * @return int - 0 == success, non-zero is failure
 * 1 = issue understanding request, 2 = issue handling request
 */
int client_connection(const int client_sock);

/**
 * @brief execute_request - executes request on server-side
 * @param const enum request_command cmd - request
 * @param const char *const sbj - null terminated / c-string sbj
 * @param const uint32_t extra_data_len - length of extra data. set to 0 if none
 * @param const char *const extra_data - pointer to extra data. set to NULL if none and ignored if extra_data_len is 0
 * @param const int client_sock - endpoint to send response to (either w/ or withoutextra data)
 * @return int - non-zero exit code is success, else failure
 * 1 is error servicing request
 */
int execute_request(const enum request_command cmd, const char *const sbj, const uint32_t extra_data_len, const char *const extra_data, const int client_sock);

#endif /* CLIENT_HANDLING_H */
