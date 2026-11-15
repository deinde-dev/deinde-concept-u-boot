.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: ping (command)

ping command
============

Synopsis
--------

::

    ping <address>

Description
-----------

The ping command sends an ICMP echo request to a host and reports whether a
reply comes back, which is the quickest way of telling whether U-Boot's network
settings reach the rest of the network.

address
    IPv4 address of the host to ping, in dotted-quad form. With the lwIP stack
    this may be a host name as well, which is looked up first

The request goes out on the current Ethernet device, the one named by the
*ethact* environment variable, using the address in *ipaddr* as its source. A
host outside the subnet given by *netmask* is reached through the router in
*gatewayip*, so a ping which works for a neighbour and fails for the rest of
the world points at that variable.

The legacy stack sends a single request and waits ten seconds for the reply.
Before that it has to learn the MAC address of the host, or of the router, which
costs an ARP request and reply, so a successful ping puts four packets on the
wire. The lwIP stack sends up to five requests, a second apart, and stops at
the first reply.

The address is required and must parse, so 0.0.0.0 shows the usage message
rather than being pinged: the legacy stack cannot tell that address apart from
one which does not parse at all.

A failed ping is retried according to the *netretry* environment variable, as
every network command is: unset or 'no' gives up after the first attempt, 'once'
tries a second time, 'yes' keeps trying until Ctrl-C, and a number gives that
many retries.

Example
-------

This pings a host which answers::

    => setenv ethact eth@10002000
    => ping 1.1.2.2
    Using eth@10002000 device
    host 1.1.2.2 is alive

A host which does not answer is reported after the ten-second wait::

    => ping 1.1.2.2
    Using eth@10002000 device
    ping failed; host 1.1.2.2 is not alive

Return value
------------

The return value $? is 0 (true) if the host answers and 1 (false) if it does
not. An address which is missing or cannot be parsed shows the usage message
and also returns 1.

Configuration
-------------

The command is only available if CONFIG_CMD_PING=y.

Both network stacks provide it, from different code: the legacy stack
(CONFIG_NET_LEGACY=y) sends the echo request itself, while with
CONFIG_NET_LWIP=y lwIP's raw API is used, which also accepts a host name.

See also
--------

* :doc:`dns<dns>` for looking up the address of a host by name
* :doc:`linklocal<linklocal>` for choosing an address without a server
* :doc:`bootp<bootp>` for obtaining an address from a BOOTP server
* :doc:`pcap<pcap>` for capturing the packets a ping puts on the wire
* :doc:`mii<mii>` for checking that the link is up at all
* :doc:`ping6<ping6>` for the same thing over IPv6
* *dhcp* for obtaining the network settings automatically
* *net* for listing the Ethernet devices which can be made current
