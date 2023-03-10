#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <argp.h>

#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "request.h"
#include "response.h"

#ifndef NOTICEBOARD_SOCK_NAME
	#error "'NOTICEBOARD_SOCK_NAME' must be set to a UNIX IPC socketfile"
#endif /* ifndef NOTICEBOARD_SOCK_NAME */

/**
 * @brief Client application to be ran each by ordinary users
 * Adds, views & removes notes
 */

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic push
const char* argp_program_bug_address = "salih.msa@outlook.com" ;
static const char args_doc[] = "COMMAND SUBJECT" ; /* description of non-option specified command line arguments */
static const char doc[] = "note -- client-side program to either write, read, or remove notes" ; /* general program documentation */
static struct argp_option options[] = { /* OPTIONS FOR ARGP. each entry stores: {NAME, KEY, ARG, FLAGS, DOC} */
	{0}
};

/**
 * @brief struct arguments - this structure is used to communicate with parse_opt (for it to store the values it parses within it)
 */
struct arguments {
	const char *cmd; /* read/write/view */

	const char *sbj; /* name of note text / subject */
};

/**
 * @brief parse_opt - deals with given arguments based on given argumentsK
 * @param int key - int correlating to char storing argument key
 * @param char *arg - argument string associated with argument key
 * @param struct argp_state *state - pointer to argp_state struct storing information about the state of the option parsing
 * @return error_t - number storing 0 upon successfully parsed values, non-zero exit code otherwise
 */
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;

	switch (key) {
		case ARGP_KEY_ARG:
			if (state->arg_num == 0) { /* if arg 1 */
				if (strcmp(arg, "write") == 0 || strcmp(arg, "read") == 0 || strcmp(arg, "remove") == 0) { /* no issue with using strcmp for 100% string literals (namely those "" and argv's) */
					arguments->cmd = arg;
				} else {
					fprintf(stderr, "Arg #1 should be any of the following: write read remove\n");
					argp_usage(state);
				}
			} else if (state->arg_num == 1) { /* if arg 2 */
				arguments->sbj = arg;
			} else if (state->arg_num > 1) { /* if we exceed */
				argp_usage(state);
			}
			break;
		case ARGP_KEY_END:
			if (state->arg_num < 2) { /* if end arg is not end of expected range ... */
				argp_usage(state);
			}
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}

	return 0;
}


static struct argp argp = { /* argp - The ARGP structure itself */
	options, /* list containing options */
	parse_opt, /* callback function to process args */
	args_doc, /* names of parameters */
	doc, /* documentation containing general program description */
};
#pragma GCC diagnostic pop /* end of argp, so end of repressing weird messages */

/**
 * @brief main - driver of `note`
 * @param int argc - number of arguments. should be 3
 * @param char **argv - list of args as c-strings, null terminated
 * @return int - zero is success, non-zero is failure
 * 1 is error in initialisation stage (be that connecting to socket), 2 is error in main functionality (be that accepting requests, reading messages), 3 is issues in cleanup
 */
int main(int argc, char **argv)
{
	/** Initialisation **/
	const char *const notes_socket = NOTICEBOARD_ROOT_DIR_NAME "/" NOTICEBOARD_SOCK_NAME; /* set actual variables to be content of macros */
	struct arguments arguments;
	argp_parse(&argp, argc, argv, 0, 0, &arguments); /* number, content, etc. of cmd-line args checked here */
	const char *cmd = arguments.cmd;
	const char *sbj = arguments.sbj;

	/* Number 1: create UNIX (IPC) socket
	 * AF_UNIX / AF_LOCAL (as opposed to AF_INET)
	 * 0 just selects first protocol which implements what's requested
	 * we connect to the IPC socket created by the server
	 * we also disable stupid SIGPIPE
	 * (from here, we also goto a cleanup section. man do i hate C sometimes)
	 */
	int exit_code = 0;
	
	const int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1) {  /* validly can be any non-negative so check for -1 which is error */
		fprintf(stderr, "Failure to create socket (errno %d: %s)\n", errno, strerror(errno));
		return 1;
	}

	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {   /* working with sockets can introduce a SIGPIPE signal
							* can't exactly use this like an exception, so ignore and purely use exit codes
							* more portable than using setsockopt(...)
							*/
		fprintf(stderr, "Failure to set signal to handle SIGPIPE (errno %d: %s)\n", errno, strerror(errno));
		exit_code = 1;
		goto eop;
	}

	struct sockaddr_un address; /* unix-derived domain sockets address */
	address.sun_family = AF_UNIX;

	if (sizeof(address.sun_path) < strlen(notes_socket) + 1) { /* this shouldn't be a problem but different OSs differ for this val. linux is 108. simply crash program if we can't fit this in. also +1 is because strlen is len - null terminator */
		fprintf(stderr, "(Internal error) Somehow the IPC socket's name is too long. Review source code\n");
		exit_code = 1;
		goto eop;
	}
	memset(address.sun_path, '\0', sizeof(address.sun_path));
	strncpy(address.sun_path, notes_socket, strlen(notes_socket));

	socklen_t addrlen = sizeof(address);

