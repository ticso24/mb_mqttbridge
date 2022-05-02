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

static a_refptr<JSON> config;
static AArray<AArray<void (*)(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)>> devfunctions;
static MQTT main_mqtt;

#ifndef timespecsub
#define timespecsub(tsp, usp, vsp)                                      \
        do {                                                            \
                (vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;          \
                (vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;       \
                if ((vsp)->tv_nsec < 0) {                               \
                        (vsp)->tv_sec--;                                \
                        (vsp)->tv_nsec += 1000000000L;                  \
                }                                                       \
        } while (0)
#endif

void
siginit()
{
	struct sigaction sa;

	sa.sa_handler = sighandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGPIPE, &sa, NULL);
}

void
sighandler(int sig)
{

	switch (sig) {
		case SIGPIPE:
		break;
		default:
		break;
	}
}

float
reg_to_f (uint16_t d0, uint16_t d1) {
	union {
		float f;
		uint16_t i[2];
	};
	i[0] = d0;
	i[1] = d1;
	return f;
}

String
d_to_s(double val, int digits)
{
	String ret;
	ret.printf("%.*lf", digits, val);

	return ret;
}

void
Epever_Triron(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		{
			auto int_inputs = mb.read_input_registers(address, 0x3000, 9);
			mqtt_data["PV array rated voltage"].set_number((double)int_inputs[0] / 100);
			mqtt_data["PV array rated current"].set_number((double)int_inputs[1] / 100);
			mqtt_data["PV array rated power"].set_number((double)((uint32_t)int_inputs[3] << 16 | int_inputs[2]) / 100);
			mqtt_data["rated voltage to battery"].set_number((double)int_inputs[4] / 100);
			mqtt_data["rated current to battery"].set_number((double)int_inputs[5] / 100);
			mqtt_data["rated power to battery"].set_number((double)((uint32_t)int_inputs[7] << 16 | int_inputs[6]) / 100);
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
			mqtt_data["rated current of load"].set_number((double)int_inputs[0] / 100);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x3100, 4);
			mqtt_data["PV voltage"].set_number((double)int_inputs[0] / 100);
			mqtt_data["PV current"].set_number((double)int_inputs[1] / 100);
			mqtt_data["PV power"].set_number((double)((int32_t)int_inputs[3] << 16 | int_inputs[2]) / 100);
		}
		if (0) {
			// value makes no sense, identic to PV power
			auto int_inputs = mb.read_input_registers(address, 0x3106, 2);
			mqtt_data["battery charging power"].set_number((double)((int32_t)int_inputs[1] << 16 | int_inputs[0]) / 100);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x310c, 4);
			mqtt_data["load voltage"].set_number((double)int_inputs[0] / 100);
			mqtt_data["load current"].set_number((double)int_inputs[1] / 100);
			mqtt_data["load power"].set_number((double)((int32_t)int_inputs[3] << 16 | int_inputs[2]) / 100);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x3110, 2);
			mqtt_data["battery temperature"].set_number((double)(int16_t)int_inputs[0] / 100);
			mqtt_data["case temperature"].set_number((double)(int16_t)int_inputs[1] / 100);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x311a, 1);
			mqtt_data["battery charged capacity"].set_number((double)int_inputs[0] / 100);
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
			mqtt_data["battery voltage"].set_number((double)int_inputs[0] / 100);
			mqtt_data["battery current"].set_number((double)((int32_t)int_inputs[2] << 16 | int_inputs[1]) / 100);
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
			mqtt_data["temperature compensation coefficient"].set_number((double)int_inputs[2] / 100);
			mqtt_data["high voltage disconnect"].set_number((double)int_inputs[3] / 100);
			mqtt_data["charging limit voltage"].set_number((double)int_inputs[4] / 100);
			mqtt_data["over voltage reconnect"].set_number((double)int_inputs[5] / 100);
			mqtt_data["equalization voltage"].set_number((double)int_inputs[6] / 100);
			mqtt_data["boost voltage"].set_number((double)int_inputs[7] / 100);
			mqtt_data["float voltage"].set_number((double)int_inputs[8] / 100);
			mqtt_data["boost reconnect voltage"].set_number((double)int_inputs[9] / 100);
			mqtt_data["low voltage reconnect"].set_number((double)int_inputs[10] / 100);
			mqtt_data["under voltage recover"].set_number((double)int_inputs[11] / 100);
			mqtt_data["under voltage warning"].set_number((double)int_inputs[12] / 100);
			mqtt_data["low voltage disconnect"].set_number((double)int_inputs[13] / 100);
			mqtt_data["discharging limit voltage"].set_number((double)int_inputs[14] / 100);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x330a, 2);
			mqtt_data["consumed energy"].set_number((double)((int32_t)int_inputs[1] << 16 | int_inputs[0]) / 100);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x3312, 2);
			mqtt_data["generated energy"].set_number((double)((int32_t)int_inputs[1] << 16 | int_inputs[0]) / 100);
		}
	}
}

