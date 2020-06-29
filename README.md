# slightlight

First, connect an st-link v2 jtag cable to the target board and flash it:
```
openocd -c "program /home/manne/Turtle_RadioShuttle/BUILD/Turtle_RadioShuttle.elf verify reset exit"
```

If you have several st-links connected to your computer at the same time,
follow the howto in openocd.cfg to resolve the serial number, then define an alias as
```
alias openocd_red='openocd -f ~/slightlight/src/openocd.cfg -c "dev red"'
```

To simplify the process of flashing several units in a row, define an alias
```
alias openocd_red_flash='while true; do openocd_red -c "program /home/manne/Turtle_RadioShuttle/BUILD/Turtle_RadioShuttle.elf verify reset exit" && echo red flash done || echo red flash FAILED; read apa; done'
```

This makes it possible to just execute openocd_red_flash, then press enter
between each flashing round.

When all units are flashed, connect at least one to your computer. When the
unit powers up, it will wait a few seconds for a computer to connect to its
serial tty. If no computer connects during this time, the tty process on the
node will shut down and thereby disappear from the computer, so make sure to
monitor the added /dev/tty*s when a unit is connected.

Run minicom like this (change the tty to whatever the name is for you)
```
minicom -D /dev/tty.usbmodem01234567891 -C serial.log
```

Given that the name of the tty is the same after the node is unplugged and then
plugged back in again, minicom does not have to be restarted.


Now you should see some output looking like this:
```
16:26:16.332092     2bca: -78.290001 dBm
16:26:16.332886     2bce: -64.919998 dBm
16:26:22.850983 TX BEACON 2bce (9 nodes) (gps[11] 59.368625 18.086642 => 1416590578 215782042)
16:26:22.852539 >>> 2bc6: -70.129997 dBm elapsed: 48.268002 s
16:26:22.853729 >>> 2bc9: -72.019997 dBm elapsed: 5.796000 s
16:26:22.854828 >>> 2bd0: -99.279999 dBm elapsed: 10.578000 s
16:26:22.855987 >>> 2bc8: -77.830002 dBm elapsed: 9.214001 s
16:26:22.857117 >>> 2bcb: -77.599998 dBm elapsed: 47.334003 s
16:26:22.858246 >>> 2bc7: -81.979996 dBm elapsed: 16.001001 s
16:26:22.859405 >>> 2bc5: -72.449997 dBm elapsed: 7.442000 s
16:26:22.860504 >>> 2bcf: -58.869999 dBm elapsed: 5.517000 s
16:26:22.861298 >>> 2bca: -93.779999 dBm elapsed: 15.647000 s
16:26:22.261078 RX BEACON 2bd0 (9 nodes) (gps[0] 59.368035 18.086518 <= 1416576485 215780568)
16:26:22.262604     2bc8: -71.489998 dBm
16:26:22.263397     2bce: -104.470001 dBm
16:26:22.264343     2bc5: -84.709999 dBm
16:26:22.265015     2bcb: -85.059998 dBm
16:26:22.265808     2bcf: -99.769997 dBm
16:26:22.266815     2bca: -80.080002 dBm
```

That output can be parsed by parse_log.pl:

```
tail -f serial.log | ./parse_log.pl
```

This command will summarise the collected data and make averages of all signal
strength values and gps-coordinates:
minRowPackets: 39
maxRowPackets: 86
numPackets:    488
header 16:56:32.664794 TX 2bce (9) gps[12] 59.368691 18.086569 (valid)
      ____2bc5____ ____2bc6____ ____2bc7____ ____2bc8____ ____2bc9____ ____2bca____ ____2bcb____ ____2bce____ ____2bcf____ ____2bd0____
