Null Utility
============

## Documentation

Sorry for the limited documentation here.  Basically null is a
combination of /dev/null, tee, md5sum, with the addition of some other
features.

## Arguments

Here are more details on some of the less obvious flags.

* [-d size]         or --dot-blocks          show a dot each X bytes of input

	With this size, you can have null output a period ('.') to standard
	error for every X bytes read in.  You can specify the size as 20k or
	100m.

* [-f output-file]  or --output-file         output file(s) to write input

	You can write any input bytes into an output file by using this
	option.  To handle multiple files, specify multiple -f options.

*  [-F]              or --flush-output        flush output to files

	This will cause null to call fflush on each of the output streams
	after it writes to them.

* [-m]              or --md5                 run input bytes through md5

	This will display the md5 signature for the input data.  If you are
	transporting data across a stream, it is useful to use the -r and -w
	options.  See below.

* [-n]              or --non-block           don't block on input

	Set the input file-descriptor to be non-blocking.  Not sure if this
	really accomplishes anything.

* [-p]              or --pass-input          pass input data to output

	This will write the input to the standard output.

*  [-r]              or --read-pagination     read pagination data

	Null can add basic pagination information into the stream.  Network
	transmissions often block the input and output to fixed sizes and add
	`\0` characters at the end as padding.  With pagination, these
	extraneous characters will be removed and so MD5 calculations (with
	-m) will be valid.  This should be used to read the output of null
	with a -w flag specified.

* [-t size]         or --throttle-size       throttle output to X bytes / sec

	This will throttle the output of null to a specific size (10k or 1m)
	per second.  This is useful if you don't want to overflow a network
	connection for instance.

* [-w]              or --write-pagination    write paginate data

	Like -r but this should be used to write output to a null with a -r
	flag specified.

## Examples

To write output to multiple log files:

	tar -cvzf - /usr | \
		null -f /backup/usr.tar.gz -f /backup2/usr.tar.gz

To transmit some files between two systems with pagination and while
reporting the md5 signature of the data.  On the remote host:

	nc -l -p 5000 | null -r -p -m | tar -xf -
On the local host:
	tar -cf - . | null -w -p -m | nc remote-hostname 5000

To see a dot ('.') for every megabyte of byte we are getting out of
our backup program.

	tar -cf - . | null -d 1m -p -f /dev/nrst0

To limit the transfer of a hierarchy to a remote system to 100kB/sec
and print a dot for every 10kB.

	tar -cf - . | null -d 10k -t 100k -p | nc remote-hostname 5000

## Repository

The newest versions of the library are available via: https://github.com/j256/null

Enjoy.  Gray Watson
