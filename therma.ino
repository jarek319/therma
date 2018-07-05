#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_HDC1000.h>

#define headerHtml "<!DOCTYPE HTML><html><meta name='viewport' content='width=device-width, initial-scale=1.0'><head><style>table, th, td {border: 3px outset #545454;border-collapse:collapse;padding:10px;text-align:center;}</style></head><body style='background-color:#121211;color:#9896B3;font-family:Verdana,Geneva,sans-serif;font-weight:100;'><center><div style='display:table;position:absolute;height:100%;width:100%;text-align:center;'><div style='display:table-cell;vertical-align:middle;'><div style='margin-left:auto;margin-right:auto;width:90%;max-width:550px;'>"
#define footerHtml "</div></div></div></body></html>\n"
#define HEATINGLEDPIN 13
#define COOLINGLEDPIN 15
#define TOUCHRESPIN 4
#define TOUCHSENSEPIN 16
#define MOTOR1PIN 14
#define MOTOR2PIN 12
#define SDAPIN 5
#define SCLPIN 2
#define NONE     0
#define PRESS    1
#define HOLD     2
#define RELEASE  3

#define DEBUG   //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.

#ifdef DEBUG    //Macros are usually in all capital letters.
  #define DEBUG_PRINT(...)    Serial.print(__VA_ARGS__)     //DPRINT is a macro, debug print
  #define DEBUG_PRINTLN(...)  Serial.println(__VA_ARGS__)   //DPRINTLN is a macro, debug print with new line
#else
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#endif

#define DEBUG_CAPSENSE 1

IPAddress myIp(1, 1, 1, 1);
IPAddress myGateway(1, 1, 1, 2);
IPAddress mySubnet(255, 255, 255, 0);

ESP8266WebServer server(80);
WiFiClientSecure client;

Adafruit_HDC1000 hdc = Adafruit_HDC1000();
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, 0, NEO_GRB + NEO_KHZ800);

uint8_t count = 0;
uint8_t decount = 0;
uint8_t stat = NONE;
uint8_t last_stat = NONE;
long timer = 0;

int maxPos = 970;
int minPos = 8;
int pos;

float roomTemperature;
float desiredTemperature;
float minTemperature = 18.0;
float maxTemperature = 32.0;
float roomHumidity;

int rate = 1000;
int deadzone = 5;

