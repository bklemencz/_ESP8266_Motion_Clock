#include <arduino.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <FS.h>
#include <TimeLib.h>
#include <TM1637Display.h>
#include <settings.h>

extern "C" {
#include "user_interface.h"
}

#include <SPI.h>
#include <Ethernet.h>

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 1, 177);
EthernetServer server(80);

String ssid,password,mqtt_server,hostName_l;

byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

WiFiClient espClient;
WiFiUDP Time_Udp;

unsigned int localPort = TIME_UDP;  // local port to listen for UDP packets

PubSubClient client(espClient);

TM1637Display display(DISP_CLK, DISP_DIO);

uint16_t prev_sec,time_disp;
uint8_t data[3];
bool Ir_Trig, Rad_Trig,MQTT_On,st_Sent;
String pubString;
time_t lastTrig;

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Time_Udp.beginPacket(address, 123); //NTP requests are to port 123
  Time_Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Time_Udp.endPacket();
}

time_t getNtpTime()
{
  while (Time_Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Time_Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Time_Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + TIME_ZONE * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

void Setup_TimeClient()
{
  Time_Udp.begin(localPort);
  setSyncProvider(getNtpTime);
  setSyncInterval(3600);
}

void setup_FS()
{
  bool ok = SPIFFS.begin();
  if (ok) {
    Serial.println("ok");
  }
  File f = SPIFFS.open("/wconf.txt", "r");
  if (!f) {
      Serial.println("file open failed");
  }
  ssid = f.readStringUntil('\n');
  password = f.readStringUntil('\n');
  mqtt_server = f.readStringUntil('\n');
  hostName_l = f.readStringUntil('\n');
  ssid.remove((ssid.length()-1));
  password.remove((password.length()-1));
  mqtt_server.remove((mqtt_server.length()-1));
  hostName_l.remove((hostName_l.length()-1));
  /////////// NEED TO RUN ONLY ONCE ///////////
    //Serial.println("Spiffs formating...");
    //SPIFFS.format();
    //Serial.println("Spiffs formatted");
}

void callback(char* topic, byte* payload, unsigned int length)
{
 // FlashLed(2, 500);
 //  TimeOut = atoi((char*)payload);
 //  Serial.print("New TimeOut: ");
 //  Serial.println(TimeOut);
}

void setup_wifi()
{
  wifi_set_phy_mode(PHY_MODE_11G);
  system_phy_set_max_tpw(21);
  WiFi.softAPdisconnect(true);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.hostname(hostName_l);
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("Connecting to: "); Serial.println(ssid.c_str());}
  Serial.println("Wifi Connected!");
  delay(1000);

}

void setup_OTA()
{
  ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
      {
        type = "filesystem";
        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        SPIFFS.end();
      }

      Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.setHostname(hostName_l.c_str());
    ArduinoOTA.begin();
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println(ArduinoOTA.getHostname());
}

void MQTT_Connect()
{
  client.setServer(mqtt_server.c_str(), 1883);
  client.setCallback(callback);
  // Subsriptions come here
  while (!client.connected())
   {
     if (client.connect(hostName_l.c_str()))
     {
       Serial.println("MQTT Online!");
       pubString = hostName_l + "status/online";
       client.publish(pubString.c_str(),"1");
     } else
   {
     delay(1000);
   }
 }
}

void Time_Disp()
{
  if (prev_sec != second())
  {
    prev_sec = second();
    data[0] = display.encodeDigit(hour()/10);
    data[1] = display.encodeDigit(hour()%10);
    if (second()%2)
      data[1] = data[1] + 0x80;
    data[2] = display.encodeDigit(minute()/10);
    data[3] = display.encodeDigit(minute()%10);
    display.setSegments(data, 4, 0);
  }
}

void Motion_Disp(bool Radar, bool Ir)
{
  data[0] = 0; data[1]=0; data[2]=0; data[3]=0;
  if (Radar) data[0]= 0b01001001;
  if (Ir) data[1] = 0b01001001;
  display.setSegments(data, 4, 0);
}

void Check_Motion(bool sendMQTT)
{
  if (digitalRead(RADAR_PIN) && !Rad_Trig)
  {
    delay(50);
    if (digitalRead(RADAR_PIN))
    {
      Rad_Trig = 1;
      if (digitalRead(IR_PIN))
      {
        if (sendMQTT)
        {
          client.publish((hostName_l + "/status/mot").c_str(),"3");
          MQTT_On = 1;
          lastTrig = now();
          //Serial.println(elapsedSecsToday(now()));
        }
        Motion_Disp(1,1);
      }
      else
      {
        if (sendMQTT)
        {
          client.publish((hostName_l + "/status/mot").c_str(),"1");
          MQTT_On = 1;
          lastTrig = now();
          //Serial.println(elapsedSecsToday(now()));
        }
        Motion_Disp(1, 0);
      }
    }
    delay(500);
  }

  if (digitalRead(IR_PIN) && !Ir_Trig)
  {
    delay(50);
    if (digitalRead(IR_PIN))
    {
      Ir_Trig = 1;
      if (digitalRead(RADAR_PIN))
      {
        if (sendMQTT)
        {
          client.publish((hostName_l + "/status/mot").c_str(),"3");
          MQTT_On = 1;
          lastTrig = now();
          //Serial.println(elapsedSecsToday(now()));
        }
        Motion_Disp(1,1);
      }
      else
      {
        if (sendMQTT)
        {
          client.publish((hostName_l + "/status/mot").c_str(),"2");
          MQTT_On = 1;
          lastTrig = now();
          //Serial.println(elapsedSecsToday(now()));
        }
        Motion_Disp(0, 1);
      }
      delay(500);
    }
  }
  if (!digitalRead(RADAR_PIN)) Rad_Trig = 0;
  if (!digitalRead(IR_PIN)) Ir_Trig = 0;
  if (!digitalRead(RADAR_PIN) & !digitalRead(IR_PIN))
  {
    if (MQTT_On)
    {
      if ((now() - lastTrig) > MOT_DELAY)
      {
        client.publish((hostName_l + "/status/mot").c_str(),"0");
        MQTT_On = 0;
      }
    }
    else if (minute()%2 && !st_Sent)        // Off Status every 2 minutes
    {
        client.publish((hostName_l + "/status/mot").c_str(),"0");
        st_Sent = 1;
    }
    if (!(minute()%2)) st_Sent = 0;
  }
  client.loop();
}


void EEprom_Write2Bytes(int Address,uint16_t Data)
{
  EEPROM.write(Address, Data / 256);
  EEPROM.write(Address+1, Data % 256);
  EEPROM.end();
}

uint16_t EEprom_Read2Bytes(int Address)
{
  return EEPROM.read(Address)*256 + EEPROM.read(Address+1);
}

void setup()
{
  Serial.begin(115200);
  setup_FS();
  EEPROM.begin(512);
  setup_wifi();
  delay(500);
  setup_OTA();
  MQTT_Connect();
  Setup_TimeClient();
  display.setBrightness(DISP_BRGT);
  pinMode(RADAR_PIN, INPUT);
  pinMode(IR_PIN, INPUT);
  pinMode(SONAR_TRIG, OUTPUT);
  pinMode(SONAR_ECHO, INPUT);
}

void loop()
{
  while (!client.connected())
   {
     if (client.connect(hostName_l.c_str()))
      {
        Serial.println("MQTT Online!");
        pubString = hostName_l + "status/online";
        client.publish(pubString.c_str(),"1");
      } else
      {
        delay(1000);
      }
    }
    Time_Disp();
    Check_Motion(USE_MQTT);

  ArduinoOTA.handle();
}
