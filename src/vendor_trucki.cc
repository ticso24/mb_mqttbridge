/*
 * Copyright (c) 2020 Bernd Walter Computer Technology
 * http://www.bwct.de
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "main.h"
#include <bwctmb/bwctmb.h>
#include <mosquitto.h>
#include "mqtt.h"
#include "vendor_trucki.h"

void
trucki_sun1000(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/cmd") {
			JSON json;
			json.parse(rxbuf[i].message);
			for (auto const& [key, value]: json.get_object()) {
				switch(std::hash<String>{}(key)) {
				case cstrhash("set power"):
					if (value.is_number()) {
						double tmp = value.get_numstr().getd();
						tmp = tmp * 10.0;
						uint16_t val = tmp;
						mb.write_register(address, 0, val);
					}
					break;
				}
			}
		}
	}

	{
		auto int_inputs = mb.read_holding_registers(address, 0, 8);
		mqtt_data["set power"].set_number(d_to_s((double)int_inputs[0] / 10.0, 1));
		mqtt_data["output power"].set_number(d_to_s((double)int_inputs[1] / 10.0, 1));
		mqtt_data["grid voltage"].set_number(d_to_s((double)int_inputs[2] / 10.0, 1));
		mqtt_data["battery voltage"].set_number(d_to_s((double)int_inputs[3] / 10.0, 1));
		mqtt_data["DAC value"].set_number(d_to_s((double)int_inputs[4], 0));
		mqtt_data["temperature"].set_number(d_to_s((double)int_inputs[7], 0));
	}
}

void
trucki_register()
{
	// register devicefunctions
	devfunctions["Trucki"]["SUN1000"] = trucki_sun1000;
	devfunctions["Trucki"]["SUN2000"] = trucki_sun1000;
}
