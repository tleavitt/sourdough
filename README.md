# Reproducing CTCP Throughput Results

Stanford CS244 - Spring 2018

To build:

	$ ./autogen.sh
	$ ./configure
	$ make

To run:
        $ cd datagrump
        $ sudo sysctl -w net.ipv4.ip_forward=1
	$ ./collect_data_bg.sh 10 ctcp throughput.log

This will run a CTCP sender and receiver over a 240 Mbps link with 10 Mbps 
background cross traffic, and log the througput measurements to ./throughput.log 
