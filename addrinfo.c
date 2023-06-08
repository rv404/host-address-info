#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <inttypes.h>
#include <sys/types.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

struct args {
	char *host;
	char *port;
	char *opt1;
	char *opt2;
	char *errstr;
	int   hasHost;
	int   hasPort;
	int   numOpts;
};

int   validateAndParseUserInput(int, char **, struct args *, struct addrinfo *);
void  showUsage(void);
char *getFlags(int);
char *getSockType(int); 
char *getFamilyType(int);
void  printSockAddr(struct addrinfo *);
int   findHost(char *, struct args *);
int   findPort(char *, struct args *);
int   findOpt(char *, struct args *, struct addrinfo *);

int main(int argc, char **argv)
{
	int    gaires;
	struct addrinfo *hostinfo = NULL;
	struct addrinfo *hinfo	  = NULL;
	struct addrinfo  hints;
	struct args 	 args;

	/* Initialize args struct */
	args.host	= NULL;
	args.port	= NULL;
	args.opt1	= NULL;
	args.opt2	= NULL;
	args.errstr	= NULL;
	args.hasHost	= 0;
	args.hasPort	= 0;
	args.numOpts	= 0;

	/* Initialize hint struct */
	hints.ai_flags		= 0;
	hints.ai_family		= 0;
	hints.ai_socktype	= 0;
	hints.ai_protocol	= 0;
	hints.ai_addrlen	= 0;
	hints.ai_canonname	= (char *) NULL;
	hints.ai_addr		= (struct sockaddr *) NULL;
	hints.ai_next		= (struct addrinfo *) NULL;

	/* Validate the user input and set up args struct; also set hints struct fields as necessary */
	if (validateAndParseUserInput(argc, argv, &args, &hints) != 0) {
		printf("Error: %s\n", args.errstr);
		showUsage();
		exit(EXIT_FAILURE);
	}

	if ((gaires = getaddrinfo(args.host, args.port, &hints, &hostinfo)) != 0) {
		printf("Error: %s\n", gai_strerror(gaires));
		exit(EXIT_FAILURE);
	}

	for (hinfo = hostinfo; hinfo != (struct addrinfo *) NULL; hinfo = hinfo->ai_next) {
		printf("++---+---+---+---+---+---+---+---+---+---+---+---+---+---+---++\n");

		/* struct addrinfo {
		 * 	int              ai_flags;
		 *	int              ai_family;
		 *	int              ai_socktype;
		 *	int              ai_protocol;
		 *	socklen_t        ai_addrlen;
		 *	struct sockaddr *ai_addr;
		 *	char            *ai_canonname;
		 *	struct addrinfo *ai_next;
		 * };
		 */

		printf("Address family...%s\n", getFamilyType(hinfo->ai_family));
		printf("Socket type......%s\n", getSockType(hinfo->ai_socktype));
		printf("Protocol.........%d\n", hinfo->ai_protocol);

		if (hinfo->ai_flags != 0) {
			printf("Flags............%s\n", getFlags(hinfo->ai_flags));
		}

		if (hinfo->ai_addr != (struct sockaddr *) NULL) {

			/*
			 * struct sockaddr {
			 *      sa_family_t      sa_family;
			 *      char             sa_data[];
			 * };
			 */

			printSockAddr(hinfo);
		}
	}
	printf("++---+---+---+---+---+---+---+---+---+---+---+---+---+---+---++\n");

	freeaddrinfo(hostinfo);
	exit(EXIT_SUCCESS);
}

void showUsage(void)
{
	puts("Determine useful address information about a host given its name and a port number.");
	puts("Usage: addrinfo [-h] [-4|-6] [-tcp|-udp|-a] host port\n");
	puts("    host      Host's name (e.g. www.google.com)");
	puts("    port      Host's port number in decimal (e.g. 80)\n");
	puts("Options:");
	puts("    Zero or more options can be entered.");
	puts("    -h        Show this help message and exit");
	puts("    -4        Only display IPv4 host addresses");
	puts("    -6        Only display IPv6 host addresses");
	puts("    -tcp      Only display host addresses that use TCP");
	puts("    -udp      Only display host addresses that use UDP");
	puts("    -a        (Default) Display all available host addresses\n");
	return;
}

