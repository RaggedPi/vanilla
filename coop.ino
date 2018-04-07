/*
  RaggedPi Project
  Arduino Uno "Vanilla" - Chicken Coop
  Written by david durost <david.durost@gmail.com>
*/
/* Includes */
#include <SoftwareSerial.h>     // Serial library
#include <Relay.h>              // Relay library
#include <Servo.h>              // Servo libraryu
#include <Wire.h>               // one wire library
#include <Time.h>               // time library
#include "DS3231/DS3231.h"      // rtc library
#include <OneButton.h>          // OneButton library

/* Constants */
// Misc
#define SDA              2      // SDA
#define SDL              3      // SDL
#define CS               10     // Chipselect
#define LED              13     // LED pin
// Pins
#define LDR_PIN          A1     // Analog pin
#define HEAT_PIN         RELAY2 // Digital pin
#define FAN_PIN          RELAY3 // Digital pin
#define LIGHT_PIN        RELAY4 // Digital pin
#define SERVO_PIN        12     // Digital pin
#define LIGHT_BTN_PIN    9      // Digital pin
#define HEAT_BTN_PIN     10     // Digital pin
#define FAN_BTN_PIN      11     // Digital pin
#define DOOR_BTN_PIN     13     // Digital pin
// Delimiters
#define OPEN             180    // Door open position
#define CLOSED           0      // Door closed position
#define FAN              0      // delimiter
#define HEATLAMP         1      // delimiter
#define LIGHT            2      // delimiter
#define DOOR             3      // delimiter
#define C                0      // delimiter
#define F                1      // delimiter
#define CURRENT          0      // delimiter
#define PREVIOUS         1      // delimiter
// Settings
#define DHT_F            true   // Use fahrenheit
#define LDR_READ_WAIT    60000  // ms

/* Variables */
Relay relays[3] = {             // relays
    Relay(FAN_PIN, true), 
    Relay(HEAT_PIN, true), 
    Relay(LIGHT_PIN, true) 
};                
Servo doorServo;                // servo
OneButton buttons[4] = {        // buttons
    OneButton(FAN_BTN_PIN, true),
    OneButton(HEAT_BTN_PIN, true),
    OneButton(LIGHT_BTN_PIN, true),
    OneButton(DOOR_BTN_PIN, true)
};
RTClib rtc;                     // read time clock
DateTime cTime;                 // current time
DS3231 thermometer;             // thermometer
int LDRReading[2];              // LDR sensor
int LDRReadingLevel[2];         // LDR sensor level
int doorPos = 0;                // door position
bool override[4] = { 0, 0, 0, 0 }; // overrides
float temp[2];                  // temperatures
double tempSetting[2];          // temperature settings
unsigned long lastLDRReadTime = 0; // ms
unsigned long lastTempCheckTime = 0; // ms

/* Utility Methods */
/**
 * Move door
 * @param  int moveTo
 */
void moveDoor(int moveTo) {
    doorServo.write(moveTo);
    doorPos = moveTo;
}
/*
    0-3 dark
    4-120 twilight
    121+ light
 */
/**
 * Is dusk
 * @return bool
 */
bool isDusk() {
    if ((cTime.unixtime() - lastLDRReadTime) <= LDR_READ_WAIT)    readLDR();
    return ((LDRReadingLevel[CURRENT] == 2) && (LDRReading[CURRENT] < LDRReading[PREVIOUS]));
}
/**
 * Is dawn
 * @return bool
 */
bool isDawn() {
    if ((cTime.unixtime() - lastLDRReadTime) <= LDR_READ_WAIT)  readLDR();
    return ((LDRReadingLevel[CURRENT] == 2) && (LDRReading[CURRENT] > LDRReading[PREVIOUS]));
}
/**
 * Fahrenheit to celsius
 * @param  float c
 * @return float
 */
float fahrenheitToCelsius(float c) {
    return c * 9 / 5 + 32;
}

/* Toggle Methods */
/**
 * Toggle door
 */
void toggleDoor() {
    if (doorPos == CLOSED)  moveDoor(OPEN);
    else    moveDoor(CLOSED);
}
/**
 * Toggle heat lamp
 */
