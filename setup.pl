#!/usr/bin/perl
use strict;
use warnings;
use Cwd qw(abs_path);
use File::Path qw(make_path remove_tree);

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

sub command_exists {
	my ($cmd) = @_;
	return system("command -v $cmd >/dev/null 2>&1") == 0;
}

sub parse_cmake_cache {
	my ($cache_path) = @_;
	my ($cache_src, $cache_build);
	open my $fh, "<", $cache_path or return;
	while (my $line = <$fh>) {
		if ($line =~ /^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$/) {
			$cache_src = $1;
		} elsif ($line =~ /^CMAKE_CACHEFILE_DIR:INTERNAL=(.*)$/) {
			$cache_build = $1;
		}
	}
	close $fh;
	return ($cache_src, $cache_build);
}

sub ensure_clean_build_dir {
	my ($src_dir, $build_dir) = @_;
	my $cache_path = "$build_dir/CMakeCache.txt";
	if (-e $cache_path) {
		my ($cache_src, $cache_build) = parse_cmake_cache($cache_path);
		my $abs_src = abs_path($src_dir);
		my $abs_build = abs_path($build_dir);
		if ((defined $cache_src && defined $abs_src && $cache_src ne $abs_src) ||
			(defined $cache_build && defined $abs_build && $cache_build ne $abs_build)) {
			print "==> Removing stale build directory: $build_dir\n";
			remove_tree($build_dir) or fail("Failed to remove $build_dir");
		}
	}
	if (!-d $build_dir) {
		make_path($build_dir) or fail("Failed to create $build_dir");
	}
}

sub get_cmake_version {
	my $out = `cmake --version 2>/dev/null`;
	if ($? != 0 || $out !~ /^cmake version (\d+\.\d+\.\d+)/m) {
		fail("cmake not found or unable to determine version");
	}
	return $1;
}

sub version_ge {
	my ($have, $need) = @_;
	my @h = split /\./, $have;
	my @n = split /\./, $need;
	for my $i (0..2) {
		$h[$i] //= 0;
		$n[$i] //= 0;
		return 1 if $h[$i] > $n[$i];
		return 0 if $h[$i] < $n[$i];
	}
	return 1;
}

sub cpu_jobs {
	my $jobs = `nproc 2>/dev/null`;
	chomp $jobs;
	if (!$jobs) {
		$jobs = `getconf _NPROCESSORS_ONLN 2>/dev/null`;
		chomp $jobs;
	}
	if (!$jobs || $jobs !~ /^\d+$/) {
		$jobs = 1;
	}
	return $jobs;
}

print "Install packages\n";
if (!command_exists("apt-get")) {
	fail("apt-get not found; install dependencies manually: cmake libssl-dev valgrind libtbb-dev build-essential git libfuse2 htop net-tools");
}
run_cmd("sudo apt-get update", "Update package lists");
run_cmd("sudo apt-get install cmake libssl-dev valgrind libtbb-dev build-essential git libfuse2 htop net-tools -y", "Install packages");

my $cmake_version = get_cmake_version();
if (!version_ge($cmake_version, "3.16.3")) {
	fail("CMake 3.16.3 or higher is required. Found $cmake_version.");
}

my $jobs = cpu_jobs();

# Updated paths after directory restructuring
my @cmake_files = ("./compute_node/CMakeLists.txt", "./tests/CMakeLists.txt", "./duckdb/CMakeLists.txt", "./memory_node/CMakeLists.txt");

foreach my $cmake_file (@cmake_files) {
	my $src_dir = $cmake_file;
	$src_dir =~ s/\/CMakeLists\.txt//g;
	my $build_dir = "$src_dir/build-release";
	print "==> $build_dir\n";
	ensure_clean_build_dir($src_dir, $build_dir);

	my $cmake_flags = "-DCMAKE_BUILD_TYPE=Release";
	if ($cmake_file =~ /duckdb/) {
		$cmake_flags .= " -DBUILD_PARQUET_EXTENSION=ON -DBUILD_TPCH_EXTENSION=ON";
	}

	run_cmd("cmake -S \"$src_dir\" -B \"$build_dir\" $cmake_flags", "Configure $src_dir");
	run_cmd("cmake --build \"$build_dir\" -- -j $jobs", "Build $src_dir");
}
