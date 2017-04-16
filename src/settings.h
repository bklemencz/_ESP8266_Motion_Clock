#ifndef __SETTINGS_H
#define __SETTINGS_H

#define TIME_ZONE 8
#define TIME_UDP 8888
#define NTP_PACKET_SIZE 48
IPAddress timeServer(208, 75, 88, 4);

#define DISP_CLK D2
#define DISP_DIO D1
#define DISP_BRGT 0

#define RADAR_PIN D0
#define IR_PIN D7

#define SONAR_TRIG D6
#define SONAR_ECHO D5

#define USE_MQTT 1
#define MOT_DELAY 15

const bool HAS_SONIC = 1;
const bool HAS_PIR = 1;
const bool HAS_RADAR = 1;



#endif
