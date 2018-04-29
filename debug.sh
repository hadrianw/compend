#!/bin/sh
set -e

name="$1"
msg="$2"

request="name=$name
msg=$msg
"

cd www/cgi-bin
echo "$request" | \
PATH_INFO=/hello.html \
CONTENT_TYPE="text/plain" \
CONTENT_LENGTH=$(echo "$request" | wc -c) \
strace ./comment
