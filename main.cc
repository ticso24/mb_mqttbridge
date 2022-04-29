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
static AArray<AArray<void (*)(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)>> devfunctions;
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
Epever_Triron(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	bool persistent = true;
	bool if_changed = true;
	int qos = 1;
	if (dev_cfg.exists("persistent")) {
		persistent = dev_cfg["persistent"];
	}
	if (dev_cfg.exists("unchanged")) {
		if_changed = !dev_cfg["unchanged"];
	}
	if (dev_cfg.exists("qos")) {
		qos = dev_cfg["qos"].get_numstr().getll();
	}

	{
		{
			auto int_inputs = mb.read_input_registers(address, 0x3000, 9);
			mqtt.publish(maintopic + "/PV array rated voltage", d_to_s((double)int_inputs[0] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/PV array rated current", d_to_s((double)int_inputs[1] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/PV array rated power", d_to_s((double)((uint32_t)int_inputs[3] << 16 | int_inputs[2]) / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/rated voltage to battery", d_to_s((double)int_inputs[4] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/rated current to battery", d_to_s((double)int_inputs[5] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/rated power to battery", d_to_s((double)((uint32_t)int_inputs[7] << 16 | int_inputs[6]) / 100, 2), persistent, if_changed, qos);
			switch(int_inputs[8]) {
			case 0x0000:
				mqtt.publish(maintopic + "/charging mode", S + "connect/disconnect", persistent, if_changed, qos);
				break;
			case 0x0001:
				mqtt.publish(maintopic + "/charging mode", S + "PWM", persistent, if_changed, qos);
				break;
			case 0x0002:
				mqtt.publish(maintopic + "/charging mode", S + "MPPT", persistent, if_changed, qos);
				break;
			}
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x300e, 1);
			mqtt.publish(maintopic + "/rated current of load", d_to_s((double)int_inputs[0] / 100, 2), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x3100, 4);
			mqtt.publish(maintopic + "/PV voltage", d_to_s((double)int_inputs[0] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/PV current", d_to_s((double)int_inputs[1] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/PV power", d_to_s((double)((int32_t)int_inputs[3] << 16 | int_inputs[2]) / 100, 2), persistent, if_changed, qos);
		}
		if (0) {
			// value makes no sense, identic to PV power
			auto int_inputs = mb.read_input_registers(address, 0x3106, 2);
			mqtt.publish(maintopic + "/battery charging power", d_to_s((double)((int32_t)int_inputs[1] << 16 | int_inputs[0]) / 100, 2), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x310c, 4);
			mqtt.publish(maintopic + "/load voltage", d_to_s((double)int_inputs[0] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/load current", d_to_s((double)int_inputs[1] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/load power", d_to_s((double)((int32_t)int_inputs[3] << 16 | int_inputs[2]) / 100, 2), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x3110, 2);
			mqtt.publish(maintopic + "/battery temperature", d_to_s((double)(int16_t)int_inputs[0] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/case temperature", d_to_s((double)(int16_t)int_inputs[1] / 100, 2), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x311a, 1);
			mqtt.publish(maintopic + "/battery charged capacity", d_to_s((double)int_inputs[0] / 100, 2), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x3201, 2);
			int state;
			state = (int_inputs[0] >> 2) & 0x3;
			switch(state) {
			case 0x0:
				mqtt.publish(maintopic + "/charging status", S + "no charging", persistent, if_changed, qos);
				break;
			case 0x1:
				mqtt.publish(maintopic + "/charging status", S + "float", persistent, if_changed, qos);
				break;
			case 0x2:
				mqtt.publish(maintopic + "/charging status", S + "boost", persistent, if_changed, qos);
				break;
			case 0x3:
				mqtt.publish(maintopic + "/charging status", S + "equalization", persistent, if_changed, qos);
				break;
			}
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x331a, 3);
			mqtt.publish(maintopic + "/battery voltage", d_to_s((double)int_inputs[0] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/battery current", d_to_s((double)((int32_t)int_inputs[2] << 16 | int_inputs[1]) / 100, 2), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_holding_registers(address, 0x9000, 15);
			switch(int_inputs[0]) {
			case 0x0000:
				mqtt.publish(maintopic + "/battery type", S + "user defined", persistent, if_changed, qos);
				break;
			case 0x0001:
				mqtt.publish(maintopic + "/battery type", S + "sealed", persistent, if_changed, qos);
				break;
			case 0x0002:
				mqtt.publish(maintopic + "/battery type", S + "GEL", persistent, if_changed, qos);
				break;
			case 0x0003:
				mqtt.publish(maintopic + "/battery type", S + "flooded", persistent, if_changed, qos);
				break;
			}
			mqtt.publish(maintopic + "/battery capacity", S + int_inputs[1], persistent, if_changed, qos);
			mqtt.publish(maintopic + "/temperature compensation coefficient", d_to_s((double)int_inputs[2] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/high voltage disconnect", d_to_s((double)int_inputs[3] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/charging limit voltage", d_to_s((double)int_inputs[4] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/over voltage reconnect", d_to_s((double)int_inputs[5] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/equalization voltage", d_to_s((double)int_inputs[6] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/boost voltage", d_to_s((double)int_inputs[7] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/float voltage", d_to_s((double)int_inputs[8] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/boost reconnect voltage", d_to_s((double)int_inputs[9] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/low voltage reconnect", d_to_s((double)int_inputs[10] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/under voltage recover", d_to_s((double)int_inputs[11] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/under voltage warning", d_to_s((double)int_inputs[12] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/low voltage disconnect", d_to_s((double)int_inputs[13] / 100, 2), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/discharging limit voltage", d_to_s((double)int_inputs[14] / 100, 2), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x330a, 2);
			mqtt.publish(maintopic + "/consumed energy", d_to_s((double)((int32_t)int_inputs[1] << 16 | int_inputs[0]) / 100, 2), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x3312, 2);
			mqtt.publish(maintopic + "/generated energy", d_to_s((double)((int32_t)int_inputs[1] << 16 | int_inputs[0]) / 100, 2), persistent, if_changed, qos);
		}
	}

	auto rxbuf = mqtt.get_rxbuf();
}

void
eastron_sdm630(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	bool persistent = true;
	bool if_changed = true;
	int qos = 1;
	if (dev_cfg.exists("persistent")) {
		persistent = dev_cfg["persistent"];
	}
	if (dev_cfg.exists("unchanged")) {
		if_changed = !dev_cfg["unchanged"];
	}
	if (dev_cfg.exists("qos")) {
		qos = dev_cfg["qos"].get_numstr().getll();
	}

	{
		{
			auto int_inputs = mb.read_input_registers(address, 0x0000, 2 * 3);
			mqtt.publish(maintopic + "/A phase voltage", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase voltage", (double)reg_to_f(int_inputs[3], int_inputs[2]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase voltage", (double)reg_to_f(int_inputs[5], int_inputs[4]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0006, 2 * 3);
			mqtt.publish(maintopic + "/A phase current", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase current", (double)reg_to_f(int_inputs[3], int_inputs[2]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase current", (double)reg_to_f(int_inputs[5], int_inputs[4]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x000c, 2 * 3);
			mqtt.publish(maintopic + "/A phase active power", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase active power", (double)reg_to_f(int_inputs[3], int_inputs[2]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase active power", (double)reg_to_f(int_inputs[5], int_inputs[4]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0012, 2 * 3);
			mqtt.publish(maintopic + "/A phase apparent power", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase apparent power", (double)reg_to_f(int_inputs[3], int_inputs[2]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase apparent power", (double)reg_to_f(int_inputs[5], int_inputs[4]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0018, 2 * 3);
			mqtt.publish(maintopic + "/A phase reactive power", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase reactive power", (double)reg_to_f(int_inputs[3], int_inputs[2]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase reactive power", (double)reg_to_f(int_inputs[5], int_inputs[4]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x001e, 2 * 3);
			mqtt.publish(maintopic + "/A phase power factor", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase power factor", (double)reg_to_f(int_inputs[3], int_inputs[2]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase power factor", (double)reg_to_f(int_inputs[5], int_inputs[4]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0024, 2 * 3);
			mqtt.publish(maintopic + "/A phase angle", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase angle", (double)reg_to_f(int_inputs[3], int_inputs[2]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase angle", (double)reg_to_f(int_inputs[5], int_inputs[4]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x003c, 2 * 3);
			mqtt.publish(maintopic + "/total reactive power", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/total power factor", (double)reg_to_f(int_inputs[7], int_inputs[6]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/total angle", (double)reg_to_f(int_inputs[9], int_inputs[8]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0046, 2 * 5);
			mqtt.publish(maintopic + "/frequency", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/forward active energy", (double)reg_to_f(int_inputs[3], int_inputs[2]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/reverse active energy", (double)reg_to_f(int_inputs[5], int_inputs[4]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/forward reactive energy", (double)reg_to_f(int_inputs[7], int_inputs[6]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/reverse reactive energy", (double)reg_to_f(int_inputs[9], int_inputs[8]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0054, 2 * 1);
			mqtt.publish(maintopic + "/total active power", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0064, 2 * 1);
			mqtt.publish(maintopic + "/total apparent power", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x015a, 2 * 6);
			mqtt.publish(maintopic + "/A phase forward active energy", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase forward active energy", (double)reg_to_f(int_inputs[3], int_inputs[2]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase forward active energy", (double)reg_to_f(int_inputs[5], int_inputs[4]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/A phase reverse active energy", (double)reg_to_f(int_inputs[7], int_inputs[6]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase reverse active energy", (double)reg_to_f(int_inputs[9], int_inputs[8]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase reverse active energy", (double)reg_to_f(int_inputs[11], int_inputs[10]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x016c, 2 * 6);
			mqtt.publish(maintopic + "/A phase forward reactive energy", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase forward reactive energy", (double)reg_to_f(int_inputs[3], int_inputs[2]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase forward reactive energy", (double)reg_to_f(int_inputs[5], int_inputs[4]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/A phase reverse reactive energy", (double)reg_to_f(int_inputs[7], int_inputs[6]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase reverse reactive energy", (double)reg_to_f(int_inputs[9], int_inputs[8]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase reverse reactive energy", (double)reg_to_f(int_inputs[11], int_inputs[10]), persistent, if_changed, qos);
		}
	}

	auto rxbuf = mqtt.get_rxbuf();
}

void
eastron_sdm220(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	bool persistent = true;
	bool if_changed = true;
	int qos = 1;
	if (dev_cfg.exists("persistent")) {
		persistent = dev_cfg["persistent"];
	}
	if (dev_cfg.exists("unchanged")) {
		if_changed = !dev_cfg["unchanged"];
	}
	if (dev_cfg.exists("qos")) {
		qos = dev_cfg["qos"].get_numstr().getll();
	}

	{
		{
			auto int_inputs = mb.read_input_registers(address, 0x0000, 2 * 1);
			mqtt.publish(maintopic + "/A phase voltage", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0006, 2 * 1);
			mqtt.publish(maintopic + "/A phase current", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x000c, 2 * 1);
			mqtt.publish(maintopic + "/A phase active power", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/total active power", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0012, 2 * 1);
			mqtt.publish(maintopic + "/A phase apparent power", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/total apparent power", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0018, 2 * 1);
			mqtt.publish(maintopic + "/A phase reactive power", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/total reactive power", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x001e, 2 * 1);
			mqtt.publish(maintopic + "/A phase power factor", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/total power factor", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0024, 2 * 1);
			mqtt.publish(maintopic + "/A phase angle", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/total angle", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x0046, 2 * 5);
			mqtt.publish(maintopic + "/frequency", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/forward active energy", (double)reg_to_f(int_inputs[3], int_inputs[2]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/reverse active energy", (double)reg_to_f(int_inputs[5], int_inputs[4]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/forward reactive energy", (double)reg_to_f(int_inputs[7], int_inputs[6]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/reverse reactive energy", (double)reg_to_f(int_inputs[9], int_inputs[8]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x00c8, 2 * 3);
			mqtt.publish(maintopic + "/AB line voltage", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/BC line voltage", (double)reg_to_f(int_inputs[3], int_inputs[2]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/CA line voltage", (double)reg_to_f(int_inputs[5], int_inputs[4]), persistent, if_changed, qos);
		}
		{
			auto int_inputs = mb.read_input_registers(address, 0x00c8, 2 * 3);
			mqtt.publish(maintopic + "/AB line voltage", (double)reg_to_f(int_inputs[1], int_inputs[0]), persistent, if_changed, qos);
		}
	}

	auto rxbuf = mqtt.get_rxbuf();
}

void
ZGEJ_powermeter(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	bool persistent = true;
	bool if_changed = true;
	int qos = 1;
	if (dev_cfg.exists("persistent")) {
		persistent = dev_cfg["persistent"];
	}
	if (dev_cfg.exists("unchanged")) {
		if_changed = !dev_cfg["unchanged"];
	}
	if (dev_cfg.exists("qos")) {
		qos = dev_cfg["qos"].get_numstr().getll();
	}

	{
		{
			auto int_inputs = mb.read_input_registers(address, 0x0018, 2 * 34);
			mqtt.publish(maintopic + "/A phase voltage", (double)reg_to_f(int_inputs[0], int_inputs[1]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase voltage", (double)reg_to_f(int_inputs[2], int_inputs[3]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase voltage", (double)reg_to_f(int_inputs[4], int_inputs[5]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/AB line voltage", (double)reg_to_f(int_inputs[6], int_inputs[7]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/BC line voltage", (double)reg_to_f(int_inputs[8], int_inputs[9]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/CA line voltage", (double)reg_to_f(int_inputs[10], int_inputs[11]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/A phase current", (double)reg_to_f(int_inputs[12], int_inputs[13]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase current", (double)reg_to_f(int_inputs[14], int_inputs[15]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase current", (double)reg_to_f(int_inputs[16], int_inputs[17]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/A phase active power", (double)reg_to_f(int_inputs[18], int_inputs[19]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase active power", (double)reg_to_f(int_inputs[20], int_inputs[21]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase active power", (double)reg_to_f(int_inputs[22], int_inputs[23]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/total active power", (double)reg_to_f(int_inputs[24], int_inputs[25]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/A phase reactive power", (double)reg_to_f(int_inputs[26], int_inputs[27]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase reactive power", (double)reg_to_f(int_inputs[28], int_inputs[29]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase reactive power", (double)reg_to_f(int_inputs[30], int_inputs[31]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/total reactive power", (double)reg_to_f(int_inputs[32], int_inputs[33]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/A phase apparent power", (double)reg_to_f(int_inputs[34], int_inputs[35]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase apparent power", (double)reg_to_f(int_inputs[36], int_inputs[37]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase apparent power", (double)reg_to_f(int_inputs[38], int_inputs[39]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/total apparent power", (double)reg_to_f(int_inputs[40], int_inputs[41]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/A phase power factor", (double)reg_to_f(int_inputs[42], int_inputs[43]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/B phase power factor", (double)reg_to_f(int_inputs[44], int_inputs[45]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/C phase power factor", (double)reg_to_f(int_inputs[46], int_inputs[47]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/total power factor", (double)reg_to_f(int_inputs[48], int_inputs[49]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/frequency", (double)reg_to_f(int_inputs[50], int_inputs[51]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/forward active energy 2", (double)reg_to_f(int_inputs[52], int_inputs[53]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/reverse active energy 2", (double)reg_to_f(int_inputs[54], int_inputs[55]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/forward reactive energy 2", (double)reg_to_f(int_inputs[56], int_inputs[57]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/reverse reactive energy 2", (double)reg_to_f(int_inputs[58], int_inputs[59]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/forward active energy", (double)reg_to_f(int_inputs[60], int_inputs[61]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/reverse active energy", (double)reg_to_f(int_inputs[62], int_inputs[63]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/forward reactive energy", (double)reg_to_f(int_inputs[64], int_inputs[65]), persistent, if_changed, qos);
			mqtt.publish(maintopic + "/reverse reactive energy", (double)reg_to_f(int_inputs[66], int_inputs[67]), persistent, if_changed, qos);
		}
	}

	auto rxbuf = mqtt.get_rxbuf();
}

void
eth_tpr(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	bool persistent = true;
	bool if_changed = true;
	int qos = 1;
	if (dev_cfg.exists("persistent")) {
		persistent = dev_cfg["persistent"];
	}
	if (dev_cfg.exists("unchanged")) {
		if_changed = !dev_cfg["unchanged"];
	}
	if (dev_cfg.exists("qos")) {
		qos = dev_cfg["qos"].get_numstr().getll();
	}

	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 4);

		mqtt.publish(maintopic + "/input0", bin_inputs[0] ? "1" : "0", persistent, if_changed, qos);
		mqtt.publish(maintopic + "/input1", bin_inputs[1] ? "1" : "0", persistent, if_changed, qos);
		mqtt.publish(maintopic + "/input2", bin_inputs[2] ? "1" : "0", persistent, if_changed, qos);
		mqtt.publish(maintopic + "/input3", bin_inputs[3] ? "1" : "0", persistent, if_changed, qos);
	}

	auto rxbuf = mqtt.get_rxbuf();
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic == maintopic + "/relais0") {
			bool val = (rxbuf[i].message == "0") ? 0 : 1;
			mb.write_coil(address, 0, val);
		}
		if (rxbuf[i].topic == maintopic + "/relais1") {
			bool val = (rxbuf[i].message == "0") ? 0 : 1;
			mb.write_coil(address, 1, val);
		}
	}
}

void
eth_tpr_ldr(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	bool persistent = true;
	bool if_changed = true;
	int qos = 1;
	if (dev_cfg.exists("persistent")) {
		persistent = dev_cfg["persistent"];
	}
	if (dev_cfg.exists("unchanged")) {
		if_changed = !dev_cfg["unchanged"];
	}
	if (dev_cfg.exists("qos")) {
		qos = dev_cfg["qos"].get_numstr().getll();
	}

	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 4);

		mqtt.publish(maintopic + "/input0", bin_inputs[0] ? "1" : "0", persistent, if_changed, qos);
		mqtt.publish(maintopic + "/input1", bin_inputs[1] ? "1" : "0", persistent, if_changed, qos);
		mqtt.publish(maintopic + "/input2", bin_inputs[2] ? "1" : "0", persistent, if_changed, qos);
		mqtt.publish(maintopic + "/input3", bin_inputs[3] ? "1" : "0", persistent, if_changed, qos);
	}

	{
		auto int_inputs = mb.read_input_registers(address, 0, 14);

		// 16bit counter - should verify for rollover and restart
		mqtt.publish(maintopic + "/counter0", S + int_inputs[0], persistent, if_changed, qos);
		mqtt.publish(maintopic + "/counter1", S + int_inputs[1], persistent, if_changed, qos);
		mqtt.publish(maintopic + "/counter2", S + int_inputs[2], persistent, if_changed, qos);
		mqtt.publish(maintopic + "/counter3", S + int_inputs[3], persistent, if_changed, qos);

		mqtt.publish(maintopic + "/ldr0", S + int_inputs[4], persistent, if_changed, qos);
		// XXX check firmware version for functional LDR1 input
		mqtt.publish(maintopic + "/ldr1", S + int_inputs[5], persistent, if_changed, qos);

		// 32 bit counter - should verify for restart if autoreset is not enabled
		{
			uint32_t tmp = (uint32_t)int_inputs[6] | (uint32_t)int_inputs[7] << 16;
			mqtt.publish(maintopic + "/counter4", S + tmp, persistent, if_changed, qos);
		}
		{
			uint32_t tmp = (uint32_t)int_inputs[8] | (uint32_t)int_inputs[9] << 16;
			mqtt.publish(maintopic + "/counter5", S + tmp, persistent, if_changed, qos);
		}
		{
			uint32_t tmp = (uint32_t)int_inputs[10] | (uint32_t)int_inputs[11] << 16;
			mqtt.publish(maintopic + "/counter6", S + tmp, persistent, if_changed, qos);
		}
		{
			uint32_t tmp = (uint32_t)int_inputs[12] | (uint32_t)int_inputs[13] << 16;
			mqtt.publish(maintopic + "/counter7", S + tmp, persistent, if_changed, qos);
		}
	}

	if (dev_cfg.exists("DS18B20")) {
		int64_t max_sensor = dev_cfg["DS18B20"].get_array().max;
		for (int64_t i = 0; i <= max_sensor; i++) {
			int16_t sensor_register = dev_cfg["DS18B20"][i]["register"].get_numstr().getll();
			try {
				uint16_t value = mb.read_input_register(address, sensor_register);
				double temp = (double)value / 16;
				mqtt.publish(maintopic + "/temperature" + i, d_to_s(temp), persistent, if_changed, qos);
			} catch (...) {
			}
		}
	}

	auto rxbuf = mqtt.get_rxbuf();
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
}

void
rs485_jalousie(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	bool persistent = true;
	bool if_changed = true;
	int qos = 1;
	if (dev_cfg.exists("persistent")) {
		persistent = dev_cfg["persistent"];
	}
	if (dev_cfg.exists("unchanged")) {
		if_changed = !dev_cfg["unchanged"];
	}
	if (dev_cfg.exists("qos")) {
		qos = dev_cfg["qos"].get_numstr().getll();
	}

	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 8);

		for (int64_t i = 0; i < 8; i++) {
			mqtt.publish(maintopic + "/input" + i, bin_inputs[i] ? "1" : "0", persistent, if_changed, qos);
		}
	}

	auto rxbuf = mqtt.get_rxbuf();
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
rs485_relais6(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	bool persistent = true;
	bool if_changed = true;
	int qos = 1;
	if (dev_cfg.exists("persistent")) {
		persistent = dev_cfg["persistent"];
	}
	if (dev_cfg.exists("unchanged")) {
		if_changed = !dev_cfg["unchanged"];
	}
	if (dev_cfg.exists("qos")) {
		qos = dev_cfg["qos"].get_numstr().getll();
	}

	// XXX no counter support yet
	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 8);

		for (int64_t i = 0; i < 8; i++) {
			mqtt.publish(maintopic + "/input" + i, bin_inputs[i] ? "1" : "0", persistent, if_changed, qos);
		}
	}
	auto rxbuf = mqtt.get_rxbuf();
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
rs485_shtc3(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	bool persistent = true;
	bool if_changed = true;
	int qos = 1;
	if (dev_cfg.exists("persistent")) {
		persistent = dev_cfg["persistent"];
	}
	if (dev_cfg.exists("unchanged")) {
		if_changed = !dev_cfg["unchanged"];
	}
	if (dev_cfg.exists("qos")) {
		qos = dev_cfg["qos"].get_numstr().getll();
	}

	auto int_inputs = mb.read_input_registers(address, 0, 2);
	double temp = (double)(int16_t)int_inputs[0] / 10.0;
	double humid = (double)int_inputs[1] / 10.0;
	mqtt.publish(maintopic + "/temperature", S + d_to_s(temp, 1), persistent, if_changed, qos);
	mqtt.publish(maintopic + "/humidity", S + d_to_s(humid, 1), persistent, if_changed, qos);

	auto rxbuf = mqtt.get_rxbuf();
}

void
rs485_laserdistance(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	bool persistent = true;
	bool if_changed = true;
	int qos = 1;
	if (dev_cfg.exists("persistent")) {
		persistent = dev_cfg["persistent"];
	}
	if (dev_cfg.exists("unchanged")) {
		if_changed = !dev_cfg["unchanged"];
	}
	if (dev_cfg.exists("qos")) {
		qos = dev_cfg["qos"].get_numstr().getll();
	}

	auto int_inputs = mb.read_input_registers(address, 0, 3);
	{
		int32_t tmp = (uint32_t)int_inputs[0] | (uint32_t)int_inputs[1] << 16;
		mqtt.publish(maintopic + "/weight", S + tmp, persistent, if_changed, qos);
	}
	mqtt.publish(maintopic + "/distance", S + int_inputs[2], persistent, if_changed, qos);

	auto rxbuf = mqtt.get_rxbuf();
}

void
eth_io88(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	bool persistent = true;
	bool if_changed = true;
	int qos = 1;
	if (dev_cfg.exists("persistent")) {
		persistent = dev_cfg["persistent"];
	}
	if (dev_cfg.exists("unchanged")) {
		if_changed = !dev_cfg["unchanged"];
	}
	if (dev_cfg.exists("qos")) {
		qos = dev_cfg["qos"].get_numstr().getll();
	}

	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 8);

		for (int i = 0; i < 8; i++) {
			mqtt.publish(maintopic + "/input" + i, bin_inputs[i] ? "1" : "0", persistent, if_changed, qos);
		}
	}

	auto rxbuf = mqtt.get_rxbuf();
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
rs485_io88(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	bool persistent = true;
	bool if_changed = true;
	int qos = 1;
	if (dev_cfg.exists("persistent")) {
		persistent = dev_cfg["persistent"];
	}
	if (dev_cfg.exists("unchanged")) {
		if_changed = !dev_cfg["unchanged"];
	}
	if (dev_cfg.exists("qos")) {
		qos = dev_cfg["qos"].get_numstr().getll();
	}

	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 8);

		for (int i = 0; i < 8; i++) {
			mqtt.publish(maintopic + "/input" + i, bin_inputs[i] ? "1" : "0", persistent, if_changed, qos);
		}
	}

	auto rxbuf = mqtt.get_rxbuf();
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
rs485_adc_dac(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	bool persistent = true;
	bool if_changed = true;
	int qos = 1;
	if (dev_cfg.exists("persistent")) {
		persistent = dev_cfg["persistent"];
	}
	if (dev_cfg.exists("unchanged")) {
		if_changed = !dev_cfg["unchanged"];
	}
	if (dev_cfg.exists("qos")) {
		qos = dev_cfg["qos"].get_numstr().getll();
	}

	{
		auto int_inputs = mb.read_input_registers(address, 0, 10);
		mqtt.publish(maintopic + "/adc0", S + int_inputs[0], persistent, if_changed, qos);
		mqtt.publish(maintopic + "/adc1", S + int_inputs[1], persistent, if_changed, qos);
		mqtt.publish(maintopic + "/adc2", S + int_inputs[2], persistent, if_changed, qos);
		mqtt.publish(maintopic + "/adc3", S + int_inputs[3], persistent, if_changed, qos);
		mqtt.publish(maintopic + "/ref", S + int_inputs[9], persistent, if_changed, qos);
	}

	auto rxbuf = mqtt.get_rxbuf();
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
rs485_rfid125_disp(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
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
			mqtt.publish(maintopic + "/key", key, false);
		}
	}

	auto rxbuf = mqtt.get_rxbuf();
}

void
rs485_rfid125(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
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
			mqtt.publish(maintopic + "/key", key, false);
		}
	}

	auto rxbuf = mqtt.get_rxbuf();
}

void
rs485_thermocouple(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	bool persistent = true;
	bool if_changed = true;
	int qos = 1;
	if (dev_cfg.exists("persistent")) {
		persistent = dev_cfg["persistent"];
	}
	if (dev_cfg.exists("unchanged")) {
		if_changed = !dev_cfg["unchanged"];
	}
	if (dev_cfg.exists("qos")) {
		qos = dev_cfg["qos"].get_numstr().getll();
	}

	{
		auto bin_inputs = mb.read_discrete_inputs(address, 0, 24);
		for (int i = 0; i < 8; i++) {
			mqtt.publish(maintopic + "/open_error" + i, bin_inputs[i * 3] ? "1" : "0", persistent, if_changed, qos);
			mqtt.publish(maintopic + "/gnd_short" + i, bin_inputs[i * 3 + 1] ? "1" : "0", persistent, if_changed, qos);
			mqtt.publish(maintopic + "/vcc_short" + i, bin_inputs[i * 3 + 2] ? "1" : "0", persistent, if_changed, qos);
		}
	}

	{
		auto int_inputs = mb.read_input_registers(address, 0, 16);
		for (int i = 0; i < 8; i++) {
			mqtt.publish(maintopic + "/temperature" + i, S + (int16_t)int_inputs[i * 2], persistent, if_changed, qos);
			mqtt.publish(maintopic + "/cold_temperature" + i, S + (int16_t)int_inputs[ i * 2 + 1], persistent, if_changed, qos);
		}
	}

	auto rxbuf = mqtt.get_rxbuf();
}

void
rs485_chamberpump(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	bool persistent = true;
	bool if_changed = true;
	int qos = 1;
	if (dev_cfg.exists("persistent")) {
		persistent = dev_cfg["persistent"];
	}
	if (dev_cfg.exists("unchanged")) {
		if_changed = !dev_cfg["unchanged"];
	}
	if (dev_cfg.exists("qos")) {
		qos = dev_cfg["qos"].get_numstr().getll();
	}

	{
		auto int_inputs = mb.read_input_registers(address, 0, 9);
		mqtt.publish(maintopic + "/adc0", S + int_inputs[0], persistent, if_changed, qos);
		mqtt.publish(maintopic + "/adc1", S + int_inputs[1], persistent, if_changed, qos);
		mqtt.publish(maintopic + "/adc2", S + int_inputs[2], persistent, if_changed, qos);
		mqtt.publish(maintopic + "/adc3", S + int_inputs[3], persistent, if_changed, qos);
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
			mqtt.publish(maintopic + "/state", "empty", persistent, if_changed, qos);
		}
		{
			uint32_t tmp = (uint32_t)int_inputs[5] | (uint32_t)int_inputs[6] << 16;
			mqtt.publish(maintopic + "/cyclecounter", S + tmp, persistent, if_changed, qos);
		}
		{
			uint32_t tmp = (uint32_t)int_inputs[7] | (uint32_t)int_inputs[8] << 16;
			mqtt.publish(maintopic + "/cycletime", S + tmp, persistent, if_changed, qos);
		}
	}

	auto rxbuf = mqtt.get_rxbuf();
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
			bool persistent = true;
			bool if_changed = true;
			int qos = 1;
			if (dev_cfg.exists("persistent")) {
				persistent = dev_cfg["persistent"];
			}
			if (dev_cfg.exists("unchanged")) {
				if_changed = !dev_cfg["unchanged"];
			}
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
						mqtt.subscribe(maintopic + "/+");
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
						mqtt.publish(maintopic + "/vendor", devdata[dev]["vendor"], persistent, if_changed, qos);
					}
					if (devdata[dev].exists("product")) {
						mqtt.publish(maintopic + "/product", devdata[dev]["product"], persistent, if_changed, qos);
					}
					if (devdata[dev].exists("version")) {
						mqtt.publish(maintopic + "/version", devdata[dev]["version"], persistent, if_changed, qos);
					}
					if (!product.empty() && !vendor.empty()) {
						(*devfunctions[vendor][product])(mb, mqtt, address, maintopic, devdata[dev], dev_cfg);
					}
					mqtt.publish(maintopic + "/status", "online", persistent, if_changed, qos);
					lasttime[dev] = now;
				}
			} catch(...) {
				mqtt.publish(maintopic + "/status", "offline", persistent, if_changed, qos);
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
		main_mqtt.publish(maintopic + "/product", "mb_mqttbridge", true);
		main_mqtt.publish(maintopic + "/version", "0.7", true);
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

