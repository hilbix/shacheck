# love make
#
# This Works is placed under the terms of the Copyright Less License,
# see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.

CFLAGS=-Wall -O3

.PHONY: all
all:	shacheck

.PHONY: data
data:	shacheck
	bash -c 'exec ./shacheck data/ create <(7za x -so ../pwned-passwords-1.0.txt.7z) <(7za x -so ../pwned-passwords-update-1.txt.7z)'

.PHONY: gdb
gdb:	shacheck
	bash -c 'exec gdb --args ./shacheck data/ create <(7za x -so ../pwned-passwords-1.0.txt.7z) <(7za x -so ../pwned-passwords-update-1.txt.7z)'

.PHONY: verify
verify:
	bash -c 'exec ./shacheck data/ dump | comm -3 - <(sort -m <(7za x -so ../pwned-passwords-1.0.txt.7z) <(7za x -so ../pwned-passwords-update-1.txt.7z) | tr -d \\r)'
