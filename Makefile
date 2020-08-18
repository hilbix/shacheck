# love make
#
# This Works is placed under the terms of the Copyright Less License,
# see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.

APPS=shacheck zmqpwcheck zmqshacheck
CFLAGS=-Wall -Wno-unused-function -O3 -DSHACHECK_WITH_ZMQ
LDLIBS=-lzmq -lcrypto

DATA=data
SAMPLE=sample

#INPUTS=$(SAMPLE)/pwned-passwords-1.0.txt.7z $(SAMPLE)/pwned-passwords-update-1.txt.7z
#INPUTS=$(SAMPLE)/pwned-passwords-sha1-ordered-by-hash-v4.7z
INPUTS=$(SAMPLE)/pwned-passwords-sha1-ordered-by-hash-v6.7z

.PHONY: all
all:	$(APPS)

.PHONY: data
data:	shacheck sample
	bash -xc 'f() { [ 0 = "$${#ARGS[@]}" ] && exec ./shacheck "$(DATA)" create "$$@"; nx="$${ARGS[0]}"; ARGS=("$${ARGS[@]:1}"); f "$$@" <(7za x -so "$$nx"); }; ARGS=("$$@"); f' . $(INPUTS)

.PHONY:	sample
sample:	$(INPUTS)

$(APPS):	oops.h zmqshacheck.h

$(INPUTS):
	@echo '!!!'
	@echo '!!! Please download $@ from https://haveibeenpwned.com/Passwords'
	@echo '!!!'
	false

.PHONY: gdb
gdb:	shacheck sample
	bash -c 'f() { [ 0 = "$${#ARGS[@]}" ] && exec gdb --args ./shacheck "$(DATA)" create "$$@"; nx="$${ARGS[0]}"; ARGS=("$${ARGS[@]:1}"); f "$$@" <(7za x -so "$$nx"); }; ARGS=("$$@"); f' . $(INPUTS)

# needs `make data` before
.PHONY: verify
verify:	sample data/ff/ff.hash
	bash -c 'f() { [ 0 = "$${#ARGS[@]}" ] && exec sort -m "$$@"; nx="$${ARGS[0]}"; ARGS=("$${ARGS[@]:1}"); f "$$@" <(7za x -so "$$nx" | sed 's/:.*$$//'); }; ARGS=("$$@"); comm -3 <(exec ./shacheck "$(DATA)" dump) <(f)' . $(INPUTS)

# needs `make data` before
.PHONY: check
check:	data/ff/ff.hash
	while read -r pw; do echo -n "$$pw" | sha1sum - | ./shacheck "$(DATA)" check; done

# needs `make data` before
.PHONY:	brute
brute:	data/ff/ff.hash
	./shacheck "$(DATA)" dump | ./shacheck "$(DATA)" check | grep -v '^FOUND: '

clean:
	git clean -fX

debian:
	apt-get install libzmq3-dev

data/ff/ff.hash:
	@echo '!!!'
	@echo '!!! please run: make data'
	@echo '!!!'
	false

