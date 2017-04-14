#ifndef __SETUP_H
#define __SETUP_H

#include <arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <FS.h>
#include <TimeLib.h>




void setup_FS();
void setup_wifi();
void sendNTPpacket(IPAddress &address);
time_t getNtpTime();
void Setup_TimeClient();
void setup_OTA();
void callback(char* topic, byte* payload, unsigned int length);
void MQTT_Connect();
void EEprom_Write2Bytes(int Address,uint16_t Data);
uint16_t EEprom_Read2Bytes(int Address);
void startUp();

#endif
