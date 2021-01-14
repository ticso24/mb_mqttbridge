#!/bin/sh
#
# Copyright (c) 2021 Bernd Walter Computer Technology
# All rights reserved.
#
# PROVIDE: mb_mqttbridge
# REQUIRE: DAEMON
# KEYWORD: FreeBSD
#
# Add the following line to /etc/rc.conf to enable mb_mqttbridge:
#
# mb_mqttbridge_enable="YES"
#

mb_mqttbridge_enable=${mb_mqttbridge_enable-"NO"}

. /etc/rc.subr

name=mb_mqttbridge
rcvar=`set_rcvar`

command=/usr/local/sbin/${name}
pidfile=/var/run/${name}.pid
sig_stop=-KILL

load_rc_config ${name}
run_rc_command "$1"

