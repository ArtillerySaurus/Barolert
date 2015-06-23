#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>

/*
   Connections
   ===========

   Sensor BMP180:
   Connect SCL to 21
   Connect SDA to 20
   Connect VDD to 3.3V DC
   Connect GROUND to common ground

   Rumble engine:
   Connect VIN to pmw 11
   Connect GROUND to common ground

   Piezo:
   Connect VIN to pmw 12
   Connect GROUND to common ground

   LED:
   Connect VIN to pmw 13
   Connect GROUND to common ground

   Button 1:
   Connect VIN to 5v
   Connect GROUND to common ground (Resistor: brown, black, orange)
   Connect OUT to pwm 2, inline with GROUND and resistor.

   Button 2:
   Connect VIN to 5v
   Connect GROUND to common ground (Resistor: brown, black, orange)
   Connect OUT to pwm 3, inline with GROUND and resistor.

   LCD:
   Connect SCL to 21
   Connect SDA to 20
   Connect VDD to 3.3V DC
   Connect GROUND to common ground


   Commands
   ===========

   Serial:
   nextScreen          // Set screen to go to the next screen.
   changeAlert         // Sets the alert value to big pressure changes.
   highAlert           // Sets the alert value to too high values.
   lowAlert            // Sets the alert value to too low values.
   test                // Gives a Serial.println().


*/

/*

Misschien dit gebruiken:
digitalWrite(ledPin, ledState);

Hiermee kunnen ze allemaal dezelfde waarde gebruiken.

*/

// Classes.
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// Constants for connecting pins.
const int button1Pin = 2;
const int button2Pin = 3;
const int rumblePin =  11;
const int piezoPin =  12;
const int ledPin =  13;

// Alert vars used for... well, the alert!
boolean alert = false;
boolean alertsDisabled = false;
String alertMessage = "";
long alertTime = 500; // The duration to sound the alert.
long alertIntervalTime = 1000; // The time to wait before sounding the alert again.

// Output vars.
boolean bmpFound = false;
boolean rumbleToggled = false;
boolean piezoToggled = false;
boolean ledToggled = false;
boolean partsToggled = false;
int outputState = LOW;

// Button vars.
int button1State = 0;
int button2State = 0;
boolean button1Released = false;
boolean button2Released = false;
long button1HoldDuration = 0;

// Barometer vars.
boolean firstMessure = true;
float upperLimit = 1030.00; // Extreme temperatures.
float lowerLimit = 995.00; // Bad weather.
float incrementChange = 1.00; // 1hPa.
float lastPressure = 0; // Container to store last pressure in.
String lcdPressure = "";
float lastTemperature = 0;
char s[32]; // Temp container for float to string conversion.

// Time vars.
long sensorIntervalTime = 3600000; // One hour.
// long sensorIntervalTime = 1000; // One hour.
long currentTime = 0;
long previousSensorTime = 0;
long previousAlertTime = 0;
// long screenTurnOffTime = 10000;
// long screenStartTime = 0;

// LCD vars.
int screenMode = 0;
String firstRow = "";
String secondRow = "";
int lastScreen = 2;
boolean selfPrint = false;

// Debug and test vars.
boolean changeAlert = false;
boolean highAlert = false;
boolean lowAlert = false;

