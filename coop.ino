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
#include <XBee.h>

/* Misc Constants */
#define SDA              2      // SDA
#define SDL              3      // SDL
#define CS               10     // Chipselect
#define LED              13     // LED pin
#define DHT_PIN          8      // Digital pin
#define DHT_F            true   // Use fahrenheit
#define LDR_PIN          A1     // Analog pin
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
// Xbee2
#define XB_ADDR1 0x0013a200 // dummy address 
#define XB_ADDR2 0x40682fa2 // change before using.
#define SPACE Serial.print(' ')

/* Enums */
enum Modes {
    MONITOR_MODE,
    SLEEP_MODE,
    MANUAL_MODE
};

/* Variables */
int LDRReading;
int LDRReadingLevel;
Modes state = MONITOR_MODE;
bool override[4] = { 0, 0, 0, 0 };
float tempC = 0.00;
float tempF = 0.00;
int angle = 0;
double temperature;
double heatTempSetting = 0.00;
double fanTempSetting = 0.00;
long lastTempCheckTime = 0;
long tempCheckDelay = 600000;
long twilightTime = 0;
long twilightDelay = 300000;
uint8_t payload[] = {0};

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
XBee xbee = XBee();
// SH + SL Address of receiving XBee
XBeeAddress64 addr64 = XBeeAddress64(XB_ADDR1, XB_ADDR2);
ZBTxRequest zbTx = ZBTxRequest(addr64, payload, sizeof(payload));
ZBTxStatusResponse txStatus = ZBTxStatusResponse();

/**
 * Toggle door
 */
void toggleDoor() {
    // If closed, open
    if (angle == CLOSED) { 
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
    for(angle = 90; angle <= 180; angle++) {
        d.write(angle);
    }
}

/**
 * Close door
 * @param  Servo d
 */
void closeDoor(Servo d) {
    for(angle = 180; angle > 90; angle--) {
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
 * Read photocells
 */
void readLDR() { 
    LDRReading = analogRead(LDR_PIN);
  
    // Set threshholds
    if (LDRReading >= 0 && LDRReading <= 3)
        LDRReadingLevel = '1';
    else if (LDRReading  >= 4 && LDRReading <= 120)
        LDRReadingLevel = '2';    
    else if (LDRReading  >= 125 )
        LDRReadingLevel = '3';    
}

/**
 * Display photocells
 */
void displayLDR() {
    Serial.print("LDR Sensor Reading: ");
    Serial.println(LDRReading);
    Serial.print("LDR Sensor Reading Level: ");
    
    if (1 == LDRReadingLevel)   Serial.println("Dark");    
    else if (2 == LDRReadingLevel)  Serial.println("Twilight");
    else if (3 == LDRReadingLevel)  Serial.println("Light");
    else    Serial.println("error obtaining reading.");
}

/**
 * Build XBEE data
 */
void buildData() {
    payload[0] = (uint8_t)tempF >> 8 & 0xff;        // temp
    payload[1] = (uint8_t)tempF & 0xff;             // temp
    payload[2] = (uint8_t)LDRReadingLevel & 0xff;   // light reading
    payload[3] = (bool)(angle > 90) & 0xff;    // door status
    payload[4] = (bool)lightRelay.isOn() & 0xff;   // interior light status
    payload[5] = (bool)heatRelay.isOn() & 0xff;     // heat lamp status
    payload[6] = (bool)fanRelay.isOn() & 0xff;      // exhaust fan status
    payload[7] = (bool)override & 0xff;             // override status
}

/**
 * Send XBEE data
 */
void sendData() {
    xbee.send(zbTx);
    if (xbee.readPacket(500)) {
        if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
            xbee.getResponse().getZBTxStatusResponse(txStatus);
            if (txStatus.getDeliveryStatus() == SUCCESS) {
                // yay!
            } else {
                // boo.
            }
        }
    } else if (xbee.getResponse().isError()) {
        // error...error...error...
    } else {
        // ...?
    }
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
    pinMode(LDR_PIN, INPUT);
    pinMode(SERVO_PIN, OUTPUT);

    Serial.println("RaggedPi Project Codename Vanilla Initializing...");
    
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
    if ((tempF < heatTempSetting) || override[HEATLAMP])
        heatRelay.on();
    else heatRelay.off();

    // Exaust Fan
    if ((tempF >= fanTempSetting) || override[FAN])
        fanRelay.on();
    else fanRelay.off();

    // Door
    /*
        0-3 dark
        4-120 twilight
        121+ light
     */
    readLDR();
    displayLDR();   
    if (((4 <= LDRReadingLevel) && 
        (120 >= LDRReading)) || 
        override[DOOR]) {  // if dusk/ dawn or overridden
        if (-1 == twilightTime || override[DOOR]) { // if dawn or overridden
            doorServo.write(OPEN);
        } else if (0 == twilightTime) { // if dusk
            twilightTime = millis(); 
        } else if (twilightDelay > (millis() - twilightTime) || !override[DOOR]) { // if dusk and after timelimit
            doorServo.write(CLOSED);
        }
    } else if (LDRReading < 4) { // if night
        twilightTime = 0; 
    }

    delay(3000);
}