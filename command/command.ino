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

  **IMPORTANT**
    The PIN D03 is *NOT* plugged on CC1101 (as opposed to most schemas
    instructions that are found on Internet).

  BUTTONS PLUGGING
  ================

  Button 0 (BTN0) plugged on D3 on one end and D4 on the other end
  Button 1 (BTN1) plugged on D3 on one end and D5 on the other end

  LEDS PLUGGING
  =============

  Green led ('OK') plugged on D7
  Red led ('ERROR') plugged on D8

  CREDITS
  =======

  [1] About "more than one button triggering interrupt when pressed"
      Solution was found here:
        https://create.arduino.cc/projecthub/Svizel_pritula/10-buttons-using-1-interrupt-2bd1f8
      Warning, this URL connects buttons to D2, here we use D3 for the buttons
      (D2 is used to manage CC1101).
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

    // Volet 1 (salon)
//#define CODE_BTN0 0x40A2BBAE
//#define CODE_BTN1 0x40A2BBAD
    // Volet 2 (salle à manger)
//#define CODE_BTN0 0x4003894D
//#define CODE_BTN1 0x4003894E
    // Volet 3 (chambre)
//#define CODE_BTN0 0x4078495E
//#define CODE_BTN1 0x4078495D

#include "common.h"

#define SEND_ASK_FOR_ACK  true

#define MYADDR           ADDR0
#define TARGETADDR       ADDR1
#define TXPOWER              1  // 0 = lowpower, 1 = highpower (= long distance)

#define BTNINT_PIN         PB3  // D03 PIN
#define BTNINT_INT       INTF1  // D03 corresponds to interrupt 1
#define BTN0_PIN           PB4  // D04 PIN
#define BTN1_PIN           PB5  // D05 PIN

#define LED_OK_PIN           7
#define LED_ERR_PIN          8
#define LED_DELAY          300

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

    // The below might not be useful, done to be sure we are in a well defined
    // state when leaving setup().
    configure_common();

#ifdef LED_ERR_PIN
    pinMode(LED_ERR_PIN, OUTPUT);
#endif
#ifdef LED_OK_PIN
    pinMode(LED_OK_PIN, OUTPUT);
#endif
}

// FIXME
#include "MemoryFree.h"

byte instr[3];

void button_pressed(short int button) {

    serial_printf("fm (in) = %i\n", freeMemory());

    byte instrval = (button == 0 ? INSTR_OPENALL : INSTR_CLOSEALL);

    for (byte i = 0; i < 3; ++i) {
        instr[i] = instrval;
    }
    byte n;
    byte r = rf.send(TARGETADDR, instr, sizeof(instr), SEND_ASK_FOR_ACK, &n);

    if (r != ERR_OK) {
        serial_printf("Sending error: %i: %s - ack=%s, sent %i time(s)\n",
                r, rf.get_err_string(r), (SEND_ASK_FOR_ACK ? "yes" : "no"), n);

#ifdef LED_ERR_PIN
        digitalWrite(LED_ERR_PIN, HIGH);
        rf.delay_ms(LED_DELAY);
        digitalWrite(LED_ERR_PIN, LOW);
#endif // LED_ERR_PIN

    } else {
        serial_printf("Message send successful - ack=%s - sent %i time(s)\n",
                (SEND_ASK_FOR_ACK ? "yes" : "no"), n);

#ifdef LED_OK_PIN
        digitalWrite(LED_OK_PIN, HIGH);
        rf.delay_ms(LED_DELAY);
        digitalWrite(LED_OK_PIN, LOW);
#endif // LED_OK_PIN

    }

    serial_printf("fm (out) = %i\n", freeMemory());
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

