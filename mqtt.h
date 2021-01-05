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

#ifndef I_MQTT
#define I_MQTT

#include "main.h"
#include <bwctmb/bwctmb.h>
#include <mosquitto.h>

class MQTT : public Base {
public:
	struct RXbuf {
		String topic;
		String message;
	};
private:
	struct mosquitto *mosq;
	AArray<String> rxdata;
	Mutex rxdata_mtx;
	Array<RXbuf> rxbuf;
	Array<String> subscribtions;
	Mutex subscribtion_mtx;

	static void int_connect_callback(struct mosquitto *mosq, void *obj, int result);
	void connect_callback(int result);
	static void int_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);
	void message_callback(const String& topic, const String& message);

public:
	String id;
	String host;
	int port;
	String username;
	String password;
	String maintopic;
	bool rxbuf_enable;
	String product;
	String version;

	MQTT();
	~MQTT();
	bool connect(void);
	void disconnect(void);
	void publish(const String& topic, const String& message, bool retain = true);
	void publish_ifchanged(const String& topic, const String& message);
	void subscribe(const String& topic);
	Array<RXbuf> get_rxbuf(const String& maintopic);
	String operator[](const String& topic);
};

#endif /* I_MQTT */
