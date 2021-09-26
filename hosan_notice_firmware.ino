#include <ESP8266WiFi.h>
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
#define HOST "www.google.com"
#define PORT 80
#define VERSION "beta-0.1.0"
#define LCD_BLANK "                "

WiFiServer server(80);
LiquidCrystal_I2C lcd(0x27,16,2);

int menu = 0;

class sonarTask : public Task {
protected:
  void setup() {}

  void loop() {       
    float distance = 0;
    float duration = 0;
    digitalWrite(TRIG,LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG,HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG,LOW);
    duration = pulseIn(ECHO,HIGH);
    distance = duration * 17/1000;

    Serial.println(distance);
    

    if (distance >= 11)
    {
      digitalWrite(LED_R, HIGH);
      digitalWrite(LED_G ,LOW);
      digitalWrite(LED_B, LOW);
    }
    else if (distance >= 8)
    {
      digitalWrite(LED_R, LOW);
      digitalWrite(LED_G ,LOW);
      digitalWrite(LED_B, HIGH);
    }
    else
    {
      digitalWrite(LED_R, LOW);
      digitalWrite(LED_G ,HIGH);
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

    if (!digitalRead(BUTTON))
    {
      return;
      
    }

    if (menu < 1) menu++;
    else menu = 0; 

    lcd.setCursor(0, 1);
    lcd.print(LCD_BLANK);

    switch (menu)
    {
      case 0:
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP());
        break;
      case 1:
        lcd.setCursor(0, 1);
        lcd.print(VERSION);
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

    Serial.println("aa");
    yield();
  }
} wifi_task;

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
  Serial.println(WIFI_SSID);

  lcd.setCursor(0, 1);
  lcd.print("Connecting WIFI");
 
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED && !digitalRead(BUTTON)) {
    delay(500);
    Serial.print(".");
  }

  digitalWrite(14, 1);
  Serial.println("");

  delay(1000);

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

  lcd.setCursor(0, 1);
  lcd.print(LCD_BLANK);
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  Scheduler.start(&button_task);
  Scheduler.start(&wifi_task);
  Scheduler.start(&sonar_task);
  Scheduler.begin();
}

void loop() {
  
}
