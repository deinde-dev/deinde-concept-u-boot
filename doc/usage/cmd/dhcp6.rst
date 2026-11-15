.. SPDX-License-Identifier: GPL-2.0+

.. index::
   single: dhcp6 (command)

dhcp6 command
=============

Synopsis
--------

::

    dhcp6 [loadAddress] [[hostIPaddr:]bootfilename]

Description
-----------

The dhcp6 command obtains an IPv6 address from a DHCPv6 server and then,
unless this is turned off, downloads the boot file the server names over
TFTP. It is the IPv6 counterpart of the :doc:`dhcp<dhcp>` command and always
uses IPv6, whatever the arguments say.

loadAddress
    memory address the file is loaded to. If not given, the address comes from
    the *loadaddr* environment variable

hostIPaddr
    address of the TFTP server to download from, framed with [] brackets

bootfilename
    name of the file to download. The server's own boot file URL overrides
    this, so a name given here is used only when the server offers none

The command sends a SOLICIT message to the all-DHCPv6-servers multicast
address ff02::1:2 on the current Ethernet device, picks a server from the
ADVERTISE messages which come back, asks that server for an address with a
REQUEST and treats the address as held once the REPLY arrives. This is the
four-message exchange of RFC 8415.

A server which sets the preference option to 255 is used as soon as its
advertise arrives; otherwise the command waits out the first solicit period,
which is about a second, so that other servers can answer, and then takes the
one with the highest preference. The solicit is retried with a doubling
interval, starting at a second, until a server answers or
CONFIG_NET_RETRY_COUNT tries have been made, which is five by default.

The address in the reply is written to the *ip6addr* environment variable,
with the prefix length the board already has, and used as the source address
of the download which follows. The boot file URL the server provides, option
59 of RFC 5970, gives both the file to download and the address to download
it from, so a URL of 'tftp://[2001:db8::2]/boot.img' sets *serverip6* to
2001:db8::2 and asks for 'boot.img'.

The file is then downloaded over TFTP, as the :doc:`tftpboot<tftpboot>`
command would do, and *filesize* and *fileaddr* are set to describe it.
Setting the *autoload* environment variable to 'no' stops this, so that the
command obtains the address alone.

Once the file is loaded the command boots it, as the *bootm* command would,
if the *autostart* environment variable is set to 'yes'.

Note that an address is obtained but never renewed: the lifetimes in the
reply are stored and not acted on.

Example
-------

This obtains an address and downloads the file the server names::

    => dhcp6 1000000
    DHCP6 SOLICIT 0
    DHCP6 REQUEST 0
    DHCP6 client bound to 2001:0db8:0000:0000:0000:0000:0000:0020
    Using eth@10002000 device
    TFTP from server 2001:0db8:0000:0000:0000:0000:0000:0002; our IP address is 2001:0db8:0000:0000:0000:0000:0000:0020
    Filename 'boot.img'.
    Load address: 0x1000000
    Loading: #
    done
    Bytes transferred = 23 (17 hex)

Addresses are shown in full, since U-Boot does not compress them. With
autoload turned off only the address is obtained::

    => setenv autoload no
    => dhcp6
    DHCP6 SOLICIT 0
    DHCP6 REQUEST 0
    DHCP6 client bound to 2001:0db8:0000:0000:0000:0000:0000:0020

The number after each message is the retry count, so a zero means the message
was answered first time. Without a server to answer, the solicit is retried
until the command gives up::

    => dhcp6
    DHCP6 SOLICIT 0
    DHCP6 SOLICIT 1
    DHCP6 SOLICIT 2
    DHCP6 SOLICIT 3
    DHCP6 SOLICIT 4
    DHCP6 FAILED, TERMINATING

Return value
------------

The return value $? is set to 0 (true) if an address is obtained, and to 1
(false) if no server answers, if the download fails or if the arguments
cannot be parsed.

Configuration
-------------

The command is only available if CONFIG_CMD_DHCP6=y, which needs
CONFIG_IPV6=y and the legacy network stack (CONFIG_NET_LEGACY=y). The lwIP
stack has no DHCPv6 client.

With CONFIG_DHCP6_RAPID_COMMIT=y the solicit asks for the rapid-commit
option, which lets a server answer it with a reply rather than an advertise.
The exchange is then two messages rather than four, and a reply without that
option is ignored.

With CONFIG_DHCP6_PXE_DHCP_OPTION=y the solicit also asks for the boot file
parameters, option 60, and the first parameter in the answer becomes the
PXELINUX configuration file which the :doc:`pxe<pxe>` command uses.

CONFIG_DHCP6_ENTERPRISE_ID sets the enterprise number sent in the vendor
class option, and CONFIG_DHCP_PXE_CLIENTARCH, unless it is 0xff, the
architecture type sent in the client architecture option. Both let a server
tell one kind of client from another.

See also
--------

* :doc:`dhcp<dhcp>` for obtaining the settings over IPv4
* :doc:`tftpboot<tftpboot>` for downloading a file over TFTP by hand, which
  also takes an IPv6 server address
* :doc:`ping6<ping6>` for checking that a host answers over IPv6
* :doc:`dns<dns>` for looking up a host once the address is held
* :doc:`net<net>` for listing the Ethernet devices an address may be asked on
* :doc:`pxe<pxe>` for downloading a PXELINUX configuration file over TFTP and
  booting what it names
* :doc:`bootm<bootm>` for booting the image once it is in memory
