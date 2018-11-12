#include <ArduinoJson.h>
#include <Wire.h> 
#include <ESP8266WiFi.h>
#include "LiquidCrystal_I2C.h"
#include "Sensitive.h"

#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

const uint16_t LCD_TIMEOUT = 900000;

boolean needToPublish = false;
volatile boolean backlightOn = false;

const byte INT_PIN_A = 12;
const byte INT_PIN_B = 13;

volatile boolean positionA = 12;
volatile boolean positionB = 13;

uint32_t lastAction;

WiFiClient client;

Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_PORT, MQTT_USERNAME, MQTT_PASSWORD);

Adafruit_MQTT_Publish buttonPress = Adafruit_MQTT_Publish(&mqtt, "relay");
Adafruit_MQTT_Subscribe messages = Adafruit_MQTT_Subscribe(&mqtt, "display");

const char* messagePayload() {
  String value = "{\"relayA\": " + String(positionA) + ", \"relayB\": " + String(positionB) + "}";

  return value.c_str();
}

void publishMessage() {
  // send message that switch was toggled
  const char* payload = messagePayload();
  buttonPress.publish(payload);
}

void messageCallback(char *data, uint16_t len) {
  const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;

  DynamicJsonBuffer jsonBuffer(capacity);
  JsonObject& msg = jsonBuffer.parseObject(data);


  const char* line1 = msg["line1"];
  const char* line2 = msg["line2"];

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

// check for status of switch A
void publishA() {
  needToPublish = true;
  if(digitalRead(INT_PIN_A) == HIGH){
    positionA = true;
  } else {
    positionA = false;
  }
}

// check for status of switch B
void publishB() {
  needToPublish = true;
  if(digitalRead(INT_PIN_B) == HIGH) {
    positionB = true;
  } else {
    positionB = false;
  }
}

void setup()
{
  // attach interrupt for Pin A
  pinMode(INT_PIN_A, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INT_PIN_A), publishA, CHANGE);

  // attach interrupt for Pin B
  pinMode(INT_PIN_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INT_PIN_B), publishB, CHANGE);

  // check initial status of Pin A
  if(digitalRead(INT_PIN_A) == HIGH) {
    positionA = true;
  } else {
    positionA = false;
  }

  // check initial status of Pin B
  if(digitalRead(INT_PIN_B) == HIGH) {
    positionB = true;
  } else {
    positionB = false;
  }
  
  // initialize the LCD
  lcd.begin();
  lcd.clear();

  // turn on the blacklight and print a message.
  lcd.backlight();
  lcd.print("Connecting...");

  // initiate wifi connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.clear();
    lcd.print("Still connecting...");
  }

  lcd.clear();
  lcd.print("Connected!");
  delay(1000);
  lcd.clear();
  lcd.print(WiFi.localIP());

  delay(500);
  
  // set callback for incoming messages
  messages.setCallback(messageCallback);
  mqtt.subscribe(&messages);

  // set flag to pubish initial state
  needToPublish = true;
}

void loop() {
  if(lastAction < millis() - LCD_TIMEOUT && backlightOn == true) {
    lcd.noBacklight();
    backlightOn = false;
  }

  if(lastAction > millis() - LCD_TIMEOUT && backlightOn == false) {
    lcd.backlight();
    backlightOn = true;
  }

  if(needToPublish) {
    needToPublish = false;
    publishMessage();
  }

  // todo: do we want this in loop()?
  MQTT_connect();

  mqtt.processPackets(100);

  if(!mqtt.ping()) {
    mqtt.disconnect();
  }
}

void MQTT_connect() {
  int8_t ret;

  if(mqtt.connected()) {
    return;
  }

  lcd.clear();
  lcd.print("Connecting MQTT");

  uint8_t retries = 3;
  while((ret = mqtt.connect()) != 0) {
    lcd.clear();
    lcd.print("Retries left: ");
    lcd.print(retries);
    mqtt.disconnect();
    delay(10000);
    retries--;
    if(retries == 0){
      lcd.clear();
      lcd.print("Could not connect");
      while(1);
    }
  }

  lcd.clear();
  lcd.print("MQTT Connected!");
}

