/*
    Copyright (C) 2018 Open Networking Foundation 

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "utils.h"

// NOTE - This function is not thread-safe.
const char* serial_number_to_str(bcmbal_serial_number* serial_number) {
#define SERIAL_NUMBER_SIZE 12
    static char buff[SERIAL_NUMBER_SIZE+1];

    sprintf(buff, "%c%c%c%c%1X%1X%1X%1X%1X%1X%1X%1X",
            serial_number->vendor_id[0],
            serial_number->vendor_id[1],
            serial_number->vendor_id[2],
            serial_number->vendor_id[3],
            serial_number->vendor_specific[0]>>4 & 0x0f,
            serial_number->vendor_specific[0] & 0x0f,
            serial_number->vendor_specific[1]>>4 & 0x0f,
            serial_number->vendor_specific[1] & 0x0f,
            serial_number->vendor_specific[2]>>4 & 0x0f,
            serial_number->vendor_specific[2] & 0x0f,
            serial_number->vendor_specific[3]>>4 & 0x0f,
            serial_number->vendor_specific[3] & 0x0f);

    return buff;
}