2bc5: ------- ( 0) -90.279 (39) -83.103 (39) -70.609 (39) -82.755 (39) -89.554 (39) -78.616 (39) -84.047 (39) -71.078 (39) -89.728 (39)  59.3685395128205329 18.0863765641025687 (39)
2bc6: -82.388 (40) ------- ( 0) -78.795 (40) -92.906 (40) -90.610 (40) -89.903 (40) -103.901 (40) -68.465 (40) -64.430 (40) -94.292 (40)  59.3686039500000078 18.0868264999999937 (40)
2bc7: -76.301 (56) -78.413 (56) ------- ( 0) -68.872 (56) -77.721 (56) -77.559 (56) -78.708 (56) -80.702 (56) -63.914 (56) -73.877 (56)  59.3685756785714105 18.0861580178571408 (56)
2bc8: -62.523 (44) -93.236 (44) -69.764 (44) ------- ( 0) -74.059 (44) -67.568 (44) -72.606 (44) -84.924 (44) -77.484 (44) -71.035 (44)  59.3683735454545456 18.0866114090909171 (44)
2bc9: -75.927 (41) -89.775 (41) -78.173 (41) -73.221 (41) ------- ( 0) -69.106 (41) -69.002 (41) -78.200 (41) -71.831 (41) -74.423 (41)  59.3683976585365940 18.0863829512195089 (41)
2bca: -82.410 (45) -96.474 (45) -78.002 (45) -67.668 (45) -69.153 (45) ------- ( 0) -79.620 (45) -94.237 (45) -77.278 (45) -73.240 (45)  59.3682874888889174 18.0863999555555530 (45)
2bcb: -72.534 (45) -104.015 (45) -78.867 (45) -71.944 (45) -69.178 (45) -78.814 (45) ------- ( 0) -85.964 (45) -74.039 (45) -83.082 (45)  0.0000000000000000 0.0000000000000000 (0)
2bce: -74.984 (87) -71.724 (87) -82.522 (87) -84.646 (87) -78.994 (87) -92.601 (87) -87.589 (87) ------- ( 0) -57.364 (87) -92.486 (87)  59.3686141149425168 18.0866126666666709 (87)
2bcf: -65.211 (49) -65.966 (49) -65.407 (49) -78.290 (49) -72.733 (49) -77.977 (49) -76.243 (49) -56.497 (49) ------- ( 0) -102.671 (49)  59.3686835102040646 18.0865591224489712 (49)
2bd0: -84.272 (43) -99.757 (43) -76.851 (43) -72.321 (43) -76.604 (43) -75.324 (43) -85.446 (43) -95.308 (43) -102.758 (43) ------- ( 0)  59.3680805813953540 18.0865464883720968 (43)

If all goes well it should show you a
complete matrix of data with no gaps. When the script is interrupted with
ctrl+c, it will generate a file called log.strengths. This file contains
averaged signal strength and distance information between each pair of
nodes. The file looks like
```
2bce 2bc7 -87.341 27.4529566743557
2bce 2bc8 -78.056 25.7444882667066
2bce 2bc9 -76.052 27.2049948134027
2bce 2bca -89.053 38.3726333413194
2bce 2bce ------- 0
2bce 2bcf -58.821 9.72157413286222
2bce 2bd0 -85.948 60.6026922066047
2bcf 2bc5 -65.835 19.4429144440803
2bcf 2bc6 -65.565 17.0749948599585
```

Columns are rx-id, tx-id, rssi-value [dBm] and distance [m].

In order to estimate the distance between any pair of nodes using signal
strength information, we need a proper rssi offset value and a decay
parameter. As the rssi offset value is different for each pair of nodes, we
need a way to estimate this. As it is natural for passive components to have
different values due to tolerances, it makes sense that the gain of the tx path
and rx path is different for every node because of the impedance matching of
the signal chain. This should introduce fixed gain differences between the
nodes and so it should be possible to module this with an rx-offset value
[dBm] and a tx-offset value [dBm]. To calibrate these values, we will use the
information contained in the log.strengths file to create a model:

```
./calibrate log.strengths calib.mdl
```

This will create the file calib.mdl that contains rx and tx offset information
so that the signal strengths can be used to compute an estimated distance.
File looks like this:

