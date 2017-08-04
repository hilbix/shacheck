*not ready yet*

# SHAcheck

Fast search trough SHA databases.

This was quickly assembled, and may contain serious bugs.

## Usage

    git clone https://github.com/hilbix/shacheck.git
    cd shacheck
    make

Download a SHA database, like from haveibeenpwned.com and extract it:

    mkdir "$(readlink -e data)"
    ./shacheck data/ create <(7za x -so ../pwned-passwords-1.0.txt.7z) <(7za x -so ../pwned-passwords-update-1.txt.7z)

Then check passwords:

    echo -n password | sha1sum | ./shacheck data/ check

or

    ./shacheck data/ check SHA..


## FAQ

I found a bug

- Please open issue@GitHub

I want to contact you

- Please open issue@GitHub

This can be improved

- You bet.  It was done in a hurry.


## License

This Works is placed under the terms of the Copyright Less License,
see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.

