#!/usr/bin/perl
use strict;
use JSON;

$| = 1;

use constant {
    WAITING_HEADER    => 0,
    EXPECTING_RX_NODE => 1,
    EXPECTING_TX_NODE => 2,
    NUM_NODES         => 10
};

my $state = WAITING_HEADER;
my $cnt = 0;
my $addrTo;
my %strengths;
my %gpsCache;
my $dumpGps = 1;
my $gpsFh;

# FIXME: a ugly hack for showing the results of the estimated coordinates. Sorry.
my $gps2 = {};
`touch gps.coords`;
open (my $gps2fh, "<gps.coords") or die;
while (my $line = <$gps2fh>)
{
    my ($node, $lat, $long) = split (/ /, $line);
    $gps2->{$node} = { lat => $lat, long => $long };
}


if ($dumpGps)
{
    open ($gpsFh, ">log.gps") or die;
}

my %lookup =
(
    "2bc6" => "andersson",
    "2bc5" => "brolin",
    "2bcb" => "dahlin",
    "2bd0" => "hamrin",
    "2bc8" => "larsson",
    "2bc9" => "ljungberg",
    "2bc7" => "mellberg",
    "2bcf" => "ravelli",
    "2bce" => "skoglund",
    "2bca" => "zlatan"
);
sub on_sigint
{
    print "on_sigint";
    print_strengths (1);
    exit (1);
}

$SIG{INT} = \&on_sigint;

while (my $line = <STDIN>)
{
    chop $line;
    if ($state == WAITING_HEADER)
    {
        # 09:52:15.066894 TX BEACON 2bd0 (9 nodes)
        # 09:52:13.546386 RX BEACON 2bce (9 nodes)
        if ($line =~ /(\d\d):(\d\d):(\d\d)\.(\d+) ([TR]X) BEACON (....) \((\d+) nodes\) \(gps\[(\d+)\] ([.0-9]+) ([.0-9]+) .. ([.0-9]+) ([.0-9]+)\)/)
        {
            my ($hh, $mm, $ss, $frac, $dir, $addr, $numNeighbourNodes, $numsat, $latitude, $longitude, $latint, $longint) = ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12);
            $addrTo = $addr;
            my $validFlag = $longint ? "valid" : "invalid";
            print "header $hh:$mm:$ss.$frac $dir $addr ($numNeighbourNodes) gps[$numsat] $latitude $longitude ($validFlag)\n";

            if ($validFlag eq "valid")
            {
                if (int($longitude) != 18 || int($latitude) != 59)
                {
                    die "header $hh:$mm:$ss.$frac $dir $addr ($numNeighbourNodes) $latitude $longitude $latint $longint ($validFlag)\n";
                }
                $gpsCache{$addrTo} ||= {latitude => 0, longitude => 0, gpsCount => 0, longitudeLast => 0, latitudeLast => 0, numsat => 0};
                my $obj = $gpsCache{$addrTo};
                $obj->{longitude} += $longitude;
                $obj->{latitude}  += $latitude;
                $obj->{longitudeLast} = $longitude;
                $obj->{latitudeLast}  = $latitude;
                $obj->{numsat}        = $numsat;
                $obj->{gpsCount}  += 1;
            }

            if ($numNeighbourNodes != NUM_NODES - 1)
            {
                print STDERR "ERROR: numNeighbourNodes=$numNeighbourNodes, ignoring data\n";
                next;
            }

            if ($dir eq "TX")
            {
                $state = EXPECTING_TX_NODE;
                #print "switching state to TX ($state)\n";
                $cnt = 0;
                %strengths = ();
            }
            elsif ($dir eq "RX")
            {
                $state = EXPECTING_RX_NODE;
                #print "switching state to RX ($state)\n";
                $cnt = 0;
                %strengths = ();
            }
            else
            {
                print STDERR "ERROR: dir=$dir\n";
            }
        }
    }
    elsif ($state == EXPECTING_RX_NODE || $state == EXPECTING_TX_NODE)
    {
        my ($hh, $mm, $ss, $frac, $addr, $strength, $elapsed);
        if ($state == EXPECTING_TX_NODE)
        {
            # 09:52:15.067535 >>> 2bce: -25.469999 dBm elapsed: 1.515000 s
            if ($line =~ /^(\d\d):(\d\d):(\d\d)\.(\d+) >>> (....): (-?\d+\.\d+) dBm elapsed: (\d+\.\d+) s$/)
            {
                ($hh,$mm,$ss,$frac,$addr,$strength,$elapsed) = ($1,$2,$3,$4,$5,$6,$7);
                #print "txnode $hh:$mm:$ss.$frac $addr $strength ($elapsed)\n";
            }
            else
            {
                print "ERROR: tx node regex did not match content: '$line'\n";
                $state = WAITING_HEADER;
            }
        }
        else
        {
            # 09:52:13.546997     2bd0: -24.969999 dBm
            if ($line =~ /^(\d\d):(\d\d):(\d\d)\.(\d+)     (....): (-?\d+\.\d+) dBm$/)
            {
                ($hh,$mm,$ss,$frac,$addr,$strength,$elapsed) = ($1,$2,$3,$4,$5,$6,0);
                #print "rxnode $hh:$mm:$ss.$frac $addr $strength\n";
            }
            else
            {
                print "ERROR: rx node regex did not match content: '$line'\n";
                $state = WAITING_HEADER;
            }
        }

        $strengths{$addr} = $strength;
        if (++$cnt == NUM_NODES - 1)
        {
            $state = WAITING_HEADER;
            on_data ();
        }
    }
    elsif ($state == EXPECTING_TX_NODE)
    {
    }
    else
    {
        print "unknown state $state\n";
    }
}

