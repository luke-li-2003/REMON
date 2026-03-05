#!/usr/bin/perl
use strict;
use warnings;

if (@ARGV != 2) {
    die "Usage: $0 <data_set_directory_name> <scale_factor>\n";
}

my $data_set_dir = $ARGV[0];
my $scale_factor = $ARGV[1];

print "TPC-H data generation: creating $data_set_dir with scale factor $scale_factor\n";
print "This will generate approximately " . ($scale_factor) . " GB of data.\n\n";



`cd tpch/; 
sh ./how_to_build.sh; 
export DSS_QUERY=./queries; 
mkdir $data_set_dir
./dbgen -s $scale_factor; 
./qgen > $data_set_dir/query.sql; 
mv *tbl $data_set_dir; 
./generate_run.pl $data_set_dir;
touch $data_set_dir/run.sql;`;

# Define file paths
my $schema_file = "tpch/$data_set_dir/schema.sql";
my $load_file = "tpch/$data_set_dir/load.sql";
my $query_file = "tpch/$data_set_dir/query.sql";
my $run_file = "tpch/$data_set_dir/run.sql";

# Open files and concatenate their contents into run.sql
open my $run_fh, '>', $run_file or die "Could not open '$run_file' $!";

foreach my $file ($schema_file, $load_file, $query_file) {
    open my $fh, '<', $file or die "Could not open '$file' $!";
    while (my $line = <$fh>) {
        print $run_fh $line;
    }
    close $fh;
}

close $run_fh;
