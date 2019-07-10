#include <unistd.h>
#include <fcntl.h>

// Linux only: uses sendfile
static int
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

static int
acquire_write_lock(int fd)
{
	int ret;
	do {
		ret = fcntl(fd, F_SETLKW, &(struct flock){
			.l_type = F_WRLCK,
			.l_whence = SEEK_SET,
		});
	} while(ret == -1 && errno == EINTR);
	if(ret == -1) {
		perror("fcntl F_SETLKW");
		return -1;
	}
	return 0;
}

static int
release_lock(int fd)
{
	return fcntl(fd, F_SETLKW, &(struct flock){
		.l_type = F_UNLCK,
		.l_whence = SEEK_SET,
	});
}

int
main(int argc, char *argv[])
{
	// FIXME: error checking

	int ret;
	int orig = open("post.orig", O_RDONLY);
	struct stat orig_stat;
	int inflight;
	int live_edit;

	// An instance of compend can still have the post opened
	link("post", "post.inflight");
	inflight = open("post.inflight", O_RDONLY);

	// Make edit live
	rename("post.edit", "post");
	live_edit = open("post", O_APPEND);

	// FIXME: in case of errors we can lose comments that were inflight
	// to recover both original and inflight are needed

	// Wait for the last compend instances
	do {
		acquire_write_lock(inflight);
		release_lock(inflight);
	} while((ret = fcntl(inflight, F_SETLEASE, F_RDLCK)) == -1 && errno == EAGAIN);

	// Copy comments added from the moment of lock to the edited post
	acquire_write_lock(live_edit);
	fstat(orig, &orig_stat);
	copy(inflight, orig_stat.st_size, live_edit);

	unlink("post.orig");
	unlink("post.inflight");

	return 0;
}
