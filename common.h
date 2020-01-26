// vim:ts=4:sw=4:tw=80:et
/*
  common.h

  Header file shared by command.ino and bridge.ino
*/

/*
  Copyright 2020 Sébastien Millet

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses>.
*/

#ifndef COMMON_H

#define COMMON_H

#define INSTR_FWD433MHZ 0x01

#define ADDR0           0x0B
#define ADDR1           0x5E

// hton = Host To Network
void uint32_hton(byte bytes[4], uint32_t code) {
    for (byte i = 0; i < 4; ++i) {
        bytes[i] = (byte)code & 0xFF;
        code >>= 8;
    }
}

// ntoh = Network to Host
uint32_t uint32_ntoh(byte bytes[4]) {
    uint32_t code = 0;
    for (signed short i = 3; i >= 0; --i) {
        code <<= 8;
        code |= bytes[i];
    }
    return code;
}

#endif // COMMON_H

