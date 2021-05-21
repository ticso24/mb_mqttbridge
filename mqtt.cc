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
#include "mqtt.h"

MQTT::MQTT()
{
	mosq = NULL;
	rxbuf_enable = false;
}

MQTT::~MQTT()
{
	disconnect();
}

bool
MQTT::connect()
{
	mosq = mosquitto_new(id.c_str(), true, this);
	int rc;

	if (mosq) {
		mosquitto_connect_callback_set(mosq, int_connect_callback);
		mosquitto_message_callback_set(mosq, int_message_callback);
		String willtopic = maintopic + "/status";
		if (!willtopic.empty()) {
			mosquitto_will_set(mosq, willtopic.c_str(), strlen("offline"), "offline", 1, true);
		}
		if (!username.empty()) {
			mosquitto_username_pw_set(mosq, username.c_str(), password.c_str());
		}
		rc = mosquitto_connect(mosq, host.c_str(), port, 5);

		mosquitto_loop_start(mosq);
		return true;
	}

	return false;
}

void
MQTT::disconnect()
{
	if (mosq) {
		mosquitto_disconnect(mosq);
		mosquitto_loop_stop(mosq, true);
		mosquitto_destroy(mosq);
	}
}

void
MQTT::publish_ifchanged(const JSON& element, const String& message)
{
	check_online(element);
	String topic = element["topic"];
	publish_ifchanged(topic, message);
}

void
MQTT::publish(const JSON& element, const String& message, bool retain)
{
	check_online(element);
	String topic = element["topic"];
	publish(topic, message, retain);
}

void
MQTT::publish_ifchanged(const String& topic, const String& message)
{
	bool send;

	rxdata_mtx.lock();
	if (rxdata[topic] != message) {
		send = true;
		rxdata[topic] = message;
	} else {
		send = false;
	}
	rxdata_mtx.unlock();
	if (send) {
		mosquitto_publish(mosq, NULL, topic.c_str(), message.length(), message.c_str(), 1, true);
	}
}

void
MQTT::publish(const String& topic, const String& message, bool retain)
{
	mosquitto_publish(mosq, NULL, topic.c_str(), message.length(), message.c_str(), 1, retain);
	rxdata_mtx.lock();
	rxdata[topic] = message;
	rxdata_mtx.unlock();
}

void
MQTT::subscribe(const String& topic)
{
	subscribtion_mtx.lock();
	mosquitto_subscribe(mosq, NULL, topic.c_str(), 0);
	subscribtions << topic;
	subscribtion_mtx.unlock();
}

void
MQTT::int_connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	MQTT* me = (MQTT*)obj;
	me->connect_callback(result);
}

void
MQTT::connect_callback(int result)
{
	if (result == 0) {
		subscribtion_mtx.lock();
		for (int64_t i = 0; i <= subscribtions.max; i++) {
			mosquitto_subscribe(mosq, NULL, subscribtions[i].c_str(), 0);
		}
		String willtopic = maintopic + "/status";
		publish(willtopic, "online", true);
		subscribtion_mtx.unlock();
	}
}

void
MQTT::int_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	MQTT* me = (MQTT*)obj;

	String topic = message->topic;
	String msg = (char*)message->payload;
	me->message_callback(topic, msg);
}

void
MQTT::message_callback(const String& topic, const String& message)
{
	rxdata_mtx.lock();
	rxdata[topic] = message;
	if (rxbuf_enable) {
		int64_t newpos = rxbuf.max + 1;
		rxbuf[newpos].topic = topic;
		rxbuf[newpos].message = message;
	}
	rxdata_mtx.unlock();
}

Array<MQTT::RXbuf>
MQTT::get_rxbuf()
{
	Array<RXbuf> tmp;
	rxdata_mtx.lock();
	std::swap(tmp, rxbuf);
	rxdata_mtx.unlock();
	return tmp;
}

void
MQTT::check_online(const JSON& element)
{
	if (element.exists("status_topic")) {
		String status_topic = element["status_topic"];
		String status = (*this)[status_topic];
		if (status != "online") {
			String topic = element["topic"];
			throw(Error(S + "device " + topic + " not online"));
		}
	}
}

MQTT::Datawrapper
MQTT::operator[](const JSON& element)
{
	check_online(element);
	String topic = element["topic"];
	return (*this)[topic];
}

MQTT::Datawrapper
MQTT::operator[](const String& topic)
{
	String ret;
	bool existing;

	rxdata_mtx.lock();
	if (rxdata.exists(topic)) {
		ret = rxdata[topic];
		existing = true;
	} else {
		existing = false;
	}
	rxdata_mtx.unlock();

	if (!existing) {
		throw Error(S + "No data for topic " + topic);
	}
	return ret;
}

