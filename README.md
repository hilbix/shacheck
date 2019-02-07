[![SHAcheck Build Status](https://api.cirrus-ci.com/github/hilbix/shacheck.svg?branch=master)](https://cirrus-ci.com/github/hilbix/shacheck/master)

> - New file format (2 or 3), not compatible to the previous variant
> - ZMQ for easy service integration
> - Hot updating (`create`) supported while data in use

# SHAcheck

Fast search trough SHA databases in the format used by haveibeenpwned.com.

This was quickly assembled.  It works.


## Usage

    git clone https://github.com/hilbix/shacheck.git
    cd shacheck
    make

Download a SHA database.  It must be sorted by SHA.  The format must be similar to the one found at haveibeenpwned.com.
Then you can prepare the `data/` directory like this:

    mkdir "$(readlink -m data)"
    ./shacheck data/ create <(7za x -so sample/pwned-passwords-1.0.txt.7z) <(7za x -so sample/pwned-passwords-update-1.txt.7z)

Afterwards you can check passwords:

    echo -n password | sha1sum | ./shacheck data/ check

or

    ./shacheck data/ check SHA..

To dump the SHAs to stdout use

    ./shacheck data/ dump [from [to]]

To run some ZMQ service use

    ./shacheck data/ zmq [ZMQsocket]

A sample program to use this service is

    ./zmqpwcheck [ZMQsocket]

Use it like this:

    ./shacheck data/ zmq &
    ./zmqpwcheck
    # feed passwords or phrases on stdin

Example:

    ./zmqpwcheck </usr/share/dict/american-english

Notes:

- Return 0 of `shacheck` means: At least one SHA was found.  Return 2 means: None of the SHAs were found.  Everything else: You cannot be sure if a SHA was found or not.
- For the very simple ZMQ request protocol to check for passwords, see `zmqpwcheck.c` function `ZMQ_pwcheck`.
- Some numbers on V6data extracted to disk in shacheck file format 2:
  - It takes 65536 files, 257 directories on disk.  (Counts are NOT included.)
  - Those need a little bit more than 10 GiB (which is a bit **less** than the compressed download with the counts!).
  - On moderate hardware (no SSD) you can expect to be able to check 25 passwords/s via the ZMQ adapter.
  - This are more than 2 million passwords per day.
  - This gives 100 TPS on-disk with a sustained read rate of 8 MiB/s
  - Hence with SSD or RAM cache you can check a much bigger number, I did not test.

- `make data` takes the INPUTS (see `Makefile`, they are expected in the directory `sample/`) and extracts them into `data/`.
- `make verify` checks, that the hashes, which were created with `make data`, indeed can re-create the source information (without counts or additional information, though).
- `make check` then allows to check for passwords which are given on stdin
- `make brute` is a little brute force check which makes sure, that all given SHAs are found


## FAQ

I found a bug

- Please open issue@GitHub

I want to contact you

- Instead you can try to open an issue@GitHub.
- Eventually I listen.
- I do not read mails.

This can be improved

- You bet.  It was done in a hurry.

`sample/pwned-password*.txt.7z` are missing

- The latest SHA database can be found at https://haveibeenpwned.com/Passwords
- Please note that these files are very big, please do not unneccessarily download them.
  - Instead please use the [Torrent](https://www.troyhunt.com/heres-1-4-billion-records-from-have-i-been-pwned-for-you-to-analyse/) and be sure to give back the bandwidth.
- I am not afiliated with haveibeenpwned nor Troy Hunt.  But I highly appreciate his work!  Thank you very much!


## License

This Works is placed under the terms of the Copyright Less License,
see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.

