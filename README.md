# SHAcheck

Fast search trough SHA databases.

This was quickly assembled, and may contain serious bugs.


## Usage

    git clone https://github.com/hilbix/shacheck.git
    cd shacheck
    make

Download a SHA database, like from haveibeenpwned.com and extract it (see `make data`):

    mkdir "$(readlink -m data)"
    ./shacheck data/ create <(7za x -so ../pwned-passwords-1.0.txt.7z) <(7za x -so ../pwned-passwords-update-1.txt.7z)

Then check passwords:

    echo -n password | sha1sum | ./shacheck data/ check

or

    ./shacheck data/ check SHA..


Notes:

- Return 0 means: At least one SHA was found.  Return 2 means: None of the SHAs were found.  Everything else: You cannot be sure if a SHA was found or not.

- `make verify` checks, that the hashes, which were created with `make data`, indeed can re-create the source information.


## FAQ

I found a bug

- Please open issue@GitHub

I want to contact you

- Instead you can try to open an issue@GitHub.
- Eventually I listen.
- I do not read mails.

This can be improved

- You bet.  It was done in a hurry.

`pwned-password*.txt.7z` are missing

- They are from https://haveibeenpwned.com/Passwords
- Please note that these files are very big, please do not unneccessarily download them.
- I am not afiliated with haveibeenpwned nor Troy Hunt.  But I highly appreciate his work!  Thank you very much!


## License

This Works is placed under the terms of the Copyright Less License,
see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.

