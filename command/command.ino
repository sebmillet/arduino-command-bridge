// vim:ts=4:sw=4:tw=80:et
/*
  command.ino

  Sends instructions on CC1101, to be "repeated" by the bridge.

  CC1101 PLUGGING
  ===============

  The CC1101 device must be plugged as follows:
    CC1101      Arduino nano
    ------      ----
    1 VCC       3.3V
    2 GND       GND
    3 MOSI      D11
    4 SCK       D13
    5 MISO      D12
    7 GDO0      D02
    8 CSN (SS)  D10

  *IMPORTANT*
    The PIN D03 is *NOT* plugged on CC1101 (as opposed to most schemas
    instructions).

  *CREDITS*
  [1] More than one button triggering interrupt when pressed
      Solution about this issue was found here:
      https://create.arduino.cc/projecthub/Svizel_pritula/
        10-buttons-using-1-interrupt-2bd1f8
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
#include <avr/sleep.h>
#include <avr/power.h>
#include "cc1101wrapper.h"

#define DEBUG

    // Volet 1 (salon)
//#define CODE_BTN0 0x40A2BBAE
//#define CODE_BTN1 0x40A2BBAD
    // Volet 2 (salle à manger)
//#define CODE_BTN0 0x4003894D
//#define CODE_BTN1 0x4003894E
    // Volet 3 (chambre)
#define CODE_BTN0 0x4078495E
#define CODE_BTN1 0x4078495D

unsigned long codes_btn0[] = { 0x40A2BBAE, 0x4003894D, 0x4078495E };
//unsigned long codes_btn0[] = { 0x40A2BBAE };
unsigned long codes_btn1[] = { 0x40A2BBAD, 0x4003894E, 0x4078495D };
//unsigned long codes_btn1[] = { 0x40A2BBAD };

#include "common.h"

#define SEND_ASK_FOR_ACK  true

#define MYADDR           ADDR0
#define TARGETADDR       ADDR1
#define TXPOWER              0  // 0 = lowpower, 1 = highpower (= long distance)

#define BTNINT_PIN         PB3  // D03 PIN
#define BTNINT_INT       INTF1  // D03 corresponds to interrupt 1
#define BTN0_PIN           PB4  // D04 PIN
#define BTN1_PIN           PB5  // D05 PIN

#ifdef DEBUG

void serial_printf(const char* fmt, ...)
     __attribute__((format(printf, 1, 2)));

void serial_printf(const char *fmt, ...) {
    char buffer[150];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    buffer[sizeof(buffer) - 1] = '\0';
    Serial.print(buffer);
}

void serial_begin(long speed) {
    Serial.begin(speed);
}

#else // DEBUG

#define serial_printf(...)
#define serial_begin(speed)

#endif // DEBUG

static RFLink rf;

void my_detachInterrupt(char n) {
    detachInterrupt(n);
}

void my_attachInterrupt(char n, void (*func)(), char int_type) {
    // About the trick below (clearing the interrupt flag before attaching
    // interrupt again), see the below URL:
    //   https://code.google.com/archive/p/arduino/issues/510
    EIFR |= (1 << n);
    attachInterrupt(n, func, int_type);
}

// See [1] about what this is all about
void configure_common() {
    pinMode(BTNINT_PIN, INPUT_PULLUP);
    pinMode(BTN0_PIN, OUTPUT);
    digitalWrite(BTN0_PIN, LOW);
    pinMode(BTN1_PIN, OUTPUT);
    digitalWrite(BTN1_PIN, LOW);
}

// Same as above, see [1] about what this is all about
void configure_distinct() {
    pinMode(BTNINT_PIN, OUTPUT);
    digitalWrite(BTNINT_PIN, LOW);
    pinMode(BTN0_PIN, INPUT_PULLUP);
    pinMode(BTN1_PIN, INPUT_PULLUP);
}

void setup() {
    serial_begin(115200);

    cc1101_attach(&rf);
    rf.set_opt_byte(OPT_EMISSION_POWER, TXPOWER);
    rf.set_opt_byte(OPT_ADDRESS, MYADDR);
    serial_printf("Device initialized\n");
}

void button_pressed(short int button) {
    const unsigned long *codes;
    byte nb_codes;
    if (button == 0) {
        codes = codes_btn0;
        nb_codes = sizeof(codes_btn0) / sizeof(*codes_btn0);
    } else {
        codes = codes_btn1;
        nb_codes = sizeof(codes_btn1) / sizeof(*codes_btn1);
    }

    serial_printf("Button #%i: sending following codes:\n", button);
#ifdef DEBUG
    for (byte i = 0; i < nb_codes; ++i) {
        serial_printf("    #%i: 0x%08lx\n", i, codes[i]);
    }
#endif

    size_t len = 2 + 4 * nb_codes;
    byte* instr = new byte[len];
    instr[0] = INSTR_FWD433MHZ;
    instr[1] = nb_codes;
    for (byte i = 0; i < nb_codes; ++i) {
        uint32_hton(&instr[2 + 4 * i], codes[i]);
    }

    serial_printf("len=%i\n", len);

    byte n;
    byte r = rf.send(TARGETADDR, instr, len, SEND_ASK_FOR_ACK, &n);

    delete []instr;

    if (r != ERR_OK) {
        serial_printf("Sending error: %i: %s - ack=%s, sent %i time(s)\n",
                r, rf.get_err_string(r), (SEND_ASK_FOR_ACK ? "yes" : "no"), n);
    } else {
        serial_printf("Message send successful - ack=%s - sent %i time(s)\n",
                (SEND_ASK_FOR_ACK ? "yes" : "no"), n);
    }
}

void dummy() { }

void non_infinite_loop() {
    configure_common();
    sleep_enable();
    my_attachInterrupt(BTNINT_INT, dummy, FALLING);
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    // FIXME
    // Not sure this is needed when sleeping in power down mode?
    power_adc_disable();

    serial_printf("Going to sleep...\n");
#ifdef DEBUG
    // Needed to have data sent over the serial line, before going to sleep
    // really.
    delay(20);
#endif

    // The below command puts the CPU asleep, that can last a while
    sleep_cpu();

    // We wake-up
    my_detachInterrupt(BTNINT_INT);

    configure_distinct();
    short int btn = -1;
    if (digitalRead(BTN0_PIN) == LOW)
        btn = 0;
    else if (digitalRead(BTN1_PIN) == LOW)
        btn = 1;

    if (btn >= 0)
        button_pressed(btn);
}

void loop() {
    non_infinite_loop();
}

