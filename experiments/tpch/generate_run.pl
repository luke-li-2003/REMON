#!/usr/bin/perl
use strict;
use warnings;
use File::Basename;

my $raw_dir = `readlink -f $ARGV[0]`; chomp $raw_dir;

print "Processing $raw_dir\n";

my @tbl_files = `readlink -f $raw_dir/*.tbl`; chomp @tbl_files;
#my $query_file = `readlink -f $raw_dir/query.sql`; chomp $query_file;

print "Removing trailing |\n";

foreach my $file (@tbl_files) {
	my $pid = fork;
	die "Error in fork: $!" unless defined $pid;

    	if ($pid == 0) {
		open(FH, '<', $file) or die $!;	
		open(RH, '>', "$file.tmp") or die $!;
		while(<FH>) {
			if($_=~s/(.*)\|$//g) {
				print RH "$1\n";
			} else {
				chomp $_;
				print RH "$_\n";
			}
		}
		close(FH);
		close(RH);
		`mv $file.tmp $file`;
		exit;
	}
}

for (1 .. scalar @tbl_files) {
    wait();
}

`cp dss.ddl $raw_dir/schema.sql`;
`touch $raw_dir/load.sql`;
open(FH, '>>', "$raw_dir/load.sql") or die $!;
foreach my $file (@tbl_files) {
	my $label = basename($file); $label =~ s/\..*//g; $label = uc($label);
	print FH "COPY $label FROM \'$file\' \(DELIMITER \'\|\'\)\;\n";
}
#`cat $query_file >> $raw_dir/load.sql`;
close(FH);
