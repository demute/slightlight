#!/usr/bin/perl
use strict;
use JSON;
use Math::Trig;

# what we are going to do below is the following
# * use a spherical model of the earth (e.g. not wgs84)
# * choose three reference points from which gps information will be used
# * first consider gps-coordinates in euclidean 3d-space
# * execute the mds algorithm and get back coordinates in a 2d-space
# * define a center point to be the average value of the reference points
# * now relative to the center point in both gps-space and mds-space,
#   we have this matrix equation that will map relative mds points to
#   relative gps-points:
#   / u \   / a11 a12 \ / x \
#   | v | = | a21 a22 | |   |
#   \ w /   \ a31 a32 / \ y /
#   where u,v,w are gps-coordinates and x,y are mds-coordinates. The system
#   has six unknowns which can be solved by inserting two reference points
#   into the equation.
# * By using the mapping above, all mds-points can now be mapped to xyz-
#   coordinates in gps-space.
# * transform gps-xyz-space to longitude/latitude.
# * output estimated gps points for all nodes.


$| = 1;

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

my $mdsFile = "/Users/manne/github/demute/slightlight/mds.coords";
my $gpsFile = "/Users/manne/github/demute/slightlight/log.gps";

# sample mds file:
# 2bc5 0.000000 0.000000
# 2bc6 9.633797 -0.000004
# 2bc7 11.322654 0.002889
# 2bc8 12.343378 -0.017016
# 2bc9 10.754396 -0.014748
# 2bca 12.494884 0.026808
# 2bce 11.999769 -0.006333
# 2bcf 11.768997 -0.025015
# 2bd0 24.735229 -0.128287

# sample lines from gps file:
# {"id":"2bca", "name":"zlatan", "lat":59.3785118999999995, "long":18.0349164000000002, "count":10,"clat":59.3783008375673091,"clong":18.0352458215426878, "numsat":2}
# {"id":"2bce", "name":"skoglund", "lat":59.3782786315789650, "long":18.0351416842105259, "count":19,"clat":59.3783008375673091,"clong":18.0352458215426878, "numsat":4}
# {"id":"2bcf", "name":"ravelli", "lat":59.3783939999999930, "long":18.0354202500000014, "count":12,"clat":59.3783008375673091,"clong":18.0352458215426878, "numsat":1}
# {"id":"2bd0", "name":"hamrin", "lat":59.3785048000000089, "long":18.0353416000000024, "count":10,"clat":59.3783008375673091,"clong":18.0352458215426878, "numsat":3}

my @refNodes = ("2bc5", "2bc7", "2bd0");

sub read_gps_ref_points
{
    my ($gpsFile, @refNodes) = @_;
    open (my $fh, "<$gpsFile") or die "open $gpsFile: $!";
    my %db;
    while (my $line = <$fh>)
    {
        my $obj = from_json ($line);
        $db{$obj->{id}} = $obj;
    }
    close ($fh);

    my $gpsRefPoints = {};
    for (@refNodes)
    {
        die "ref node not found: $_" unless (exists $db{$_});
        $gpsRefPoints->{$_} = {latitude => $db{$_}->{lat}, longitude => $db{$_}->{long}};
    }
    return $gpsRefPoints;
}

sub read_mds_points
{
    my ($mdsFile, @refNodes) = @_;
    open (my $fh, "<$mdsFile") or die "open $mdsFile: $!";
    my %db;
    my $mdsPoints = {};
    while (my $line = <$fh>)
    {
        chop $line;
        my ($id, $x, $y) = split (/ /, $line);
        $mdsPoints->{$id} = {x => $x+0, y => $y+0};

    }
    close ($fh);
    return $mdsPoints;
}

my $gpsRefPoints = read_gps_ref_points ($gpsFile, @refNodes);
my $mdsPoints = read_mds_points ($mdsFile);

my $pi = 3.1415926535897932384626433832795;
my $nRefs = scalar @refNodes;
my ($mdsRefCenterX, $mdsRefCenterY) = (0,0);
my ($gpsRefCenterX, $gpsRefCenterY, $gpsRefCenterZ) = (0,0,0);

