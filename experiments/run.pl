#!/usr/bin/perl
use strict;
use warnings;
use File::Copy qw(copy);
use File::Path qw(make_path);

my $rlimit_available = eval { require BSD::Resource; 1; };
my $rlimit_error = $@;
my $use_shell_ulimit = 0;

# Example run script for TPC-H experiments
# Usage: ./run.pl <logdir>
# Results will be saved to ./results/<logdir>/

sub fail {
  my ($msg) = @_;
  print STDERR "ERROR: $msg\n";
  exit 1;
}

sub format_cmd {
  my (@cmd) = @_;
  my @parts = map { /\s/ ? "'$_'" : $_ } @cmd;
  return join(" ", @parts);
}

sub shell_quote {
  my ($s) = @_;
  $s =~ s/'/'"'"'/g;
  return "'$s'";
}

sub run_cmd {
  my ($label, @cmd) = @_;
  my $use_ulimit = 0;
  if (@cmd && ref($cmd[0]) eq "HASH") {
    my $opts = shift @cmd;
    $use_ulimit = $opts->{ulimit} ? 1 : 0;
  }
  print "==> $label\n" if defined $label;
  if ($use_ulimit && $use_shell_ulimit) {
    my $limit = $use_shell_ulimit;
    my $cmd_str = join(" ", map { shell_quote($_) } @cmd);
    my $shell_cmd = "ulimit -n $limit || echo \"WARNING: unable to raise NOFILE to $limit\"; exec $cmd_str";
    system("sh", "-c", $shell_cmd);
  } else {
    system(@cmd);
  }
  if ($? == -1) {
    fail(($label // "Command") . " failed to execute: $!");
  } elsif ($? & 127) {
    my $signal = ($? & 127);
    my $core = ($? & 128) ? " (core dumped)" : "";
    fail(($label // "Command") . " died with signal $signal$core: " . format_cmd(@cmd));
  } elsif (($? >> 8) != 0) {
    my $exit_code = $? >> 8;
    fail(($label // "Command") . " failed (exit $exit_code): " . format_cmd(@cmd));
  }
}

sub raise_nofile_limit {
  my ($target) = @_;
  if (!$rlimit_available) {
    $use_shell_ulimit = $target if defined $target;
    if ($use_shell_ulimit) {
      print STDERR "WARNING: BSD::Resource not available; will try 'ulimit -n $use_shell_ulimit' via shell for the experiment.\n";
    } else {
      print STDERR "WARNING: BSD::Resource not available; cannot raise NOFILE limit. Set it with 'ulimit -n' before running.\n";
    }
    return;
  }

  my $rlimit_nofile = BSD::Resource::RLIMIT_NOFILE();
  my $rlim_infinity = BSD::Resource::RLIM_INFINITY();
  my ($soft, $hard) = BSD::Resource::getrlimit($rlimit_nofile);
  my $soft_str = ($soft == $rlim_infinity) ? "infinity" : $soft;
  my $hard_str = ($hard == $rlim_infinity) ? "infinity" : $hard;
  print "==> NOFILE limit (soft/hard): $soft_str/$hard_str\n";

  return unless defined $target;

  my $desired = $target;
  if ($hard != $rlim_infinity && $desired > $hard) {
    $desired = $hard;
  }
  if ($desired <= $soft) {
    return;
  }

  my $rc = BSD::Resource::setrlimit($rlimit_nofile, $desired, $hard);
  if (!defined $rc || $rc != 0) {
    print STDERR "WARNING: Unable to raise NOFILE limit to $desired\n";
    return;
  }
  my ($new_soft, $new_hard) = BSD::Resource::getrlimit($rlimit_nofile);
  my $new_soft_str = ($new_soft == $rlim_infinity) ? "infinity" : $new_soft;
  my $new_hard_str = ($new_hard == $rlim_infinity) ? "infinity" : $new_hard;
  print "==> NOFILE limit updated to (soft/hard): $new_soft_str/$new_hard_str\n";
}

sub check_file {
  my ($path) = @_;
  if (!-e $path) {
    fail("Missing file: $path");
  }
  if (!-r $path) {
    fail("File not readable: $path");
  }
  if (-z $path) {
    fail("File is empty: $path");
  }
}

if (@ARGV != 1) {
  die "Usage: $0 <logdir>\n";
}

my $logdir = $ARGV[0];
my $expName = "tpchindi_1SF";
my $exp_bin = "build/experiment";
my $swap_dir = "build/remon_swap";
my $results_dir = "results";
my $log_results_dir = "$results_dir/$logdir";
my $data_dir = "./tpch/testset_1";
my $query_dir = "./tpch/queries";
my $config_src = "default.config";
my $config_dst = "build/remon.config";
my $log_data = "Local memory only, 4GB RAM limit, SF=1 dataset";

my $nofile_target = $ENV{REMON_NOFILE};
if (!defined $nofile_target) {
  $nofile_target = 1048576;
}
raise_nofile_limit($nofile_target);

eval { make_path($swap_dir, $results_dir, $log_results_dir); };
if ($@) {
  fail("Failed to create directories: $@");
}

if (!-x $exp_bin) {
  fail("Missing $exp_bin (run build.pl first)");
}
if (!-d $data_dir) {
  fail("Missing data directory: $data_dir");
}
if (!-d $query_dir) {
  fail("Missing query directory: $query_dir");
}
check_file("$data_dir/schema.sql");
check_file("$data_dir/load.sql");
for my $i (1..22) {
  check_file("$query_dir/$i.sql");
}
if (!-e $config_src) {
  fail("Missing config file: $config_src");
}
if (!copy($config_src, $config_dst)) {
  fail("Failed to copy $config_src to $config_dst: $!");
}

print "\n>>> Running TPC-H individual query test (SF=1, 4GB RAM limit)\n---------------\n";

# -dataDir: path to generated TPC-H data (schema.sql, load.sql, *.tbl files)
# -queryDir: path to query templates (1.sql through 22.sql)
# -logDir: subdirectory name for results (created under ./results/)
# -logName: output file name (without .txt extension)
# -logData: description string for the log file
run_cmd(
  "Run $expName",
  { ulimit => 1 },
  $exp_bin,
  "tpch_individual",
  "-dataDir=$data_dir/",
  "-queryDir=$query_dir/",
  "-logDir=$logdir",
  "-logName=$expName",
  "-logData=$log_data"
);

print "Finished $expName\n";
