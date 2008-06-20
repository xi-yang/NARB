#
#	Copyright (c) 2007 Linux Kinetics, LLC
#	All rights reserved.
#
#    Redistribution and use in source and binary forms are permitted
#    provided that the above copyright notice and this paragraph are
#    duplicated in all such forms and that any documentation, advertising
#    materials, and other materials related to such distribution and use
#    acknowledge that the software was developed by the University of
#    Southern California, Information Sciences Institute.  The name of the
#    University may not be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
#    THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED
#    WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
#    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
# Author: Jaroslav Flidr
# June 1, 2008
#
# File: Client.pm
#

package GMPLS::Client;

use strict;
use warnings;
use Socket;
use GMPLS::Constants;
use Aux;

BEGIN {
	use Exporter   ();
	our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
	$VERSION = sprintf "%d.%03d", q$Revision: 1.1 $ =~ /(\d+)/g;
	@ISA         = qw(Exporter);
	@EXPORT      = qw();
	%EXPORT_TAGS = ();
	@EXPORT_OK   = qw();
}
our @EXPORT_OK;

sub new {
	shift;
	my ($n, $q)  = @_;
	my $self = {
		name => $n,
		queue => $q
	};
	bless $self;
	return $self;
}

sub run() {
	my $self = shift;
	my $d;
	Aux::print_dbg_run("starting (%s) client queue\n", $$self{name});
	while(!$::ctrlC) {
		# this blocking queue is being controlled from WS server
		$d = $$self{queue}->dequeue();
		if(defined($d)) {
			if($$d{cmd} == CTRL_CMD) {
				if($$d{type} == TERM_T_T) {
					last;
				}
			}
		}
	}
	Aux::print_dbg_run("exiting %s client queue\n", $$self{name});
}

1;