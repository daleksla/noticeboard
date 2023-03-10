#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "request.h"
#include "response.h"
#include "client_handling.h"

/**
 * @brief Definitions of functionality to manage each server-client relationship
 */

int client_connection(const int client_sock)
{
	int exit_code = 0;
	struct ucred peer_cred;
	int uid_len;
	struct Request client_request;
	client_request.extra_data_content = NULL;
	char *filename = NULL;

	/* initialise necessary details */
	socklen_t peer_cred_len = sizeof(peer_cred); /* as usual, getsockopt takes a mutable iot so this is as such */

	if (getsockopt(client_sock, SOL_SOCKET, SO_PEERCRED, &peer_cred, &peer_cred_len) != 0) { /* we want to access the uid of user behind IPC socket via UNIX API */
		fprintf(stderr, "Error manipulating client sock (errno %d: %s)\n", errno, strerror(errno));
		exit_code = 1;
		goto end;
	}

	uid_len = snprintf(NULL, 0, "%d", peer_cred.uid); /* we work out the text representation of the uid - needed to append to subject name */
	if (uid_len <= 0) {
		fprintf(stderr, "Error determining formatting for sock uid to-be-appended-to-subject\n");
		exit_code = 1;
		goto end;
	}

	client_request.extra_data_content = malloc(MAX_EXTRA_DATA_LEN);
	if (client_request.extra_data_content == NULL) {
		fprintf(stderr, "Error allocating necessary heap memory (errno %d: %s)\n", errno, strerror(errno));
		exit_code = 1;
		goto end;
	}
	memset(client_request.extra_data_content, '\0', MAX_EXTRA_DATA_LEN);
	memset(client_request.sbj_content, '\0', MAX_SBJ_LEN);

	/* get details */
	if (request_recv(&client_request, client_sock) != 0) {
		fprintf(stderr, "Error during request receival\n");
		exit_code = 1;
		goto end;
	}

	/* act upon details */
	filename = malloc(MAX_SBJ_LEN + uid_len + 1);
	if (filename == NULL) {
		fprintf(stderr, "Error allocating necessary heap memory (errno %d: %s)\n", errno, strerror(errno));
		exit_code = 1;
		goto end;
	}
	memset(filename, '\0', MAX_SBJ_LEN + uid_len + 1);
	memcpy(filename, client_request.sbj_content, MAX_SBJ_LEN);
	if (snprintf(filename + client_request.sbj_len, ((MAX_SBJ_LEN + uid_len) - client_request.sbj_len) - 1, "%d", peer_cred.uid) <= 0) { /* create the filename - subject + uid. write until last byte - memset would've set null terminator for last byte */
		fprintf(stderr, "Error creating subject + uid\n");
		exit_code = 1;
		goto end;
	}

	if (execute_request(client_request.cmd, filename, client_request.extra_data_len, client_request.extra_data_content, client_sock) != 0) {
		exit_code = 2;
		goto end;
	}

end: /* this is why I hate using C for even slightly bigger projects. resource control is a bloody nightmare
	  * tl;dr this function is called repeatedly and is main body
	  * cannot afford memory leaks so horrible but required cleanup
	  */
	if (client_request.extra_data_content != NULL) {
		free(client_request.extra_data_content);
		client_request.extra_data_content = NULL;
	}

	if (filename != NULL) {
		free(filename);
		filename = NULL;
	}

	struct Response resp;
	resp.status = (exit_code != 0 ? FAIL : OK);
	resp.extra_data_len = 0;
	resp.extra_data_content = NULL;

	if (response_send(&resp, client_sock) != 0) {
		fprintf(stderr, "Error during sending acknowledgement response\n");
		exit_code = (exit_code != 0 ? exit_code : 2);
	}

	return exit_code;
}

/**
 * @brief check_file_exists - see if file exists
 * @param const char *const filename - null-terminated array / c-string pertaining to name of file
 * @return int - Boolean as to file's existance. 1 if file exists, 0 if not
 */
static inline int check_file_exists(const char *const filename)
{
	const int access_code = access(filename, F_OK);
	return (access_code == 0);
}

int execute_request(const enum request_command cmd, const char *const sbj, const uint32_t extra_data_len, const char *const extra_data, const int client_sock)
{
	if (cmd == ADD) { /* based on command, execute different paths */
		if (check_file_exists(sbj) == 1) {
			fprintf(stderr, "Cannot overwrite existing note of same name\n");
			return 1;
		}

		FILE *new_file = fopen(sbj, "w");
		if (new_file == NULL) {
			fprintf(stderr, "Error opening '%s' as write-file (errno %d: %s)\n", sbj, errno, strerror(errno));
			return 1;
		}

		if (fwrite(extra_data, sizeof(uint8_t), extra_data_len, new_file) != extra_data_len) {
			fprintf(stderr, "Error writing to file %s\n", sbj);
			fclose(new_file);
			return 1;
		}

		if (fclose(new_file) == EOF) {
			fprintf(stderr, "Error closing '%s' as write-file (errno %d: %s)\n", sbj, errno, strerror(errno));
		}

		fprintf(stdout, "Created note titled %s\n", sbj);
	} else if (cmd == GET) {
		if (check_file_exists(sbj) == 0) {
			fprintf(stderr, "Cannot get contents of non-existant note\n");
			return 1;
		}

		FILE *new_file = fopen(sbj, "r");
		if (new_file == NULL) {
			fprintf(stderr, "Error opening '%s' as read-file (errno %d: %s)\n", sbj, errno, strerror(errno));
			return 1;
		}

		char file_content[MAX_EXTRA_DATA_LEN];
		const size_t bytes_read = fread(file_content, sizeof(uint8_t), MAX_EXTRA_DATA_LEN, new_file);
		if (bytes_read == 0) {
			fprintf(stderr, "Error reading anything from file %s\n", sbj);
			fclose(new_file);
			return 1;
		}

		if (fclose(new_file) == EOF) {
			fprintf(stderr, "Error closing '%s' as write-file (errno %d: %s)\n", sbj, errno, strerror(errno));
		}

		struct Response resp;
		resp.status = DATA;
		resp.extra_data_len = (uint32_t)bytes_read; /* wouldn't cause overflow or crunching from 8 -> 4 bytes as the upper count of readable bytes is MAX_EXTRA_DATA_LEN which is tiny. But leaving as an explicit comment as this could be an issue if it was significantly higher */
		resp.extra_data_content = file_content;

		if (response_send(&resp, client_sock) != 0) {
			fprintf(stderr, "Error sending response to GET request\n");
			return 1;
		}

		fprintf(stdout, "Retrieved note titled %s\n", sbj);
	} else if (cmd == REMOVE) {
		if (check_file_exists(sbj) == 0) {
			fprintf(stderr, "Cannot delete non-existant note\n");
			return 1;
		}

		if (unlink(sbj) != 0) {
			fprintf(stderr, "Unable to delete file %s (errno %d: %s)\n", sbj, errno, strerror(errno));
			return 1;
		}

		fprintf(stdout, "Removed note titled %s\n", sbj);
	}

	return 0;
}
