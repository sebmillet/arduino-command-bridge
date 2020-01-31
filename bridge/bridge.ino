// vim:ts=4:sw=4:tw=80:et
/*
  bridge.ino

  Receive commands using CC1101 device and forwards it on 433MHZ.
*/

/*
  Copyright 2020 Sébastien Millet

  ARDUINO-COMMAND-BRIDGE is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  ARDUINO-COMMAND-BRIDGE is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses>.
*/

#include <Arduino.h>
#include "cc1101wrapper.h"

#define DEBUG

#include "common.h"

#define MYADDR          ADDR1
#define SOURCEADDR      ADDR0
#define TXPOWER            0  // 0 = low power, 1 = high power (= long distance)

#ifndef DEBUG

#define dbg(a)
#define dbgf(...)
#define dbgbin(a, b, c)

#else

#include "debug.h"

#endif

static RFLink rf;

void setup() {
    Serial.begin(115200);
    cc1101_attach(&rf);
    rf.set_opt_byte(OPT_ADDRESS, MYADDR);
    rf.set_opt_byte(OPT_EMISSION_POWER, TXPOWER);
    rf.set_auto_sleep(true);
    dbg("Device initialized");
}

void fwd433mhz(uint32_t code) {
    delay(200);
    dbgf("Forwarding code 0x%lx to 433Mhz TX device", code);
}

char buffer[60];

void loop() {
    byte len, sender, r;
    if ((r = rf.receive(&buffer, sizeof(buffer), &len, &sender))
        != ERR_OK) {
        if (r != ERR_TIMEOUT)
            dbgf("Reception error: %i: %s", r, rf.get_err_string(r));
    } else {
        if (len >= sizeof(buffer))
            len = sizeof(buffer) - 1;
        if (len >= 5 && buffer[0] == INSTR_FWD433MHZ &&
            2 + 4 * (int)(buffer[1]) == len) {
            byte nb_codes = buffer[1];
            for (byte i = 0; i < nb_codes; ++i) {
                fwd433mhz(uint32_ntoh((byte*)&buffer[2 + 4 * i]));
            }
        } else {
            dbgf("Don't understand instruction received, len = %i",
                          len);
        }
    }
}

