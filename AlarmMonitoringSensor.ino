/*
  Alarm Monitoring Sensor (powered by mySensors on a Arduino Nano)
  By: Jean-Francois Auger
  Contact: nechry@gmail.com
  License: GNU LGPL 2.1+
  https://github.com/nechry/AlarmMonitoringSensor/

  Purpose:

  This is a monitoring system for home alarm control panel, base on mySensors
  I use Light Sensors to monitor alarm leds:
    operation mode,
	trigger,
	bell is activated,
	maintenance.
  The LEDs have tree states, Off, blink and On

  INPUT:
  Pin 3, 4, 5 and 6 for led sensor.
  OUTPUT:
  Pin 8 and 7 to report the current operation mode.

  NRF24L01+:
  See http://www.mysensors.org/build/connect_radio for more details
  9	    CE
  10	CSN/CS
  13	SCK
  11	MOSI
  12	MISO
  2	    IRQ

*/

#include <SPI.h>
#include <MySensor.h>

#define Operation_Child_Id 0
#define Trigger_Child_Id 1
#define Bell_Child_Id 2
#define Maintenance_Child_Id 3

// input sensors pins
#define Operation_Sensor_Pin 3
#define Trigger_Sensor_Pin 4
#define Bell_Sensor_Pin 5
#define Maintenance_Sensor_Pin 6

//output leds to repesente operation
#define Red_Led_Pin 8
#define Blue_Led_Pin 7

unsigned long SLEEP_TIME = 5; // Sleep time between reads (in milliseconds)

enum alarm_status {
  Unknown,
  Desactivated,
  Blinking,
  Activated
};

MySensor gw;
MyMessage messageOperation(Operation_Child_Id, V_LIGHT_LEVEL);
MyMessage messageTrigger(Trigger_Child_Id, V_LIGHT_LEVEL);
MyMessage messageRaise(Bell_Child_Id, V_LIGHT_LEVEL);
MyMessage messageMaintenance(Maintenance_Child_Id, V_LIGHT_LEVEL);

int lastLedOperation = Unknown;
int lastLedTrigger = Unknown;
int lastLedBell = Unknown;
int lastLedMaintenance = Unknown;

// define, how many sensors
#define SENSORS 4
// add each led sensor
int pins[SENSORS] = { Operation_Sensor_Pin, Trigger_Sensor_Pin, Bell_Sensor_Pin, Maintenance_Sensor_Pin };

// the current state of each sensor (off, blink, on)
int states[SENSORS];
int pending_states[SENSORS];

// how many blinks have been counted on each sensor
int blinks[SENSORS];

// Serial port speed
#define SERIAL_SPEED 115200

// Reporting interval on serial port, in milliseconds
#define REPORT_INTERVAL 5000

void setup()
{
  Serial.begin(SERIAL_SPEED);



  //start comm with gateway
  gw.begin(NULL, 1);

  // Send the sketch version information to the gateway and Controller
  gw.sendSketchInfo("Alarm Monitoring Sensor", "1.1");

  // Register all sensors to gateway (they will be created as child devices)
  gw.present(Operation_Child_Id, S_LIGHT_LEVEL);
  gw.present(Trigger_Child_Id, S_LIGHT_LEVEL);
  gw.present(Bell_Child_Id, S_LIGHT_LEVEL);
  gw.present(Maintenance_Child_Id, S_LIGHT_LEVEL);


  /* initialize each led sensor */
  for (int i = 0; i < SENSORS; i++) {
    pinMode(pins[i], INPUT);
    states[i] = HIGH;
    pending_states[i] = HIGH;
    blinks[i] = 0;
  }
  //setup led status as output for mode
  pinMode(Red_Led_Pin, OUTPUT);
  pinMode(Blue_Led_Pin, OUTPUT);
  //desactivate leds
  digitalWrite(Red_Led_Pin, HIGH);
  digitalWrite(Blue_Led_Pin, HIGH);

  //do a small blink effect as startup completed
  digitalWrite(Red_Led_Pin, LOW);
  delay(250);
  digitalWrite(Red_Led_Pin, HIGH);
  delay(250);
  digitalWrite(Blue_Led_Pin, LOW);
  delay(250);
  digitalWrite(Blue_Led_Pin, HIGH);
}

