ARDUINO-COMMAND-BRIDGE
======================

Sends orders from a nano to another nano and have target forward order to 433Mhz
TX.
The two nanos communicate with a CC1101 plugged on each and using rflink
library.

Below, "nano #1" is the "command" and "nano #2" is the "bridge".

1. Nano #1: get order (= read button status)
2. Nano #1: send order by CC1101
3. Nano #2: receive order from CC1101
4. Nano #2: send order on 433 Mhz TX

433Mhz usually works up to 10-20 meters. By using CC1101/868Mhz transmission, I
can command 433Mhz devices from a farther away distance, like 50 meters or more.