int validateAndParseUserInput(int argcount, char **userargv, struct args *args, struct addrinfo *hint)
{
	/*
	 * Returns 0 if the input is appropriate and sets up the args struct.
	 * Otherwise returns 1 and sets an error message string in the args struct.
	 */

	int i;

	/* Ensure at least 2 arguments were provided */ 
	if (argcount < 2) {
		args->errstr = "Too few arguments (must provide at least a host argument).";
		return 1;
	}

	/* Ensure no more than 5 arguments were provided */ 
	if (argcount > 5) {
		args->errstr = "Too many arguments provided.";
		return 1;
	}

	/* Scan all user args and store them into the correct args struct field if they are valid */
	for (i = 1; i < argcount; i++) {
		/* Bail out if an error was encountered during parsing */
		if (args->errstr != (char *) NULL) return 1;

		/* Scan userargv for at most 2 options */
		if ((args->numOpts == 0) || (args->numOpts == 1)) {
			if (findOpt(userargv[i], args, hint) == 1) continue;
		}

		/* Scan userargv for a port argument */
		if (args->hasPort == 0) {
			if (findPort(userargv[i], args) == 1) continue;
		}

		/* Scan userargv for a host name argument */
		if (args->hasHost == 0) {
			if (findHost(userargv[i], args) == 1) continue;
		}
	}

	/* Handles the default/"-a" option if another AF_* option has not been selected */
	if (hint->ai_family == 0) hint->ai_family = AF_UNSPEC;
	
	/* Ensure a valid host argument and a valid port argument was found */ 
	if ((args->hasHost == 0) || (args->host == NULL)) {
		args->errstr = "No valid host name provided";
		return 1;
	}

	return 0;
}

int findOpt(char *argstr, struct args *args, struct addrinfo *h)
{
	/*
	 * Return 1 and do not set error string if a valid option is found; return 1 and set error string
	 * if an invalid option is found; else return 0 if not an option argument.
	 */

	size_t len = strlen(argstr);

	if (strncmp(argstr, "-", 1) == 0) {
		if ((strncmp(argstr, "-h", 2) == 0) && (len == 2)) {
			/* Display help and immediately exit the program */
			showUsage();
			exit(EXIT_SUCCESS);

		} else if ((strncmp(argstr, "-4", 2) == 0) && (len == 2)) {
			if (args->numOpts == 0) {
				/* Obtain only IPv4 addresses */
				args->opt1   = argstr;
				h->ai_family = AF_INET;
				args->numOpts++;
				return 1;

			} else if ((args->numOpts == 1) && (h->ai_family == 0)) {
				/* Obtain only IPv4 addresses if AF_INET6 was not already set */
				args->opt2   = argstr;
				h->ai_family = AF_INET;
				args->numOpts++;
				return 1;

			} else {
				return 1;
			}

		} else if ((strncmp(argstr, "-6", 2) == 0) && (len == 2)) {
			if (args->numOpts == 0) {
				/* Obtain only IPv6 addresses */
				args->opt1   = argstr;
				h->ai_family = AF_INET6;
				args->numOpts++;
				return 1;

			} else if ((args->numOpts == 1) && (h->ai_family == 0)) {
				/* Obtain only IPv6 addresses if AF_INET was not already set */
				args->opt2   = argstr;
				h->ai_family = AF_INET6;
				args->numOpts++;
				return 1;

			} else {
				return 1;
			}

		} else if ((strncmp(argstr, "-tcp", 4) == 0) && (len == 4)) {
			if (args->numOpts == 0) {
				/* Obtain only TCP addresses */
				args->opt1     = argstr;
				h->ai_socktype = SOCK_STREAM;
				args->numOpts++;
				return 1;

			} else if ((args->numOpts == 1) && (h->ai_socktype == 0)) {
				/* Obtain only TCP addresses if SOCK_DGRAM was not already set */
				args->opt2     = argstr;
				h->ai_socktype = SOCK_STREAM;
				args->numOpts++;
				return 1;

			} else {
				return 1;
			}

		} else if ((strncmp(argstr, "-udp", 4) == 0) && (len == 4)) {
			if (args->numOpts == 0) {
				/* Obtain only UDP addresses */
				args->opt1     = argstr;
				h->ai_socktype = SOCK_DGRAM;
				args->numOpts++;
				return 1;

			} else if ((args->numOpts == 1) && (h->ai_socktype == 0)) {
				/* Obtain only UDP addresses if SOCK_STREAM was not already set */
				args->opt2     = argstr;
				h->ai_socktype = SOCK_DGRAM;
				args->numOpts++;
				return 1;

			} else {
				return 1;
			}

		} else {
			args->errstr = "Invalid option";
			return 1;
		}

	} else {
		return 0;
	}
}

