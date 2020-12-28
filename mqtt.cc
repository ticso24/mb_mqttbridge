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
		mosquitto_publish(mosq, NULL, willtopic.c_str(), strlen("online"), "online", 1, true);
		String producttopic = maintopic + "/product";
		mosquitto_publish(mosq, NULL, producttopic.c_str(), strlen("mb_mqttbridge"), "mb_mqttbridge", 1, true);
		String versiontopic = maintopic + "/version";
		mosquitto_publish(mosq, NULL, versiontopic.c_str(), strlen("0.1"), "0.1", 1, true);
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
MQTT::publish_ifchanged(String topic, String message)
{
	bool send;

	rxdata_mtx.lock();
	if (rxdata[topic] != message) {
		send = true;
	} else {
		send = false;
	}
	rxdata[topic] = message;
	rxdata_mtx.unlock();
	if (send) {
		mosquitto_publish(mosq, NULL, topic.c_str(), message.length(), message.c_str(), 1, true);
	}
}

void
MQTT::publish(String topic, String message, bool retain)
{
	mosquitto_publish(mosq, NULL, topic.c_str(), message.length(), message.c_str(), 1, retain);
	rxdata_mtx.lock();
	rxdata[topic] = message;
	rxdata_mtx.unlock();
}

void
MQTT::subscribe(String topic)
{
	mosquitto_subscribe(mosq, NULL, topic.c_str(), 0);
}

void
MQTT::int_connect_callback(struct mosquitto *mosq, void *obj, int result)
{
}

void
MQTT::int_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	MQTT* me = (MQTT*)obj;

	String topic = message->topic;
	String msg = message->payload;
	me->message_callback(topic, msg);
}

void
MQTT::message_callback(String topic, String message)
{
	rxdata_mtx.lock();
	rxdata[topic] = message;
	int64_t newpos = rxbuf.max + 1;
	rxbuf[newpos].topic = topic;
	rxbuf[newpos].message = message;
	rxdata_mtx.unlock();
}

Array<MQTT::RXbuf>
MQTT::get_rxbuf(const String& maintopic)
{
	Array<RXbuf> tmp;
	rxdata_mtx.lock();
	for (int64_t i = 0; i <= rxbuf.max; i++) {
		if (rxbuf[i].topic.strncmp(maintopic)) {
			tmp << rxbuf[i];
			rxbuf.del(i);
			i--;
		}
	}
	rxdata_mtx.unlock();
	return tmp;
}

String
MQTT::get_topic(const String& topic)
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