void
eastron_sdm630(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		{
			auto int_inputs = mb.read_input_registers(address, 0x0000, 2 * 3);
			mqtt_data["A phase voltage"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["B phase voltage"].set_number((double)reg_to_f(int_inputs[3], int_inputs[2]));
			mqtt_data["C phase voltage"].set_number((double)reg_to_f(int_inputs[5], int_inputs[4]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0006, 2 * 3);
			mqtt_data["A phase current"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["B phase current"].set_number((double)reg_to_f(int_inputs[3], int_inputs[2]));
			mqtt_data["C phase current"].set_number((double)reg_to_f(int_inputs[5], int_inputs[4]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x000c, 2 * 3);
			mqtt_data["A phase active power"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["B phase active power"].set_number((double)reg_to_f(int_inputs[3], int_inputs[2]));
			mqtt_data["C phase active power"].set_number((double)reg_to_f(int_inputs[5], int_inputs[4]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0012, 2 * 3);
			mqtt_data["A phase apparent power"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["B phase apparent power"].set_number((double)reg_to_f(int_inputs[3], int_inputs[2]));
			mqtt_data["C phase apparent power"].set_number((double)reg_to_f(int_inputs[5], int_inputs[4]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0018, 2 * 3);
			mqtt_data["A phase reactive power"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["B phase reactive power"].set_number((double)reg_to_f(int_inputs[3], int_inputs[2]));
			mqtt_data["C phase reactive power"].set_number((double)reg_to_f(int_inputs[5], int_inputs[4]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x001e, 2 * 3);
			mqtt_data["A phase power factor"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["B phase power factor"].set_number((double)reg_to_f(int_inputs[3], int_inputs[2]));
			mqtt_data["C phase power factor"].set_number((double)reg_to_f(int_inputs[5], int_inputs[4]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0024, 2 * 3);
			mqtt_data["A phase angle"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["B phase angle"].set_number((double)reg_to_f(int_inputs[3], int_inputs[2]));
			mqtt_data["C phase angle"].set_number((double)reg_to_f(int_inputs[5], int_inputs[4]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x003c, 2 * 3);
			mqtt_data["total reactive power"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["total power factor"].set_number((double)reg_to_f(int_inputs[7], int_inputs[6]));
			mqtt_data["total angle"].set_number((double)reg_to_f(int_inputs[9], int_inputs[8]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0046, 2 * 5);
			mqtt_data["frequency"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["forward active energy"].set_number((double)reg_to_f(int_inputs[3], int_inputs[2]));
			mqtt_data["reverse active energy"].set_number((double)reg_to_f(int_inputs[5], int_inputs[4]));
			mqtt_data["forward reactive energy"].set_number((double)reg_to_f(int_inputs[7], int_inputs[6]));
			mqtt_data["reverse reactive energy"].set_number((double)reg_to_f(int_inputs[9], int_inputs[8]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0054, 2 * 1);
			mqtt_data["total active power"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0064, 2 * 1);
			mqtt_data["total apparent power"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x015a, 2 * 6);
			mqtt_data["A phase forward active energy"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["B phase forward active energy"].set_number((double)reg_to_f(int_inputs[3], int_inputs[2]));
			mqtt_data["C phase forward active energy"].set_number((double)reg_to_f(int_inputs[5], int_inputs[4]));
			mqtt_data["A phase reverse active energy"].set_number((double)reg_to_f(int_inputs[7], int_inputs[6]));
			mqtt_data["B phase reverse active energy"].set_number((double)reg_to_f(int_inputs[9], int_inputs[8]));
			mqtt_data["C phase reverse active energy"].set_number((double)reg_to_f(int_inputs[11], int_inputs[10]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x016c, 2 * 6);
			mqtt_data["A phase forward reactive energy"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["B phase forward reactive energy"].set_number((double)reg_to_f(int_inputs[3], int_inputs[2]));
			mqtt_data["C phase forward reactive energy"].set_number((double)reg_to_f(int_inputs[5], int_inputs[4]));
			mqtt_data["A phase reverse reactive energy"].set_number((double)reg_to_f(int_inputs[7], int_inputs[6]));
			mqtt_data["B phase reverse reactive energy"].set_number((double)reg_to_f(int_inputs[9], int_inputs[8]));
			mqtt_data["C phase reverse reactive energy"].set_number((double)reg_to_f(int_inputs[11], int_inputs[10]));
		}
	}
}

void
eastron_sdm220(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		{
			auto int_inputs = mb.read_input_registers(address, 0x0000, 2 * 1);
			mqtt_data["A phase voltage"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0006, 2 * 1);
			mqtt_data["A phase current"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x000c, 2 * 1);
			mqtt_data["A phase active power"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["total active power"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0012, 2 * 1);
			mqtt_data["A phase apparent power"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["total apparent power"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0018, 2 * 1);
			mqtt_data["A phase reactive power"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["total reactive power"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x001e, 2 * 1);
			mqtt_data["A phase power factor"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["total power factor"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0024, 2 * 1);
			mqtt_data["A phase angle"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["total angle"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0046, 2 * 5);
			mqtt_data["frequency"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["forward active energy"].set_number((double)reg_to_f(int_inputs[3], int_inputs[2]));
			mqtt_data["reverse active energy"].set_number((double)reg_to_f(int_inputs[5], int_inputs[4]));
			mqtt_data["forward reactive energy"].set_number((double)reg_to_f(int_inputs[7], int_inputs[6]));
			mqtt_data["reverse reactive energy"].set_number((double)reg_to_f(int_inputs[9], int_inputs[8]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x00c8, 2 * 3);
			mqtt_data["AB line voltage"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
			mqtt_data["BC line voltage"].set_number((double)reg_to_f(int_inputs[3], int_inputs[2]));
			mqtt_data["CA line voltage"].set_number((double)reg_to_f(int_inputs[5], int_inputs[4]));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x00c8, 2 * 3);
			mqtt_data["AB line voltage"].set_number((double)reg_to_f(int_inputs[1], int_inputs[0]));
		}
	}
}

void
ZGEJ_powermeter(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		{
			auto int_inputs = mb.read_input_registers(address, 0x0018, 2 * 34);
			mqtt_data["A phase voltage"].set_number((double)reg_to_f(int_inputs[0], int_inputs[1]));
			mqtt_data["B phase voltage"].set_number((double)reg_to_f(int_inputs[2], int_inputs[3]));
			mqtt_data["C phase voltage"].set_number((double)reg_to_f(int_inputs[4], int_inputs[5]));
			mqtt_data["AB line voltage"].set_number((double)reg_to_f(int_inputs[6], int_inputs[7]));
			mqtt_data["BC line voltage"].set_number((double)reg_to_f(int_inputs[8], int_inputs[9]));
			mqtt_data["CA line voltage"].set_number((double)reg_to_f(int_inputs[10], int_inputs[11]));
			mqtt_data["A phase current"].set_number((double)reg_to_f(int_inputs[12], int_inputs[13]));
			mqtt_data["B phase current"].set_number((double)reg_to_f(int_inputs[14], int_inputs[15]));
			mqtt_data["C phase current"].set_number((double)reg_to_f(int_inputs[16], int_inputs[17]));
			mqtt_data["A phase active power"].set_number((double)reg_to_f(int_inputs[18], int_inputs[19]));
			mqtt_data["B phase active power"].set_number((double)reg_to_f(int_inputs[20], int_inputs[21]));
			mqtt_data["C phase active power"].set_number((double)reg_to_f(int_inputs[22], int_inputs[23]));
			mqtt_data["total active power"].set_number((double)reg_to_f(int_inputs[24], int_inputs[25]));
			mqtt_data["A phase reactive power"].set_number((double)reg_to_f(int_inputs[26], int_inputs[27]));
			mqtt_data["B phase reactive power"].set_number((double)reg_to_f(int_inputs[28], int_inputs[29]));
			mqtt_data["C phase reactive power"].set_number((double)reg_to_f(int_inputs[30], int_inputs[31]));
			mqtt_data["total reactive power"].set_number((double)reg_to_f(int_inputs[32], int_inputs[33]));
			mqtt_data["A phase apparent power"].set_number((double)reg_to_f(int_inputs[34], int_inputs[35]));
			mqtt_data["B phase apparent power"].set_number((double)reg_to_f(int_inputs[36], int_inputs[37]));
			mqtt_data["C phase apparent power"].set_number((double)reg_to_f(int_inputs[38], int_inputs[39]));
			mqtt_data["total apparent power"].set_number((double)reg_to_f(int_inputs[40], int_inputs[41]));
			mqtt_data["A phase power factor"].set_number((double)reg_to_f(int_inputs[42], int_inputs[43]));
			mqtt_data["B phase power factor"].set_number((double)reg_to_f(int_inputs[44], int_inputs[45]));
			mqtt_data["C phase power factor"].set_number((double)reg_to_f(int_inputs[46], int_inputs[47]));
			mqtt_data["total power factor"].set_number((double)reg_to_f(int_inputs[48], int_inputs[49]));
			mqtt_data["frequency"].set_number((double)reg_to_f(int_inputs[50], int_inputs[51]));
			mqtt_data["forward active energy 2"].set_number((double)reg_to_f(int_inputs[52], int_inputs[53]));
			mqtt_data["reverse active energy 2"].set_number((double)reg_to_f(int_inputs[54], int_inputs[55]));
			mqtt_data["forward reactive energy 2"].set_number((double)reg_to_f(int_inputs[56], int_inputs[57]));
			mqtt_data["reverse reactive energy 2"].set_number((double)reg_to_f(int_inputs[58], int_inputs[59]));
			mqtt_data["forward active energy"].set_number((double)reg_to_f(int_inputs[60], int_inputs[61]));
			mqtt_data["reverse active energy"].set_number((double)reg_to_f(int_inputs[62], int_inputs[63]));
			mqtt_data["forward reactive energy"].set_number((double)reg_to_f(int_inputs[64], int_inputs[65]));
			mqtt_data["reverse reactive energy"].set_number((double)reg_to_f(int_inputs[66], int_inputs[67]));
		}
	}
}

void
eth_tpr(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/cmd") {
			JSON json;
			json.parse(rxbuf[i].message);
			Array<String> keys = json.get_object().getkeys();
			for (int64_t j = 0; j <= keys.max; j++) {
				String key = keys[j];
				if (key == "relais0") {
					bool val = json[key];
					mb.write_coil(address, 0, val);
				}
				if (key == "relais1") {
					bool val = json[key];
					mb.write_coil(address, 1, val);
				}
			}
		}
	}

	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 4);
		Array<JSON> inputs;

		inputs[0] = bin_inputs[0];
		inputs[1] = bin_inputs[1];
		inputs[2] = bin_inputs[2];
		inputs[3] = bin_inputs[3];
		mqtt_data["input"] = inputs;
	}
	{
		auto bin_coils = mb.read_coils(address, 0, 2);

		Array<JSON> relais;
		relais[0] = bin_coils[0];
		relais[1] = bin_coils[1];
		mqtt_data["relais"] = relais;
	}
}

void
eth_tpr_ldr(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/relais0") {
			bool val = (rxbuf[i].message == "0") ? 0 : 1;
			mb.write_coil(address, 0, val);
		}
		if (rxbuf[i].topic == maintopic + "/relais1") {
			bool val = (rxbuf[i].message == "0") ? 0 : 1;
			mb.write_coil(address, 1, val);
		}
		if (rxbuf[i].topic == maintopic + "/counter_autoreset4") {
			bool val = (rxbuf[i].message == "0") ? 0 : 1;
			mb.write_coil(address, 2, val);
		}
		if (rxbuf[i].topic == maintopic + "/counter_autoreset5") {
			bool val = (rxbuf[i].message == "0") ? 0 : 1;
			mb.write_coil(address, 3, val);
		}
		if (rxbuf[i].topic == maintopic + "/counter_autoreset6") {
			bool val = (rxbuf[i].message == "0") ? 0 : 1;
			mb.write_coil(address, 4, val);
		}
		if (rxbuf[i].topic == maintopic + "/counter_autoreset7") {
			bool val = (rxbuf[i].message == "0") ? 0 : 1;
			mb.write_coil(address, 5, val);
		}
	}

	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 4);
		Array<JSON> inputs;

		inputs[0] = bin_inputs[0];
		inputs[1] = bin_inputs[1];
		inputs[2] = bin_inputs[2];
		inputs[3] = bin_inputs[3];
		mqtt_data["input"] = inputs;
	}
	{
		auto bin_coils = mb.read_coils(address, 0, 2);

		Array<JSON> relais;
		relais[0] = bin_coils[0];
		relais[1] = bin_coils[1];
		mqtt_data["relais"] = relais;
	}
	{
		auto int_inputs = mb.read_input_registers(address, 0, 14);

		{
			// 16bit counter - should verify for rollover and restart
			Array<JSON> counters;
			counters[0].set_number(S + int_inputs[0]);
			counters[1].set_number(S + int_inputs[1]);
			counters[2].set_number(S + int_inputs[2]);
			counters[3].set_number(S + int_inputs[3]);

			// 32 bit counter - should verify for restart if autoreset is not enabled
			{
				uint32_t tmp = (uint32_t)int_inputs[6] | (uint32_t)int_inputs[7] << 16;
				counters[4].set_number(S + tmp);
			}
			{
				uint32_t tmp = (uint32_t)int_inputs[8] | (uint32_t)int_inputs[9] << 16;
				counters[5].set_number(S + tmp);
			}
			{
				uint32_t tmp = (uint32_t)int_inputs[10] | (uint32_t)int_inputs[11] << 16;
				counters[6].set_number(S + tmp);
			}
			{
				uint32_t tmp = (uint32_t)int_inputs[12] | (uint32_t)int_inputs[13] << 16;
				counters[7].set_number(S + tmp);
			}

			mqtt_data["counters"] = counters;
		}

		{
			Array<JSON> ldrs;
			ldrs[0].set_number(S + int_inputs[4]);
			// XXX check firmware version for functional LDR1 input
			ldrs[1].set_number(S + int_inputs[5]);
			mqtt_data["ldrs"] = ldrs;
		}

	}

	if (dev_cfg.exists("DS18B20")) {
		Array<JSON> ds18b20;
		int64_t max_sensor = dev_cfg["DS18B20"].get_array().max;
		for (int64_t i = 0; i <= max_sensor; i++) {
			int16_t sensor_register = dev_cfg["DS18B20"][i]["register"].get_numstr().getll();
			try {
				uint16_t value = mb.read_input_register(address, sensor_register);
				double temp = (double)value / 16;
				AArray<JSON> sensor;
				sensor["temperature"].set_number(S + temp);
				ds18b20[i] = sensor;
			} catch (...) {
			}
		}
		mqtt_data["ds18b20"] = ds18b20;
	}
}

void
rs485_jalousie(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 8);

		Array<JSON> inputs;
		for (int64_t i = 0; i < 8; i++) {
			inputs[i] = bin_inputs[i];
		}
		mqtt_data["input"] = inputs;
	}

	for (int64_t i = 0; i <= rxbuf.max; i++) {
		for (int64_t n = 0; n < 3; n++) {
			if (rxbuf[i].topic == maintopic + "/blind" + n) {
				if (rxbuf[i].message == "stop") {
					mb.write_coil(address, n * 2, 0);
					usleep(100000);
					mb.write_coil(address, n * 2 + 1, 0);
				} else if (rxbuf[i].message == "up") {
					mb.write_coil(address, n * 2, 0);
					usleep(100000);
					mb.write_coil(address, n * 2 + 1, 0);
					usleep(100000);
					mb.write_coil(address, n * 2, 1);
				} else if (rxbuf[i].message == "down") {
					mb.write_coil(address, n * 2, 0);
					usleep(100000);
					mb.write_coil(address, n * 2 + 1, 1);
					usleep(100000);
					mb.write_coil(address, n * 2, 1);
				}
				bool val = (rxbuf[i].message == "0") ? 0 : 1;
				mb.write_coil(address, n, val);
			}
		}
	}
}

void
rs485_relais6(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	// XXX no counter support yet
	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 8);

		Array<JSON> inputs;
		for (int64_t i = 0; i < 8; i++) {
			inputs[i] = bin_inputs[i];
		}
		mqtt_data["input"] = inputs;
	}

	for (int64_t i = 0; i <= rxbuf.max; i++) {
		for (int64_t n = 0; n < 6; n++) {
			if (rxbuf[i].topic == maintopic + "/relais" + n) {
				bool val = (rxbuf[i].message == "0") ? 0 : 1;
				mb.write_coil(address, n, val);
			}
		}
	}
}

void
rs485_shtc3(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	auto int_inputs = mb.read_input_registers(address, 0, 2);
	double temp = (double)(int16_t)int_inputs[0] / 10.0;
	double humid = (double)int_inputs[1] / 10.0;
	Array<JSON> shtc;
	AArray<JSON> sensor;
	sensor["temperature"].set_number(S + temp);
	sensor["humidity"].set_number(S + humid);
	shtc[0] = sensor;
	mqtt_data["SHTC3"] = shtc;
}

void
rs485_laserdistance(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	auto int_inputs = mb.read_input_registers(address, 0, 3);
	{
		Array<JSON> weights;
		int32_t tmp = (uint32_t)int_inputs[0] | (uint32_t)int_inputs[1] << 16;
		weights[0].set_number(S + tmp);
		mqtt_data["weight"] = weights;
	}
	{
		Array<JSON> distances;
		distances[0].set_number(S + int_inputs[2]);
		mqtt_data["distance"] = distances;
	}
}

void
eth_io88(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 8);

		Array<JSON> inputs;
		for (int i = 0; i < 8; i++) {
			inputs[i] = bin_inputs[i];
		}
		mqtt_data["input"] = inputs;
	}

	for (int64_t i = 0; i <= rxbuf.max; i++) {
		for (int r = 0; r < 8; r++) {
			if (rxbuf[i].topic == maintopic + "/output" + r) {
				bool val = (rxbuf[i].message == "0") ? 0 : 1;
				mb.write_coil(address, r, val);
			}
		}
	}
}

void
rs485_io88(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 8);

		Array<JSON> inputs;
		for (int i = 0; i < 8; i++) {
			inputs[i] = bin_inputs[i];
		}
		mqtt_data["input"] = inputs;
	}

	for (int64_t i = 0; i <= rxbuf.max; i++) {
		for (int r = 0; r < 8; r++) {
			if (rxbuf[i].topic == maintopic + "/output" + r) {
				bool val = (rxbuf[i].message == "0") ? 0 : 1;
				mb.write_coil(address, r, val);
			}
		}
		for (int r = 0; r < 8; r++) {
			if (rxbuf[i].topic == maintopic + "/pwm" + r) {
				int16_t val = rxbuf[i].message.getll();
				mb.write_register(address, r, val);
			}
		}
	}
}

void
rs485_adc_dac(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		auto int_inputs = mb.read_input_registers(address, 0, 10);
		Array<JSON> adc;
		adc[0].set_number(S + int_inputs[0]);
		adc[1].set_number(S + int_inputs[1]);
		adc[2].set_number(S + int_inputs[2]);
		adc[3].set_number(S + int_inputs[3]);
		mqtt_data["adc"] = adc;
		mqtt_data["ref"].set_number(S + int_inputs[9]);
	}

	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/dac0") {
			int16_t val = rxbuf[i].message.getll();
			mb.write_register(address, 0, val);
		}
		if (rxbuf[i].topic == maintopic + "/dac1") {
			int16_t val = rxbuf[i].message.getll();
			mb.write_register(address, 1, val);
		}
	}
}

void
rs485_rfid125_disp(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		auto int_inputs = mb.read_input_registers(address, 0, 11);
		if (int_inputs[0] != 0) {
			String key;
			String tmp;
			uint8_t nibble;
			for (int i = 1; i <= int_inputs[0]; i++) {
				nibble = (int_inputs[i] >> 4) & 0x0f;
				tmp.printf("%c", (nibble > 9) ? 'a' - 10 + nibble : '0' + nibble);
				key += tmp;
				nibble = int_inputs[i] & 0x0f;
				tmp.printf("%c", (nibble > 9) ? 'a' - 10 + nibble : '0' + nibble);
				key += tmp;
				if (i < int_inputs[0]) {
					key += ":";
				}
			}
			mqtt_data["key"] = key;
		}
	}
}

void
rs485_rfid125(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		auto int_inputs = mb.read_input_registers(address, 0, 11);
		if (int_inputs[0] != 0) {
			String key;
			String tmp;
			uint8_t nibble;
			for (int i = 1; i <= int_inputs[0]; i++) {
				nibble = (int_inputs[i] >> 4) & 0x0f;
				tmp.printf("%c", (nibble > 9) ? 'a' - 10 + nibble : '0' + nibble);
				key += tmp;
				nibble = int_inputs[i] & 0x0f;
				tmp.printf("%c", (nibble > 9) ? 'a' - 10 + nibble : '0' + nibble);
				key += tmp;
				if (i < int_inputs[0]) {
					key += ":";
				}
			}
			mqtt_data["key"] = key;
		}
	}
}

void
rs485_thermocouple(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 24);
		auto int_inputs = mb.read_input_registers(address, 0, 16);
		Array<JSON> sensors;
		for (int i = 0; i < 8; i++) {
			AArray<JSON> sensor;

			sensor["open_error"] = bin_inputs[i * 3];
			sensor["gnd_short"] = bin_inputs[i * 3 + 1];
			sensor["vcc_short"] = bin_inputs[i * 3 + 2];
			sensor["temperature"].set_number(S + (int16_t)int_inputs[i * 2]);
			sensor["cold_temperature"].set_number(S + (int16_t)int_inputs[ i * 2 + 1]);
			sensors[i] = sensor;
		}
		mqtt_data["thermocouple"] = sensors;
	}
}

void
rs485_chamberpump(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		auto int_inputs = mb.read_input_registers(address, 0, 9);
		{
			Array<JSON> adc;
			adc[0].set_number(S + int_inputs[0]);
			adc[1].set_number(S + int_inputs[1]);
			adc[2].set_number(S + int_inputs[2]);
			adc[3].set_number(S + int_inputs[3]);
			mqtt_data["adc"] = adc;
		}
		{
			String state;
			switch(int_inputs[4]) {
			case 0:
				state = "idle";
				break;
			case 1:
				state = "filling";
				break;
			case 2:
				state = "full";
				break;
			case 3:
				state = "emptying";
				break;
			case 4:
				state = "empty";
				break;
			case 5:
				state = "unknown";
			}
			mqtt_data["state"] = state;
		}
		{
			uint32_t tmp = (uint32_t)int_inputs[5] | (uint32_t)int_inputs[6] << 16;
			mqtt_data["cyclecounter"].set_number(S + tmp);
		}
		{
			uint32_t tmp = (uint32_t)int_inputs[7] | (uint32_t)int_inputs[8] << 16;
			mqtt_data["cycletime"].set_number(S + tmp);
		}
	}

	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/triggerlevel_top") {
			uint16_t val = rxbuf[i].message.getll();
			mb.write_register(address, 0, val);
		}
		if (rxbuf[i].topic == maintopic + "/triggerlevel_bottom") {
			uint16_t val = rxbuf[i].message.getll();
			mb.write_register(address, 1, val);
		}
	}
}

