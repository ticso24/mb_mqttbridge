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
#include "vendor_bwct.h"

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
			uint16_t sensor_register = dev_cfg["DS18B20"][i]["register"].get_numstr().getll();
			try {
				int16_t value = mb.read_input_register(address, sensor_register);
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
	uint32_t major = -1;
	uint32_t minor = -1;

	if (devdata.exists("version")) {
		Array<String> v = devdata["version"].split(".");
		if (v.max >= 0) {
			major = v[0].getll();
		}
		if (v.max >= 1) {
			minor = v[1].getll();
		}
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

	if (major >= 0 && minor >= 7) {
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
	}

	if (major >= 0 && minor >= 8) {
		auto bin_times = mb.read_input_registers(address, 32, 2 * 8);

		Array<JSON> times;
		for (int i = 0; i < 8; i++) {
			uint64_t tmp = 0;
			for (int j = 0; j < 2; j++) {
				tmp |= bin_times[i * 4 + j] << (j * 16);
			}
			times[i].set_number(d_to_s((((double)tmp) / 10000.0), 2));
		}
		mqtt_data["counttime"] = times;
	}

	if (major >= 0 && minor >= 10) {
		auto bin_times = mb.read_input_registers(address, 48, 2 * 8);

		Array<JSON> times;
		for (int i = 0; i < 8; i++) {
			uint64_t tmp = 0;
			for (int j = 0; j < 2; j++) {
				tmp |= bin_times[i * 4 + j] << (j * 16);
			}
			times[i].set_number(d_to_s((((double)tmp) / 10000.0), 2));
		}
		mqtt_data["counttime_timer"] = times;
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
			uint16_t sensor_register = dev_cfg["DS18B20"][i]["register"].get_numstr().getll();
			try {
				int16_t value = mb.read_input_register(address, sensor_register);
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
bwct_register()
{
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
}

