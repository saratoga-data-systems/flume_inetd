# flume_inetd
Flume inetd for Windows, forked from Bugfixes for Davide Libenzi's wininetd

This provides a way to summon flume on Windows, using inetd like on linux.
The code was forked from wininetd 0.7 and modified to build with VS2015 in 64 bit mode, and to use the fixed location
C:\Flume\etc\flume_inetd.conf for the configuration file for the service.