void*
ModbusLoop(void * arg)
{
	Array<AArray<String>> devdata;
	int64_t bus = *(int64_t*)arg;
	delete (int64_t*)arg;

	a_refptr<JSON> my_config = config;
	JSON& cfg = *my_config.get();
	JSON& bus_cfg = cfg["modbuses"][bus];
	String host = bus_cfg["host"];
	String port = bus_cfg["port"];
	String threadname = String() + "mb[" + host + "]@" + port;
	pthread_setname_np(pthread_self(), threadname.c_str());
	Modbus mb(host, port);
	if (bus_cfg.exists("ignore_sequence")) {
		bool ignore_sequence;
		ignore_sequence = bus_cfg["ignore_sequence"];
		mb.set_ignore_sequence(ignore_sequence);
	}

	Array<MQTT> dev_mqtts;
	Array<struct timespec> lasttime;

	for (int64_t dev = 0; dev <= bus_cfg["devices"].get_array().max; dev++) {
		clock_gettime(CLOCK_MONOTONIC, &lasttime[dev]);
	}

	for(;;) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		for (int64_t dev = 0; dev <= bus_cfg["devices"].get_array().max; dev++) {
			JSON& dev_cfg = bus_cfg["devices"][dev];
			JSON mqtt_data;
			int qos = 0;
			if (dev_cfg.exists("qos")) {
				qos = dev_cfg["qos"].get_numstr().getll();
			}

			String maintopic = dev_cfg["maintopic"];
			uint8_t address = dev_cfg["address"].get_numstr().getll();
			if (!dev_mqtts.exists(dev)) {
				MQTT& mqtt = dev_mqtts[dev];
				JSON& mqtt_cfg = cfg["mqtt"];
				String id = mqtt_cfg["id"];
				if (!id.empty()) {
					id += S + "[" + host + "]" + port + "/" + address;
				}
				mqtt.id = id;
				String host = mqtt_cfg["host"];
				mqtt.host = host;
				String port = mqtt_cfg["port"];
				mqtt.port = port.getll();
				String username = mqtt_cfg["username"];
				mqtt.username = username;
				String password = mqtt_cfg["password"];
				mqtt.password = password;
				mqtt.maintopic = maintopic;
				mqtt.rxbuf_enable = true;
				mqtt.connect();
			};
			MQTT& mqtt = dev_mqtts[dev];
			try {
				if (!devdata[dev].exists("vendor")) {
					String vendor;
					if (dev_cfg.exists("vendor")) {
						String tmp = dev_cfg["vendor"];
						vendor = tmp;
					} else {
						vendor = mb.identification(address, 0);
					}
					devdata[dev]["vendor"] = vendor;
				}
				String vendor = devdata[dev]["vendor"];
				if (!devdata[dev].exists("product")) {
					String product;
					if (dev_cfg.exists("product")) {
						String tmp = dev_cfg["product"];
						product = tmp;
					} else {
						product = mb.identification(address, 1);
					}
					devdata[dev]["product"] = product;
				}
				String product = devdata[dev]["product"];
				if (!product.empty() && !vendor.empty()) {
					if (!devfunctions.exists(vendor) || !devfunctions[vendor].exists(product)) {
						throw(Error(S + "unknown product " + vendor + " " + product));
					}
				}
				if (!devdata[dev].exists("version")) {
					String version;
					if (dev_cfg.exists("version")) {
						String tmp = dev_cfg["version"];
						version = tmp;
					} else {
						version = mb.identification(address, 2);
					}
					devdata[dev]["version"] = version;
				}
				if (devdata[dev]["maintopic"].empty()) {
					// at this stage we know the device and can handle incoming data
					devdata[dev]["maintopic"] = maintopic;
					String product = devdata[dev]["product"];
					if (!product.empty()) {
						// only suscribe, if we have a handler function
						mqtt.subscribe(maintopic + "/cmd");
					}
				}
				bool poll = true;
				double intervall = 1.0;
				if (dev_cfg.exists("min_pollintervall")) {
					String tmp = dev_cfg["min_pollintervall"].get_numstr();
					intervall = (double)tmp.getd();
				}
				struct timespec timespecdiff;
				timespecsub(&now, &lasttime[dev], &timespecdiff);
				double timediff = (double)(timespecdiff.tv_sec) + (double)(timespecdiff.tv_nsec) / 1000000000;
				if (timediff < intervall) {
					poll = false;
				}
				if (poll) {
					if (devdata[dev].exists("vendor")) {
						mqtt_data["vendor"] = devdata[dev]["vendor"];
					}
					if (devdata[dev].exists("product")) {
						mqtt_data["product"] = devdata[dev]["product"];
					}
					if (devdata[dev].exists("version")) {
						mqtt_data["version"] = devdata[dev]["version"];
					}
					if (!product.empty() && !vendor.empty()) {
						auto rxbuf = mqtt.get_rxbuf();
						(*devfunctions[vendor][product])(mb, rxbuf, mqtt_data, address, maintopic, devdata[dev], dev_cfg);
					}
					mqtt.publish(maintopic + "/data", mqtt_data.generate(), false, false, qos);
					mqtt.publish(maintopic + "/status", "online", false, false, qos);
					lasttime[dev] = now;
				}
			} catch(...) {
				mqtt.publish(maintopic + "/status", "offline", false, false, qos);
				sleep(1);
			}
		}

		usleep(10000); // sleep 10ms
	}

	return NULL;
}

