#!/usr/bin/perl -T
use warnings;
use strict;


# This is used for the usage output
=pod

=head1 SYNOPSIS

verify-pacman-repo-db.pl [options] <database file> ...

 Options:
   --help, -h         Show short help message
   --debug            Enable debug output
   --checksum, -c     Verify checksums of packages
   --thread, -t <num> Use num threads to verify packages. Default: 1
                      NOTE: Each thread uses up to approx. 128MiB of memory

=cut

package main;
use Getopt::Long;
use Pod::Usage;

exit main();

sub main {
	my %opts = (
		threads => 1,
	);

	Getopt::Long::Configure ("bundling");
	pod2usage(-verbose => 0) if (@ARGV== 0);
	GetOptions(\%opts, "help|h", "debug", "threads|t=i", "checksum|c") or pod2usage(2);
	pod2usage(0) if $opts{help};

	my $verifier = Verifier->new(\%opts);

	for my $repodb (@ARGV) {
		$verifier->check_repodb($repodb);
	}

	$verifier->finalize();
	return $verifier->get_error_status();
}

package Verifier;
use Archive::Tar;
use Digest::MD5;
use Digest::SHA;
use File::Basename;
use threads;
use threads::shared;
use Thread::Queue;

sub new {
	my $class = shift;
	my $opts = shift;

	my $self :shared = shared_clone({
		opts => \%{$opts},
		package_queue => Thread::Queue->new(),
		output_queue => Thread::Queue->new(),
		workers => [],
		errors => 0,
	});

	bless $self, $class;
	$self->start_workers();
	return $self;
}

sub start_workers {
	my $self = shift;

	threads->new(\&_worker_output_queue, $self);

	for (my $i = 0; $i < $self->{opts}->{threads}; $i++) {
		my $thr :shared = shared_clone(threads->new(\&_worker_package_queue, $self));
		push @{$self->{workers}}, $thr;
	}
}

sub _worker_package_queue {
	my $self = shift;
	while (my $workpack = $self->{package_queue}->dequeue()) {
		my $dbdata = $self->_parse_db_entry($workpack->{db_desc_content});
		$self->{errors} += $self->_verify_db_entry($workpack->{dirname}, $dbdata);
	}
}

sub _worker_output_queue {
	my $self = shift;
	while (my $output = $self->{output_queue}->dequeue()) {
		print STDERR $output;
	}
}

sub finalize {
	my $self = shift;

	$self->{package_queue}->end();
	$self->_join_threads($self->{workers});

	$self->{output_queue}->end();
	$self->_join_threads([threads->list]);
}

sub _join_threads {
	my $self = shift;
	my $threads = shift;

	for my $thr (@{$threads}) {
		if ($thr->tid && !threads::equal($thr, threads->self)) {
			print "waiting for thread ".$thr->tid()." to finish\n" if $self->{opts}->{debug};
			$thr->join;
		}
	}
}

sub get_error_status {
	my $self = shift;

	return $self->{errors} > 0;
}

sub check_repodb {
	my $self = shift;
	my $repodb = shift;

	my $db = Archive::Tar->new();
	$db->read($repodb);

	my $dirname = dirname($repodb);
	my $pkgcount = 0;

	my @files = $db->list_files();
	for my $file_object ($db->get_files()) {
		if ($file_object->name =~ m/^([^\/]+)\/desc$/) {
			my $package = $1;
			$self->{package_queue}->enqueue({
					package => $package,
					db_desc_content => $file_object->get_content(),
					dirname => $dirname,
				});
			$pkgcount++;
		}
	}

	$self->_debug(sprintf("Queued %d package(s) from database '%s'\n", $pkgcount, $repodb));
}

sub _parse_db_entry {
	my $self = shift;
	my $content = shift;
	my %db;
	my $key;

	for my $line (split /\n/, $content) {
		if ($line eq '') {
			$key = undef;
		} elsif ($key) {
			push @{$db{$key}}, $line;
		} elsif ($line =~ m/^%(.+)%$/) {
			$key = $1;
		} else {
			die "\$key not set. Is the db formatted incorrectly?" unless $key;
		}
	}
	return \%db;
}

sub _output {
	my $self = shift;
	my $output = shift;

	return if $output eq "";

	$output = sprintf("Thread %s: %s", threads->self->tid(), $output);
	$self->{output_queue}->enqueue($output);
}

sub _debug {
	my $self = shift;
	my $output = shift;
	$self->_output($output) if $self->{opts}->{debug};
}

sub _verify_db_entry {
	my $self = shift;
	my $basedir = shift;
	my $dbdata = shift;
	my $ret = 0;
	my $output = "";

	# verify package exists
	my $pkgfile = $basedir.'/'.$dbdata->{FILENAME}[0];
	$self->_debug(sprintf("Checking package %s\n", $dbdata->{FILENAME}[0]));
	unless (-e $pkgfile) {
		$self->_output(sprintf("Package file missing: %s\n", $pkgfile));
		return 1;
	}

	$ret += $self->_verify_package_size($dbdata, $pkgfile);
	$ret += $self->_verify_package_checksum($dbdata, $pkgfile) if $self->{opts}->{checksum};

	return $ret;
}

sub _verify_package_size {
	my $self = shift;
	my $dbdata = shift;
	my $pkgfile = shift;

	my $csize = $dbdata->{CSIZE}[0];
	my $filesize = (stat($pkgfile))[7];
	unless ($csize == $filesize) {
		$self->_output(sprintf("Package file has incorrect size: %d vs %d: %s\n", $csize, $filesize, $pkgfile));
		return 1;
	}
	return 0;
}

sub _verify_package_checksum {
	my $self = shift;
	my $dbdata = shift;
	my $pkgfile = shift;

	my $md5 = Digest::MD5->new;
	my $sha = Digest::SHA->new(256);

	my $content;
	# 128MiB to keep random IO low when using multiple threads (only works for large packages though)
	my $chunksize = 1024*1024*128;
	open my $fh, "<", $pkgfile;
	while (read($fh, $content, $chunksize)) {
		$md5->add($content);
		$sha->add($content);
	}

	my $expected_sha = $dbdata->{SHA256SUM}[0];
	my $expected_md5 = $dbdata->{MD5SUM}[0];
	my $got_md5 = $md5->hexdigest;
	my $got_sha = $sha->hexdigest;

	unless ($expected_sha eq $got_sha and $expected_md5 eq $got_md5) {
		my $output;
		$output .= sprintf "Package file has incorrect checksum: %s\n", $pkgfile;
		$output .= sprintf "expected: SHA %s\n", $expected_sha;
		$output .= sprintf "got:      SHA %s\n", $got_sha;
		$output .= sprintf "expected: MD5 %s\n", $expected_md5;
		$output .= sprintf "got:      MD5 %s\n", $got_md5;
		$self->_output($output);
		return 1;
	}
	return 0;
}

