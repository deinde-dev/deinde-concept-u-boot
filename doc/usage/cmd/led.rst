.. SPDX-License-Identifier: GPL-2.0-or-later

.. index::
   single: led (command)

led command
===========

Synopsis
--------

::

    led <led_label> <on|off|toggle>
    led <led_label> blink <blink-freq in ms>
    led <led_label>
    led list

led <led_label>
---------------

Get state of the LED whose label is the one passed as ``<led_label>`` argument.
The LED is probed if it is not active yet.

Provided ``<led_label>`` is *module_led*, the possible outputs for this command
are::

    LED 'module_led': off
    LED 'module_led': on
    LED 'module_led': blink

led <led_label> on|off|toggle
-----------------------------

Turn on, off or toggle state of the LED whose label is the one passed as
``<led_label>`` argument.

The state may be shortened to any leading part of its name, the first state to
match being the one used. So *o* means off, since off comes before on, and *t*
means toggle. A name which matches no state at all shows the usage message.

led <led_label> blink <blink-freq in ms>
----------------------------------------

Make the LED whose label is the one passed as ``<led_label>`` argument blink at
a frequency specified by the argument ``<blink-freq in ms>``.

The frequency is parsed as a decimal number and its unit is milliseconds. The
duty cycle is 50%. Example::

    led blue blink 1000

will make the *blue*-labeled LED blink with a state (on or off) kept for 500ms
before switching to the other state (respectively off or on) for 500ms,
looping endlessly.

Not every driver can blink an LED. One which cannot reports an error and leaves
the LED as it is.

led list
--------

List all available LEDs by their label and provide their known state, which can
be either *off*, *on* or *blink*.

An LED appears in the list only if it has a label, which is read from the
*label* property of its devicetree node, or built from the *color* and
*function* properties, or taken from the name of the node.

The list does not probe anything, so an LED which nothing has asked for yet
has its state shown as *<inactive>*.

Examples
--------

This shows the nine LEDs of the sandbox devicetree. The two with a
*default-state* property are probed at start-up so that it can be applied, so
those two have a state to show before anything asks for one::

    => led list
    sandbox:red     <inactive>
    sandbox:green   <inactive>
    sandbox:default_on on
    default_off     off
    red:status-20   <inactive>
    green:status    <inactive>
    :status         <inactive>
    green:          <inactive>
    sandbox:function <inactive>
    => led sandbox:red
    LED 'sandbox:red': off
    => led sandbox:red on
    => led sandbox:red
    LED 'sandbox:red': on
    => led sandbox:red toggle
    => led sandbox:red
    LED 'sandbox:red': off
    => led sandbox:green off
    => led list
    sandbox:red     off
    sandbox:green   off
    sandbox:default_on on
    default_off     off
    red:status-20   <inactive>
    green:status    <inactive>
    :status         <inactive>
    green:          <inactive>
    sandbox:function <inactive>

A label which no LED has is reported, as is an operation the driver cannot
carry out. The sandbox LEDs are driven through GPIOs, which cannot blink on
their own::

    => led nosuch
    LED 'nosuch' not found (err=-19)
    => led sandbox:red blink 1000
    LED 'sandbox:red' operation failed (err=-38)

Configuration
-------------

The *led* commands are only available if ``CONFIG_CMD_LED=y``.

The *led <led_label> blink* command is only available if ``CONFIG_CMD_LED=y``
and either ``CONFIG_LED_BLINK=y`` or ``CONFIG_LED_SW_BLINK=y``.

Return value
------------

The return value $? is set to 0 (true) if the command succeeds. If the label
names no LED, or the driver cannot do what is asked of it, the return value $?
is set to 1 (false). It is also set to 1 when the command shows its usage
message, which it does when the label is missing, when the state is not one it
knows, or when blink is given no frequency.

See also
--------

* :doc:`gpio<gpio>` for driving the pin an LED is wired to directly
* :doc:`button<button>` for the other simple device whose state the board
  exposes