```
-84.722939
2bc5 8.034563 13.617014
2bc6 18.980757 15.633726
2bc7 29.158213 26.018345
2bc8 18.871574 19.810904
2bc9 16.123148 15.380740
2bca 21.360142 21.035397
2bce 17.372471 15.640604
2bcf 24.955650 25.529778
2bd0 36.736626 38.686241
```

First line is the k-value that describes how fast/slow the distance changes
with changing rssi-value. Then on the remaining lines comes id, rx-offset
[dBm] and tx-offset [dBm]. After compensating for the offsets, the resulting
signal strength value can be used to compute the distance between any pair of
nodes using the log normal shadowing model.

Now, for any generated log.strengths file, the calib.mdl file can be used to
estimate distances between all pairs of nodes using only signal strength
information. This can then be used to compute a set of xy-coordinates, using
multidimensional scaling (MDS):

```
./mds calib.mdl log.strengths > mds.coords
```

An example of the output from the command is

```
2bc5 0.000000 0.000000
2bc6 35.083996 0.000000
2bc7 70.203613 5.289337
2bc8 28.134365 2.448218
2bc9 38.196949 2.721239
2bca 20.550093 1.058581
2bce 28.789101 2.630650
2bcf 30.022669 2.170929
2bd0 30.677643 6.510629
```

First column is id, then x-coordinate [m] and y-coordinate [m].
Since the mds algorithm introduces an arbitrary offset, arbitrary rotation and
arbitrary mirroring of the points, the coordinates are transformed such that
the first node is always placed in the center, the second node on the positive
x-axis and the third node above the x-axis. Then the mds algorithm will
produce the same coordinates every time it is fed with the same input.

Next step is to verify the result using the gps-coordinates from the
gps-modules. To do that we have to map the mds-coordinates to
gps-coordinates. Before mapping to latitude and longitude, we're interested in
describing the point in cartesian "xyz" space.
We do this by using three reference points of which we will need to use the
gps-information. First, define a center point in both gps-space and mds-space,
then relativie to this point, we create a linear mapping that will bring
points in mds xy-space to gps xyz-space. Then, using standard cartesian to
polar mapping of the coordinates, we get the latitude and the longitude.

Unfortunately, some things are still hardcoded, so the script static.pl will do the
computations above, using the file mds.coords and log.gps:

```
./static.pl
2bc5 59.3685395128205 18.0863765641026
2bca 59.3686321246821 18.0863326924511
2bc6 59.3689165255866 18.0862153685605
2bd0 59.3680805813953 18.0865464883721
2bcf 59.3685991850044 18.0863422840497
2bc7 59.3685756785714 18.0861580178571
2bc9 59.3686203701179 18.0863310038848
2bce 59.3685302458283 18.0863699029177
2bc8 59.3685453069253 18.0863642001795
```

Until further refactoring is done, this can be saved to a file called
gps.coords and that will trigger parse_log.pl to read that file upon startup.
By doing so, parse_log.pl will output a second gps-coordinate, e.g. the
estimated (static) one, to log.gps:

```
tail -f log.gps
{"id":"2bcf", "name":"ravelli", "lat":59.3686835102040646, "long":18.0865591224489712, "count":49,"clat":59.3684617823126644,"clong":18.0864970750348242, "numsat":0, "lat2":59.3685337152264978, "long2":18.0867043283375004 }
{"id":"2bd0", "name":"hamrin", "lat":59.3680805813953540, "long":18.0865464883720968, "count":43,"clat":59.3684617823126644,"clong":18.0864970750348242, "numsat":0, "lat2":59.3686466666666988, "long2":18.0865329999999993 }
...
```

That output can then be used to feed a websocket server:
```
tail -f log.gps | node websocket/server.js
```
When the websocket is running, the nodes can be shown on a map by entering
the correct url to slight.html, which should also point at the correct
server's ip address and port.
