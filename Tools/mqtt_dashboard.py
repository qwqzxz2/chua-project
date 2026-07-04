#!/usr/bin/env python3
"""
SEMS MQTT Dashboard
Subscribes to MQTT topic and displays real-time sensor data.
Usage: python mqtt_dashboard.py
Requires: pip install paho-mqtt
"""
import json
import os
import sys
from datetime import datetime

BROKER = os.getenv("MQTT_BROKER", "broker.emqx.io")
PORT = int(os.getenv("MQTT_PORT", "1883"))
TOPIC = os.getenv("MQTT_TOPIC", "sems/data")
CLIENT_ID = os.getenv("MQTT_CLIENT_ID", "sems_dashboard")

def on_connect(client, userdata, flags, rc):
    print(f"[Connected to {BROKER}:{PORT}]")
    client.subscribe(TOPIC)

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        ts = datetime.fromtimestamp(data.get("ts", 0) / 1000)
        temp = data.get("temp", "N/A")
        humid = data.get("humid", "N/A")
        press = data.get("press", "N/A")
        aqi = data.get("aqi", "N/A")
        light = data.get("light", "N/A")
        print(f"[{ts}] Temp:{temp}C Hum:{humid}% Press:{press}hPa AQI:{aqi}ppm Light:{light}lux")
    except Exception as e:
        print(f"[Parse Error] {e}")

if __name__ == "__main__":
    try:
        import paho.mqtt.client as mqtt
    except ImportError:
        print("Please install: pip install paho-mqtt")
        sys.exit(1)

    client = mqtt.Client(client_id=CLIENT_ID)
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT, 60)
    print(f"Subscribing to {TOPIC}...")
    client.loop_forever()
