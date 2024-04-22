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
#include "vendor_zgej.h"

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
zgej_register()
{
	// register devicefunctions
	devfunctions["Shanghai Chujin Electric"]["Panel Powermeter"] = ZGEJ_powermeter;
}

