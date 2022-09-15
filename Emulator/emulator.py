print("""
##########################
#  
# Solarpanelanalytics sensordata emulator
#
# - For: Zuyd Hogeschool
# - Authors: 
#     - https://Github.com/Riktastic
#
# - Version: 1.0 (9 june 2021)
#
##########################
""")


###########################################
# Libraries
###########################################

import configparser
from datetime import datetime

# Required for MQTT
from paho.mqtt import client as mqtt_client

# Required for parsing data
import csv

# For randomising data
import random

from time import sleep

###########################################
# Script
###########################################

# Connect to MQTT
def connect_mqtt(mqtt_broker: str, mqtt_username: str, mqtt_password: str, mqtt_port: str, mqtt_clientid: str) -> mqtt_client:
    mqtt_cursor = mqtt_client.Client(mqtt_clientid)
    mqtt_cursor.username_pw_set(mqtt_username, mqtt_password)

    try:
        mqtt_cursor.connect(mqtt_broker, mqtt_port)
    except Exception as error:
        print("Connection to the MQTT broker failed! Error: " + str(error))
        exit()
    else:
        print("Connection to the MQTT broker was succesful. \n")

    return mqtt_cursor

# Runs when opening this file.
def run():
    # Reading the config file
    config = configparser.ConfigParser()
    config.read('config.ini')

    mqtt_broker = config['MQTT']['Broker']
    mqtt_username = config['MQTT']['Username']
    mqtt_password = config['MQTT']['Password']
    mqtt_port = int(config['MQTT']['Port'])
    mqtt_clientid = config['MQTT']['ClientID']
    mqtt_topic = config['MQTT']['Topic']


    # Connecting to MQTT
    mqtt_cursor = connect_mqtt(mqtt_broker, mqtt_username, mqtt_password, int(mqtt_port), mqtt_clientid)

    print("Publishing new messages using with the topic: " + mqtt_topic)

    print("\n--------------------\n")

    # Listen to new messages

    owners_and_devices = [
        [1,1, "2020-1-1 12:34:56"], [1,2, "2019-1-1 12:34:56"], [1,3, "2018-1-1 9:34:56"], [1,4, "2020-1-1 12:34:56"], [1,5, "2020-1-1 6:34:56"],
        [2,1, "2016-4-3 5:34:56"], [2,2, "2019-1-13 13:34:56"], [2,3, "2019-1-13 13:34:56"], [2,4, "2021-1-4 14:24:36"], [2,5, "2018-3-4 13:34:56"]
    ]

    while True:
        x = int(random.uniform(0, len(owners_and_devices) - 1))
        time =  str(int(random.uniform(2017, 2021))) + "-" + str(int(round(random.uniform(1, 12), 0))) + "-" + str(int(round(random.uniform(1, 28), 0))) + " " + str(int(round(random.uniform(1, 23), 0))) + ":" + str(int(round(random.uniform(10, 59), 0))) + ":" + str(int(round(random.uniform(10, 59), 0))) # String, format: %Y-%m-%d %H:%M:%S.
        deviceid = owners_and_devices[x][0] # Integer, begins from 1, is infinite. Defines the ID of the device. ID's should be used uniquely.
        ownerid = owners_and_devices[x][1] # Integer, begins from 1, is infinite. Defines the ID of the owner. ID's are uniquely conjugated with a person and one or multiple devices.
        installationdate = owners_and_devices[x][2] # "YYYY-mm-dd HH:MM:SS"
        uptime = int(random.uniform(1, 100000)) # Seconds
        wifisignalstrength = int(random.uniform(-30, -90)) # DBM
        temperature = round(random.uniform(-1, 60), 2)  # Float, 2 decimals, the DS18B20 sensor should measure from aproximately -55 to 125 degrees celcius. It's accuracy is around +/- 0,5 degrees celcius.
        lux = round(random.uniform(0, 54612.5), 2) # Float, 2 decimals, the BH1750 sensor reads from 0.0 up to 54612.5 lux.
        amps = round(random.uniform(0, 0.4), 4) # Float, 4 decimals, the SCT013 50A can read up to 50A. Out solarpanels have a combined theoretical max output of 400 mA (0,4A).

        payload = str(time) + "," + str(deviceid) + "," + str(ownerid) + "," + str(installationdate) + ","  + str(uptime) + "," + str(wifisignalstrength) + "," + str(temperature) + "," + str(lux) + "," + str(amps)
        print(payload)

        mqtt_cursor.publish(mqtt_topic, payload=payload)

        sleep(5)
if __name__ == '__main__':
    run()
