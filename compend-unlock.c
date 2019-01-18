WIP pseudocode

int
append(char *src, int pos, char *dst)
{
	s = open(src)
	d = open(dst, O_APPEND)
	lseek(s, pos, SEEK_POS)
	
	do {
		size = sendfile(d, s, &pos, whole_size)
	} while(size > 0);

	/*or

	read(s, buf)
	write(d, buf)
	*/
}

wait_until_write_closed(file)
{
	// Linux only
	fd = open(file, rdonly)
	while(fcntl(fd, F_SETLEASE, F_RDLCK) < 0) {
		usleep()
	}
	close(fd)
}

int
main(int argc, char *argv[])
{
	append(post, orig.st_size, post.edit)
	exec(cp post.edit post.orig)

	link(post, post.inflight)
	rename(post.edit, post)

	wait_until_write_closed(post.inflight)

	append(post.inflight, orig.st_size, post)

	unlink(post.inflight)
	
	return 0;
}
