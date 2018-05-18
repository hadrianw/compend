WIP

int
append(char *src, int pos, char *dst)
{
	s = open(src)
	d = open(dst, O_APPEND)
	lseek(s, pos, SEEK_POS)
	
	fork()
	in = s, out = d
	exec(dd bs=BUFSIZ)
}

int
main(int argc, char *argv[])
{
	append(post, orig.st_size, post.edit)
	exec(cp post.edit post.orig)

	link(post, post.inflight)
	rename(post.edit, post)

	wait_until_closed(post.inflight)

	append(post.inflight, orig.st_size, post)

	unlink(post.inflight)
	
	return 0;
}
