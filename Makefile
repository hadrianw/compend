all: www/cgi-bin/comment

www/cgi-bin:
	mkdir -p "$@"

www/cgi-bin/comment: compend www/cgi-bin
	cp $< $@

compend: compend.c
	gcc -Wall -Wextra -Wno-sign-compare -pedantic $< -o $@

httpd:
	busybox httpd -p 8080 -h www -f

.PHONY: httpd all
