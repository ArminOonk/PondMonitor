#include <DallasTemperature.h>

#include <OneWire.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include "private.h"

const int numberOfSensors = 3;
const int sensorIdSize = 8;
enum sensor_t{
  SHALLOW,
  AIR,
  DEEP,
};

const uint16_t aport = 8266;
String myHostname = "";
String uniqueID;// HEX string 

WiFiServer TelnetServer(aport);
WiFiClient Telnet;
WiFiUDP OTA;

// Temperature
OneWire  onewire(5);  // on pin D4 LoLin board 
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&onewire);

byte addr[sensorIdSize];

// #define THINGSPEAK_KEY ABCDEFGH
char host[] = "api.thingspeak.com";
String GET = "/update?api_key=" + String(THINGSPEAK_KEY) + "&field1=";
const int updateTimeout = 15*60*1000; // Thingspeak update rate
const int rebootTimeout = 1000*60*60*6;

DeviceAddress sensorID[numberOfSensors] = {
    {0x28, 0xFF, 0x2B, 0x56, 0x74, 0x15, 0x03, 0x91},
    {0x28, 0xFF, 0x5D, 0x88, 0x73, 0x15, 0x02, 0x50},
    {0x28, 0xFF, 0xF0, 0x1E, 0x61, 0x15, 0x03, 0x3C}};

float temperatures[numberOfSensors] = {666.0};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(10);
  
  uniqueID = getUniqueID();
  myHostname = "pond";
  
  Serial.println("Connecting to " + String(ssid));
  delay(10);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    if(millis() > 2*60*1000) { // 2 minutes
      ESP.restart();
    }
  }

  // Start OTA
  MDNS.begin(myHostname.c_str());
  MDNS.addService("arduino", "tcp", aport);
  OTA.begin(aport);
  TelnetServer.begin();
  TelnetServer.setNoDelay(true);

  sensors.begin();
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  // report parasite power requirements
  Serial.print("Parasite power is: "); 
  if (sensors.isParasitePowerMode()) { 
    Serial.println("ON");
  }
  else {
    Serial.println("OFF");
  }

  for(int i=0; i<numberOfSensors; i++){
    sensors.setResolution(sensorID[i], 12);
    printAddress(sensorID[i]);
    Serial.println(" has resolution of " + String(sensors.getResolution(sensorID[i])) + " bits");
  }
  ESP.wdtEnable(0);
}

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}
long lastTimeReport = 0;
long nextTimeReport = 0;
int lastLoopTime = 0;
const int loopTimeout = 10000;
void loop() {
  if(millis() - lastLoopTime > loopTimeout) {
    lastLoopTime = millis();

    Serial.print("Requesting temperatures...");
    sensors.requestTemperatures();
    Serial.println("DONE");

    for(int i=0; i<numberOfSensors; i++){
      temperatures[i] = sensors.getTempC(sensorID[i]);
    }
    
    Serial.println("Temperature Deep : " + String(temperatures[DEEP]) + " Shallow: " + String(temperatures[SHALLOW]) + " Air: " + String(temperatures[AIR]));
    if(millis() > nextTimeReport)
    {
      if(WiFi.status() != WL_CONNECTED) {
        Serial.println("Wifi not connected -> Restarting");
        ESP.restart();
      }
      
      SendThingspeak(temperatures[DEEP], temperatures[SHALLOW], temperatures[AIR]);
    }      
  }

  if(millis() > (lastTimeReport + 2*updateTimeout)) {
    Serial.println("No server response -> Restarting");
    ESP.restart();
  }
    
  checkOTA();

  if(millis() > rebootTimeout){
    Serial.println("Restart every 6 hours -> Restarting");
    ESP.restart();
  }
  
  delay(1);
}


void SendThingspeak(float deepTemp, float shallowTemp, float airTemp){
  Serial.println("connecting to " + String(host));
    
  WiFiClient client;
  const int httpPort = 80;
  if (client.connect(host, httpPort)) 
  {
    String url = GET + String(deepTemp);
    url += "&field2=" + String(shallowTemp);
    url += "&field3=" + String(airTemp);
    url += "&field4=" + String(0.001*millis());
    
    Serial.println("Requesting URL: " + String(url));
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
    delay(10);
    // Read all the lines of the reply from server and print them to Serial
    int lineNr = 0;
    while(client.available()){
      String line = client.readStringUntil('\r');
      line.trim();
      
      if(line.startsWith("Status: ")) {
        String statusCode = line.substring(8,12);
        if(statusCode.toInt() == 200) {
          Serial.println("Data received");
          lastTimeReport = millis();
        }
      }
        
      Serial.println("#" + String(lineNr) + ": " + line);
      lineNr++;
      yield();
    }
      
      Serial.println();
      Serial.println("closing connection");

      nextTimeReport = millis() + updateTimeout;
    
  } else {
    Serial.println("connection failed");
  }
}

