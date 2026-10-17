.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: sntp (command)

sntp command
============

Synopsis
--------

::

    sntp [serverip]
    sntp [servername]  # NET_LWIP=y && DNS=y only

Description
-----------

The sntp command gets the current time from an NTP time server and
synchronizes the Real Time Clock (RTC) of the board. This command needs
the server's IP address to be given on the command line or via the
`ntpserverip` environment variable.

The time which the command sets is printed as it sets it, in the form::

    Date: 2026-12-25 Time: 12:00:30

The address of the NTP server does not need to be given if the DHCP server
provides one. The legacy network stack (`CONFIG_NET_LEGACY=y`) can only use the
first NTP server provided in the `ntp-servers` DHCP option.

When the network stack is lwIP (`CONFIG_NET_LWIP=y`) and DNS resolution
is enabled (`CONFIG_DNS=y`), then the sntp command accepts a server
name as an argument.

The network time is sent as UTC. So, if you want to set the RTC to any local
time different from UTC, you need to set the `timeoffset` environment variable.

Round-trip delay compensation is not implemented/not enabled. In practice
this should not matter much given that the RTC API does not have sub-second
resolution, and round-trip times are typically 10 to 100 ms at most.

The command gives up if the server has not answered after about ten seconds.
Ctrl-C stops it earlier.

Examples
--------

::

    => env set ntpserverip 109.190.177.205
    => date
    Date: 2025-06-16 (Monday)    Time: 15:19:35
    => date reset
    Reset RTC...
    Date: 2000-01-01 (Saturday)    Time:  0:00:00
    => date
    Date: 2000-01-01 (Saturday)    Time:  0:00:03
    => sntp
    Date: 2025-06-16 Time: 15:19:43
    => date
    Date: 2025-06-16 (Monday)    Time: 15:19:47
    => env set timeoffset 7200
    => sntp
    Date: 2025-06-16 Time: 17:19:55
    => date
    Date: 2025-06-16 (Monday)    Time: 17:19:57

With `CONFIG_NET_LWIP=y` and `CONFIG_DNS=y`:

::

    => date reset
    Reset RTC...
    Date: 2000-01-01 (Saturday)    Time:  0:00:00
    => sntp 0.us.pool.ntp.org
    Date: 2025-06-16 Time: 15:10:59

Configuration
-------------

The sntp command is only available if CONFIG_CMD_SNTP=y.

Return value
------------

The return value $? is set to 0 (true) if the time came back from the server
and the real-time clock was set from it.

If an error occurs, the return value $? is set to 1 (false). The command fails
when no server is named at all, when the name or address it is given cannot be
resolved, and when the server does not answer before the timeout or Ctrl-C ends
the wait. The legacy stack reports those three as 'ntpserverip not set', 'Bad
NTP server IP address' and 'SNTP failed: host <address> not responding'.

The clock is left alone in each of those cases. Giving more than one argument
shows the usage message instead.

See also
--------

* :doc:`dns<dns>` for finding the address of a time server by name
* :doc:`date<date>` for reading and setting the real-time clock
* :doc:`bootp<bootp>` for picking up the server address from the network
  along with the rest of the settings
