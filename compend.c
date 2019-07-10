#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <ctype.h>
#include <time.h>


#define ALLOWED_PATH "/hello.html"


#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define LEN(a) (sizeof(a) / sizeof(a)[0])

#define NAMEHDR "name="
#define MSGHDR "msg="

/*
static const char template[] =
	"<div id=\"%s\"><small>%s at %s</small>\n"
	"<p>\n"
	"%.*s"
	"</div>\n";
*/

int
compend(void)
{
	int ret;
	long content_length;
	char in_buf[8192+1];
	int in_len;
	char buf[BUFSIZ];
	int file_fd;
	FILE *file;
	int i;
	__uint128_t random;
	char *name;
	int name_len;
	char *msg;
	int msg_len;
	char id[17] = {0};
	char timestr[32];

	char *path = getenv("PATH_INFO");
	if(strcmp(path, ALLOWED_PATH)) {
		fputs("Bad path\n", stderr);
		return -1;
	}

	if(strcmp(getenv("CONTENT_TYPE"), "text/plain")) {
		fputs("Bad CONTENT_TYPE\n", stderr);
		return -1;
	}

	content_length = strtol(getenv("CONTENT_LENGTH"), NULL, 10);
	if(content_length < 0 || content_length > (long)sizeof(in_buf)) {
		fputs("Bad CONTENT_LENGTH\n", stderr);
		return -1;
	}

	in_len = fread(in_buf, 1, sizeof(in_buf), stdin);
	if(in_len >= (int)sizeof(in_buf)) {
		fputs("Too much data\n", stderr);
		return -1;
	}

	if(ferror(stdin) || !feof(stdin) || (content_length > 0 && content_length != in_len)) {
		perror("Error reading data");
		return -1;
	}

	name = in_buf;
	name_len = in_len;

	if(name_len < (int)strlen(NAMEHDR) || memcmp(name, NAMEHDR, strlen(NAMEHDR))) {
		fputs("Bad name header\n", stderr);
		return -1;
	}

	name = &in_buf[strlen(NAMEHDR)];
	name_len = in_len - strlen(NAMEHDR);

	char *lf = memchr(name, '\n', name_len);
	if(lf == NULL) {
		fputs("Bad name\n", stderr);
		return -1;
	}
	name_len = lf - name;

	msg = &lf[1];
	msg_len = in_len - strlen(NAMEHDR) - name_len;

	if(msg_len < (int)strlen(MSGHDR) || memcmp(msg, MSGHDR, strlen(MSGHDR))) {
		fputs("Bad msg header\n", stderr);
		return -1;
	}

	msg = &msg[strlen(MSGHDR)];
	msg_len -= strlen(MSGHDR);

	getentropy(&random, sizeof(random));

	strftime(timestr, sizeof(timestr), "%F %T %z",
		gmtime(&(time_t){time(NULL)})
	);

	chdir("..");
	file_fd = open(path+1, O_WRONLY | O_APPEND);
	if(file_fd == -1) {
		perror("Error opening file");
		return -1;
	}

	// FIXME:
	// The lock is enough to make it safe for interleaving writes,
	// but the problem remains for interleaved reads and writes.
	// It can happen that someone sees a part of a comment.
	// To make it safe use pwrite and change the offset in the end
	// That makes use of stdio not possible
	do {
		ret = fcntl(file_fd, F_SETLKW, &(struct flock){
			.l_type = F_WRLCK,
			.l_whence = SEEK_SET,
		});
	} while(ret == -1 && errno == EINTR);
	if(ret == -1) {
		perror("fcntl F_SETLKW");
		return -1;
	}

	file = fdopen(file_fd, "a");
	if(file == NULL) {
		perror("fdopen");
		return -1;
	}

	if(prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT) == -1) {
		perror("prctl PR_SET_SECCOMP SECCOMP_MODE_STRICT");
		return -1;
	}

	/**** from now on only read, write and _exit syscalls allowed ****/
	
	// explicitly set buffer to avoid fstat probe
	setbuf(file, buf);

	fputs("<div id=\"", file);
	for(int i = 0; i < 16; i++) {
		id[i] = 'a' + ((random >> i) & 0xf);
	}
	fputs(id, file);
	fputs("\">\n<small>", file);

	for(i = 0; i < name_len; i++) {
		switch(name[i]) {
		case '<':
			fputs("&lt;", file);
			break;
		case '>':
			fputs("&gt;", file);
			break;
		case '&':
			fputs("&amp;", file);
			break;
		default:
			if(iscntrl(name[i])) {
				fputc('^',  file);
				fputc(name[i] + 64, file);
			} else {
				fputc(name[i], file);
			}
		}
	}

	fputs(" at ", file);

	fputs(timestr, file);

	fputs("</small>\n<p>\n", file);

	for(i = 0; i < msg_len; i++) {
		switch(msg[i]) {
		case '<':
			fputs("&lt;", file);
			break;
		case '>':
			fputs("&gt;", file);
			break;
		case '&':
			fputs("&amp;", file);
			break;
		case '\r':
			/* do nothing */
			break;
		case '\n':
			if(i+1 >= msg_len) {
				/* do nothing */
			} else if(msg[i+1] != '\n') {
				fputs("<br>", file);
			} else {
				for(i++; i+1 < msg_len; i++) {
					if(msg[i+1] != '\r' && msg[i+1] != '\n') {
						fputs("\n<p>", file);
						break;
					}
				}
			}
			fputc('\n', file);
			break;
		default:
			if(iscntrl(msg[i])) {
				fputc('^',  file);
				fputc(msg[i] + 64, file);
			} else {
				fputc(msg[i], file);
			}
		}
	}

	fputs("</div>\n", file);

	fflush(file);

	setbuf(stdout, buf);
	printf(
		"HTTP/1.0 302 Ok\r\n"
		"Status: 302 Moved\r\n"
		"Location: %s#%s\r\n"
		"\r\n",
		path, id
	);
	fflush(stdout);

	return 0;
}

int
main(void)
{
	int ret = compend();
	syscall(SYS_exit, ret);
	return ret;
}
