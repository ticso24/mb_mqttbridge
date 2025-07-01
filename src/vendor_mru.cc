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
#include "vendor_mru.h"

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
mru_register()
{
	// register devicefunctions
	devfunctions["MRU"]["SWG100"] = mru_swg100;
}
