#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define LEN(a) (sizeof(a) / sizeof(a)[0])

static const char name_hdr[] = "name=";
static const char msg_hdr[] = "msg=";

static const char template[] =
	"<div id=\"%s\"><small>%s at %s</small>\n"
	"<p>\n"
	"%.*s"
	"</div>\n";

int
main(int argc, char *argv[])
{
	char inbuf[4096];
	char *in = inbuf;
	char outbuf[8192] = "";
	char *path = getenv("PATH_INFO");
	if(strcmp(path, "/hello.html")) {
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
	int fd = open(path+1, O_WRONLY | O_APPEND);
	
	prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);
	
	int len = atoi(getenv("CONTENT_LENGTH"));
	if(len > sizeof(inbuf)) {
		fputs("Bad CONTENT_LENGTH\n", stderr);
		syscall(SYS_exit, -1);
		return -1;
	}
	
	len = read(STDIN_FILENO, inbuf, MIN(len, sizeof(inbuf)));
	int rest = len;

	if(rest < sizeof(name_hdr)-1) {
		fputs("Bad name header length\n", stderr);
		syscall(SYS_exit, -1);
		return -1;
	}
	if(memcmp(inbuf, name_hdr, sizeof(name_hdr)-1)) {
		fputs("Bad name header\n", stderr);
		syscall(SYS_exit, -1);
		return -1;
	}
	in += sizeof(name_hdr)-1;
	rest -= sizeof(name_hdr)-1;
	char *nl = memchr(in, '\n', rest);
	char *name = in;
	*nl = '\0';
	int name_len = nl - in;
	in += name_len + 1;
	rest -= name_len + 1;
	
	if(rest < sizeof(msg_hdr)-1) {
		fputs("Bad msg header length\n", stderr);
		syscall(SYS_exit, -1);
		return -1;
	}
	if(memcmp(in, msg_hdr, sizeof(msg_hdr)-1)) {
		fputs("Bad msg header\n", stderr);
		syscall(SYS_exit, -1);
		return -1;
	}
	in += sizeof(msg_hdr)-1;
	rest -= sizeof(msg_hdr)-1;
	char *msg = in;
	int msg_len = rest;
	
	int outlen = snprintf(outbuf, sizeof(outbuf), template,
		"random", name, "today", rest, msg);
	write(fd, outbuf, outlen);

	outlen = snprintf(outbuf, sizeof(outbuf),
		"HTTP/1.0 302 Ok\r\n"
		"Status: 302 Moved\r\n"
		"Location: %s\r\n"
		"\r\n",
		path
	);
	write(STDOUT_FILENO, outbuf, outlen);
	syscall(SYS_exit, 0);
	return 0;
}
