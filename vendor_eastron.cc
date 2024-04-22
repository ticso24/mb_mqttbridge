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
#include "vendor_eastron.h"

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
eastron_register()
{
	// register devicefunctions
	devfunctions["Eastron"]["SDM220"] = eastron_sdm220;
	devfunctions["Eastron"]["SDM630"] = eastron_sdm630;
	devfunctions["Eastron"]["SDM72"] = eastron_sdm630;
}

