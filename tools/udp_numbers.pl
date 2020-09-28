#!/usr/bin/perl
#

use warnings;
use v5.16.0;
use List::Util qw/sum/;

die "Need to be root to set kernel max recv buffer size" unless $> == 0;
$SIG{CHLD} = 'IGNORE';

my $result_file_name = shift or die "Usage: sudo $0 <output>";
open my $ofh, '>', $result_file_name or die "$result_file_name: $!";

my @buffers = get_buf_sizes();

# just use one port for now - focus on how rps affects packet drops
my @rps   = qw/300000 400000 500000/;

for my $buf (@buffers) {
        for my $rps (@rps) {
            test_udp( $buf, $rps );
        }
}

close $ofh;

sub test_udp {
    my ( $buf, $rps ) = @_;

    set_rmem_max($buf);
    my $pid = fork();
    if ( $pid == 0 ) {    # child
        exec("./bazel-bin/spectatord_main");
    }
    elsif ( $pid > 0 ) {

        my @dropped;
        my $prev_dropped = 0;
        for ( 1 .. 4 ) {
            system("./bazel-bin/metrics_gen -u -r $rps");

            # get number of dropped packets
            my $sum_dropped = get_dropped();
            my $dropped     = $sum_dropped - $prev_dropped;
            $prev_dropped = $sum_dropped;

            say STDERR
              "Run #$_ for [$rps, $buf] = $dropped";
            push @dropped, $dropped;
            sleep 1;
        }

        # get the average for the runs
        my $dropped = avg(@dropped);

        say STDERR "Avg for [$rps, $buf] = $dropped (@dropped)";
        say $ofh "UDP,$rps,$buf,$dropped";
        kill 'KILL' => $pid;
        sleep(5);
        unlink '/run/spectatord/spectatord.unix';
    }
}

sub get_buf_sizes {
    my @mb = qw/16 128/;
    return map { $_ * 1024 * 1024 } @mb;
}

sub get_dropped {
    open my $fh, '<', '/proc/net/udp';
    chomp( my @lines = <$fh> );
    close $fh;
    shift @lines;

    my $dropped = 0;
    for my $line (@lines) {
        $line =~ s/^\s+//;
        my @fields = split /[\s:]+/, $line;
        my $port   = hex( $fields[2] );
        next if $port < 1234 || $port > 1250;
        $dropped += $fields[-1];
    }
    $dropped;
}

sub avg {
    if ( @_ > 0 ) {
        sum(@_) / @_;
    }
    else {
        0;
    }
}

sub set_rmem_max {
    my $bufsiz    = shift;
    my $file_name = '/proc/sys/net/core/rmem_max';

    say STDERR "Setting rmem_max=$bufsiz";
    open my $proc_fh, '>', $file_name or die "$file_name: $!";
    print $proc_fh $bufsiz;
    close $proc_fh;

    # ensure we get the right bufsiz
    open $proc_fh, '<', $file_name or die "$file_name: $!";
    chomp( my $line = <$proc_fh> );
    close $proc_fh;
    die "$line != $bufsiz" unless $line == $bufsiz;
}