void toggleHeatLamp() {
    if (relays[HEATLAMP].isOff())  relays[HEATLAMP].on();
    else    relays[HEATLAMP].off();
}
/**
 * Toggle fan
 */
void toggleFan() {
    if (relays[FAN].isOff())    relays[FAN].on();
    else    relays[FAN].off();
}
/**
 * Toggle light
 */
void toggleLight() {
    if (relays[LIGHT].isOff())  relays[LIGHT].on();
    else relays[LIGHT].off();
}

/* Read Methods */
/**
 * Read temperature
 */
void readTemps() {
    temp[C] = thermometer.getTemperature();
    temp[F] = fahrenheitToCelsius(temp[C]);
    if (isnan(temp[C])) temp[C] = 0.00;
    else if (isnan(temp[F]))  temp[F] = 32.00;
    lastTempCheckTime = cTime.unixtime();
}
/**
 * Read photocells
 */
void readLDR() { 
    LDRReading[PREVIOUS] = LDRReading[CURRENT];
    LDRReading[CURRENT] = analogRead(LDR_PIN);
    lastLDRReadTime = cTime.unixtime();
    
    LDRReadingLevel[PREVIOUS] = LDRReadingLevel[CURRENT];
    if (LDRReading[CURRENT] >= 0 && LDRReading[CURRENT] <= 3)
        LDRReadingLevel[CURRENT] = 1;
    else if (LDRReading[CURRENT]  >= 4 && LDRReading[CURRENT] <= 120)
        LDRReadingLevel[CURRENT] = 2;    
    else if (LDRReading[CURRENT]  >= 125 )
        LDRReadingLevel[CURRENT] = 3;    
}

/* Print Methods */
/**
 * Print temps
 */
void printTemps(int CorF=3) {
    if (C == CorF || 3 == CorF) {
        Serial.print("Temp: ");
        Serial.print(temp[C], 1);
        Serial.println("C\t");
    }
    if (F == CorF || 3 == CorF) {
        Serial.print("Temp: ");
        Serial.print(temp[F], 1);
        Serial.println("F\t");
    }
}
/**
 * Print photocells
 */
void printLDR() {
    Serial.print("LDR Sensor Reading: ");
    Serial.println(LDRReading[CURRENT], 1);
    Serial.print("LDR Sensor Reading Level: ");
    
    if (1 == LDRReadingLevel[CURRENT])   Serial.println("Dark");    
    else if (2 == LDRReadingLevel[CURRENT])  Serial.println("Twilight");
    else if (3 == LDRReadingLevel[CURRENT])  Serial.println("Light");
    else    Serial.println("error obtaining reading.");
}

/**
 * Setup
 */
void setup() {
    Serial.begin(9600);
    while (!Serial);

    Wire.begin();

    pinMode(LED, OUTPUT);
    pinMode(CS, OUTPUT);
    pinMode(LDR_PIN, INPUT);
    pinMode(SERVO_PIN, OUTPUT);

    Serial.println("RaggedPi Project Codename Vanilla Initializing...");
    
    for (int x=0; x<sizeof(relays); x++)    relays[x].begin();
    delay(600);
    
    buttons[DOOR].attachClick(toggleDoor);
    buttons[LIGHT].attachClick(toggleLight);
    buttons[HEATLAMP].attachClick(toggleHeatLamp);
    buttons[FAN].attachClick(toggleFan);
    delay(600);    
}
/**
 * Loop
 */
void loop() {
    cTime = now();

    for (int x=0; x<sizeof(buttons); x++) buttons[x].tick();

    // Get temperature
    readTemps();

    // Heatlamp
    if ((temp[F] < tempSetting[HEATLAMP]))  relays[HEATLAMP].on();
    else relays[HEATLAMP].off();

    // Exaust Fan
    if ((temp[F] >= tempSetting[FAN]))  relays[FAN].on();
    else relays[FAN].off();

    // Door
    if (isDawn())   moveDoor(OPEN);
    if (isDusk())   moveDoor(CLOSED);

    delay(3000);
}