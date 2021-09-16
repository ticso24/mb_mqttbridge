# mb_mqttbridge

mb_mqttbridge is a daemon to bridge [Modbus](https://modbus.org)/TCP devices into MQTT.
For non TCP devices it is expected to run a bridge device or software, like the [BWCT](https://www.bwct.de/) DIN-ETH-IO88 device for RS485 Modbus/RTU.

## Installation

[libbwct](https://github.com/ticso24/libbwct),
[libbwctmb](https://github.com/ticso24/libbwctmb) and
[Mosquitto](https://mosquitto.org/) client libraries are required

```sh
make
make install
```

## Source Code

The source code is available under
  * GitHub: <https://github.com/ticso24/mb_mqttbridge>

## License
BSD