unsigned long loops = 0;
unsigned long next_print = millis() + REPORT_INTERVAL;
bool lastLedOperation_skip = true;
void loop()
{
  // go through each LED sensor
  for (int index = 0; index < SENSORS; index++) {
    // read current value
    int ledState = digitalRead(pins[index]);
    // check if state is changed:
    if (ledState != states[index]) {
      states[index] = ledState;
      // no light to light shows as HIGH to LOW change on the pin,
      // as the photodiode starts to conduct
      if (ledState == LOW) {
        blinks[index] += 1;
      }
    }
  }

  // Every 10000 rounds (quite often), check current uptime
  // and if REPORT_INTERVAL has passed, print out current values.
  // If this would take too long time (slow serial speed) or happen
  // very often, we could miss some blinks while doing this.
  loops++;
  if (loops == 10000) {
    loops = 0;
    if (millis() >= next_print) {
      next_print += REPORT_INTERVAL;
      Serial.println("+"); // start marker
      for (int index = 0; index < SENSORS; index++) {
        //print a report of led current bink detection
        Serial.print(index, DEC);
        Serial.print(" ");
        Serial.print(blinks[index], DEC);
        Serial.print(" ");
        //led interpreter
        int currentLedStatus = Unknown;
        if (blinks[index] > 1) {
          currentLedStatus = Blinking;
          Serial.println("Partial");
        }
        else if (states[index] == LOW) {
          currentLedStatus = Activated;
          Serial.println("Activated");
        }
        else {
          currentLedStatus = Desactivated;
          Serial.println("Desactivated");
        }

        //report to gateway for each sensor if changes
        switch (index) {
          case 0 :
            {
              //check last alarm mode
              if (currentLedStatus != lastLedOperation) {
                if (lastLedOperation_skip) {
                  lastLedOperation_skip = false;
                }
                else {
                  lastLedOperation_skip = true;
                  //reset led mode
                  digitalWrite(Red_Led_Pin, HIGH);
                  digitalWrite(Blue_Led_Pin, HIGH);
                  //activate leds
                  switch (currentLedStatus) {
                    case Blinking:
                      digitalWrite(Blue_Led_Pin, LOW);
                      break;
                    case Activated:
                      digitalWrite(Red_Led_Pin, LOW);
                      break;
                    default:
                      Serial.println("Unknown");
                      break;
                  }
                  //send a report to the gateway
                  gw.send(messageOperation.set(currentLedStatus));
                  //save led operation
                  lastLedOperation = currentLedStatus;
                }
              }
              break;
            }
          case 1:
            {
              //check last alarm trigger
              if (lastLedTrigger != currentLedStatus) {
                //send report to gateway
                gw.send(messageTrigger.set(currentLedStatus));
                //save led trigger
                lastLedTrigger = currentLedStatus;
              }
              break;
            }
          case 2:
            {
              //check last alarm raise
              if (lastLedBell != currentLedStatus) {
                //send report to gateway
                gw.send(messageRaise.set(currentLedStatus));
                //save led bell state
                lastLedBell = currentLedStatus;
              }
              break;
            }
          case 3:
            {
              if (lastLedMaintenance != currentLedStatus) {
                //send report to gateway
                gw.send(messageMaintenance.set(currentLedStatus));
                //save led maintenance
                lastLedMaintenance = currentLedStatus;
              }
              break;
            }
        }
        //reset the blink counter
        blinks[index] = 0;
      }
      Serial.println("-"); // end marker
    }
  }
}
