#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#include <errno.h>

#include "response.h"
#include "request.h"

/**
 * @brief Definition of functionality to manage requests from client to server
 */

int request_send(const struct Request *const client_request, const int server_sock)
{
	if (client_request == NULL) {
		fprintf(stderr, "Request struct to fill cannot be NULL\n");
		return 1;
	}

	if (client_request->sbj_content == NULL) {
		fprintf(stderr, "Mandatory subject content field points cannot be invalid - was given %p\n", client_request->sbj_content);
		return 1;
	}

	if (client_request->extra_data_len != 0 && client_request->extra_data_content == NULL) {
		fprintf(stderr, "Extra data to be sent requested (%lu) but memory location invalid (%p)\n", (const uint64_t)client_request->extra_data_len, client_request->extra_data_content);
		return 1;
	}

	if (
		send(server_sock, &client_request->cmd, sizeof(client_request->cmd), 0) != sizeof(client_request->cmd)
		||
		send(server_sock, &client_request->sbj_len, sizeof(client_request->sbj_len), 0) != sizeof(client_request->sbj_len)
		||
		send(server_sock, client_request->sbj_content, client_request->sbj_len, 0) != (ssize_t)client_request->sbj_len
		||
		send(server_sock, &client_request->extra_data_len, sizeof(client_request->extra_data_len), 0) != sizeof(client_request->extra_data_len)
	) { /* || is short circuit operator so once one fails it'll die (https://en.wikipedia.org/wiki/Short-circuit_evaluation) */
		fprintf(stderr, "Error sending request (errno %d: %s)\n", errno, strerror(errno));
		return 2;
	}

	if (client_request->extra_data_len > 0) { /* we've already verified whether extra_data_content is valid or not so we just check if we need to send anything at all */
		if (send(server_sock, client_request->extra_data_content, client_request->extra_data_len, 0) != (ssize_t)client_request->extra_data_len) {
			fprintf(stderr, "Error sending request (errno %d: %s)\n", errno, strerror(errno));
			return 2;
		}
	}

	return 0;
}

int request_recv(struct Request *const client_request, const int client_sock)
{
	if (client_request == NULL) {
		fprintf(stderr, "Request struct to fill cannot be NULL\n");
		return 2;
	}

	/* reading command */
	if (read(client_sock, &client_request->cmd, sizeof(client_request->cmd)) != sizeof(client_request->cmd)) {
		fprintf(stderr, "Error reading command component (errno %d: %s)\n", errno, strerror(errno));
		return 1;
	}

	if (client_request->cmd != ADD && client_request->cmd != GET && client_request->cmd != REMOVE) {
		fprintf(stderr, "Invalid request: command unrecognised\n");
		return 2;
	}

	/* reading sbj_len */
	ssize_t those_read = read(client_sock, &client_request->sbj_len, sizeof(client_request->sbj_len));
	if (those_read != sizeof(client_request->sbj_len)) {
		fprintf(stderr, "Error reading subject length component (errno %d: %s) (given %ld, wanted %ld)\n", errno, strerror(errno), those_read, sizeof(client_request->sbj_len));
		return 1;
	}

	if (client_request->sbj_len < 1 || client_request->sbj_len > MAX_SBJ_LEN) {
		fprintf(stderr, "Invalid subject length: bad length (%u)\n", client_request->sbj_len);
		return 2;
	}

	/* reading sbj_content */
	if (client_request->sbj_content == NULL) {
		fprintf(stderr, "Subject content field cannot be NULL\n");
		return 2;
	}

	if (read(client_sock, client_request->sbj_content, client_request->sbj_len) != (ssize_t)client_request->sbj_len) {
		fprintf(stderr, "Error reading subject content component (errno %d: %s)\n", errno, strerror(errno));
		return 1;
	}

	for (size_t i = 0; i < client_request->sbj_len; ++i) { /* sanitise input - subject forms filename plus generally has expectation to be reasonable */
		const char current_chr = client_request->sbj_content[i];
		switch (current_chr) {
			case ';':
			case '/':
			case '.':
			case '\\':
				fprintf(stderr, "Subject content field contains '%c', which is an invalid character\n", current_chr);
				return 2;
		}
	}

	size_t last_initial_whitespace; /* records where the last whitespace is */
	for (last_initial_whitespace = 0; last_initial_whitespace < client_request->sbj_len; ++last_initial_whitespace) {
		const char current_chr = client_request->sbj_content[last_initial_whitespace];
		if (current_chr != '\t' && current_chr != '\n' && current_chr != ' ' && current_chr != '\r') {
			break;
		}
	}

	if (last_initial_whitespace >= MAX_SBJ_LEN) { /* we need at least one valid character to form a sbj / filename */
		fprintf(stderr, "Subject must consist of one valid character excluding preceeding whitespace\n");
		return 2;
	} else if (last_initial_whitespace > 0) {
		memcpy(client_request->sbj_content, client_request->sbj_content + last_initial_whitespace, MAX_SBJ_LEN - last_initial_whitespace);
	}

	uint8_t *const end_of_sbj = memchr(client_request->sbj_content, '\0', MAX_SBJ_LEN); /* we need to make sure that, if the null terminator was passed in with client_request.sbj_content, that we account for its length to the byte prior */
	if (end_of_sbj != NULL) {
		client_request->sbj_len = end_of_sbj - client_request->sbj_content;
	}

	/* reading extra_data_len */
	if (read(client_sock, &client_request->extra_data_len, sizeof(client_request->extra_data_len)) != sizeof(client_request->extra_data_len)) {
		fprintf(stderr, "Error reading extra data length component (errno %d: %s)\n", errno, strerror(errno));
		return 1;
	}

	if (client_request->extra_data_len > 0) { /* reading sbj_content conditionally */
		if (!client_request->extra_data_content) {
			fprintf(stderr, "Extra data content field cannot be NULL\n");
			return 2;
		}

		if (read(client_sock, client_request->extra_data_content, client_request->extra_data_len) != (ssize_t)client_request->extra_data_len) {
			fprintf(stderr, "Error reading extra data content component (errno %d: %s)\n", errno, strerror(errno));
			return 1;
		}
	}

	return 0;
}
