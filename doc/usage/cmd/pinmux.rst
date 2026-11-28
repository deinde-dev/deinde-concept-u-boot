.. SPDX-License-Identifier: GPL-2.0+:

.. index::
   single: pinmux (command)

pinmux command
==============

Synopsis
--------

::

    pinmux list
    pinmux dev [pincontroller-name]
    pinmux status [-a | pin-name]

Description
-----------

The pinmux command is used to show the pin-controller muxing.

The 'pinmux list' command displays the available pin-controllers, with the
driver bound to each and the name of its parent device. A pin-controller
which fails to probe is left out of the list, its driver reporting why.

The 'pinmux dev' command selects the pin-controller for next commands.

    pincontroller-name
        name of the pin-controller to select

With no name it shows the pin-controller already selected. A name which no
pin-controller has leaves nothing selected, so the next 'pinmux status'
needs a selection of its own.

The 'pinmux status' command displays the pin muxing information.

    \-a
        display pin muxing of all pin-controllers.
    pin-name
        name of the pin to display

With neither the option nor a name it shows every pin of the pin-controller
which 'pinmux dev' has selected. A pin name is looked up on every
pin-controller in turn, whichever one is selected, and the first match is
shown.

Example
-------

Showing which pin-controllers are available, on sandbox::

    => pinmux list
    | Device                        | Driver                        | Parent
    | pinctrl-gpio                  | sandbox_pinctrl_gpio          | root_driver
    | gpios@c000                    | qcom_pmic_pinctrl             | gpios@c000
    | pins-test                     | sandbox_pins_test             | root_driver
    | pinctrl                       | sandbox_pinctrl               | root_driver
       single_of_to_plat() single-pinctrl pinctrl-single-no-width: missing register width
    | pinctrl-single-pins           | single-pinctrl                | root_driver
    | pinctrl-single-bits           | single-pinctrl                | root_driver

Selecting one and showing the muxing of each of its pins::

    => pinmux dev pinctrl
    dev: pinctrl
    => pinmux status
    P0        : UART TX.
    P1        : UART RX.
    P2        : I2S SCK.
    P3        : I2S SD.
    P4        : I2S WS.
    P5        : GPIO0 bias-pull-up input-disable.
    P6        : GPIO1 drive-open-drain.
    P7        : GPIO2 bias-pull-down input-enable.
    P8        : GPIO3 bias-disable.

Showing a single pin::

    => pinmux status P0
    P0        : UART TX.

Showing every pin of every pin-controller. The full output runs to several
hundred lines on sandbox, so it is shortened here::

    => pinmux status -a
    --------------------------
    pinctrl-gpio:
    a0        : gpio input .
    a1        : gpio input .
    a2        : gpio input .
    ...
    --------------------------
    pinctrl:
    P0        : UART TX.
    P1        : UART RX.
    ...

Return value
------------

The return value $? is 0 (true) if the sub-command did its work, and 1
(false) if the named pin-controller or pin could not be found, if no
pin-controller is selected, or if the command line was not understood.

Note that 'pinmux status -a' returns 0 even where a pin-controller cannot
report its muxing, since it shows what it can of each one in turn.

Configuration
-------------

The pinmux command is only available if CONFIG_CMD_PINMUX=y.

See also
--------

* :doc:`mux<mux>` for the mux controllers which switch a shared line between
  several destinations, a different kind of mux from the pin muxing here
* :doc:`gpio<gpio>` for reading and setting the pins which a pin-controller
  has muxed as GPIOs