my %maxAbsDiffs;
my $numPackets = 0;
sub on_data
{
    $numPackets++;
    $maxAbsDiffs{$addrTo} = 0;
    #print "on_data $addrTo\n";
    for my $addrFrom (keys %strengths)
    {
        my $strength = $strengths{$addrFrom};
        #print "$addrTo <- $addrFrom: $strength\n";
        update_strength ($addrFrom, $addrTo, $strength);
    }
    print_strengths (0);
}

my %cache;
my $lastTo = "";
sub update_strength
{
    my ($from,$to,$strength,$lat,$long) = @_;
    my $key = "$from:$to";
    $cache{$key} ||= {strength => 0, count => 0};
    my $obj = $cache{$key};

    my $oldStrength = $obj->{strength};
    my $oldCount    = $obj->{count};

    $obj->{strength}  += $strength;
    $obj->{count}     += 1;

    if ($oldCount)
    {
        my $new = $obj->{strength} / $obj->{count};
        my $old = $oldStrength / $oldCount;

        my $absDiff = abs ($new - $old);
        if ($maxAbsDiffs{$to} < $absDiff)
        {
            $maxAbsDiffs{$to} = $absDiff;
        }
    }
    else
    {
        $maxAbsDiffs{$to} = 1000;
    }
    $lastTo = $to;
}

sub print_strengths
{
    my ($saveToFile) = @_;
    my $minCnt = 10000000;
    my $maxCnt = 0;
    my %m;
    for my $key (keys %cache)
    {
        my ($from,$to) = split (/:/, $key);
        $m{$from} = 1;
        $m{$to} = 1;
    }
    my @nodes = sort keys %m;

    print "      ";
    for my $from (@nodes)
    {
        print "____${from}____ ";
    }
    print "\n";

    my $fh;
    if ($saveToFile) {
        open ($fh, ">log.strengths") or die;
    }
    for my $to (@nodes)
    {

        print "$to: ";
        for my $from (@nodes)
        {
            my $key = "$from:$to";
            my $obj = $cache{$key};
            my $strength = ($obj && $obj->{count}) ? sprintf ("%+7.3f", $obj->{strength} / $obj->{count}) : "-------";
            my $cnt = sprintf ("%2d", ($obj && $obj->{count}) || 0);
            #print "$from -> $to: $strength ($cnt)\n";
            print "$strength ($cnt) ";
            if ($minCnt > $cnt && $from ne $to) {
                $minCnt = $cnt + 0;
            }
            if ($maxCnt < $cnt && $from ne $to) {
                $maxCnt = $cnt + 0;
            }
            if ($saveToFile) {
                my $gpsDist = get_node_dist ($to, $from);
                print $fh "$to $from $strength $gpsDist\n";
            }
        }
        #print " <--- last" if ($to eq $lastTo);

        my $gobj = $gpsCache{$to};
        my $latAvg  = $gobj->{gpsCount} ? $gobj->{latitude}  / $gobj->{gpsCount} : 0;
        my $longAvg = $gobj->{gpsCount} ? $gobj->{longitude} / $gobj->{gpsCount} : 0;
        print sprintf (" %0.16f %0.16f (%d)\n", $latAvg, $longAvg, $gobj->{gpsCount});

        if ($dumpGps && $gobj->{gpsCount})
        {
            my $name = $lookup{$to};
            my $clat = 0;
            my $clong = 0;
            my $c = 0;
            for my $node (@nodes)
            {
                my $nodeObj = $gpsCache{$node};
                next unless $nodeObj;
                next unless $nodeObj->{gpsCount};
                my $latAvg  = $nodeObj->{latitude}  / $nodeObj->{gpsCount};
                my $longAvg = $nodeObj->{longitude} / $nodeObj->{gpsCount};
                $clat  += $latAvg;
                $clong += $longAvg;
                $c++;
            }
            $clat /= $c;
            $clong /= $c;
            print $gpsFh sprintf ("{\"id\":\"$to\", \"name\":\"$name\", \"lat\":%0.16f, \"long\":%0.16f, \"count\":%d,\"clat\":%0.16f,\"clong\":%0.16f, \"numsat\":%d, \"lat2\":%0.16f, \"long2\":%0.16f }\n", $latAvg, $longAvg, $gobj->{gpsCount},$clat,$clong,$gobj->{numsat}, $gps2->{$to}->{lat}, $gps2->{$to}->{long});
            $gpsFh->flush ();
        }
    }
    if ($saveToFile) {
        close ($fh);
    }

    my $m = 0;
    for my $mDiff (values %maxAbsDiffs)
    {
        $m = $mDiff if ($m < $mDiff);
    }

    print "maxAbsDiff:    $m\n";
    print "minRowPackets: $minCnt\n";
    print "maxRowPackets: $maxCnt\n";
    print "numPackets:    $numPackets\n";
}

