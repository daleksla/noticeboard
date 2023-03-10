#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <argp.h>

#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "client_handling.h"

#ifndef NOTICEBOARD_ROOT_DIR_NAME
	#error "'NOTICEBOARD_ROOT_DIR_NAME' must be explicitly set to a directory"
#endif /* ifndef NOTICEBOARD_ROOT_DIR_NAME */

#ifndef NOTICEBOARD_DIR_NAME
	#error "'NOTICEBOARD_DIR_NAME' must be explicitly set to a directory"
#endif /* ifndef NOTICEBOARD_DIR_NAME */

#ifndef NOTICEBOARD_SOCK_NAME
	#error "'NOTICEBOARD_SOCK_NAME' must be explicitly set to a UNIX IPC socketfile"
#endif /* ifndef NOTICEBOARD_SOCK_NAME */

/**
 * @brief Server application to be ran by one managerial user
 * Maintains notes whilst preventing unauthorised access
 * To make this program actually useful, you'd probably want to multithread it (which wouldn't be hard, but you'd need to add a (shared) mutex)
 */

#define SOCKET_PERMISSIONS 766 /* read write execute by us, rw for else */
#define NOTE_PERMISSIONS 700 /* read write by us, not by anyone else */

/**
 * @brief main - driver of `noticeboard`
 * @return int - zero is success, non-zero is failure
 * 1 is error in initialisation stage (be that chroot'ing, forming notes directory, forming notes socket), 2 is error in main functionality (be that accepting requests, reading messages), 3 is issues in cleanup
 */
int main(void)
{
	/** Initialisation **/
	const char *const root_dir = NOTICEBOARD_ROOT_DIR_NAME; /* extracting args from argp struct */
	const char *const notes_folder = NOTICEBOARD_DIR_NAME; /* set actual variables to be content of macros */
	const char *const notes_socket = NOTICEBOARD_SOCK_NAME;

	/* Number 1: chroot to the note directory
	 * There's absolutely no need for this program to have access to any other file
	 * Here's the issue: chroot'ing is a privileged operations
	 * It's also bad practise to check if your a superuser (e.g. root) as there can be multiple
	 * Else kill the program
	 */
	if (chroot(root_dir) != 0 && errno != EPERM) {
		fprintf(stderr, "Failure to chroot into '%s' (even though we are running as superuser) (errno %d: %s)\n", root_dir, errno, strerror(errno));
		return 1;
	}

	/* Number 2: create subdirectory with very restricted permissions
	 * this is where given notes are written out to as files
	 */
	fprintf(stdout, "Creating restricted folder for notes @ (%s/)%s\n", root_dir, notes_folder);
	if (mkdir(notes_folder, NOTE_PERMISSIONS) != 0) {
		/* if there's a failure then its a weird issue OR (most likely) this programs been ran twice and folder exists still */
		if (errno == EEXIST) { /* programs dies if former, latter can be tolerated. we'll just keep writing to it */
			struct stat statbuf;
			fprintf(stdout, "Attempting to figure out permissions of existing folder to see if it's ours\n");
			if (stat(notes_folder, &statbuf) != 0) {
				fprintf(stderr, "Unable to get permissions of existing directory (errno %d: %s)\n", errno, strerror(errno));
				return 1;
			}

			if (statbuf.st_mode != 17068) { /* TODO mask out the other bytes so we're just left with permission bytes, then compare. for now we now what value it should be though */
				fprintf(stderr, "Failure to create notes directory - directory exists BUT wrong permissions so not ours (ours: %d, theirs: %d) (errno %d: %s)\n", NOTE_PERMISSIONS, statbuf.st_mode, errno, strerror(errno));
				return 1;
			}
		} else {
			fprintf(stderr, "Failure to create notes directory (errno %d: %s)\n", errno, strerror(errno));
			return 1;
		}
	}

	/* Number 3: create UNIX (IPC) socket
	 * AF_UNIX / AF_LOCAL (as opposed to AF_INET)
	 * 0 just selects first protocol which implements what's requested
	 * we configure options to make our sockets work reliably by diabling signal issues & enabling port re-use
	 * (from this point on, we jump to a cleanup section)
	 */
	fprintf(stdout, "Creating socket handle\n");
	int exit_code = 0;

	const int server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server_sock == -1) {  /* validly can be any non-negative so check for -1 which is error */
		fprintf(stderr, "Failure to create socket (errno %d: %s)\n", errno, strerror(errno));
		return 1;
	}

	if (
		setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) != 0
		&&
		setsockopt(server_sock, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) != 0
	) {
		fprintf(stderr, "Failure to set port / sockfile recycling (errno %d: %s)\n", errno, strerror(errno));
		exit_code = 1;
		goto eop;
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

	/* TODO check if socketfile exists, with select permissions
	 	* if it does, unlink it
	 	* else (it exists but not right permissions), kill program
	 */

	fprintf(stdout, "Creating UNIX domain socket @ (%s/)%s\n", root_dir, notes_socket);
	if (bind(server_sock, (struct sockaddr*)&address, addrlen) != 0) {
		fprintf(stderr, "Failure to bind socket to socketfile (errno %d: %s)\n", errno, strerror(errno));
		exit_code = 1;
		goto eop;
	}

	if (chmod(notes_socket, SOCKET_PERMISSIONS) != 0) {
		fprintf(stderr, "Failure to set permissions for socketfile (errno %d: %s)\n", errno, strerror(errno));
		exit_code = 1;
		goto eop;
	}

	fprintf(stdout, "Setting listener to socket\n");
	if (listen(server_sock, 100) != 0) { /* set socket up to serve as a server, 3 define the maximum length to which the queue of pending connections */
		fprintf(stderr, "Failure to set socket as listener (i.e. a server) (errno %d: %s)\n", errno, strerror(errno));
		exit_code = 1;
		goto eop;
	}

	if (chdir(notes_folder) != 0) { /* now that we've set everything up, we'll chdir again to the notes_folder. socket exists already so we're fine */
		fprintf(stderr, "Failure to chdir into sub-directory of notes '%s' (errno %d: %s)\n", notes_folder, errno, strerror(errno));
		exit_code = 1;
		goto eop;
	}

	/** Main Program **/
	/* Number 4: accept one connection at a time */
	while (1) {
		const int client_sock = accept(server_sock, NULL, NULL);
		if (client_sock < 0) { /* validly can be any non-negative so check for -1 which is error */
			fprintf(stderr, "Unexpected issue when creating server-client dedicated socket (errno %d: %s)\n", errno, strerror(errno));
			continue;
		}
		fprintf(stdout, "Established new client-server connection using socket %d\n", client_sock);

		if (client_connection(client_sock) != 0) { /* handles getting connection, sending acknowledgements */
			fprintf(stderr, "Issue when handling client (socket %d)\n", client_sock);
			/* we don't exit - issue with one client cannot terminate system */
		}

		fprintf(stdout, "Terminating client on socket %d\n", client_sock);
		if (close(client_sock) != 0) { /* attempt to close socket whilst reporting errors */
			fprintf(stderr, "Error closing socket %d (errno %d: %s)\n", client_sock, errno, strerror(errno));
		}
	}

	/** End of Program (EOP) **/
eop:
	if (close(server_sock) != 0) { /* attempt to close socket whilst reporting errors */
		fprintf(stderr, "Error closing socket %d (errno %d: %s)\n", server_sock, errno, strerror(errno));
		exit_code = 3;
	}
	return exit_code;
}
