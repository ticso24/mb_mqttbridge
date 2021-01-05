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
static SArray<Modbus*> mbs; // XXX no automatic deletion
static AArray<void (*)(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)> devfunctions;
static MQTT main_mqtt;

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

void
eth_tpr(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		SArray<bool> bin_inputs = mb.read_discrete_inputs(address, 0, 4);

		mqtt.publish_ifchanged(maintopic + "/input0", bin_inputs[0] ? "1" : "0");
		mqtt.publish_ifchanged(maintopic + "/input1", bin_inputs[1] ? "1" : "0");
		mqtt.publish_ifchanged(maintopic + "/input2", bin_inputs[2] ? "1" : "0");
		mqtt.publish_ifchanged(maintopic + "/input3", bin_inputs[3] ? "1" : "0");
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
	{
		SArray<bool> bin_inputs = mb.read_discrete_inputs(address, 0, 4);

		mqtt.publish_ifchanged(maintopic + "/input0", bin_inputs[0] ? "1" : "0");
		mqtt.publish_ifchanged(maintopic + "/input1", bin_inputs[1] ? "1" : "0");
		mqtt.publish_ifchanged(maintopic + "/input2", bin_inputs[2] ? "1" : "0");
		mqtt.publish_ifchanged(maintopic + "/input3", bin_inputs[3] ? "1" : "0");
	}

	{
		SArray<uint16_t> int_inputs = mb.read_input_registers(address, 0, 14);

		// 16bit counter - should verify for rollover and restart
		mqtt.publish_ifchanged(maintopic + "/counter0", S + int_inputs[0]);
		mqtt.publish_ifchanged(maintopic + "/counter1", S + int_inputs[1]);
		mqtt.publish_ifchanged(maintopic + "/counter2", S + int_inputs[2]);
		mqtt.publish_ifchanged(maintopic + "/counter3", S + int_inputs[3]);

		mqtt.publish_ifchanged(maintopic + "/ldr0", S + int_inputs[4]);
		// XXX check firmware version for functional LDR1 input
		mqtt.publish_ifchanged(maintopic + "/ldr1", S + int_inputs[5]);

		// 32 bit counter - should verify for restart if autoreset is not enabled
		{
			uint32_t tmp = (uint32_t)int_inputs[6] | (uint32_t)int_inputs[7] << 16;
			mqtt.publish_ifchanged(maintopic + "/counter4", S + tmp);
		}
		{
			uint32_t tmp = (uint32_t)int_inputs[8] | (uint32_t)int_inputs[9] << 16;
			mqtt.publish_ifchanged(maintopic + "/counter5", S + tmp);
		}
		{
			uint32_t tmp = (uint32_t)int_inputs[10] | (uint32_t)int_inputs[11] << 16;
			mqtt.publish_ifchanged(maintopic + "/counter6", S + tmp);
		}
		{
			uint32_t tmp = (uint32_t)int_inputs[12] | (uint32_t)int_inputs[13] << 16;
			mqtt.publish_ifchanged(maintopic + "/counter7", S + tmp);
		}
	}

	if (dev_cfg.exists("DS18B20")) {
		int64_t max_sensor = dev_cfg["DS18B20"].get_array().max;
		for (int64_t i = 0; i <= max_sensor; i++) {
			int16_t sensor_register = dev_cfg["DS18B20"][i]["register"].get_numstr().getll();
			try {
				uint16_t value = mb.read_input_register(address, sensor_register);
				double temp = (double)value / 16;
				mqtt.publish_ifchanged(maintopic + "/temperature" + i, S + temp);
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
jalousie(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		SArray<bool> bin_inputs = mb.read_discrete_inputs(address, 0, 8);

		for (int64_t i = 0; i < 8; i++) {
			mqtt.publish_ifchanged(maintopic + "/input" + i, bin_inputs[i] ? "1" : "0");
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
relais6(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	// XXX no counter support yet
	{
		SArray<bool> bin_inputs = mb.read_discrete_inputs(address, 0, 8);

		for (int64_t i = 0; i < 8; i++) {
			mqtt.publish_ifchanged(maintopic + "/input" + i, bin_inputs[i] ? "1" : "0");
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
shct3(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	SArray<uint16_t> int_inputs = mb.read_input_registers(address, 0, 2);
	mqtt.publish_ifchanged(maintopic + "/temperature", S + (int16_t)int_inputs[0]);
	mqtt.publish_ifchanged(maintopic + "/humidity", S + int_inputs[1]);

	auto rxbuf = mqtt.get_rxbuf();
}

void
laserdistance(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	SArray<uint16_t> int_inputs = mb.read_input_registers(address, 0, 3);
	{
		int32_t tmp = (uint32_t)int_inputs[0] | (uint32_t)int_inputs[1] << 16;
		mqtt.publish_ifchanged(maintopic + "/weight", S + tmp);
	}
	mqtt.publish_ifchanged(maintopic + "/distance", S + int_inputs[2]);

	auto rxbuf = mqtt.get_rxbuf();
}

void
io88(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		SArray<bool> bin_inputs = mb.read_discrete_inputs(address, 0, 8);

		for (int i = 0; i < 8; i++) {
			mqtt.publish_ifchanged(maintopic + "/input" + i, bin_inputs[i] ? "1" : "0");
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
adc_dac(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		SArray<uint16_t> int_inputs = mb.read_input_registers(address, 0, 10);
		mqtt.publish_ifchanged(maintopic + "/adc0", S + int_inputs[0]);
		mqtt.publish_ifchanged(maintopic + "/adc1", S + int_inputs[1]);
		mqtt.publish_ifchanged(maintopic + "/adc2", S + int_inputs[2]);
		mqtt.publish_ifchanged(maintopic + "/adc3", S + int_inputs[3]);
		mqtt.publish_ifchanged(maintopic + "/ref", S + int_inputs[9]);
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
rfid125_disp(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		SArray<uint16_t> int_inputs = mb.read_input_registers(address, 0, 11);
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
rfid125(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		SArray<uint16_t> int_inputs = mb.read_input_registers(address, 0, 11);
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
thermocouple(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		SArray<bool> bin_inputs = mb.read_discrete_inputs(address, 0, 24);
		for (int i = 0; i < 8; i++) {
			mqtt.publish_ifchanged(maintopic + "/open_error" + i, bin_inputs[i * 3] ? "1" : "0");
			mqtt.publish_ifchanged(maintopic + "/gnd_short" + i, bin_inputs[i * 3 + 1] ? "1" : "0");
			mqtt.publish_ifchanged(maintopic + "/vcc_short" + i, bin_inputs[i * 3 + 2] ? "1" : "0");
		}
	}

	{
		SArray<uint16_t> int_inputs = mb.read_input_registers(address, 0, 16);
		for (int i = 0; i < 8; i++) {
			mqtt.publish_ifchanged(maintopic + "/temperature" + i, S + (int16_t)int_inputs[i * 2]);
			mqtt.publish_ifchanged(maintopic + "/cold_temperature" + i, S + (int16_t)int_inputs[ i * 2 + 1]);
		}
	}

	auto rxbuf = mqtt.get_rxbuf();
}

void
chamberpump(Modbus& mb, MQTT& mqtt, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)
{
	{
		SArray<uint16_t> int_inputs = mb.read_input_registers(address, 0, 9);
		mqtt.publish_ifchanged(maintopic + "/adc0", S + int_inputs[0]);
		mqtt.publish_ifchanged(maintopic + "/adc1", S + int_inputs[1]);
		mqtt.publish_ifchanged(maintopic + "/adc2", S + int_inputs[2]);
		mqtt.publish_ifchanged(maintopic + "/adc3", S + int_inputs[3]);
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
			mqtt.publish_ifchanged(maintopic + "/state", "empty");
		}
		{
			uint32_t tmp = (uint32_t)int_inputs[5] | (uint32_t)int_inputs[6] << 16;
			mqtt.publish_ifchanged(maintopic + "/cyclecounter", S + tmp);
		}
		{
			uint32_t tmp = (uint32_t)int_inputs[7] | (uint32_t)int_inputs[8] << 16;
			mqtt.publish_ifchanged(maintopic + "/cycletime", S + tmp);
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
	JSON& modbuses = cfg["modbuses"];
	String host = modbuses[bus]["host"];
	String port = modbuses[bus]["port"];
	Modbus mb(host, port);
	Array<MQTT> dev_mqtts;

	for(;;) {
		for (int64_t dev = 0; dev <= bus_cfg["devices"].get_array().max; dev++) {
			JSON& dev_cfg = bus_cfg["devices"][dev];
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
					String product = mb.identification(address, 0);
					devdata[dev]["vendor"] = product;
					mqtt.publish_ifchanged(maintopic + "/vendor", product);
					if (devdata[dev]["vendor"] == "Bernd Walter Computer Technology") {
						if (!devfunctions.exists(product)) {
							throw(Error(S + "unknown product" + product));
						}
						devdata[dev]["product"] = "";
					}
				}
				if (!devdata[dev].exists("product")) {
					String tmp = mb.identification(address, 1);
					devdata[dev]["product"] = tmp;
					mqtt.publish_ifchanged(maintopic + "/product", tmp);
				}
				if (!devdata[dev].exists("version")) {
					String tmp = mb.identification(address, 2);
					devdata[dev]["version"] = tmp;
					mqtt.publish_ifchanged(maintopic + "/version", tmp);
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
				if (devdata[dev]["vendor"] == "Bernd Walter Computer Technology") {
					String product = devdata[dev]["product"];
					if (!product.empty()) {
						(*devfunctions[product])(mb, mqtt, address, maintopic, devdata[dev], dev_cfg);
					}
				}
				mqtt.publish_ifchanged(maintopic + "/status", "online");
			} catch(...) {
				mqtt.publish_ifchanged(maintopic + "/status", "offline");
				sleep(1);
			}
		}
		usleep(10000); // sleep 10ms
	}

	return NULL;
}

int
main(int argc, char *argv[]) {
	String configfile = "/usr/local/etc/mb_mqttbridge.conf";
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
		main_mqtt.connect();
		String willtopic = maintopic + "/status";
		main_mqtt.publish(willtopic, "online", true);
		main_mqtt.publish(maintopic + "/product", "mb_mqttbridge", true);
		main_mqtt.publish(maintopic + "/version", "0.4", true);
	} else {
		printf("no mqtt setup in config\n");
		exit(1);
	}

	if (!cfg.exists("modbuses")) {
		printf("no modbus setup in config\n");
		exit(1);
	}

	// register devicefunctions
	devfunctions["Ethernet-MB twin power relay / 4ch input"] = eth_tpr;
	devfunctions["Ethernet-MB RS485 / twin power relay / 4ch input / LDR / DS18B20"] = eth_tpr_ldr;
	devfunctions["MB 3x jalousie actor / 8ch input"] = jalousie;
	devfunctions["MB 6x power relay / 8ch input"] = relais6;
	devfunctions["RS485-SHTC3"] = shct3;
	devfunctions["RS485-Laserdistance-Weight"] = laserdistance;
	devfunctions["RS485-IO88"] = io88;
	devfunctions["MB ADC DAC"] = adc_dac;
	devfunctions["125kHz RFID Reader / Display"] = rfid125_disp;
	devfunctions["125kHz RFID Reader / Writer-Beta"] = rfid125;
	devfunctions["RS485-THERMOCOUPLE"] = thermocouple;
	devfunctions["RS485-Chamberpump"] = chamberpump;

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