for my $node (@refNodes)
{
    my $latitude  = $gpsRefPoints->{$node}->{latitude};
    my $longitude = $gpsRefPoints->{$node}->{longitude};
    my $r     = 6371e3;
    my $theta = (90 - $latitude) / 180 * $pi;
    my $phi   = $longitude / 180 * $pi;

    $gpsRefPoints->{$node}->{x} = $r * sin ($theta) * cos ($phi);
    $gpsRefPoints->{$node}->{y} = $r * sin ($theta) * sin ($phi);
    $gpsRefPoints->{$node}->{z} = $r * cos ($theta);

    $mdsRefCenterX += $mdsPoints->{$node}->{x} / $nRefs;
    $mdsRefCenterY += $mdsPoints->{$node}->{y} / $nRefs;
    $gpsRefCenterX += $gpsRefPoints->{$node}->{x} / $nRefs;
    $gpsRefCenterY += $gpsRefPoints->{$node}->{y} / $nRefs;
    $gpsRefCenterZ += $gpsRefPoints->{$node}->{z} / $nRefs;
}
#print "$gpsRefCenterX $gpsRefCenterY $gpsRefCenterZ\n";

my $x0 = $mdsPoints->{$refNodes[0]}->{x} - $mdsRefCenterX;
my $y0 = $mdsPoints->{$refNodes[0]}->{y} - $mdsRefCenterY;

my $x1 = $mdsPoints->{$refNodes[1]}->{x} - $mdsRefCenterX;
my $y1 = $mdsPoints->{$refNodes[1]}->{y} - $mdsRefCenterY;

my $u0 = $gpsRefPoints->{$refNodes[0]}->{x} - $gpsRefCenterX;
my $v0 = $gpsRefPoints->{$refNodes[0]}->{y} - $gpsRefCenterY;
my $w0 = $gpsRefPoints->{$refNodes[0]}->{z} - $gpsRefCenterZ;

my $u1 = $gpsRefPoints->{$refNodes[1]}->{x} - $gpsRefCenterX;
my $v1 = $gpsRefPoints->{$refNodes[1]}->{y} - $gpsRefCenterY;
my $w1 = $gpsRefPoints->{$refNodes[1]}->{z} - $gpsRefCenterZ;

my $a12 = ($x1 * $u0 - $x0 * $u1) / ($x1 * $y0 - $x0 * $y1);
my $a22 = ($x1 * $v0 - $x0 * $v1) / ($x1 * $y0 - $x0 * $y1);
my $a32 = ($x1 * $w0 - $x0 * $w1) / ($x1 * $y0 - $x0 * $y1);

my $a11 = ($u0 - $a12 * $y0) / $x0;
my $a21 = ($v0 - $a22 * $y0) / $x0;
my $a31 = ($w0 - $a32 * $y0) / $x0;

my $gpsPoints = {};
for my $node (keys %$mdsPoints)
{
    my $mdsX = $mdsPoints->{$node}->{x} - $mdsRefCenterX;
    my $mdsY = $mdsPoints->{$node}->{y} - $mdsRefCenterY;

    my $gpsX = $a11 * $mdsX + $a12 * $mdsY + $gpsRefCenterX;
    my $gpsY = $a21 * $mdsX + $a22 * $mdsY + $gpsRefCenterY;
    my $gpsZ = $a31 * $mdsX + $a32 * $mdsY + $gpsRefCenterZ;

    my $r = sqrt ($gpsX**2 + $gpsY**2 + $gpsZ**2);
    my $thetaRad = $pi / 2 - acos ($gpsZ / $r);
    my $phiRad   = atan ($gpsY / $gpsX);

    my $lat  = $thetaRad / $pi * 180;
    my $long = $phiRad   / $pi * 180;
    $gpsPoints->{$node}->{latitude}  = $lat;
    $gpsPoints->{$node}->{longitude} = $long;

    print "$node $lat $long\n";
}