void confirmPage() {
  DEBUG_PRINTLN("Got Post");
  pos = server.arg("sliderPos").toInt();
  DEBUG_PRINTLN(pos);
  desiredTemperature = mapfloat(pos, 0.0, 1023.0, minTemperature, maxTemperature);
  moveSlider(pos);
  setupPage();
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max){
 return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void setupPage() {
  String s = headerHtml;
  s += "<form action='/' method='post'><input type='range' name='sliderPos' min='";
  s += minPos;
  s += "' max='";
  s += maxPos;
  s += "' step='1' value='";
  s += pos;
  s += "' style='width:100%;' onchange='this.form.submit()'></form>";
  s += footerHtml;
  server.send(200, "text/html", s);
  DEBUG_PRINTLN("Client disconnected");
}

uint8_t CapSense(uint8_t receive_p, uint8_t send_p, uint8_t thresold, uint8_t holdtime, boolean holdrepeat) {
   
    boolean Read = digitalRead(receive_p);

    if(millis()%300==0) Serial.println("Capsense\tstat\tDRead\tcount\tdecount");
        Serial.print(stat);
        Serial.print("\t");
        Serial.print(Read);
        Serial.print("\t");
        //Serial.print(CapSense(float_pin, send_pin, 3, 2500, 1));
        //Serial.print("\t");
        Serial.print(count);
        Serial.print("\t");
        Serial.print(decount);
        Serial.print("\t");
        Serial.println(timer);
   
    if(Read){// && stat == NONE){
       count++;
       decount = 0;
    }else count=0;
   
    if(!Read && (stat == PRESS || stat == HOLD)){
        decount++;
    }
       
    if(count >= thresold && stat == NONE){
        count = 0;
        decount = 0;
        stat = PRESS;
        timer = millis();
        return PRESS;      
    }
    if(stat == PRESS && millis()-timer >= holdtime){
        stat = HOLD;
        return HOLD;
    }  
   
    if(stat == RELEASE){
        stat = NONE;
        return NONE;
    }
    if(decount >= 15){
        decount = 0;
        stat = RELEASE;
        return RELEASE;
    }
   
    if(stat == HOLD && holdrepeat){
        return HOLD;  
    }
   
    return NONE;
}

void setup() {
  pinMode(HEATINGLEDPIN, OUTPUT);
  pinMode(COOLINGLEDPIN, OUTPUT);
  pinMode(MOTOR1PIN, OUTPUT);
  pinMode(MOTOR2PIN, OUTPUT);
  digitalWrite(HEATINGLEDPIN, HIGH);
  digitalWrite(COOLINGLEDPIN, HIGH);
  digitalWrite(MOTOR1PIN, HIGH);
  digitalWrite(MOTOR2PIN, HIGH);

  pinMode(TOUCHSENSEPIN,INPUT);
  pinMode(TOUCHRESPIN,OUTPUT);
  analogWrite(TOUCHRESPIN, 512); //I don't know if this is the right value
  
  
  Serial.begin(115200);
  DEBUG_PRINTLN("Serial Online");
  
  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(16,0,0));
  pixels.show();
  DEBUG_PRINTLN("Pixels Online");

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(myIp, myGateway, mySubnet);
  DEBUG_PRINTLN("WiFi Online");

  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  DEBUG_PRINT("WiFi MAC : ");
  DEBUG_PRINTLN(WiFi.softAPmacAddress());

  server.begin();
  DEBUG_PRINTLN("Server Started");

  server.on("/", HTTP_POST, confirmPage);
  server.on("/", setupPage);
  DEBUG_PRINTLN("Server Pages Set, Waiting for Clients");
  String macId = String(mac[WL_MAC_ADDR_LENGTH-2],HEX)+String(mac[WL_MAC_ADDR_LENGTH-1],HEX);
  macId.toUpperCase();
  String apString = "Therma-Setup- " + macId;
  WiFi.softAP(apString.c_str());
  DEBUG_PRINT("WiFi SSID Set : ");
  DEBUG_PRINTLN(apString);
  
  Wire.pins(SDAPIN,SCLPIN);
  if (!hdc.begin()) {
    Serial.println("Couldn't find sensor!");
    while (1){for(int i=0;i<32;i++){pixels.setPixelColor(0,pixels.Color(i,0,0));pixels.show();}}
  }
}

void moveSlider(int x){
  int analogIn = analogRead(A0);
  while (analogIn < (x - deadzone)){
    digitalWrite(MOTOR1PIN, LOW);
    delayMicroseconds(rate);
    digitalWrite(MOTOR1PIN, HIGH); 
    analogIn = analogRead(A0);
  }
  while (analogIn > (x + deadzone)){
    digitalWrite(MOTOR2PIN, LOW);
    delayMicroseconds(rate);
    digitalWrite(MOTOR2PIN, HIGH);
    analogIn = analogRead(A0);
  }
}

void heatCool(){
  if ((roomTemperature+1) < desiredTemperature) analogWrite(HEATINGLEDPIN, LOW); //Heating
  else digitalWrite(HEATINGLEDPIN, HIGH);
  if ((roomTemperature-1) > desiredTemperature) analogWrite(COOLINGLEDPIN, LOW);  //Cooling
  else digitalWrite(COOLINGLEDPIN, HIGH);
}

void showColor(int color){
  if (color < 512) pixels.setPixelColor(0, pixels.Color( 0                            , map(color, 0, 511, 0, 63)   , map(color, 0, 511, 255, 0)     ));
  else             pixels.setPixelColor(0, pixels.Color( map(color, 512, 1023, 0, 127), map(color, 512, 1023, 63, 0), 0));
  pixels.show();
}

void loop() {
  server.handleClient();
  
  pos = analogRead(A0);
  desiredTemperature = mapfloat(pos, 0.0, 1023.0, minTemperature, maxTemperature);
  roomTemperature = hdc.readTemperature();

  DEBUG_PRINT("PS: ");
  DEBUG_PRINT(pos);
  DEBUG_PRINT(" DT: ");
  DEBUG_PRINT(desiredTemperature);
  DEBUG_PRINT(" RT: ");
  DEBUG_PRINT(roomTemperature);
  DEBUG_PRINT(" TH: ");
  DEBUG_PRINTLN(CapSense(TOUCHSENSEPIN, TOUCHRESPIN, 3, 2500, 1));
  
  showColor(pos);
  heatCool();
}
