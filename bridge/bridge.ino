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

#include "common.h"

#define MYADDR          ADDR1
#define SOURCEADDR      ADDR0
#define TXPOWER             1  // 0 = lowpower, 1 = highpower (= long distance)

#define LED_RECV_PIN        6
#define LED_DELAY         180

//unsigned long codes_openall[] = { 0x40A2BBAE, 0x4003894D, 0x4078495E };
unsigned long codes_openall[] = { 0x43210BAE, 0x4321094D, 0x4321095E };
#define CODE_OPENALL_NB (sizeof(codes_openall) / sizeof(*codes_openall))
//unsigned long codes_closeall[] = { 0x40A2BBAD, 0x4003894E, 0x4078495D };
unsigned long codes_closeall[] = { 0x98765BAD, 0x9876594E, 0x9876595D };
#define CODE_CLOSEALL_NB (sizeof(codes_closeall) / sizeof(*codes_closeall))

//
// RF433MHZ TX MANAGEMENT
//

#define RF_TRANSMIT_PIN     4
#define RF_REPEAT_CODE_SEND 6
const unsigned int RF_DELAYFACTOR =  114;
// RF_ constants below correspond to transmission delays.
// The exact transmission duration is "x RF_DELAYFACTOR", in microseconds.
#define RF_TICK  10   // Smallest interval between two signal changes
#define RF_SEP   48   // Sequence separator
#define RF_LONG  500  // Long delay before transmission

void rf_send_signal(byte val, unsigned int factor);
void rf_send_code(const uint32_t code);

void rf_send_instruction(const uint32_t code) {
    rf_send_signal(1, RF_LONG);
    rf_send_signal(0, RF_TICK);
    rf_send_signal(1, RF_SEP);

    for (int i = 0; i < RF_REPEAT_CODE_SEND; ++i)
        rf_send_code(code);

    rf_send_signal(0, RF_TICK);
    rf_send_signal(1, RF_TICK);
    rf_send_signal(0, 0);
}

void rf_send_code(const uint32_t code) {
    uint32_t mask = 1ul << 31;

        // Send leading 0 (don't know why... seems to be part of protocol)
    rf_send_signal(0, RF_TICK);
    rf_send_signal(1, RF_TICK);

    while (mask) {
        byte b = mask & code ? 1 : 0;

        rf_send_signal(b, RF_TICK);
        rf_send_signal(1 - b, RF_TICK);

        mask >>= 1;
    }

    rf_send_signal(1, RF_SEP);
    rf_send_signal(0, 0);
}

void rf_send_signal(byte val, unsigned int factor) {
    digitalWrite(RF_TRANSMIT_PIN, val ? LOW : HIGH);
    unsigned long int d = (unsigned long int)factor *
                          (unsigned long int)RF_DELAYFACTOR;
    if (d >= 1000)
        delay(d / 1000);
    delayMicroseconds(d % 1000);
}

//
// END OF RF433MHZ TX MANAGEMENT
//


static RFLink rf;

void setup() {
    serial_begin(115200);

    cc1101_attach(&rf);
    rf.set_opt_byte(OPT_ADDRESS, MYADDR);
    rf.set_opt_byte(OPT_EMISSION_POWER, TXPOWER);
    rf.set_auto_sleep(true);
    serial_printf("Device initialized\n");
#ifdef DEBUG
    delay(20);
#endif

    pinMode(RF_TRANSMIT_PIN, OUTPUT);
#ifdef LED_RECV_PIN
    pinMode(LED_RECV_PIN, OUTPUT);
#endif
}

void send433mhz(uint32_t code) {
    serial_printf("Sending code 0x%lx to 433Mhz TX device\n", code);
    rf_send_instruction(code);
}

char buffer[3];

void loop() {
    byte len, sender, r;
    if ((r = rf.receive(&buffer, sizeof(buffer), &len, &sender))
        != ERR_OK) {
        if (r != ERR_TIMEOUT)
            serial_printf("Reception error: %i: %s\n", r, rf.get_err_string(r));
    } else {
        if (len > sizeof(buffer))
            len = sizeof(buffer);
        if (len == 3) {
            byte count_openall = 0;
            byte count_closeall = 0;
            for (byte i = 0; i < 3; ++i) {
                if (buffer[i] == INSTR_OPENALL)
                    ++count_openall;
                else if (buffer[i] == INSTR_CLOSEALL)
                    ++count_closeall;
            }
            byte to_do = INSTR_UNDEFINED;
            if (count_openall >= 2)
                to_do = INSTR_OPENALL;
            else if (count_closeall >= 2)
                to_do = INSTR_CLOSEALL;

#ifdef LED_RECV_PIN
            digitalWrite(LED_RECV_PIN, HIGH);
            rf.delay_ms(LED_DELAY);
            digitalWrite(LED_RECV_PIN, LOW);
#endif

            unsigned long *codes = nullptr;
            byte nb = 0;
            if (to_do == INSTR_OPENALL) {
                codes = codes_openall;
                nb = CODE_OPENALL_NB;
            } else if (to_do == INSTR_CLOSEALL) {
                codes = codes_closeall;
                nb = CODE_CLOSEALL_NB;
            }
            for (byte i = 0; i < nb; ++i) {
                send433mhz(codes[i]);
            }
        } else {
            serial_printf("Don't understand instruction received, len = %i\n",
                          len);
        }
    }
}