int
main(int argc, char *argv[]) {
	String configfile = "/usr/local/etc/mb_mqttbridge.json";
	String pidfile = "/var/run/mb_mqttbridge.pid";

	openlog(argv[0], LOG_PID, LOG_LOCAL0);

	int ch;
	bool debug = false;

	while ((ch = getopt(argc, argv, "c:dp:")) != -1) {
		switch (ch) {
		case 'c':
			configfile = optarg;
			break;
		case 'd':
			debug = true;
			break;
		case 'p':
			pidfile = optarg;
			break;
		case '?':
			default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (!debug) {
		daemon(0, 0);
	}

	// write pidfile
	{
		pid_t pid;
		pid = getpid();
		File pfile;
		pfile.open(pidfile, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		pfile.write(String(pid) + "\n");
		pfile.close();
	}

	{
		File f;
		f.open(configfile, O_RDONLY);
		String json(f);
		config = new(JSON);
		config->parse(json);
	}

	mosquitto_lib_init();

	a_refptr<JSON> my_config = config;
	JSON& cfg = *my_config.get();

	if (cfg.exists("mqtt")) {
		JSON& mqtt_cfg = cfg["mqtt"];
		String id = mqtt_cfg["id"];
		main_mqtt.id = id;
		String host = mqtt_cfg["host"];
		main_mqtt.host = host;
		String port = mqtt_cfg["port"];
		main_mqtt.port = port.getll();
		String username = mqtt_cfg["username"];
		main_mqtt.username = username;
		String password = mqtt_cfg["password"];
		main_mqtt.password = password;
		String maintopic = mqtt_cfg["maintopic"];
		main_mqtt.maintopic = maintopic;
		main_mqtt.rxbuf_enable = true;
		main_mqtt.autoonline = true;
		main_mqtt.connect();
		String willtopic = maintopic + "/status";
		main_mqtt.publish(willtopic, "online", true);
		JSON mqtt_data;
		mqtt_data["product"] = String("mb_mqttbridge");
		mqtt_data["version"] = String("0.9");
		main_mqtt.publish(maintopic + "/data", mqtt_data.generate(), true);
	} else {
		printf("no mqtt setup in config\n");
		exit(1);
	}

	if (!cfg.exists("modbuses")) {
		printf("no modbus setup in config\n");
		exit(1);
	}

	// register devicefunctions
	devfunctions["Bernd Walter Computer Technology"]["Ethernet-MB twin power relay / 4ch input"] = eth_tpr;
	devfunctions["Bernd Walter Computer Technology"]["Ethernet-MB RS485 / twin power relay / 4ch input / LDR / DS18B20"] = eth_tpr_ldr;
	devfunctions["Bernd Walter Computer Technology"]["MB 3x jalousie actor / 8ch input"] = rs485_jalousie;
	devfunctions["Bernd Walter Computer Technology"]["MB 6x power relay / 8ch input"] = rs485_relais6;
	devfunctions["Bernd Walter Computer Technology"]["RS485-SHTC3"] = rs485_shtc3;
	devfunctions["Bernd Walter Computer Technology"]["RS485-Laserdistance-Weight"] = rs485_laserdistance;
	devfunctions["Bernd Walter Computer Technology"]["RS485-IO88"] = rs485_io88;
	devfunctions["Bernd Walter Computer Technology"]["ETH-IO88"] = eth_io88;
	devfunctions["Bernd Walter Computer Technology"]["MB ADC DAC"] = rs485_adc_dac;
	devfunctions["Bernd Walter Computer Technology"]["125kHz RFID Reader / Display"] = rs485_rfid125_disp;
	devfunctions["Bernd Walter Computer Technology"]["125kHz RFID Reader / Writer-Beta"] = rs485_rfid125;
	devfunctions["Bernd Walter Computer Technology"]["RS485-THERMOCOUPLE"] = rs485_thermocouple;
	devfunctions["Bernd Walter Computer Technology"]["RS485-Chamberpump"] = rs485_chamberpump;
	devfunctions["Epever"]["Triron"] = Epever_Triron;
	devfunctions["Epever"]["Tracer"] = Epever_Triron;
	devfunctions["Shanghai Chujin Electric"]["Panel Powermeter"] = ZGEJ_powermeter;
	devfunctions["Eastron"]["SDM220"] = eastron_sdm220;
	devfunctions["Eastron"]["SDM630"] = eastron_sdm630;

	// start poll loops
	JSON& modbuses = cfg["modbuses"];
	for (int64_t bus = 0; bus <= modbuses.get_array().max; bus++) {
		int64_t* busno = new(int64_t);
		*busno = bus;
		pthread_t modbus_thread;
		pthread_create(&modbus_thread, NULL, ModbusLoop, busno);
		pthread_detach(modbus_thread);
	}

	for (;;) {
		sleep(10);
	}
	return 0;
}

void
usage(void) {

	printf("usage: mb_mqttbridge [-d] [-c configfile] [-p pidfile]\n");
	exit(1);
}

