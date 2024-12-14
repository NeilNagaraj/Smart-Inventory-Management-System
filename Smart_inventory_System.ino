#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include "WiFiEsp.h"
#include <WiFiEspClient.h>  // Include WiFiEspClient for HTTP requests
#include <ArduinoMqttClient.h>

#ifndef HAVE_HWSERIAL1
#include "SoftwareSerial.h"
SoftwareSerial Serial1(2, 3); // RX, TX
#endif

LiquidCrystal_I2C lcd(0x27, 20, 4);

Servo myservo;

char ssid[] = "Suraj S23 Ultra";
char pass[] = "******";
int status = WL_IDLE_STATUS;

WiFiEspClient wifiClient;  // Client object for sending HTTP requests
MqttClient mqttClient(wifiClient); // MQTT client object

const char broker[] = "broker.hivemq.com";
int port = 1883;

const char topic_1[] = "inventory_1";
String message_1 = "EMPTY";

const char topic_2[] = "inventory_2";
String message_2 = "EMPTY";

const long interval = 1000;
unsigned long previousMillis = 0;

const int IR_PIN_1 = 4;
const int IR_PIN_2 = 5;
const int servo_pin = 6;
const int trigPin = 7;
const int echoPin = 8;

int distance;
int pos = 0;
int IT_State_1;
int IT_State_2;
String Inventory_1;
String Inventory_2;

void setup() {
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    pinMode(IR_PIN_1, INPUT);
    pinMode(IR_PIN_2, INPUT);

    Serial.begin(9600);
    Serial1.begin(9600);

    lcd.init();
    lcd.backlight();

    WiFi.init(&Serial1);

    if (WiFi.status() == WL_NO_SHIELD) {
        Serial.println("WiFi shield not present");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("No Wifi ");
        lcd.setCursor(0, 1);
        lcd.print("shield present");
        delay(1000);
        while (true);
    }

    while (status != WL_CONNECTED) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Attempting to ");
        lcd.setCursor(0, 1);
        lcd.print("connect wifi");
        delay(1000);
        status = WiFi.begin(ssid, pass);
    }

    Serial.println("You're connected to the network");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("You're connected");
    lcd.setCursor(0, 1);
    lcd.print("to network");
    delay(1000);

    // Connect to external MQTT broker
    if (!mqttClient.connect(broker, port)) {
        Serial.print("MQTT connection failed! Error code = ");
        Serial.println(mqttClient.connectError());
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("MQTT connection");
        lcd.setCursor(0, 1);
        lcd.print("failed");
        delay(1000);
        while (1);
    }

    myservo.attach(servo_pin);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Motor Calibration");
    lcd.setCursor(0, 1);
    lcd.print("started");
    delay(1000);
    for (pos = 15; pos <= 160; pos += 1) {
        myservo.write(pos);
        delay(15);
    }
    for (pos = 160; pos >= 13; pos -= 1) {
        myservo.write(pos);
        delay(15);
    }
    myservo.detach();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Motor Calibration");
    lcd.setCursor(0, 1);
    lcd.print("Done");
    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IOT Based Car");
    lcd.setCursor(0, 1);
    lcd.print("Inventory System");
    delay(1000);
}

void loop() {
    unsigned long currentMillis = millis();
  
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        distance = ultra();
        if ((distance < 10) && ((Inventory_1 == "EMPTY") || (Inventory_2 == "EMPTY"))) {
            myservo.attach(servo_pin); 
            for (pos = 5; pos <= 160; pos += 1) {
                myservo.write(pos);
                delay(15);
            }
            for (pos = 160; pos >= 5; pos -= 1) {
                myservo.write(pos);
                delay(15);
            }
            myservo.detach();
        }

        IT_State_1 = digitalRead(IR_PIN_1);
        if (IT_State_1 == LOW) {
            Inventory_1 = "FULL";
            message_1 = "FULL";
        } else {
            Inventory_1 = "EMPTY";
            message_1 = "EMPTY";
        }

        IT_State_2 = digitalRead(IR_PIN_2);
        if (IT_State_2 == LOW) {
            Inventory_2 = "FULL";
            message_2 = "FULL";
        } else {
            Inventory_2 = "EMPTY";
            message_2 = "EMPTY";
        }

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Slot One =" + Inventory_1);
        lcd.setCursor(0, 1);
        lcd.print("Slot Two =" + Inventory_2);
        delay(1000);

        String timestamp = String(millis());

        // Send to External MQTT Broker
        mqttClient.beginMessage(topic_1);
        mqttClient.print(message_1 + " at " + timestamp);
        mqttClient.endMessage();
        delay(200);
        
        mqttClient.beginMessage(topic_2);
        mqttClient.print(message_2 + " at " + timestamp);
        mqttClient.endMessage();
        delay(200);

        // Prepare payload to send to Flask server (AWS EC2)
        String payload_1 = "s1=" + message_1 + " at " + timestamp;
        String payload_2 = "s2=" + message_2 + " at " + timestamp;

        sendToFlaskServer(payload_1, payload_2);  // Send to Flask server

        delay(200);
    }
}

void sendToFlaskServer(String payload_1, String payload_2) {
    // Create an HTTP request to send the data to Flask server (AWS EC2)
    wifiClient.connect("ec2-public-ip", 5000);  // Connect to EC2 server (Flask app)

    // HTTP POST request
    wifiClient.print("POST /update_inventory HTTP/1.1\r\n");
    wifiClient.print("Host: your-ec2-public-ip\r\n");
    wifiClient.print("Content-Type: application/x-www-form-urlencoded\r\n");
    wifiClient.print("Content-Length: " + String(payload_1.length() + payload_2.length() + 17) + "\r\n");
    wifiClient.print("\r\n");
    wifiClient.print("s1=" + payload_1 + "&s2=" + payload_2);  // Send the payload

    // Wait for the response (optional)
    delay(100);
    while (wifiClient.available()) {
        char c = wifiClient.read();
        Serial.write(c);
    }

    wifiClient.stop();  // Close the connection
}

int ultra() {
    int result = 0;
    unsigned long kesto = 0;
    unsigned long matka = 0;

    for (int i = 0; i < 3; i++) {
        digitalWrite(trigPin, LOW); 
        delayMicroseconds(2); 
        digitalWrite(trigPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(trigPin, LOW);
        delayMicroseconds(1);

        kesto = pulseIn(echoPin, HIGH);
        matka = matka + (kesto / 58.2);
        delay(10);
    }
    matka /= 3;
    delay(10);
    result = matka;
    return(result);
}
