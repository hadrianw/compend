#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
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

static int
matchstr(const char *str)
{
	int ch;
	do {
		ch = getchar();
	} while(*str && ch != EOF && ch == *str++);
	if(ch != EOF) {
		ungetc(ch, stdin);
	}
	return *str == '\0';
}

static int
getcharx()
{
	int ch;
	do {
		ch = getchar();
	} while(ch == '\r');
	return ch;
}

// FIXME: proper error checking
int
main(void)
{
	char buf[BUFSIZ];
	char fbuf[4096 * 4];
	// FIXME: do not let overflowing of fbuf
	int flen = 0;
	FILE *file;
	int ch;
	__uint128_t random;
	char id[17] = {0};
	char timestr[32];
	int inlen;

	char *path = getenv("PATH_INFO");
	if(strcmp(path, ALLOWED_PATH)) {
		fputs("Bad path\n", stderr);
		syscall(SYS_exit, -1);
		return -1;
	}

	if(strcmp(getenv("CONTENT_TYPE"), "text/plain")) {
		fputs("Bad CONTENT_TYPE\n", stderr);
		syscall(SYS_exit, -1);
		return -1;
	}
	
	chdir("..");
	file = fdopen(open(path+1, O_WRONLY | O_APPEND), "a");

	getentropy(&random, sizeof(random));

	strftime(timestr, sizeof(timestr), "%F %T %z",
		gmtime(&(time_t){time(NULL)})
	);
	
	prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);
	// from now on only read, write and _exit syscalls allowed
	
	inlen = atoi(getenv("CONTENT_LENGTH"));
	if(inlen > sizeof(fbuf)) {
		fputs("Bad CONTENT_LENGTH\n", stderr);
		syscall(SYS_exit, -1);
		return -1;
	}

	// explicitly set buffer to avoid fstat probe
	setbuf(stdin, buf);
	setbuffer(file, fbuf, sizeof(fbuf));

	if(!matchstr(NAMEHDR)) {
		fputs("Bad name header\n", stderr);
		syscall(SYS_exit, -1);
		return -1;
	}
	
	fputs("<div id=\"", file);
	for(int i = 0; i < 16; i++) {
		id[i] = 'a' + ((random >> i) & 0xf);
	}
	fputs(id, file);
	fputs("\">\n<small>", file);

	for(ch = getcharx(); ch != '\n' && ch != EOF; ch = getcharx()) {
		switch(ch) {
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
			if(iscntrl(ch)) {
				fputc('^',  file);
				fputc(ch + 64, file);
			} else {
				fputc(ch, file);
			}
		}
	}

	fputs(" at ", file);

	fputs(timestr, file);

	fputs("</small>\n<p>\n", file);

	if(!matchstr(MSGHDR)) {
		fputs("Bad msg header\n", stderr);
		syscall(SYS_exit, -1);
		return -1;
	}

	for(ch = getcharx(); ch != EOF; ch = getcharx()) {
		if(ch == '\r') {
			continue;
		}
		switch(ch) {
		case '<':
			fputs("&lt;", file);
			break;
		case '>':
			fputs("&gt;", file);
			break;
		case '&':
			fputs("&amp;", file);
			break;
		case '\n':
			ch = getcharx();
			if(ch != '\n') {
				fputs("<br>\n", file);
			} else {
				do {
					ch = getcharx();
				} while(ch == '\n');
				if(ch != EOF) {
					fputs("\n<p>\n", file);
				} else {
					fputc('\n', file);
				}
			}
			ungetc(ch, stdin);
			break;
		default:
			if(iscntrl(ch)) {
				fputc('^',  file);
				fputc(ch + 64, file);
			} else {
				fputc(ch, file);
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

	syscall(SYS_exit, 0);
	return 0;
}
