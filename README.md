# assignment_1
## README

Note taking system for assessment 1, option 1 for the 'Secure Software & Hardware' module. Written in C99

### Summarising code

- The client code `note` connects to the socketfile
- It uses command line arguments to determine actions prior to sending and then sends


- The program `noticeboard` creates a UNIX IPC socketfile and acts as a server, accepting incoming connections
- It manages a directory which only it has permissions to access (700). It stores all user data here
- Server handles response. Sends confirmation back

- Structured requests are *sent* to the server, using the packet format below:
>>>|   Command ID (uint8_t)  |  Subject Length (uint32_t)  |                     Subject Content (char[])            | Extra Data Length (uint32_t) | Extra Data (void*)                                        |
>>>|:----------------------------:|:-------------------------:|:-------------------------------------------------------:|:--------------------------:|------------------------------------------------------------|
>>>| 0 (add), 1 (get), 2 (remove) | 1 to MAX_SBJ_LEN | *Number of characters as noted in Subject Length field* | 0 - MAX_EXTRA_DATA_LEN          | *Number of characters as noted in Extra Data Length field* |

- Structured responses are sent *from* the server, using the packet format below:
>>> | Status code (unsigned int) | Extra Data Length (uint32_t) |                    Extra Data (void*)                     |
>>> |:--------------------------:|:--------------------------:|:----------------------------------------------------------:|
>>> | 0 (OK), 1 (Fail)           | 0 - MAX_EXTRA_DATA_LEN          | *Number of characters as noted in Extra Data Length field* |

The 'Extra Data*' fields are optional as the fields are not always used up
>>> For example, adding a note requires an additional argument of the note's content to be sent to the server
>>> Similarly, when asking to view a note, data pertaining to the contents of the note is required too
>>> All other cases simply don't care

### Building

The build process makes use of the GNU `make` utility

Commands implemented:
- `make (all)` - builds all files
- `make clean` - deletes all compiled output

### Using

Run an instance of `noticeboard` (tested mainly as root but others work).

Have as many clients as you want running `note`. It is a simple command line utility which has the following operations at hand:
- When you run the program with the arguments `write <SUBJECT>`, it will read standard input and create a file in the directory maintained by `noticeboard` called 'SUBJECT_XXXX' (where XXXX is a random string to make the filename unique) containing the text, and print out XXXX
- When you run the program with the arguments `read <SUBSTR>`, it prints out all the notes whose subject contains 'SUBSTR' (i.e. matching regex *SUBSTR*)
- When you run the program with the arguments `note remove XXXX`, it removes the note ending in 'XXXX'

For the latter application, try switching between running as root (uid 0) and your normal account - you'll find everything acts independantly of each other.

---

* You may need to delete `noticeboard.sock` (or whatever you've chosen to named the socketfile in the Makefile) between calls to `noticeboard` - it's not really a resource and the code to avoid cloberring it, particularly as root, is more trouble and virtually zero gain (from a security perspective).

* It is not recommended to use `gdb` to run the programs. Due to the way the program is setup, if errors are encountered then sockets are closed on one end, with telling the other, and cause SIGPIPE errors. I've ignored the signals but `gdb` disregards this.

* Following from the point above, there is no issue with Valgrind - it functioned perfectly
