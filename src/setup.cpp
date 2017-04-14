#include <setup.h>

String ssid,password,mqtt_server,hostName;
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
IPAddress timeServer(132, 163, 4, 101);
const int timeZone = 1;

WiFiClient espClient;
WiFiUDP Time_Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
PubSubClient client(espClient);

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
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

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


void Setup_TimeClient()
{
  Time_Udp.begin(localPort);
  setSyncProvider(getNtpTime);
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
  hostName = f.readStringUntil('\n');
  ssid.remove((ssid.length()-1));
  password.remove((password.length()-1));
  mqtt_server.remove((mqtt_server.length()-1));
  hostName.remove((hostName.length()-1));
  /////////// NEED TO RUN ONLY ONCE ///////////
  //  Serial.println("Spiffs formating...");
  //  SPIFFS.format();
  //  Serial.println("Spiffs formatted");
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
  WiFi.softAPdisconnect(true);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.hostname(hostName);
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("Connecting to: "); Serial.println(ssid.c_str());}
  Serial.println("Wifi Connected!");
  delay(1000);
  client.setServer(mqtt_server.c_str(), 1883);
  client.setCallback(callback);
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
    ArduinoOTA.setHostname(hostName.c_str());
    ArduinoOTA.begin();
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println(ArduinoOTA.getHostname());
}

void MQTT_Connect()
{
  while (!client.connected())
   {
     if (client.connect(hostName.c_str()))
     {
       Serial.println("MQTT Online!");
       client.publish("Remote/Online","1");
     } else
   {
     delay(1000);
   }
 }
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

void startUp()
{
  Serial.begin(115200);
  setup_FS();
  EEPROM.begin(512);
  setup_wifi();
  delay(500);
  setup_OTA();
  MQTT_Connect();

}