void setup(void)
{

  Serial.begin(9600);
  Serial.println("Starting");
  pinMode(rumblePin, OUTPUT);
  pinMode(piezoPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(button1Pin, INPUT);
  pinMode(button2Pin, INPUT);

  lcd.begin (16, 2); // initialize the lcd.
  setScreen(); // Setup as first stuff.

  if (!bmp.begin()) {
    Serial.print("No BMP085 detected");
  } else {
    bmpFound = true;
  }
  Serial.println("Finished starting");
}

void loop(void)
{

  currentTime = millis();

  if (bmpFound) {
    if ((currentTime - previousSensorTime) > sensorIntervalTime || firstMessure || (highAlert || lowAlert || changeAlert)) {
      previousSensorTime = currentTime;

      sensors_event_t event;
      bmp.getEvent(&event);

      if (event.pressure) {

        float currentPressure = event.pressure;
        float pressureDifference = lastPressure - currentPressure;

        // Sort the alerts , first function is least important, last most.

        if (currentPressure >= upperLimit || highAlert) {
          alert = true;
          alertMessage = "hPa too high!";
        }

        if (currentPressure <= lowerLimit || lowAlert) {
          alert = true;
          alertMessage = "hPa too low!";
        }

        if (pressureDifference >= incrementChange || changeAlert) {
          alert = true;
          alertMessage = "hPa changes!";
        }

        if (!alert) {
          alertMessage = "No alert.";
        }

        lastPressure = currentPressure;

        float temperature;
        bmp.getTemperature(&temperature);
        lastTemperature = temperature;


        if (firstMessure) {
          firstMessure = false;
        }


      } else {
        Serial.println("Sensor error");
      }
    }
  }

  // if (alert) {
  // rumbleToggled = true;
  // piezoToggled = true;
  // ledToggled = true;
  // } else {
  // rumbleToggled = false;
  // piezoToggled = false;
  // ledToggled = false;
  // }

  // Call functions to run.
  checkButton1State();
  checkButton2State();
  toggleOutput();
  // idleScreen();
  handleSerialMonitorInput();

}

void handleSerialMonitorInput() {
  String content = "";
  while (Serial.available()) {
    content = Serial.readString();
  }

  if (content != "") {
    Serial.println("Command used: " + content);
  }

  if (content == "nextScreen") {
    setScreen();
  }

  if (content == "test") {
    Serial.println("Test success!");
  }

  if (content == "changeAlert") {
    changeAlert = !changeAlert;
    highAlert = false;
    lowAlert = false;
  }

  if (content == "highAlert") {
    highAlert = !highAlert;
    changeAlert = false;
    lowAlert = false;
  }

  if (content == "lowAlert") {
    lowAlert = !lowAlert;
    changeAlert = false;
    highAlert = false;
  }

}

void toggleOutput() {
  if (alert && !alertsDisabled) {
    if ((currentTime - previousAlertTime) > alertIntervalTime) {
      if ((currentTime - alertTime) > alertIntervalTime && partsToggled) {
        Serial.println("4");
        outputState = LOW;
        partsToggled = false;
        alertTime = currentTime;
      } else {
        Serial.println("5");
        outputState = HIGH;
        partsToggled = true;
        previousAlertTime = currentTime;
      }
    }

    setOutput();

  } else {
    outputState = LOW;
  }
}

void setOutput() {
  digitalWrite(rumblePin, outputState);
  digitalWrite(piezoPin, outputState);
  digitalWrite(ledPin, outputState);
}

void checkButton1State() {
  button1State = digitalRead(button1Pin);
  if (button1State == HIGH) {
    if (button1Released) {
      button1Released = false;
      setScreen();
    }

  } else {
    button1Released = true;
  }
}

void checkButton2State() {
  button2State = digitalRead(button2Pin);
  if (button2State == HIGH) {
    if (button2Released) {
      if (screenMode == 3) {
        alertsDisabled = !alertsDisabled;
        screenMode = 2;
        setScreen();
      }

      if (alert) {
        alert = false;
        changeAlert = false;
        highAlert = false;
        lowAlert = false;

        outputState = LOW;
        setOutput();
      }
    }

    button2Released = false;
  } else {
    button2Released = true;
  }
}

void setScreen() {

  // screenStartTime = currentTime;

  if (screenMode > lastScreen) {
    screenMode = 0;
  }

  selfPrint = false;

  switch (screenMode) {
    case 0:
      firstRow = "";
      secondRow = "";
      lcd.setBacklight(0);
      break;

    case 1:

      selfPrint = true;

      lcd.clear();
      lcd.home();
      // lcd.backlight();
      lcd.setBacklight(255);

      lcd.setCursor(0, 0);
      lcd.write(dtostrf(lastPressure, 7, 2, s));
      lcd.print(" hPa");

      lcd.setCursor(0, 1);
      lcd.write(dtostrf(lastTemperature, 7, 2, s));
      lcd.write(" ");
      lcd.print((char)223);
      lcd.print("C");
      break;

    case 2:
      lcd.setBacklight(255);
      firstRow = alertMessage;
      secondRow = "Turn off alerts?";
      if (alertsDisabled) {
        secondRow = "Turn on alerts?";
      }
      break;

    default:
      lcd.setBacklight(255);
      firstRow = "-1";
      secondRow = "-1";
      break;
  }

  if (!selfPrint) {
    lcd.clear();
    lcd.home();

    // Row 1.
    lcd.setCursor(0, 0);
    lcd.print(firstRow);

    // Row 2.
    lcd.setCursor(0, 1);
    lcd.print(secondRow);
  }

  screenMode++;

}
