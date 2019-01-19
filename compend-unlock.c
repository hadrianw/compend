#include <unistd.h>
#include <fcntl.h>

// Linux only: uses sendfile
int
copy(int src, off_t off, int dst)
{
	struct stat s_stat;
	fstat(src, &s_stat);
	
	if(s_stat.st_size < off) {
		return -1;
	}
	size_t count = s_stat.st_size - off;
	ssize_t size = 0;

	for(; size >= 0 && count > 0; count -= size) {
		size = sendfile(dst, src, &off, count);
	}

	/* or something like this

	lseek(src, off, SEEK_POS);
	do {
		size = read(src, buf, sizeof buf);
		write(dst, buf, size);
	} while();
	*/
}

// Linux only: uses fcntl F_SETLEASE
int
wait_until_write_closed(char *path)
{
	int fd = open(file, O_RDONLY);
	while(fcntl(fd, F_SETLEASE, F_RDLCK) < 0 && errno == EAGAIN) {
		usleep(500 * 1000)
	}
	// don't care about the return
	close(fd);
	return 0;
}

int
main(int argc, char *argv[])
{
	int live = open("post", O_RDONLY);
	int orig = open("post.orig", O_RDONLY);
	int edit = open("post.edit", O_APPEND);
	int inflight;

	struct stat o_stat;
	fstat(orig, &o_stat);
	copy(live, o_stat.st_size, edit);

	rename("post.edit", "post.orig");

	link("post", "post.inflight");
	inflight = live;

	struct stat e_stat;
	fstat(edit, &e_stat);
	rename("post.edit", "post");
	live = edit;

	wait_until_write_closed(inflight);

	//append(post.inflight, orig.st_size, post)
	copy(inflight, e_stat.st_size, live);

	unlink("post.inflight");
	
	return 0;
}
