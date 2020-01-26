ARDUINO-COMMAND-BRIDGE
======================

Sends orders from a "command" from a nano to another nano and have target, the
bridge, forward order to 433 Mhz TX.

The two nano communicate with a CC1101 plugged on each and using rflink library.

Below, "nano #1" is the "command" and "nano #2" is the "bridge".

1. Nano #1: order read (button)
2. Nano #1: send order by CC1101
3. Nano #2: receive order
4. Nano #2: send order on 433 Mhz TX

433 Mhz usually works up to 10-20 meters. By using CC1101/868Mhz transmission, I
can command 433 Mhz devices from a farther away distance.

