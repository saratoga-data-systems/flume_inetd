# flume_inetd
Flume inetd for Windows, forked from Bugfixes for Davide Libenzi's wininetd

This provides a way to summon flume on Windows, using inetd like on linux.
The code was forked from wininetd 0.7 and modified to build with VS2015 in 64 bit mode, and to use the fixed location
C:\Flume\etc\flume_inetd.conf for the configuration file for the service.

Changes were renaming the service from wininetd to flume_inetd, as well as the
above listed configuration changes. We are using a 64 bit release build from
VS2015 with Flume. The original html file is still included here in the repo.

This repo resides at https://github.com/saratoga-data-systems/flume_inetd.git
