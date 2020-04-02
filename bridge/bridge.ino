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

#define RF_AUTO_SLEEP    true

#define LED_RECV_PIN        6
#define LED_DELAY         120

#define ARRAYSZ(a) (sizeof(a) / sizeof(*a))

unsigned long codes_openall[] = { 0x40A2BBAE, 0x4003894D, 0x4078495E };
//unsigned long codes_openall[] = { 0x43210BAE, 0x4321094D, 0x4321095E };
unsigned long codes_closeall[] = { 0x40A2BBAD, 0x4003894E, 0x4078495D };
//unsigned long codes_closeall[] = { 0x98765BAD, 0x9876594E, 0x9876595D };

typedef struct {
    byte code;
    unsigned long *array_codes;
    byte nb_codes;
} code_t;

code_t codes[] = {
    { INSTR_OPENALL,  codes_openall,  ARRAYSZ(codes_openall)  },
    { INSTR_CLOSEALL, codes_closeall, ARRAYSZ(codes_closeall) }
};

#define CODE_VOLET2_OPEN  0x4003894D
#define CODE_VOLET2_CLOSE 0x4003894E

typedef struct {
    unsigned long code_0;
    unsigned long delay_ms;
    unsigned long code_after_delay;
} delayed_code_t;

delayed_code_t delayed_codes[] = {
//    { 0x9876594E, 16500, 0x4321094D }
    { 0x4003894E, 16500, 0x4003894D }
};

void rf_send_signal(byte val, unsigned int factor);
void rf_send_code(const uint32_t code);
void rf_send_instruction(const uint32_t code);

typedef struct {
    bool is_set;
    unsigned long when_ms;
    unsigned long code;
} queue_t;

class Sender {
    private:
        RFLink* rflink;
        queue_t queue[4];

    public:
        Sender(RFLink* arg_rflink) {
            rflink = arg_rflink;
            for (byte i = 0; i < ARRAYSZ(queue); ++i)
                queue[i].is_set = false;
        };

        void send(unsigned long code, bool manage_delayed = true);
        void send_queued_codes();
};

void Sender::send(uint32_t code, bool manage_delay) {
    serial_printf("Sending code 0x%lx to 433Mhz TX device\n", code);
    if (manage_delay) {
        for (byte i = 0; i < ARRAYSZ(delayed_codes); ++i) {
            if (code == delayed_codes[i].code_0) {
                byte j;
                for (j = 0; j < ARRAYSZ(queue); ++j) {
                    if (!queue[j].is_set) {
                        queue[j].is_set = true;
                        queue[j].when_ms = millis() + delayed_codes[i].delay_ms;
                        queue[j].code = delayed_codes[i].code_after_delay;
                        serial_printf("Adding code to queue\n");
                        break;
                    }
                }
                if (j >= ARRAYSZ(queue))
                    serial_printf("Unable to add code to queue\n");
            }
        }
    }
    rflink->delay_ms(10);
    rf_send_instruction(code);
}

void Sender::send_queued_codes() {
    byte nb_in_queue;
    do {
        rflink->delay_ms(5);
        unsigned long t = millis();
        for (byte i = 0; i < ARRAYSZ(queue); ++i) {
            if (queue[i].is_set) {
                if (t >= queue[i].when_ms) {
                    send(queue[i].code, false);
                    queue[i].is_set = false;
                }
            }
        }

        nb_in_queue = 0;
        for (byte i = 0; i < ARRAYSZ(queue); ++i) {
            if (queue[i].is_set)
                ++nb_in_queue;
        }
    } while (nb_in_queue >= 1);
}


//
// RF433MHZ TX MANAGEMENT
//

#define RF_TRANSMIT_PIN     4
#define RF_REPEAT_CODE_SEND 7
const unsigned int RF_DELAYFACTOR =  114;
// RF_ constants below correspond to transmission delays.
// The exact transmission duration is "x RF_DELAYFACTOR", in microseconds.
#define RF_ZERO   0   // No interval - just change signal status
#define RF_TICK  10   // Smallest interval between two signal changes
#define RF_SEP   48   // Sequence separator
#define RF_LONG  500  // Long delay before transmission

void rf_send_instruction(const uint32_t code) {
    rf_send_signal(1, RF_LONG);
    rf_send_signal(0, RF_TICK);
    rf_send_signal(1, RF_SEP);

    for (int i = 0; i < RF_REPEAT_CODE_SEND; ++i)
        rf_send_code(code);

    rf_send_signal(0, RF_TICK);
    rf_send_signal(1, RF_TICK);
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
    rf_send_signal(0, RF_ZERO);
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
static Sender tx(&rf);

void setup() {
    serial_begin(115200);

    cc1101_attach(&rf);
    rf.set_opt_byte(OPT_ADDRESS, MYADDR);
    rf.set_opt_byte(OPT_EMISSION_POWER, TXPOWER);
    rf.set_auto_sleep(RF_AUTO_SLEEP);
    serial_printf("Device initialized\n");
#ifdef DEBUG
    delay(20);
#endif

    pinMode(RF_TRANSMIT_PIN, OUTPUT);
#ifdef LED_RECV_PIN
    pinMode(LED_RECV_PIN, OUTPUT);
#endif
}

byte buffer[3];

void loop() {
    byte len, sender, r;
    serial_printf("rf.receive()\n");
    if ((r = rf.receive(&buffer, sizeof(buffer), &len, &sender))
        != ERR_OK) {

        if (r != ERR_TIMEOUT)
            serial_printf("Reception error: %i: %s\n", r, rf.get_err_string(r));

    } else {

        if (len > sizeof(buffer))
            len = sizeof(buffer);

        if (len == 3) {

            byte count_codes[ARRAYSZ(codes)];
            for (byte i = 0; i < ARRAYSZ(codes); ++i)
                count_codes[i] = 0;
            for (byte i = 0; i < 3; ++i)
                for (byte j = 0; j < ARRAYSZ(codes); ++j)
                    if (buffer[i] == codes[j].code)
                        count_codes[j]++;

#ifdef DEBUG
            for (byte i = 0; i < 3; ++i) {
                serial_printf("  byte[%i] = %u\n", i, (unsigned int)buffer[i]);
            }
            for (byte i = 0; i < ARRAYSZ(codes); ++i) {
                serial_printf("  code[%i] = %i\n", i, count_codes[i]);
            }
#endif

            code_t *pcode = nullptr;
            for (byte i = 0; i < ARRAYSZ(codes); ++i)
                if (count_codes[i] >= 2)
                    pcode = &codes[i];

            if (pcode) {
                serial_printf("Received a code to forward\n");
#ifdef LED_RECV_PIN
                digitalWrite(LED_RECV_PIN, HIGH);
                rf.delay_ms(LED_DELAY);
                digitalWrite(LED_RECV_PIN, LOW);
#endif
                for (byte i = 0; i < pcode->nb_codes; ++i) {
                    tx.send(pcode->array_codes[i]);
                }
                tx.send_queued_codes();

#ifdef LED_RECV_PIN
                digitalWrite(LED_RECV_PIN, HIGH);
                rf.delay_ms(LED_DELAY);
                digitalWrite(LED_RECV_PIN, LOW);
                rf.delay_ms(LED_DELAY);
                digitalWrite(LED_RECV_PIN, HIGH);
                rf.delay_ms(LED_DELAY);
                digitalWrite(LED_RECV_PIN, LOW);
#endif

            } else {
                serial_printf("No code to forward\n");
            }

        } else {
            serial_printf("Don't understand instruction received, len = %i\n",
                          len);
        }
    }
}

