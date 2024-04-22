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
#include "vendor_epever.h"

void
Epever_Triron(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		{
			auto int_inputs = mb.read_input_registers(address, 0x3000, 9);
			mqtt_data["PV array rated voltage"].set_number(d_to_s((double)int_inputs[0] / 100, 2));
			mqtt_data["PV array rated current"].set_number(d_to_s((double)int_inputs[1] / 100, 2));
			mqtt_data["PV array rated power"].set_number(d_to_s((double)((uint32_t)int_inputs[3] << 16 | int_inputs[2]) / 100, 2));
			mqtt_data["rated voltage to battery"].set_number(d_to_s((double)int_inputs[4] / 100, 2));
			mqtt_data["rated current to battery"].set_number(d_to_s((double)int_inputs[5] / 100, 2));
			mqtt_data["rated power to battery"].set_number(d_to_s((double)((uint32_t)int_inputs[7] << 16 | int_inputs[6]) / 100, 2));
			switch(int_inputs[8]) {
			case 0x0000:
				mqtt_data["charging mode"] =  "connect/disconnect";
				break;
			case 0x0001:
				mqtt_data["charging mode"] = "PWM";
				break;
			case 0x0002:
				mqtt_data["charging mode"] = "MPPT";
				break;
			}
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x300e, 1);
			mqtt_data["rated current of load"].set_number(d_to_s((double)int_inputs[0] / 100, 2));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x3100, 4);
			mqtt_data["PV voltage"].set_number(d_to_s((double)int_inputs[0] / 100, 2));
			mqtt_data["PV current"].set_number(d_to_s((double)int_inputs[1] / 100, 2));
			mqtt_data["PV power"].set_number(d_to_s((double)((int32_t)int_inputs[3] << 16 | int_inputs[2]) / 100, 2));
		}
		if (0) {
			// value makes no sense, identic to PV power
			auto int_inputs = mb.read_input_registers(address, 0x3106, 2);
			mqtt_data["battery charging power"].set_number(d_to_s((double)((int32_t)int_inputs[1] << 16 | int_inputs[0]) / 100, 2));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x310c, 4);
			mqtt_data["load voltage"].set_number(d_to_s((double)int_inputs[0] / 100, 2));
			mqtt_data["load current"].set_number(d_to_s((double)int_inputs[1] / 100, 2));
			mqtt_data["load power"].set_number(d_to_s((double)((int32_t)int_inputs[3] << 16 | int_inputs[2]) / 100, 2));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x3110, 2);
			mqtt_data["battery temperature"].set_number(d_to_s((double)(int16_t)int_inputs[0] / 100, 2));
			mqtt_data["case temperature"].set_number(d_to_s((double)(int16_t)int_inputs[1] / 100, 2));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x311a, 1);
			mqtt_data["battery charged capacity"].set_number(d_to_s((double)int_inputs[0] / 100, 2));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x3201, 2);
			int state;
			state = (int_inputs[0] >> 2) & 0x3;
			switch(state) {
			case 0x0:
				mqtt_data["charging status"] =  "no charging";
				break;
			case 0x1:
				mqtt_data["charging status"] = "float";
				break;
			case 0x2:
				mqtt_data["charging status"] = "boost";
				break;
			case 0x3:
				mqtt_data["charging status"] = "equalization";
				break;
			}
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x331a, 3);
			mqtt_data["battery voltage"].set_number(d_to_s((double)int_inputs[0] / 100, 2));
			mqtt_data["battery current"].set_number(d_to_s((double)((int32_t)int_inputs[2] << 16 | int_inputs[1]) / 100, 2));
		}
		{
			auto int_inputs = mb.read_holding_registers(address, 0x9000, 15);
			switch(int_inputs[0]) {
			case 0x0000:
				mqtt_data["battery type"] = "user defined";
				break;
			case 0x0001:
				mqtt_data["battery type"] = "sealed";
				break;
			case 0x0002:
				mqtt_data["battery type"] = "GEL";
				break;
			case 0x0003:
				mqtt_data["battery type"] = "flooded";
				break;
			}
			mqtt_data["battery capacity"].set_number(S + int_inputs[1]);
			mqtt_data["temperature compensation coefficient"].set_number(d_to_s((double)int_inputs[2] / 100, 2));
			mqtt_data["high voltage disconnect"].set_number(d_to_s((double)int_inputs[3] / 100, 2));
			mqtt_data["charging limit voltage"].set_number(d_to_s((double)int_inputs[4] / 100, 2));
			mqtt_data["over voltage reconnect"].set_number(d_to_s((double)int_inputs[5] / 100, 2));
			mqtt_data["equalization voltage"].set_number(d_to_s((double)int_inputs[6] / 100, 2));
			mqtt_data["boost voltage"].set_number(d_to_s((double)int_inputs[7] / 100, 2));
			mqtt_data["float voltage"].set_number(d_to_s((double)int_inputs[8] / 100, 2));
			mqtt_data["boost reconnect voltage"].set_number(d_to_s((double)int_inputs[9] / 100, 2));
			mqtt_data["low voltage reconnect"].set_number(d_to_s((double)int_inputs[10] / 100, 2));
			mqtt_data["under voltage recover"].set_number(d_to_s((double)int_inputs[11] / 100, 2));
			mqtt_data["under voltage warning"].set_number(d_to_s((double)int_inputs[12] / 100, 2));
			mqtt_data["low voltage disconnect"].set_number(d_to_s((double)int_inputs[13] / 100, 2));
			mqtt_data["discharging limit voltage"].set_number(d_to_s((double)int_inputs[14] / 100, 2));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x330a, 2);
			mqtt_data["consumed energy"].set_number(d_to_s((double)((int32_t)int_inputs[1] << 16 | int_inputs[0]) / 100, 2));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x3312, 2);
			mqtt_data["generated energy"].set_number(d_to_s((double)((int32_t)int_inputs[1] << 16 | int_inputs[0]) / 100, 2));
		}
	}
}

void
epever_register()
{
	// register devicefunctions
	devfunctions["Epever"]["Triron"] = Epever_Triron;
	devfunctions["Epever"]["Tracer"] = Epever_Triron;
}