//	fprintf(stdout, "Creating listener to a UNIX domain socket @ %s\n", notes_socket);
	if (connect(sock, (struct sockaddr*)&address, addrlen) != 0) { /* set socket up to serve as a server */
		fprintf(stderr, "Failure to connect socket to end-point (errno %d: %s)\n", errno, strerror(errno));
		exit_code = 1;
		goto eop;
	}

	/** Main Program **/

	/* Number 2: send data
	 * determine course of action based of cmd:
	   * add: reach message from stdin, send to server
	   * view: just send subject to server and await response
	   * remove: just send subject to server
	 * we'll be using switch-case, which will basically copy bytes over to a buffer
	 * send string matching a valid API command (add = add, see = view, rmv = remove) - 3 letters just to make it easier
	 * await confirmation / data
	 */
//	fprintf(stdout, "Determining request to send to server\n");
	struct Request req;

	if (strcmp(cmd, "write") == 0) {
		req.cmd = ADD;
	} else if (strcmp(cmd, "read") == 0) {
		req.cmd = GET;
	} else if (strcmp(cmd, "remove") == 0) {
		req.cmd = REMOVE;
	} else {
		fprintf(stderr, "Invalid command (%s). Not sure why input parser didn't catch this...\n", cmd);
		exit_code = 2;
		goto eop;
	}
//	fprintf(stdout, "Request: %s -> %d\n", cmd, req.cmd);

	req.sbj_len = strlen(sbj); /* set sbj_len */
	if (req.sbj_len > sizeof(req.sbj_content)) {
		fprintf(stderr, "Command-line subject exceeds acceptabled length (maximum %u, was given %lu)\n", req.sbj_len, sizeof(req.sbj_content));
		return 2;
	}

#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic push
	memcpy(req.sbj_content, (uint8_t*)sbj, sizeof(req.sbj_content)); /* set sbj_content */
#pragma GCC diagnostic pop /* this is a really tricky issue due to my warning setup. i know this specific issue isn't a problem */ 

	/* we need to know what to set for extra_data_* */
	if (req.cmd == ADD) { /* we need to read into stdin for this, so the send procedure requires reading in and sending out */
		char *file_contents = malloc(MAX_EXTRA_DATA_LEN); /* read from stdin using fgets & ioctl */
		if (!file_contents) {
			fprintf(stderr, "Allocating block failed\n");
			exit_code = 2;
			goto eop;
		}

		memset(file_contents, '\0', MAX_EXTRA_DATA_LEN);

		write(STDOUT_FILENO, "> ", sizeof("> ")); /* prompt */
		ssize_t bytes_read = read(STDIN_FILENO, file_contents, MAX_EXTRA_DATA_LEN);
		if (bytes_read <= 0) {
			fprintf(stderr, "Failure to get any contents from stdin (errno %d: %s)\n", errno, strerror(errno));
			free(file_contents);
			exit_code = 2;
			goto eop;
		}

		req.extra_data_len = bytes_read;
		req.extra_data_content = (uint8_t*)file_contents;

		int ret = request_send(&req, sock);
		free(file_contents);
		if (ret != 0) {
			exit_code = 2;
			goto eop;
		}
	} else { /* for viewing and removal, we just send the subject / arg 2 */
		req.extra_data_len = 0;
		req.extra_data_content = NULL;

		if (request_send(&req, sock) != 0) {
			exit_code = 2;
			goto eop;
		}
	}

	sleep(1); /* we wait 1 second. either the server responds or we're screwed */

	if (req.cmd == GET) { /* we expect two responses when we make a GET request - the payload and then an ack */
		struct Response resp;

		resp.extra_data_content = malloc(MAX_EXTRA_DATA_LEN + 1);
		if (resp.extra_data_content == NULL) {
			fprintf(stderr, "Unable to allocate buffer to actually read file (errno %d: %s)\n", errno, strerror(errno));
			exit_code = 1;
			goto eop;
		}
		memset(resp.extra_data_content, '\0', MAX_EXTRA_DATA_LEN + 1);

		if (response_recv(&resp, sock) != 0 || resp.status != DATA) {
			fprintf(stderr, "Error getting data response\n");
			free(resp.extra_data_content);
			exit_code = 2;
			goto eop; /* no point even checking for positive acknowledgement and our GET... failed to get */
		}

		fprintf(stdout, "Note: %s\n", (char*)resp.extra_data_content);

		free(resp.extra_data_content);
		resp.extra_data_content = NULL;
	}

	struct Response resp;
	resp.extra_data_content = NULL;

	if (response_recv(&resp, sock) != 0) {
		fprintf(stderr, "Error getting ACK response\n");
		exit_code = 2;
		goto eop;
	}

	if (resp.status != OK) {
		fprintf(stderr, "Error getting good response\n");
		exit_code = 2;
		goto eop;
	}

	/** End of Program (EOP) **/
eop:
	if (close(sock) != 0) { /* attempt to close socket whilst reporting errors */
		fprintf(stderr, "Error closing socket %d (errno %d: %s)\n", sock, errno, strerror(errno));
		exit_code = 3;
	}
	return exit_code;
}