sub tan
{
    return sin ($_[0]) / cos ($_[0]);
}

sub wgs84_polar_to_cartesian
{
    my ($lat, $long) = @_;
    my $a = 6378137.0;      # equatorial, semi major axis
    my $b = 6356752.314245; # polar, semi minor axis

    # world is shaped like if someone was pressing on the poles
    # (due to more centrifugal force around the equator),
    # resulting in a smaller polar axis than equatorial.
    # First, use the equation for an ellipse:
    # x^2/a^2 + z^2/b^2 = 1
    #
    # and let theta be the angle in the cartesian coordinate system, tan(theta) = z/x
    #
    # insert expression for z in the elliptic equation and get x as a function of theta:
    # x = ab / sqrt (b^2 + a^2 * tan^2 (theta)).
    # z = b/a  sqrt (a^2 - x^2)
    #
    # Now apply the longitude by rotating the vector (x,0) in the xy-plane. Then you get the vector x * (cos(phi), sin(phi)).
    # x is now pointing out from the center of the globe toward the equator at same line as united kingdom.
    # z is now pointing up towards the north pole
    # y is making up a right handed coordinate system so it is pointing towards Asia.
    # For stockholm
    #    x > 0 (close to 0.5*$a)
    #    y > 0 (close to 0 compared to $a)
    #    z > 0 (close to 0.86 * $b)
    #
    # x 3078864.15654508
    # y 970880.297827299
    # z 5500783.44816461

    # Let the intermediate x-coordinate we get (before rotating in the xy-plane) be called t.

    my $theta = $lat / 360 * 2 * 3.141592653589;
    my $phi   = $long / 360 * 2 * 3.141592653589;
    my $t = $a*$b / sqrt ($b*$b + ($a*tan ($theta)) ** 2);

    my $x = $t * cos ($phi);
    my $y = $t * sin ($phi);
    my $z = $b / $b * sqrt ($a*$a - $t * $t);

    return ($x, $y, $z);
}

sub get_node_dist
{
    my ($node0, $node1) = @_;
    my $obj0 = $gpsCache{$node0};
    my $obj1 = $gpsCache{$node1};
    if (!$obj0 || !$obj1 || !$obj0->{gpsCount} || !$obj1->{gpsCount})
    {
        print STDERR "insufficient gps inormation $node0 <-> $node1\n";
        return 0;
    }

    my $lat0Avg  = $obj0->{gpsCount} ? $obj0->{latitude}  / $obj0->{gpsCount} : 0;
    my $long0Avg = $obj0->{gpsCount} ? $obj0->{longitude} / $obj0->{gpsCount} : 0;
    my $lat1Avg  = $obj1->{gpsCount} ? $obj1->{latitude}  / $obj1->{gpsCount} : 0;
    my $long1Avg = $obj1->{gpsCount} ? $obj1->{longitude} / $obj1->{gpsCount} : 0;

    my $dist = get_dist ($lat0Avg, $long0Avg, $lat1Avg, $long1Avg);
    return $dist;
}

sub get_dist
{
    my ($lat0, $long0, $lat1, $long1) = @_;
    my ($x0, $y0, $z0) = wgs84_polar_to_cartesian ($lat0, $long0);
    my ($x1, $y1, $z1) = wgs84_polar_to_cartesian ($lat1, $long1);

    my $dist = sqrt (($x0-$x1)**2 + ($y0-$y1)**2 + ($z0-$z1)**2);
    return $dist;
}
