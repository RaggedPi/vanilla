/*
  RaggedPi Project
  Arduino Uno "Vanilla" - Chicken Coop
  Written by david durost <david.durost@gmail.com>
*/
/* Includes */
#include <SoftwareSerial.h>     // Serial library
#include <Relay.h>              // Relay library
#include <Servo.h>              // Servo libraryu
#include <DHT.h>                // Thermometer library
#include <OneButton.h>          // OneButton library

/* Misc Constants */
#define SDA              2      // SDA
#define SDL              3      // SDL
#define CS               10     // Chipselect
#define LED              13     // LED pin
#define DHT_PIN          8      // Digital pin
#define DHT_F            true   // Use fahrenheit
#define PHOTOCELL_PIN    A0     // Analog pin
#define HEAT_PIN         RELAY2 // Digital pin
#define FAN_PIN          RELAY3 // Digital pin
#define LIGHT_PIN        RELAY4 // Digital pin
#define SERVO_PIN        12     // Digital pin
#define SLEEP_TIME       90000  // ms
#define LIGHT_BTN_PIN    9      // Digital pin
#define HEAT_BTN_PIN     10     // Digital pin
#define FAN_BTN_PIN      11     // Digital pin
#define DOOR_BTN_PIN     13     // Digital pin
#define OPEN             180    // Door open position
#define CLOSED           0      // Door closed position
#define LIGHT            0
#define HEATLAMP         1
#define FAN              2
#define DOOR             3

/* Objects */
Relay fanRelay(FAN_PIN, LOW);
Relay heatRelay(HEAT_PIN, LOW);
Relay lightRelay(LIGHT_PIN, LOW);
Servo doorServo;
DHT dht(DHT_PIN, DHT11);
OneButton doorBtn(DOOR_BTN_PIN, true);
OneButton heatBtn(HEAT_BTN_PIN, true);
OneButton lightBtn(LIGHT_BTN_PIN, true);
OneButton fanBtn(FAN_BTN_PIN, true);

/* Enums */
enum Modes {
    MONITOR_MODE,
    SLEEP_MODE,
    MANUAL_MODE
};

/* Variables */
int photocellReading;
int photocellReadingLevel;
Modes state = MONITOR_MODE;
bool override[4] = { 0, 0, 0, 0 };
float tempC = 0.00;
float tempF = 0.00;
int doorPos = 0;
double temperature;
double heatTempSetting = 0.00;
double fanTempSetting = 0.00;
long lastTempCheckTime = 0;
long tempCheckDelay = 600000;
long lastTwilightTime = 0;
long twilightDelay = 300000;

/**
 * Toggle door
 */
void toggleDoor() {
    // If closed, open
    if (doorPos == CLOSED) { 
        Serial.println("Opening door.");
        doorServo.write(OPEN);
    } 
    // else close
    else { 
        Serial.println("Closing door.");
        doorServo.write(CLOSED);
    }
}

/**
 * Toggle door
 */
void toggleHeatLamp() {
    // If closed, open
    if (heatRelay.isOff()) { 
        Serial.println("Heatlamp on.");
        heatRelay.on();
    } 
    // else close
    else { 
        Serial.println("Heatlamp off.");
        heatRelay.off();
    }
}

/**
 * Override door functionality (hold door open/ closed indefinately)
 */
void overrideDoor() {
    ~override[DOOR];
    Serial.println("Overriding door..");
    toggleDoor();
}

/**
 * Open door
 * @param  Servo d
 */
void openDoor(Servo d) {
    for(int angle = 90; angle <= 180; angle++) {
        d.write(angle);
    }
}

/**
 * Close door
 * @param  Servo d
 */
void closeDoor(Servo d) {
    for(int angle = 180; angle > 90; angle--) {
        d.write(angle);
    }
}

/**
 * Read temperature
 */
void readTemps() {
    tempC = dht.readTemperature();
    tempF = dht.readTemperature(true);
    if (isnan(tempC)) {
        Serial.println("[ERROR] Failed to read DHT sensor (C).");
        tempC = 0.00;
    } else if (isnan(tempF)) {
        Serial.println("[ERROR] Failed to read DHT sensor (F).");
        tempF = 32.00;
    }
}

/**
 * Display temps
 */
void displayTemps() {
    Serial.print("Temp: ");
    Serial.print(tempC);
    Serial.println("C\t");
    Serial.print("Temp: ");
    Serial.print(tempF);
    Serial.println("F\t");
}

/**
 * Setup
 */
void setup() {
    Serial.begin(9600);
    while (!Serial);

    pinMode(DHT_PIN, INPUT);
    pinMode(LED, OUTPUT);
    pinMode(CS, OUTPUT);
    pinMode(PHOTOCELL_PIN, INPUT);
    pinMode(SERVO_PIN, OUTPUT);

    Serial.println("RaggedPi Project Codename Nutmeg Initializing...");
    
    Serial.print("Initializing relays...");
    fanRelay.begin();
    heatRelay.begin();
    lightRelay.begin();
    delay(600);
    Serial.println("[OK]");
    Serial.print("Initializing dht...");
    dht.begin();
    delay(600);
    Serial.println("[OK]");
    Serial.print("Initializing buttons...");
    doorBtn.attachClick(toggleDoor);
    doorBtn.attachPress(overrideDoor);
    heatBtn.attachClick(toggleHeatLamp);
    delay(600);
    Serial.println("[OK]");

    /* Initialized ************************************************************/
    Serial.println("System initialized.");
    delay(600);
}

void loop() {
    doorBtn.tick();
    heatBtn.tick();
    lightBtn.tick();
    fanBtn.tick();

    readTemps();
    displayTemps();

    // Heatlamp
    if ((tempF < heatTempSetting) || override[HEAT])
        heatlampRelay.on();
    else heatlampRelay.off();

    // Exaust Fan
    if ((tempF >= fanTempSetting) || override[FAN])
        fanRelay.on();
    else fanRelay.off();

    // Door/*
    /*
        0-3 dark
        4-120 twilight
        121+ light
     */
    readPhotocells();
    displayPhotocells();   
    if (((4 <= photocellReadingLevel) && 
        (120 >= photocellReading)) || 
        override[DOOR]) {  // if dusk/ dawn or overridden
        if (-1 == twilightTime || override[DOOR]) { // if dawn or overridden
            doorServo.write(OPEN);
        } else if (0 == twilightTime) { // if dusk
            twilightTime = millis(); 
        } else if (twilightDelay > (millis() - twilightTime) || !override[DOOR]) { // if dusk and after timelimit
            doorServo.write(CLOSED);
        }
    } else if (photocellReading < 4) { // if night
        twilightTime = 0; 
    }

    delay(3000);
}