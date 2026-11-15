.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: ping6 (command)

ping6 command
=============

Synopsis
--------

::

    ping6 <address>

Description
-----------

The ping6 command sends an ICMPv6 echo request to a host and reports whether a
reply comes back. It is the IPv6 counterpart of the ping command.

address
    IPv6 address of the host to ping, in any of the forms string_to_ip6()
    accepts. A host name is not looked up, since the resolver handles IPv4
    addresses only

The request goes out on the current Ethernet device, the one named by the
*ethact* environment variable, using the address in *ip6addr* as its source.
That variable carries the prefix length as well, as in 2001:db8::2/64, which is
what decides whether the host is on the same link or has to be reached through
the router in *gatewayip6*.

Where CONFIG_IPV6_ROUTER_DISCOVERY=y and neither of those is set, the first
IPv6 command of the session looks for a router first: it sends up to three
router solicitations four seconds apart and reports the gateway address and
prefix length a router advertisement gives it. Setting *gatewayip6* and a
prefix length by hand skips that search.

The command then has to learn the MAC address of the host, or of the router,
which it asks for with a neighbour solicitation. It tries three times, five
seconds apart, before giving up; once the address is known the echo request
itself is allowed ten seconds for its reply.

The address printed in each message is the full form, with every group written
out, whatever form it was typed in.

Example
-------

Sandbox's emulated Ethernet device answers neighbour solicitations and pings
for any address, so the output below is from a sandbox run. Naming a router
keeps the command from sending a router solicitation first::

    => setenv ip6addr 2001:db8::1/64
    => setenv gatewayip6 2001:db8::ff
    => ping6 2001:db8::2
    Using eth@10002000 device
    host 2001:0db8:0000:0000:0000:0000:0000:0002 is alive

A host which does not answer the neighbour solicitation is reported once the
three tries run out, five seconds apart::

    => ping6 2001:db8::2
    Using eth@10002000 device

    Neighbour discovery retry count exceeded; starting again
    ping6 failed; host 2001:0db8:0000:0000:0000:0000:0000:0002 is not alive

Return value
------------

The return value $? is 0 (true) if the host answers and 1 (false) if it does
not. An address which is missing or cannot be parsed shows the usage message
and also returns 1.

Configuration
-------------

The command is only available if CONFIG_CMD_PING6=y, which needs
CONFIG_IPV6=y. It is provided by the legacy network stack alone; there is no
lwIP version.

See also
--------

* :doc:`ping<ping>` for the same thing over IPv4
* :doc:`dns<dns>` for looking up the address of a host by name
* :doc:`pcap<pcap>` for capturing the packets a ping puts on the wire
* :doc:`mii<mii>` for checking that the link is up at all
* *dhcp6* for obtaining the IPv6 settings automatically
* :doc:`net<net>` for listing the Ethernet devices which can be made current