int findPort(char *argstr, struct args *args)
{
	/* Return 1 if a valid port argument is found, else return 0. */

	int i;
	size_t len;
	unsigned long portnum;
	unsigned char *c_array = (unsigned char *) argstr;

	/* Ensure this is not a renegade option since the lead "-" can be mistaken for a minus sign */
	if (strncmp(argstr, "-", 1) == 0) return 0;

	/* Ensure port argument is 1-5 charaters long given uint16_t range restriction */
	len = strlen(argstr);
	if ((len < 1) || (len > 5)) return 0;

	/* Ensure port argument only contains decimal digits */
	for (i = 0; i < len; i++) {
		if (isdigit(c_array[i]) == 0) return 0;
	}

	/* Ensure port argument is a number between 1-65535 since it will be stored in an uint16_t */
	portnum = strtoul(argstr, NULL, 10);
	if ((portnum < 1) || (portnum > 65535)) {
		return 0;
	} else {
		args->port    = argstr;
		args->hasPort = 1;
		return 1;
	}
}

int findHost(char *argstr, struct args *args)
{
	/* Return 1 if a valid host argument is found, else return 0. */
	/* By now, findOpt has removed up to 2 strings starting with "-" and findPort has removed
		up to 1 string with only decimal numbers in it from the user's args array. */

	int i;
	size_t len;
	unsigned char *c_array = (unsigned char *) argstr;

	/* Ensure this is not a renegade option that somehow made it through */
	if (strncmp(argstr, "-", 1) == 0) return 0;

	/* Ensure host name argument is no more than 100 characters and at least 5 characters long ("x.com") */
	len = strlen(argstr);
	if ((len < 5) || (len > 100)) return 0;

	/* Ensure host name argument only contains printable characters */
	for (i = 0; i < len; i++) {
		if (isgraph(c_array[i]) == 0) return 0;
	}

	args->host    = argstr;
	args->hasHost = 1;
	return 1;
}

char *getFlags(int flags)
{
	// Converting flags to a str and substituting "NONE" if flags is 0
	char *flags_str;

	if (flags != 0) {
		sprintf(flags_str, "%d", flags);
		return flags_str;
	} else {
		return "NONE";
	}
}

char *getSockType(int socktype)
{
	/*
	 *  SOCK_STREAM     1
	 *  SOCK_DGRAM      2
	 *  SOCK_RAW        3
	 *  SOCK_RDM        4
	 *  SOCK_SEQPACKET  5
	 *  SOCK_DCCP       6
	 *  SOCK_PACKET     10
	 */

	switch (socktype) {
	case 1:
		return "TCP  (SOCK_STREAM)";
	case 2:
		return "UDP  (SOCK_DGRAM)";
	case 3:
		return "RAW  (SOCK_RAW)";
	case 4:
		return "SOCK_RDM";
	case 5:
		return "SOCK_SEQPACKET";
	case 6:
		return "SOCK_DCCP";
	case 10:
		return "SOCK_PACKET";
	default:
		return "UNKNOWN";
	}
}

char *getFamilyType(int addrfam)
{
	/*  
	 *  AF_INET         2
	 *  AF_INET6        10
	 */

	switch (addrfam) {
	case 2:
		return "IPv4 (AF_INET)";
	case 10:
		return "IPv6 (AF_INET6)";
	default:
		return "UNKNOWN";
	}
}

void printSockAddr(struct addrinfo *info)
{
	/* Parse the IPv4/IPv6 address from the sockaddr structure */
	unsigned int i, x, y, len;
	unsigned char *data = (unsigned char *) info->ai_addr;
	uint16_t port;

	/* Print out IPv4 or IPv6 address, byte-by-byte */
	len = (unsigned int) info->ai_addrlen;
	if ((info->ai_family == 2) && (len > 7)) {
		/* AF_INET == 2 */
		printf("IPv4 address.....");
		for (x = 4; x < 8; x++) {
			printf("%u", data[x]);
			if (x < 7) printf(".");
			if (x == 7) printf("\n");
		}
	} else if ((info->ai_family == 10) && (len > 23)) {
		/* AF_INET6 == 10 */
		printf("IPv6 address.....");
		for (y = 8; y < 24; y++) {
			printf("%02x", data[y]);
			if ((y % 2 == 1) && (y < 23)) printf(":");
			if (y == 23) printf("\n");
		}
	}

	port = ntohs(*((uint16_t *) &data[2]));
	if (port > 0) {
		printf("Port.............%"PRIu16"\n", port);	
	}

	return;
}
