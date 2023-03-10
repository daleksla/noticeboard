#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>

#include "response.h"

/**
 * @brief Definition of functionality to manages responses from server to client
 */

int response_send(const struct Response *const server_response, const int client_sock)
{
	if (server_response == NULL) {
		fprintf(stderr, "Response struct to fill cannot be NULL\n");
		return 1;
	}

	if (server_response->status != OK && server_response->status != DATA && server_response->status != FAIL) {
		fprintf(stderr, "Invalid response type\n");
		return 1;
	}

	if (server_response->extra_data_len != 0 && server_response->extra_data_content == NULL) {
		fprintf(stderr, "Extra data to be sent requested (%lu) but memory location invalid (%p)\n", (const uint64_t)server_response->extra_data_len, server_response->extra_data_content);
		return 1;
	}

	if (server_response->extra_data_len > MAX_EXTRA_DATA_LEN) {
		fprintf(stderr, "Data requested to be sent is larger than maximum message length (maximum %d, given %u)\n", MAX_EXTRA_DATA_LEN, server_response->extra_data_len);
		return 1;
	}

	if (
		send(client_sock, &server_response->status, sizeof(server_response->status), 0) != sizeof(server_response->status)
		||
		send(client_sock, &server_response->extra_data_len, sizeof(server_response->extra_data_len), 0) != sizeof(server_response->extra_data_len)
	) { /* || is short circuit operator so once one fails it'll die (https://en.wikipedia.org/wiki/Short-circuit_evaluation) */
		fprintf(stderr, "Error sending response (errno %d: %s)\n", errno, strerror(errno));
		return 2;
	}

	if (server_response->extra_data_len > 0) { /* we've already verified whether extra_data_content is valid or not so we just check if we need to send anything at all */
		if (send(client_sock, server_response->extra_data_content, server_response->extra_data_len, 0) != (ssize_t)server_response->extra_data_len) {
			fprintf(stderr, "Error sending response (errno %d: %s)\n", errno, strerror(errno));
			return 2;
		}
	}

	return 0;
}

int response_recv(struct Response *const server_response, const int client_sock)
{
	if (server_response == NULL) {
		fprintf(stderr, "Response struct to fill cannot be NULL\n");
		return 2;
	}

	/* reading command */
	if (read(client_sock, &server_response->status, sizeof(server_response->status)) != sizeof(server_response->status)) {
		fprintf(stderr, "Error reading command component (errno %d: %s)\n", errno, strerror(errno));
		return 1;
	}

	if (server_response->status != OK && server_response->status != DATA && server_response->status != FAIL) {
		fprintf(stderr, "Unprocessable response: command unrecognised\n");
		return 2;
	}

	/* reading extra_data_len */
	if (read(client_sock, &server_response->extra_data_len, sizeof(server_response->extra_data_len)) != sizeof(server_response->extra_data_len)) {
		fprintf(stderr, "Error reading extra data length component (errno %d: %s)\n", errno, strerror(errno));
		return 1;
	}

	if (server_response->extra_data_len > 0) { /* reading sbj_content conditionally */
		if (!server_response->extra_data_content) {
			fprintf(stderr, "Extra data available but buffer is non-existant\n");
			return 0; /* here's the thing - the client should know whether they expect or want extra data or not
				   * therefore not providing a buffer doesn't cause an issue
				   */
		}

		if (read(client_sock, server_response->extra_data_content, server_response->extra_data_len) != (ssize_t)server_response->extra_data_len) {
			fprintf(stderr, "Error reading extra data content component (errno %d: %s)\n", errno, strerror(errno));
			return 1;
		}
	}

	return 0;
}
