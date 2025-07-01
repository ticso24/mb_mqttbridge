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
#include "vendor_epever.h"
#include "vendor_zgej.h"
#include "vendor_eastron.h"
#include "vendor_mru.h"
#include "vendor_trucki.h"

AArray<AArray<void (*)(Modbus& mb, Array<MQTT::RXbuf>& rxbuf, JSON& mqtt_data, uint8_t address, const String& maintopic, AArray<String>& devdata, JSON& dev_cfg)>> devfunctions;
static std::shared_ptr<JSON> config;
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

void*
ModbusLoop(void * arg)
{
	Array<AArray<String>> devdata;
	int64_t bus = *(int64_t*)arg;
	delete (int64_t*)arg;

	auto my_config = config;
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
							char buf[256];

							struct tm stm;
							localtime_r(&uts_time, &stm);
							strftime(buf, 256 - 1, "%Y-%m-%dT%H:%M:%S%z", &stm);
							date_str = buf;
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
		config.reset(new(JSON));
		config->parse(json);
	}

	mosquitto_lib_init();

	auto my_config = config;
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
	bwct_register();
	epever_register();
	zgej_register();
	eastron_register();
	mru_register();
	trucki_register();

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

