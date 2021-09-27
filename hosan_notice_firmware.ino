#pragma GCC optimize ("-O2")
#include <ESP8266WiFi.h>
#include "wpa2_enterprise.h"
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Scheduler.h>
#include <Firebase_ESP_Client.h>
// #include "addons/TokenHelper.h"
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

FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

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
    digitalWrite(TRIG,LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG,HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG,LOW);
    duration = pulseIn(ECHO,HIGH);
    sonarDistance = duration * 17/1000;
    sonarPercent = duration * 105/1000;

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
    Serial.print(Firebase.ready());
    if (Firebase.ready())
    {
      FirebaseJson content;
      
      content.clear();
      if (GENDER == 0)
      {
        content.set("fields/man/integerValue", String(sonarPercent).c_str());  
      } else {
        content.set("fields/woman/integerValue", String(sonarPercent).c_str());
      }

      Serial.print("Update a document... ");

      char p1[] = "toilet_paper/";
      char p2[] = FIREBASE_DOC_ID;

      if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", strcat(p1, p2), content.raw(), GENDER == 0 ? "man" : "woman"))
        Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
      else
        Serial.println(fbdo.errorReason());
      delay(60000);
    }
    else delay(1000);
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
    // Connecting to Firebase
    lcd.setCursor(0, 1);
    lcd.print("Connecting DB...");
  
    // config.service_account.json.path = FIREBASE_SERVICE_ACCOUNT_FILE;
    // config.service_account.json.storage_type = mem_storage_type_flash;
    // config.token_status_callback = tokenStatusCallback;
    // config.max_token_generation_retry = 5;
    // auth.token.uid = "";
    config.api_key = FIREBASE_API_KEY;

    auth.user.email = FIREBASE_USER_EMAIL;
    auth.user.password = FIREBASE_USER_PASSWORD;

    Firebase.reconnectWiFi(true);
    Firebase.begin(&config, &auth);

    /*
    while (!Firebase.ready()) {
      delay(500);
      Serial.print(".");
    }
    */
  
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
