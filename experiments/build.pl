#!/usr/bin/perl
use strict;
use warnings;

sub fail {
	my ($msg) = @_;
	print STDERR "ERROR: $msg\n";
	exit 1;
}

sub run_cmd {
	my ($cmd, $label) = @_;
	print "==> $label\n" if defined $label;
	system($cmd);
	if ($? != 0) {
		my $exit_code = $? >> 8;
		fail(($label // "Command") . " failed (exit $exit_code): $cmd");
	}
}

print ">>> Building experiment executable...\n\n";

run_cmd("rm -rf build", "Clean build directory");
run_cmd("mkdir -p build", "Create build directory");
run_cmd("cd build && cmake -DCMAKE_BUILD_TYPE=Release ../", "Configure experiments");
run_cmd("cd build && make -j\$(nproc 2>/dev/null || sysctl -n hw.ncpu)", "Build experiments");
run_cmd("cd build && mkdir -p remon_swap", "Prepare remon_swap directory");

print "Build complete. Run ./run.pl <logdir> to execute experiments.\n";
