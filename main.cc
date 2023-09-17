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
empty(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
}

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
eastron_sdm630(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		{
			auto int_inputs = mb.read_input_registers(address, 0x0000, 2 * 3);
			mqtt_data["A phase voltage"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["B phase voltage"].set_number(d_to_s(reg_to_f(int_inputs[3], int_inputs[2]), 3));
			mqtt_data["C phase voltage"].set_number(d_to_s(reg_to_f(int_inputs[5], int_inputs[4]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0006, 2 * 3);
			mqtt_data["A phase current"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["B phase current"].set_number(d_to_s(reg_to_f(int_inputs[3], int_inputs[2]), 3));
			mqtt_data["C phase current"].set_number(d_to_s(reg_to_f(int_inputs[5], int_inputs[4]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x000c, 2 * 3);
			mqtt_data["A phase active power"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["B phase active power"].set_number(d_to_s(reg_to_f(int_inputs[3], int_inputs[2]), 3));
			mqtt_data["C phase active power"].set_number(d_to_s(reg_to_f(int_inputs[5], int_inputs[4]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0012, 2 * 3);
			mqtt_data["A phase apparent power"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["B phase apparent power"].set_number(d_to_s(reg_to_f(int_inputs[3], int_inputs[2]), 3));
			mqtt_data["C phase apparent power"].set_number(d_to_s(reg_to_f(int_inputs[5], int_inputs[4]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0018, 2 * 3);
			mqtt_data["A phase reactive power"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["B phase reactive power"].set_number(d_to_s(reg_to_f(int_inputs[3], int_inputs[2]), 3));
			mqtt_data["C phase reactive power"].set_number(d_to_s(reg_to_f(int_inputs[5], int_inputs[4]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x001e, 2 * 3);
			mqtt_data["A phase power factor"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["B phase power factor"].set_number(d_to_s(reg_to_f(int_inputs[3], int_inputs[2]), 3));
			mqtt_data["C phase power factor"].set_number(d_to_s(reg_to_f(int_inputs[5], int_inputs[4]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0024, 2 * 3);
			mqtt_data["A phase angle"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["B phase angle"].set_number(d_to_s(reg_to_f(int_inputs[3], int_inputs[2]), 3));
			mqtt_data["C phase angle"].set_number(d_to_s(reg_to_f(int_inputs[5], int_inputs[4]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x003c, 2 * 3);
			mqtt_data["total reactive power"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["total power factor"].set_number(d_to_s(reg_to_f(int_inputs[7], int_inputs[6]), 3));
			mqtt_data["total angle"].set_number(d_to_s(reg_to_f(int_inputs[9], int_inputs[8]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0046, 2 * 5);
			mqtt_data["frequency"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["forward active energy"].set_number(d_to_s(reg_to_f(int_inputs[3], int_inputs[2]), 3));
			mqtt_data["reverse active energy"].set_number(d_to_s(reg_to_f(int_inputs[5], int_inputs[4]), 3));
			mqtt_data["forward reactive energy"].set_number(d_to_s(reg_to_f(int_inputs[7], int_inputs[6]), 3));
			mqtt_data["reverse reactive energy"].set_number(d_to_s(reg_to_f(int_inputs[9], int_inputs[8]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0054, 2 * 1);
			mqtt_data["total active power"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0064, 2 * 1);
			mqtt_data["total apparent power"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x015a, 2 * 6);
			mqtt_data["A phase forward active energy"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["B phase forward active energy"].set_number(d_to_s(reg_to_f(int_inputs[3], int_inputs[2]), 3));
			mqtt_data["C phase forward active energy"].set_number(d_to_s(reg_to_f(int_inputs[5], int_inputs[4]), 3));
			mqtt_data["A phase reverse active energy"].set_number(d_to_s(reg_to_f(int_inputs[7], int_inputs[6]), 3));
			mqtt_data["B phase reverse active energy"].set_number(d_to_s(reg_to_f(int_inputs[9], int_inputs[8]), 3));
			mqtt_data["C phase reverse active energy"].set_number(d_to_s(reg_to_f(int_inputs[11], int_inputs[10]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x016c, 2 * 6);
			mqtt_data["A phase forward reactive energy"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["B phase forward reactive energy"].set_number(d_to_s(reg_to_f(int_inputs[3], int_inputs[2]), 3));
			mqtt_data["C phase forward reactive energy"].set_number(d_to_s(reg_to_f(int_inputs[5], int_inputs[4]), 3));
			mqtt_data["A phase reverse reactive energy"].set_number(d_to_s(reg_to_f(int_inputs[7], int_inputs[6]), 3));
			mqtt_data["B phase reverse reactive energy"].set_number(d_to_s(reg_to_f(int_inputs[9], int_inputs[8]), 3));
			mqtt_data["C phase reverse reactive energy"].set_number(d_to_s(reg_to_f(int_inputs[11], int_inputs[10]), 3));
		}
	}
}

void
eastron_sdm220(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		{
			auto int_inputs = mb.read_input_registers(address, 0x0000, 2 * 1);
			mqtt_data["A phase voltage"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0006, 2 * 1);
			mqtt_data["A phase current"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x000c, 2 * 1);
			mqtt_data["A phase active power"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["total active power"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0012, 2 * 1);
			mqtt_data["A phase apparent power"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["total apparent power"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0018, 2 * 1);
			mqtt_data["A phase reactive power"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["total reactive power"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x001e, 2 * 1);
			mqtt_data["A phase power factor"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["total power factor"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0024, 2 * 1);
			mqtt_data["A phase angle"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["total angle"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0046, 2 * 5);
			mqtt_data["frequency"].set_number(d_to_s(reg_to_f(int_inputs[1], int_inputs[0]), 3));
			mqtt_data["forward active energy"].set_number(d_to_s(reg_to_f(int_inputs[3], int_inputs[2]), 3));
			mqtt_data["reverse active energy"].set_number(d_to_s(reg_to_f(int_inputs[5], int_inputs[4]), 3));
			mqtt_data["forward reactive energy"].set_number(d_to_s(reg_to_f(int_inputs[7], int_inputs[6]), 3));
			mqtt_data["reverse reactive energy"].set_number(d_to_s(reg_to_f(int_inputs[9], int_inputs[8]), 3));
		}
	}
}

void
ZGEJ_powermeter(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		{
			auto int_inputs = mb.read_input_registers(address, 0x0018, 2 * 34);
			mqtt_data["A phase voltage"].set_number(d_to_s(reg_to_f(int_inputs[0], int_inputs[1]), 3));
			mqtt_data["B phase voltage"].set_number(d_to_s(reg_to_f(int_inputs[2], int_inputs[3]), 3));
			mqtt_data["C phase voltage"].set_number(d_to_s(reg_to_f(int_inputs[4], int_inputs[5]), 3));
			mqtt_data["AB line voltage"].set_number(d_to_s(reg_to_f(int_inputs[6], int_inputs[7]), 3));
			mqtt_data["BC line voltage"].set_number(d_to_s(reg_to_f(int_inputs[8], int_inputs[9]), 3));
			mqtt_data["CA line voltage"].set_number(d_to_s(reg_to_f(int_inputs[10], int_inputs[11]), 3));
			mqtt_data["A phase current"].set_number(d_to_s(reg_to_f(int_inputs[12], int_inputs[13]), 3));
			mqtt_data["B phase current"].set_number(d_to_s(reg_to_f(int_inputs[14], int_inputs[15]), 3));
			mqtt_data["C phase current"].set_number(d_to_s(reg_to_f(int_inputs[16], int_inputs[17]), 3));
			mqtt_data["A phase active power"].set_number(d_to_s(reg_to_f(int_inputs[18], int_inputs[19]), 3));
			mqtt_data["B phase active power"].set_number(d_to_s(reg_to_f(int_inputs[20], int_inputs[21]), 3));
			mqtt_data["C phase active power"].set_number(d_to_s(reg_to_f(int_inputs[22], int_inputs[23]), 3));
			mqtt_data["total active power"].set_number(d_to_s(reg_to_f(int_inputs[24], int_inputs[25]), 3));
			mqtt_data["A phase reactive power"].set_number(d_to_s(reg_to_f(int_inputs[26], int_inputs[27]), 3));
			mqtt_data["B phase reactive power"].set_number(d_to_s(reg_to_f(int_inputs[28], int_inputs[29]), 3));
			mqtt_data["C phase reactive power"].set_number(d_to_s(reg_to_f(int_inputs[30], int_inputs[31]), 3));
			mqtt_data["total reactive power"].set_number(d_to_s(reg_to_f(int_inputs[32], int_inputs[33]), 3));
			mqtt_data["A phase apparent power"].set_number(d_to_s(reg_to_f(int_inputs[34], int_inputs[35]), 3));
			mqtt_data["B phase apparent power"].set_number(d_to_s(reg_to_f(int_inputs[36], int_inputs[37]), 3));
			mqtt_data["C phase apparent power"].set_number(d_to_s(reg_to_f(int_inputs[38], int_inputs[39]), 3));
			mqtt_data["total apparent power"].set_number(d_to_s(reg_to_f(int_inputs[40], int_inputs[41]), 3));
			mqtt_data["A phase power factor"].set_number(d_to_s(reg_to_f(int_inputs[42], int_inputs[43]), 3));
			mqtt_data["B phase power factor"].set_number(d_to_s(reg_to_f(int_inputs[44], int_inputs[45]), 3));
			mqtt_data["C phase power factor"].set_number(d_to_s(reg_to_f(int_inputs[46], int_inputs[47]), 3));
			mqtt_data["total power factor"].set_number(d_to_s(reg_to_f(int_inputs[48], int_inputs[49]), 3));
			mqtt_data["frequency"].set_number(d_to_s(reg_to_f(int_inputs[50], int_inputs[51]), 3));
			mqtt_data["forward active energy 2"].set_number(d_to_s(reg_to_f(int_inputs[52], int_inputs[53]), 3));
			mqtt_data["reverse active energy 2"].set_number(d_to_s(reg_to_f(int_inputs[54], int_inputs[55]), 3));
			mqtt_data["forward reactive energy 2"].set_number(d_to_s(reg_to_f(int_inputs[56], int_inputs[57]), 3));
			mqtt_data["reverse reactive energy 2"].set_number(d_to_s(reg_to_f(int_inputs[58], int_inputs[59]), 3));
			mqtt_data["forward active energy"].set_number(d_to_s(reg_to_f(int_inputs[60], int_inputs[61]), 3));
			mqtt_data["reverse active energy"].set_number(d_to_s(reg_to_f(int_inputs[62], int_inputs[63]), 3));
			mqtt_data["forward reactive energy"].set_number(d_to_s(reg_to_f(int_inputs[64], int_inputs[65]), 3));
			mqtt_data["reverse reactive energy"].set_number(d_to_s(reg_to_f(int_inputs[66], int_inputs[67]), 3));
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
				if (key == "relay") {
					Array<JSON>& relay = json[key].get_array();
					for (int64_t x = 0; x <= relay.max && x < 2; x++) {
						if (relay[x].is_boolean()) {
							bool val = relay[x];
							mb.write_coil(address, x, val);
						}
					}
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

		Array<JSON> relay;
		relay[0] = bin_coils[0];
		relay[1] = bin_coils[1];
		mqtt_data["relay"] = relay;
	}
}

void
mru_swg100(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		{
			auto int_inputs = mb.read_input_registers(address, 0, 40);
			AArray<JSON> values;
			{
				// 0 values[Analysator Status (weitere Informationen siehe unten)
				uint32_t val = (uint32_t)int_inputs[0] << 16 | int_inputs[1];
				values["Power-On"] = (bool)(val & 0x0001);
				values["System-Alarm"] = (bool)(val & 0x0002);
				values["Luftspülung"] = (bool)(val & 0x0004);
				values["Messung (Vorbereitung der Messung, nicht am messen!)"] = (bool)(val & 0x0008);
				values["Derzeitige Messstelle"].set_number((uint32_t)(val & 0x00f0) >> 4);
				values["Ein Sensor wird gerade gespült"] = (bool)(val & 0x0000);
				values["Ein Sensor ist gerade weggeschaltet"] = (bool)(val & 0x0000);
				values["Gasmessung im Gehäuse"] = (bool)(val & 0x0000);
				values["Stand-By"] = (bool)(val & 0x0000);
				values["Auto-Kalibration"] = (bool)(val & 0x0000);
				values["Service fällig"] = (bool)(val & 0x0000);
				values["Warnung: Summe der gemessenen Gase ist > 100%"] = (bool)(val & 0x0000);
				values["Steuerwort der Externen Steuerung"].set_number((uint32_t)(val & 0xf000) >> 12);
			}
			{
				// 2 U32 System Alarm (weitere Informationen siehe unten)
				uint32_t val = (uint32_t)int_inputs[2] << 16 | int_inputs[3];
				values["Mainboard offline"] = (bool)(val & 0x0001);
				values["Mainboard ist im Bootloader Modus"] = (bool)(val & 0x0002);
				values["CH4 Umgebung > threshold value"] = (bool)(val & 0x0004);
				values["Kondensat"] = (bool)(val & 0x0008);
				values["Gasdurchfluss < 20 l/h"] = (bool)(val & 0x0010);
				values["Lüfterdrehzahl < 900 min-1"] = (bool)(val & 0x0020);
				values["T-Gaskühler zu hoch"] = (bool)(val & 0x0040);
				values["T-Gaskühler zu niedrig"] = (bool)(val & 0x0080);
				values["T-Sensor > 55°C"] = (bool)(val & 0x0100);
				values["T-Sensor < 5°C"] = (bool)(val & 0x0200);
				values["Gaskühler-Modul Offline"] = (bool)(val & 0x2000);
				values["T-Vor-Gaskühler zu hoch"] = (bool)(val & 0x4000);
				values["T-Vor-Gaskühler zu niedrig"] = (bool)(val & 0x8000);
			}
			values["Seriennummer"].set_number((uint32_t)int_inputs[4] << 16 | int_inputs[5]);
			values["Analysatortyp"].set_number((uint32_t)int_inputs[6] << 16 | int_inputs[7]);
			values["Firmware Version"].set_number((uint32_t)int_inputs[8] << 16 | int_inputs[9]);
			values["Verstrichene Sekunden seit dem Einschalten"].set_number((uint32_t)int_inputs[10] << 16 | int_inputs[11]);
			values["Fehlerzähler Modbus-Pakete"].set_number((uint32_t)int_inputs[12] << 16 | int_inputs[13]);
			values["CH4 umgebung [%] voltage"].set_number(d_to_s(reg_to_f(int_inputs[15], int_inputs[14]), 3));
			values["CH4 umgebung [% LEL]"].set_number(d_to_s(reg_to_f(int_inputs[17], int_inputs[16]), 3));
			values["T-sensor [°C/°F]"].set_number(d_to_s(reg_to_f(int_inputs[19], int_inputs[18]), 3));
			values["Gasdurchfluss [l/h]"].set_number(d_to_s(reg_to_f(int_inputs[21], int_inputs[20]), 3));
			values["T-Gaskühler [°C/°F]"].set_number(d_to_s(reg_to_f(int_inputs[23], int_inputs[22]), 3));
			values["Lüfterdrehzahl [U/min]"].set_number(d_to_s(reg_to_f(int_inputs[25], int_inputs[24]), 3));
			values["Messpumpendrehzahl [U/min]"].set_number(d_to_s(reg_to_f(int_inputs[27], int_inputs[26]), 3));
			values["P-barometrisch [hPa]"].set_number(d_to_s(reg_to_f(int_inputs[29], int_inputs[28]), 3));
			values["P-barometrisch [inchHG]"].set_number(d_to_s(reg_to_f(int_inputs[31], int_inputs[30]), 3));
			values["T-Vor-Gaskühler [°C/°F]"].set_number(d_to_s(reg_to_f(int_inputs[33], int_inputs[32]), 3));
			mqtt_data["status"] = values;
		}
		{
			Array<JSON> measurements;
			for (int i = 0; i < 2; i++) {
				auto int_inputs = mb.read_input_registers(address, 40 + i * 30, 30);
				AArray<JSON> values;
				{
					// 0 values[Analysator Status (weitere Informationen siehe unten)
					uint32_t val = (uint32_t)int_inputs[0] << 16 | int_inputs[1];
					values["Power-On"] = (bool)(val & 0x0001);
					values["System-Alarm"] = (bool)(val & 0x0002);
					values["Luftspülung"] = (bool)(val & 0x0004);
					values["Messung (Vorbereitung der Messung, nicht am messen!)"] = (bool)(val & 0x0008);
					values["Derzeitige Messstelle"].set_number((uint32_t)(val & 0x00f0) >> 4);
					values["Ein Sensor wird gerade gespült"] = (bool)(val & 0x0000);
					values["Ein Sensor ist gerade weggeschaltet"] = (bool)(val & 0x0000);
					values["Gasmessung im Gehäuse"] = (bool)(val & 0x0000);
					values["Stand-By"] = (bool)(val & 0x0000);
					values["Auto-Kalibration"] = (bool)(val & 0x0000);
					values["Service fällig"] = (bool)(val & 0x0000);
					values["Warnung: Summe der gemessenen Gase ist > 100%"] = (bool)(val & 0x0000);
					values["Steuerwort der Externen Steuerung"].set_number((uint32_t)(val & 0xf000) >> 12);
				}
				{
					// 2 U32 System Alarm (weitere Informationen siehe unten)
					uint32_t val = (uint32_t)int_inputs[2] << 16 | int_inputs[3];
					values["Mainboard offline"] = (bool)(val & 0x0001);
					values["Mainboard ist im Bootloader Modus"] = (bool)(val & 0x0002);
					values["CH4 Umgebung > threshold value"] = (bool)(val & 0x0004);
					values["Kondensat"] = (bool)(val & 0x0008);
					values["Gasdurchfluss < 20 l/h"] = (bool)(val & 0x0010);
					values["Lüfterdrehzahl < 900 min-1"] = (bool)(val & 0x0020);
					values["T-Gaskühler zu hoch"] = (bool)(val & 0x0040);
					values["T-Gaskühler zu niedrig"] = (bool)(val & 0x0080);
					values["T-Sensor > 55°C"] = (bool)(val & 0x0100);
					values["T-Sensor < 5°C"] = (bool)(val & 0x0200);
					values["Gaskühler-Modul Offline"] = (bool)(val & 0x2000);
					values["T-Vor-Gaskühler zu hoch"] = (bool)(val & 0x4000);
					values["T-Vor-Gaskühler zu niedrig"] = (bool)(val & 0x8000);
				}
				values["O2 [%]"].set_number(d_to_s(reg_to_f(int_inputs[5], int_inputs[4]), 3));
				values["CO2 [%]"].set_number(d_to_s(reg_to_f(int_inputs[7], int_inputs[6]), 3));
				values["CH4 [%]"].set_number(d_to_s(reg_to_f(int_inputs[9], int_inputs[8]), 3));
				values["H2S [ppm]"].set_number(d_to_s(reg_to_f(int_inputs[11], int_inputs[10]), 3));
				values["H2 [ppm]"].set_number(d_to_s(reg_to_f(int_inputs[13], int_inputs[12]), 3));
				values["Heizwert [MJ/kg]"].set_number(d_to_s(reg_to_f(int_inputs[15], int_inputs[14]), 3));
				values["Brennwert [MJ/kg]"].set_number(d_to_s(reg_to_f(int_inputs[17], int_inputs[16]), 3));
				values["Heizwert [MJ/m³]"].set_number(d_to_s(reg_to_f(int_inputs[19], int_inputs[18]), 3));
				values["Brennwert [MJ/m³]"].set_number(d_to_s(reg_to_f(int_inputs[21], int_inputs[20]), 3));
				values["CO [ppm]"].set_number(d_to_s(reg_to_f(int_inputs[23], int_inputs[22]), 3));
				values["CH4 [ppm]"].set_number(d_to_s(reg_to_f(int_inputs[25], int_inputs[24]), 3));
				values["CO2 [ppm]"].set_number(d_to_s(reg_to_f(int_inputs[27], int_inputs[26]), 3));
				values["N2 [%]"].set_number(d_to_s(reg_to_f(int_inputs[29], int_inputs[28]), 3));
				measurements[i] = values;
			}
			mqtt_data["measurements"] = measurements;
		}
	}
}

void
eth_tpr_ldr(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/cmd") {
			JSON json;
			json.parse(rxbuf[i].message);
			Array<String> keys = json.get_object().getkeys();
			for (int64_t j = 0; j <= keys.max; j++) {
				String key = keys[j];
				if (key == "relay") {
					Array<JSON>& relay = json[key].get_array();
					for (int64_t x = 0; x <= relay.max && x < 2; x++) {
						if (relay[x].is_boolean()) {
							bool val = relay[x];
							mb.write_coil(address, x, val);
						}
					}
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

		Array<JSON> relay;
		relay[0] = bin_coils[0];
		relay[1] = bin_coils[1];
		mqtt_data["relay"] = relay;
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

			// 32 bit counter - should verify for rollover and restart
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
				sensor["temperature"].set_number(d_to_s(temp, 4));
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
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/cmd") {
			JSON json;
			json.parse(rxbuf[i].message);
			Array<String> keys = json.get_object().getkeys();
			for (int64_t j = 0; j <= keys.max; j++) {
				String key = keys[j];
				if (key == "relay") {
					Array<JSON>& relay = json[key].get_array();
					for (int64_t x = 0; x <= relay.max && x < 6; x++) {
						if (relay[x].is_boolean()) {
							bool val = relay[x];
							mb.write_coil(address, x, val);
						}
					}
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
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 8);

		Array<JSON> inputs;
		for (int64_t i = 0; i < 8; i++) {
			inputs[i] = bin_inputs[i];
		}
		mqtt_data["input"] = inputs;
	}

	{
		auto bin_coils = mb.read_coils(address, 0, 6);

		Array<JSON> relay;
		for (int64_t i = 0; i < 6; i++) {
			relay[i] = bin_coils[i];
		}
		mqtt_data["relay"] = relay;
	}
}

void
rs485_relais6(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/cmd") {
			JSON json;
			json.parse(rxbuf[i].message);
			Array<String> keys = json.get_object().getkeys();
			for (int64_t j = 0; j <= keys.max; j++) {
				String key = keys[j];
				if (key == "relay") {
					Array<JSON>& relay = json[key].get_array();
					for (int64_t x = 0; x <= relay.max && x < 6; x++) {
						if (relay[x].is_boolean()) {
							bool val = relay[x];
							mb.write_coil(address, x, val);
						}
					}
				}
			}
		}
	}

	// XXX no counter support yet
	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 8);

		Array<JSON> inputs;
		for (int64_t i = 0; i < 8; i++) {
			inputs[i] = bin_inputs[i];
		}
		mqtt_data["input"] = inputs;
	}

	{
		auto bin_coils = mb.read_coils(address, 0, 6);

		Array<JSON> relay;
		for (int64_t i = 0; i < 6; i++) {
			relay[i] = bin_coils[i];
		}
		mqtt_data["relay"] = relay;
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
	sensor["temperature"].set_number(d_to_s(temp, 1));
	sensor["humidity"].set_number(d_to_s(humid, 1));
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
	double version = -1;
	if (devdata.exists("version")) {
		version = devdata["version"].getd();
	}

	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/cmd") {
			JSON json;
			json.parse(rxbuf[i].message);
			Array<String> keys = json.get_object().getkeys();
			for (int64_t j = 0; j <= keys.max; j++) {
				String key = keys[j];
				if (key == "output") {
					Array<JSON>& output = json[key].get_array();
					for (int64_t x = 0; x <= output.max && x < 8; x++) {
						if (output[x].is_boolean()) {
							bool val = output[x];
							mb.write_coil(address, x, val);
						}
					}
				} else if (key == "pwm_enable") {
					Array<JSON>& tmp = json[key].get_array();
					for (int64_t x = 0; x <= tmp.max && x < 8; x++) {
						if (tmp[x].is_boolean()) {
							bool val = tmp[x];
							mb.write_coil(address, x + 8, val);
						}
					}
				} else if (key == "pwm_value") {
					Array<JSON>& tmp = json[key].get_array();
					for (int64_t x = 0; x <= tmp.max && x < 8; x++) {
						if (tmp[x].is_number()) {
							uint16_t val = tmp[x].get_numstr().getll();
							mb.write_coil(address, x, val);
						}
					}
				} else if (key == "pwm_max") {
					Array<JSON>& tmp = json[key].get_array();
					for (int64_t x = 0; x <= tmp.max && x < 8; x++) {
						if (tmp[x].is_number()) {
							uint16_t val = tmp[x].get_numstr().getll();
							mb.write_coil(address, x + 8, val);
						}
					}
				}
			}
		}
	}

	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 8);

		Array<JSON> inputs;
		for (int i = 0; i < 8; i++) {
			inputs[i] = bin_inputs[i];
		}
		mqtt_data["input"] = inputs;
	}

	{
		auto data = mb.read_coils(address, 0, 16);

		Array<JSON> outputs;
		for (int i = 0; i < 8; i++) {
			outputs[i] = data[i];
		}
		mqtt_data["output"] = outputs;

		Array<JSON> pwm_enables;
		for (int i = 0; i < 8; i++) {
			pwm_enables[i] = data[i + 8];
		}
		mqtt_data["pwm_enable"] = pwm_enables;
	}

	{
		auto data = mb.read_holding_registers(address, 0, 16);

		Array<JSON> pwm_values;
		for (int i = 0; i < 8; i++) {
			pwm_values[i] = (int64_t)data[i];
		}
		mqtt_data["pwm_value"] = pwm_values;

		Array<JSON> pwm_max;
		for (int i = 0; i < 8; i++) {
			pwm_max[i] = (int64_t)data[i + 8];
		}
		mqtt_data["pwm_max"] = pwm_max;
	}

	if (version >= 0.7) {
		auto bin_counter = mb.read_input_registers(address, 0, 4 * 8);

		Array<JSON> counters;
		uint64_t vals[8];
		for (int i = 0; i < 8; i++) {
			uint64_t tmp = 0;
			for (int j = 0; j < 4; j++) {
				tmp |= bin_counter[i * 4 + j] << (j * 16);
			}
			counters[i].set_number(S + tmp);
			vals[i] = tmp;
		}
		mqtt_data["counter"] = counters;

		if (version == 0.7) {
			// emulate firmware version 0.8 timediff feature

			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);

			Array<JSON> times;
			for (int i = 0; i < 8; i++) {
				static uint64_t lastvals[8];
				static bool first_run[8] = {true, true, true, true, true, true, true, true};
				static struct timespec lasttime[8];
				double timediffs[8];

				if (vals[i] != lastvals[i]) {
					if (first_run[i]) {
						first_run[i] = false;
					} else {
						uint64_t diff = vals[i] - lastvals[i];

						struct timespec timespecdiff;
						timespecsub(&now, &lasttime[i], &timespecdiff);
						double timediff = (double)(timespecdiff.tv_sec) + (double)(timespecdiff.tv_nsec) / 1000000000;
						timediffs[i] = timediff / (double)diff;
					}
					lasttime[i] = now;
				}
				times[i].set_number(d_to_s((((double)timediffs[i])), 2));

				lastvals[i] = vals[i];
			}
		}

	}

	if (version >= 0.8) {
		auto bin_times = mb.read_input_registers(address, 0, 2 * 8);

		Array<JSON> times;
		for (int i = 0; i < 8; i++) {
			uint64_t tmp = 0;
			for (int j = 0; j < 2; j++) {
				tmp |= bin_times[i * 4 + j] << (j * 16);
			}
			times[i].set_number(d_to_s((((double)tmp) / 5000.0), 2));
		}
		mqtt_data["counttime"] = times;
	}
}

void
eth_io88p(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	eth_io88(mb, rxbuf, mqtt_data, address, maintopic, devdata, dev_cfg);

	if (dev_cfg.exists("DS18B20")) {
		Array<JSON> ds18b20;
		int64_t max_sensor = dev_cfg["DS18B20"].get_array().max;
		for (int64_t i = 0; i <= max_sensor; i++) {
			int16_t sensor_register = dev_cfg["DS18B20"][i]["register"].get_numstr().getll();
			try {
				uint16_t value = mb.read_input_register(address, sensor_register);
				double temp = (double)value / 16;
				AArray<JSON> sensor;
				sensor["temperature"].set_number(d_to_s(temp, 4));
				ds18b20[i] = sensor;
			} catch (...) {
			}
		}
		mqtt_data["ds18b20"] = ds18b20;
	}
}

void
rs485_io88(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/cmd") {
			JSON json;
			json.parse(rxbuf[i].message);
			Array<String> keys = json.get_object().getkeys();
			for (int64_t j = 0; j <= keys.max; j++) {
				String key = keys[j];
				if (key == "output") {
					Array<JSON>& output = json[key].get_array();
					for (int64_t x = 0; x <= output.max && x < 8; x++) {
						if (output[x].is_boolean()) {
							bool val = output[x];
							mb.write_coil(address, x, val);
						}
					}
				} else if (key == "pwm") {
					Array<JSON>& pwm = json[key].get_array();
					for (int64_t x = 0; x <= pwm.max; x++) {
						if (pwm[x].is_number()) {
							uint16_t val = pwm[x].get_numstr().getll();
							mb.write_register(address, x, val);
						}
					}
				}
			}
		}
	}

	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 8);

		Array<JSON> inputs;
		for (int i = 0; i < 8; i++) {
			inputs[i] = bin_inputs[i];
		}
		mqtt_data["input"] = inputs;
	}

	{
		auto bin_coils = mb.read_coils(address, 0, 8);

		Array<JSON> outputs;
		for (int i = 0; i < 8; i++) {
			outputs[i] = bin_coils[i];
		}
		mqtt_data["output"] = outputs;
	}
}

void
rs485_adc_dac(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/cmd") {
			JSON json;
			json.parse(rxbuf[i].message);
			Array<String> keys = json.get_object().getkeys();
			for (int64_t j = 0; j <= keys.max; j++) {
				String key = keys[j];
				if (key == "dac") {
					Array<JSON>& dac = json[key].get_array();
					for (int64_t x = 0; x <= dac.max && x < 2; x++) {
						if (dac[x].is_number()) {
							double tmp = dac[x].get_numstr().getd();
							tmp = tmp / 11.0 * 1.0; // normalize for output resistors
							tmp = tmp * (1 << 12) / 2.048; // normalize for DAC value range
							uint16_t val = tmp;
							mb.write_register(address, x, val);
						}
					}
				}
			}
		}
	}

	{
		auto int_inputs = mb.read_input_registers(address, 0, 10);
		Array<JSON> adc;
		for (int i = 0; i < 4; i++) {
			const int reg_values[] = {2, 1, 8, 7};
			double tmp = int_inputs[reg_values[i]];
			tmp = tmp / (1 << 10) * 1.1; // normalize for ADC value range
			tmp = tmp * 11.0 / 1.0; // normalize for input resistors
			adc[i].set_number(d_to_s(tmp, 3));
		}
		mqtt_data["adc"] = adc;
		mqtt_data["ref"].set_number(S + int_inputs[9]);
	}

	{
		auto int_outputs = mb.read_holding_registers(address, 0, 2);
		Array<JSON> dac;
		for (int i = 0; i < 2; i++) {
			double tmp = int_outputs[i];
			tmp = tmp / (1 << 12) * 2.048; // normalize for DAC value range
			tmp = tmp * 11.0 / 1.0; // normalize for output resistors
			dac[i].set_number(d_to_s(tmp, 3));
		}
		mqtt_data["dac"] = dac;
	}
}

void
rs485_adc_dac_30(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/cmd") {
			JSON json;
			json.parse(rxbuf[i].message);
			Array<String> keys = json.get_object().getkeys();
			for (int64_t j = 0; j <= keys.max; j++) {
				String key = keys[j];
				if (key == "dac") {
					Array<JSON>& dac = json[key].get_array();
					for (int64_t x = 0; x <= dac.max && x < 2; x++) {
						if (dac[x].is_number()) {
							double tmp = dac[x].get_numstr().getd();
							tmp = tmp / 11.0 * 1.0; // normalize for output resistors
							tmp = tmp * (1 << 12) / 2.048; // normalize for DAC value range
							uint16_t val = tmp;
							mb.write_register(address, x, val);
						}
					}
				}
			}
		}
	}

	{
		auto int_inputs = mb.read_input_registers(address, 0, 10);
		Array<JSON> adc;
		for (int i = 0; i < 4; i++) {
			const int reg_values[] = {2, 1, 8, 7};
			double tmp = int_inputs[reg_values[i]];
			tmp = tmp / (1 << 10) * 1.1; // normalize for ADC value range
			tmp = tmp * (10000 + 560) / 560; // normalize for input resistors
			adc[i].set_number(d_to_s(tmp, 3));
		}
		mqtt_data["adc"] = adc;
		mqtt_data["ref"].set_number(S + int_inputs[9]);
	}

	{
		auto int_outputs = mb.read_holding_registers(address, 0, 2);
		Array<JSON> dac;
		for (int i = 0; i < 2; i++) {
			double tmp = int_outputs[i];
			tmp = tmp / (1 << 12) * 2.048; // normalize for DAC value range
			tmp = tmp * 11.0 / 1.0; // normalize for output resistors
			dac[i].set_number(d_to_s(tmp, 3));
		}
		mqtt_data["dac"] = dac;
	}
}

void
rs485_adc_dac_2_dacs(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/cmd") {
			JSON json;
			json.parse(rxbuf[i].message);
			Array<String> keys = json.get_object().getkeys();
			for (int64_t j = 0; j <= keys.max; j++) {
				String key = keys[j];
				if (key == "dac") {
					Array<JSON>& dac = json[key].get_array();
					for (int64_t x = 0; x <= dac.max && x < 2; x++) {
						if (dac[x].is_number()) {
							double tmp = dac[x].get_numstr().getd();
							tmp = tmp / 11.0 * 1.0; // normalize for output resistors
							tmp = tmp * (1 << 12) / 2.048; // normalize for DAC value range
							uint16_t val = tmp;
							mb.write_register(address, x, val);
						}
					}
				}
			}
		}
	}

	{
		auto int_outputs = mb.read_holding_registers(address, 0, 4);
		Array<JSON> dac;
		for (int i = 0; i < 2; i++) {
			double tmp = int_outputs[i];
			tmp = tmp / (1 << 12) * 2.048; // normalize for DAC value range
			tmp = tmp * 11.0 / 1.0; // normalize for output resistors
			dac[i].set_number(d_to_s(tmp, 3));
		}
		mqtt_data["dac"] = dac;
	}
}

void
rs485_adc_dac_2(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	rs485_adc_dac_2_dacs(mb, rxbuf, mqtt_data, address, maintopic, devdata, dev_cfg);

	{
		auto int_inputs = mb.read_input_registers(address, 0, 4);
		Array<JSON> adc;
		for (int i = 0; i < 4; i++) {
			double tmp = int_inputs[i];
			tmp = tmp / (1 << 10) * 1.1; // normalize for ADC value range
			tmp = tmp * 11.0 / 1.0; // normalize for input resistors
			adc[i].set_number(d_to_s(tmp, 3));
		}
		mqtt_data["adc"] = adc;
	}
}

void
rs485_adcp_dac_2(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	rs485_adc_dac_2(mb, rxbuf, mqtt_data, address, maintopic, devdata, dev_cfg);

	{
		auto int_inputs = mb.read_input_registers(address, 5, 8);
		Array<JSON> adc;
		for (int i = 0; i < 4; i++) {
			double tmp = int_inputs[i * 2] | (int_inputs[i * 2 + 1] << 16);
			tmp = tmp / (1 << 10) * 1.1; // normalize for ADC value range
			tmp = tmp * 11.0 / 1.0; // normalize for input resistors
			adc[i].set_number(d_to_s(tmp, 3));
		}
		mqtt_data["adc2"] = adc;
	}
}

void
rs485_adcc_dac_2(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	rs485_adc_dac_2_dacs(mb, rxbuf, mqtt_data, address, maintopic, devdata, dev_cfg);

	{
		auto int_inputs = mb.read_input_registers(address, 0, 4);
		Array<JSON> adc;
		for (int i = 0; i < 4; i++) {
			double tmp = int_inputs[i];
			tmp = tmp / (1 << 10) * 1.1; // normalize for ADC value range
			tmp = tmp * 11.0 / 1.0; // normalize for input resistors
			// XXX TODO convert to current
			adc[i].set_number(d_to_s(tmp, 3));
		}
		mqtt_data["adc"] = adc;
	}
}

void
rs485_adccp_dac_2(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	rs485_adcc_dac_2(mb, rxbuf, mqtt_data, address, maintopic, devdata, dev_cfg);

	{
		auto int_inputs = mb.read_input_registers(address, 5, 8);
		Array<JSON> adc;
		for (int i = 0; i < 4; i++) {
			double tmp = int_inputs[i * 2] | (int_inputs[i * 2 + 1] << 16);
			tmp = tmp / (1 << 10) * 1.1; // normalize for ADC value range
			tmp = tmp * 11.0 / 1.0; // normalize for input resistors
			// XXX TODO convert to current
			adc[i].set_number(d_to_s(tmp, 3));
		}
		mqtt_data["adc2"] = adc;
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
			sensor["temperature"].set_number(d_to_s(((double)(int16_t)int_inputs[i * 2]) / 4.0, 2));
			sensor["cold_temperature"].set_number(d_to_s(((double)(int16_t)int_inputs[ i * 2 + 1]) / 16.0, 2));
			sensors[i] = sensor;
		}
		mqtt_data["thermocouple"] = sensors;
	}
}

void
rs485_ina226(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		auto int_inputs = mb.read_input_registers(address, 0, 4);
		Array<JSON> sensors;

		{
			AArray<JSON> sensor;

			double tmpd;
			int32_t tmp;

			tmp = int_inputs[0] | (int_inputs[1] << 16);
			tmpd = (double)tmp / 1.25 / 1000;
			sensor["voltage"].set_number(d_to_s(tmpd, 6));

			tmp = int_inputs[2] | (int_inputs[3] << 16);
			tmpd = (double)tmp / 2.5 / 1000.0 / 1000.0;
			sensor["shunt_voltage"].set_number(d_to_s(tmpd, 6));
			sensors[0] = sensor;
		}
		mqtt_data["shunts"] = sensors;
	}
}

void
rs485_valve(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/cmd") {
			JSON json;
			json.parse(rxbuf[i].message);
			Array<String> keys = json.get_object().getkeys();
			for (int64_t j = 0; j <= keys.max; j++) {
				String key = keys[j];
				if (key == "speed") {
					uint16_t val = json[key].get_numstr().getll();
					mb.write_register(address, 0, val);
				}
				if (key == "position") {
					double val = json[key].get_numstr().getd();
					mb.write_register(address, 1, (int16_t)(val * 100.0));
				}
			}
		}
	}

	{
		auto val = mb.read_holding_registers(address, 0, 2);

		mqtt_data["speed"].set_number((uint64_t)val[0]);

		double tmpd;
		tmpd = ((double)(int16_t)val[1]) / 100.0;
		mqtt_data["position"].set_number(d_to_s(tmpd, 2));
	}

	{
		auto val = mb.read_input_registers(address, 0, 6);

		double tmpd;
		tmpd = ((double)(int16_t)val[0]) / 100.0;
		mqtt_data["sensor_position"].set_number(d_to_s(tmpd, 2));

		tmpd = ((double)(int16_t)val[4]) * 3.3 / 32768;
		tmpd = 27.0 - (tmpd - 0.706) / 0.001721;;
		mqtt_data["temperature"].set_number(d_to_s(tmpd, 2));
	}
}

void
rs485_chamberpump(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/cmd") {
			JSON json;
			json.parse(rxbuf[i].message);
			Array<String> keys = json.get_object().getkeys();
			for (int64_t j = 0; j <= keys.max; j++) {
				String key = keys[j];
				if (key == "triggerlevel_top") {
					uint16_t val = json[key].get_numstr().getll();
					mb.write_register(address, 0, val);
				}
				if (key == "triggerlevel_bottom") {
					uint16_t val = json[key].get_numstr().getll();
					mb.write_register(address, 1, val);
				}
				if (key == "start_trigger") {
					bool val = json[key];
					mb.write_coil(address, 1, val);
				}
				if (key == "auto_start") {
					bool val = json[key];
					mb.write_coil(address, 0, val);
				}
			}
		}
	}

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
			mqtt_data["statenum"].set_number(S + int_inputs[4]);
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
}

void
rs485_conductive_level(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/cmd") {
			JSON json;
			json.parse(rxbuf[i].message);
			Array<String> keys = json.get_object().getkeys();
			for (int64_t j = 0; j <= keys.max; j++) {
				String key = keys[j];
				if (key == "output") {
					Array<JSON>& output = json[key].get_array();
					for (int64_t x = 0; x <= output.max && x < 4; x++) {
						if (output[x].is_boolean()) {
							bool val = output[x];
							mb.write_coil(address, x, val);
						}
					}
				}
			}
		}
	}

	{
		auto int_inputs = mb.read_input_registers(address, 0, 4);
		{
			Array<JSON> adc;
			adc[0].set_number(S + int_inputs[0]);
			adc[1].set_number(S + int_inputs[1]);
			adc[2].set_number(S + int_inputs[2]);
			adc[3].set_number(S + int_inputs[3]);
			mqtt_data["adc"] = adc;
		}
	}

	{
		auto bin_coils = mb.read_coils(address, 0, 4);

		Array<JSON> outputs;
		for (int i = 0; i < 4; i++) {
			outputs[i] = bin_coils[i];
		}
		mqtt_data["output"] = outputs;
	}
}

void
trucki_sun1000(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/cmd") {
			JSON json;
			json.parse(rxbuf[i].message);
			Array<String> keys = json.get_object().getkeys();
			for (int64_t j = 0; j <= keys.max; j++) {
				String key = keys[j];
				if (key == "set power") {
					if (json[key].is_number()) {
						double tmp = json[key].get_numstr().getd();
						tmp = tmp * 10.0;
						uint16_t val = tmp;
						mb.write_register(address, 0, val);
					}
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
			{
				AArray<JSON> tmp;
				mqtt_data = tmp;
			}
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
					{
						struct timespec tp;
						clock_gettime(CLOCK_REALTIME_FAST, &tp);
						time_t uts_time = tp.tv_sec;
						String date_str;
						{
							a_ptr<char> buf;
							buf = new char[256];

							struct tm stm;
							localtime_r(&uts_time, &stm);
							strftime(buf.get(), 256 - 1, "%Y-%m-%dT%H:%M:%S%z", &stm);
							date_str = buf.get();
						}
						mqtt_data["time"] = date_str;
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
		{
			AArray<JSON> tmp;
			mqtt_data = tmp;
		}
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
	devfunctions["Bernd Walter Computer Technology"]["ETH-IO88F"] = eth_io88;
	devfunctions["Bernd Walter Computer Technology"]["ETH-IO88P"] = eth_io88p;
	devfunctions["Bernd Walter Computer Technology"]["ETH-IO88FP"] = eth_io88p;
	devfunctions["Bernd Walter Computer Technology"]["MB ADC DAC"] = rs485_adc_dac;
	devfunctions["Bernd Walter Computer Technology"]["MB ADC DAC-30"] = rs485_adc_dac_30;
	devfunctions["Bernd Walter Computer Technology"]["125kHz RFID Reader / Display"] = rs485_rfid125_disp;
	devfunctions["Bernd Walter Computer Technology"]["125kHz RFID Reader / Writer-Beta"] = rs485_rfid125;
	devfunctions["Bernd Walter Computer Technology"]["RS485-TCK"] = rs485_thermocouple;
	devfunctions["Bernd Walter Computer Technology"]["RS485-Chamberpump"] = rs485_chamberpump;
	devfunctions["Bernd Walter Computer Technology"]["RS485-conductive-level"] = rs485_conductive_level;
	devfunctions["Bernd Walter Computer Technology"]["RS485-INA226"] = rs485_ina226;
	devfunctions["Bernd Walter Computer Technology"]["RS485-Valve"] = rs485_valve;
	devfunctions["Bernd Walter Computer Technology"]["RS485-ADC-DAC-2"] = rs485_adc_dac_2;
	devfunctions["Bernd Walter Computer Technology"]["RS485-ADCP-DAC-2"] = rs485_adcp_dac_2;
	devfunctions["Bernd Walter Computer Technology"]["RS485-ADCC-DAC-2"] = rs485_adcc_dac_2;
	devfunctions["Bernd Walter Computer Technology"]["RS485-ADCCP-DAC-2"] = rs485_adccp_dac_2;
	devfunctions["Bernd Walter Computer Technology"]["ETH-MULTI-RS485"] = empty;
	devfunctions["Epever"]["Triron"] = Epever_Triron;
	devfunctions["Epever"]["Tracer"] = Epever_Triron;
	devfunctions["Shanghai Chujin Electric"]["Panel Powermeter"] = ZGEJ_powermeter;
	devfunctions["Eastron"]["SDM220"] = eastron_sdm220;
	devfunctions["Eastron"]["SDM630"] = eastron_sdm630;
	devfunctions["Eastron"]["SDM72"] = eastron_sdm630;
	devfunctions["MRU"]["SWG100"] = mru_swg100;
	devfunctions["Trucki"]["SUN1000"] = trucki_sun1000;
	devfunctions["Trucki"]["SUN2000"] = trucki_sun1000;

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

