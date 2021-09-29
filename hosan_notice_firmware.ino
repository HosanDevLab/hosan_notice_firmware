#pragma GCC optimize ("-O2")
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "wpa2_enterprise.h"
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Scheduler.h>
#include "config.h"
#define TRIG 2 // D4
#define ECHO 16 // D0
#define LED_R 14 // D5
#define LED_G 12 // D6
#define LED_B 13 // D7
#define BUTTON 15 // D8
#define VERSION "beta-0.1.0"
#define LCD_BLANK "                "

WiFiServer server(80);
LiquidCrystal_I2C lcd(0x27,16,2);

int menu = 0;
float sonarDistance = 0;
float sonarPercent = 0;

void lcdPrintDistance()
{
  lcd.setCursor(0, 1);
  lcd.print(LCD_BLANK);
  lcd.setCursor(0, 1);
  lcd.print(sonarDistance);
  lcd.print("cm (");
  lcd.print(sonarPercent);
  lcd.print("%)");
}

class sonarTask : public Task {
protected:
  void setup() {}

  void loop() {       
    float duration = 0;
    digitalWrite(TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG, LOW);
    duration = pulseIn(ECHO,HIGH);
    sonarDistance = duration * 17/1000;
    // sonarPercent = 100 - (duration * 105/1000);
    sonarPercent = 100 - ((sonarDistance - 4) / 7 * 100);
    if (sonarPercent < 0) sonarPercent = 0;

    Serial.println(sonarDistance);

    if (menu == 0)
    {
      lcdPrintDistance();
    }

    if (sonarDistance >= 11)
    {
      digitalWrite(LED_R, LOW);
      digitalWrite(LED_G ,HIGH);
      digitalWrite(LED_B, LOW);
    }
    else if (sonarDistance >= 8)
    {
      digitalWrite(LED_R, LOW);
      digitalWrite(LED_G ,LOW);
      digitalWrite(LED_B, HIGH);
    }
    else
    {
      digitalWrite(LED_R, HIGH);
      digitalWrite(LED_G ,LOW);
      digitalWrite(LED_B, LOW);
    }

    delay(1000);
  }
} sonar_task;

class buttonTask : public Task {
protected:
  void setup() {}

  void loop() {
    delay(100);

    if (!digitalRead(BUTTON)) return;

    if (menu < 2) menu++;
    else menu = 0; 

    lcd.setCursor(0, 1);
    lcd.print(LCD_BLANK);

    switch (menu)
    {
      case 0:
        lcdPrintDistance();
        break;
      case 1:
        lcd.setCursor(0, 1);
        if (WiFi.status() == WL_CONNECTED) lcd.print(WiFi.localIP());
        else lcd.print("OFFLINE MODE");
        break;
      case 2:
        lcd.setCursor(0, 1);
        lcd.print(VERSION);
        break;
    }

    while (digitalRead(BUTTON));
    yield();
  }
} button_task;

class wifiTask : public Task {
protected:
  void setup() {}

  void loop() {
    delay(5000);

    yield();
  }
} wifi_task;

class dataPostTask : public Task {
protected:
  void setup () {}

  void loop() {    
    WiFiClient client;
    HTTPClient http;
    http.begin(client, BACKEND_HOST);

    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["id"] = FIREBASE_DOC_ID;
    doc["percent"] = sonarPercent;
    doc["type"] = "man";

    String jsonChar;
    serializeJson(doc, Serial);
    serializeJson(doc, jsonChar);
    
    int httpResponseCode = http.POST(jsonChar);

    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    http.end();
    
    delay(30000);
    yield();
  }
} data_post_task;

void setup() {
  pinMode(TRIG, OUTPUT); // D3
  pinMode(ECHO, INPUT); // D4
  pinMode(LED_R, OUTPUT); // D5
  pinMode(LED_G, OUTPUT); // D6
  pinMode(LED_B, OUTPUT); // D7
  pinMode(BUTTON, INPUT_PULLUP); // D8
  Serial.begin(9600);
  delay(10);

  lcd.init();
  lcd.backlight();
  lcd.print("  Hosan Notice  ");
  lcd.setCursor(0, 1);
  lcd.print("   Booting...   ");

  lcd.setCursor(0, 1);
  lcd.print("Scanning WIFIs..");
  
  int numberOfNetworks = WiFi.scanNetworks();
 
  for(int i =0; i<numberOfNetworks; i++){
      Serial.print("Network name: ");
      Serial.println(WiFi.SSID(i));
      Serial.print("Signal strength: ");
      Serial.println(WiFi.RSSI(i));
      Serial.println("-----------------------");
  }
 
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(USE_EAP ? EAP_SSID : WIFI_SSID);
  if (USE_EAP) Serial.println("USING EAP");

  lcd.setCursor(0, 0);
  lcd.print("Connecting WIFI");
  lcd.setCursor(0, 1);
  lcd.print("button to skip");

  if (USE_EAP)
  {
    wifi_set_opmode(STATION_MODE);
    
    // Configure SSID
    struct station_config wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strcpy((char *)wifi_config.ssid, EAP_SSID);
    wifi_station_set_config(&wifi_config);
    
    // DO NOT use authentication using certificates
    wifi_station_clear_cert_key();
    wifi_station_clear_enterprise_ca_cert();
    
    // Authenticate using username/password
    wifi_station_set_wpa2_enterprise_auth(1);
    wifi_station_set_enterprise_identity((uint8 *)EAP_USERNAME, strlen(EAP_USERNAME));
    wifi_station_set_enterprise_username((uint8 *)EAP_USERNAME, strlen(EAP_USERNAME));
    wifi_station_set_enterprise_password((uint8 *)EAP_PASSWORD, strlen(EAP_PASSWORD));
    
    // Connect
    wifi_station_connect();
  } else {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  while (WiFi.status() != WL_CONNECTED && !digitalRead(BUTTON)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  // LED red when WIFI connected
  digitalWrite(14, 1);

  lcd.setCursor(0, 0);
  lcd.print("  Hosan Notice  ");

  if (WiFi.status() == WL_CONNECTED)
  {
    // Start the server
    lcd.setCursor(0, 1);
    lcd.print("Starting Server");
    
    server.begin();
    Serial.println("Server started");
    
   
    // Print the IP address
    Serial.print("Use this URL to connect: ");
    Serial.print("http://");
    Serial.print(WiFi.localIP());
    Serial.println("/");
  }

  lcd.setCursor(0, 1);
  lcd.print(LCD_BLANK);
  lcd.setCursor(0, 1);
  if (WiFi.status() == WL_CONNECTED) lcd.print(WiFi.localIP());
  else lcd.print("OFFLINE MODE");

  // Begin background tasks
  Scheduler.start(&button_task);
  Scheduler.start(&wifi_task);
  Scheduler.start(&sonar_task);
  Scheduler.start(&data_post_task);
  Scheduler.begin();
}

void loop() {
  
}
