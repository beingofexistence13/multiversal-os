# The client writes a message to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd does not pass it via IPv6 UDP to the IPv4 loghost.
# Find the message in client, file, pipe, syslogd log.
# Check that the syslogd logs the error.

use strict;
use warnings;

our %args = (
    syslogd => {
	loghost => '@udp6://127.0.0.1',
	loggrep => {
	    qr/syslogd\[\d+\]: bad hostname "\@udp6:\/\/127.0.0.1"/ => '>=1',
	    get_testgrep() => 1,
	},
    },
    server => {
	noserver => 1,
    },
);

1;
