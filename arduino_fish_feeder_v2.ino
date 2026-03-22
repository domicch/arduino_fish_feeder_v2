#include <Stepper.h>

#include <RTClib.h>
#include <Servo.h>


/**
 * Connections:
 *
 * D6 - servo control pin
 * D7 - test button
 * A4 - CLK SDA
 * A5 - CLK SCL
*/

class Scheduler {
    private:
        RTC_DS3231 rtc;

        int schedules[20][6] = { // [year, month, day, hour, minute, second]
            {2026, 3, 22, 0, 10, 0},            
        };

        bool isAlarmSet = false;
        DateTime nextAlarm;
        void setAlarm();
        String printDateTime(DateTime datetime);
        String printTime(DateTime datetime);

    public:
        void init();
        void setTime();
        String printNow();
        String printAlarm();
        bool checkAlarm();
        void clearAlarm();
        DateTime now();
};

void Scheduler::init() {
    rtc.begin();
    rtc.disable32K();
}

void Scheduler::setTime() {
    // ask user to enter new date and time
    const char txt[6][15] = { "year [4-digit]", "month [1~12]", "day [1~31]",
                            "hours [0~23]", "minutes [0~59]", "seconds [0~59]"};
    String str = "";
    long newDate[6];

    while (Serial.available()) {
    Serial.read();  // clear serial buffer
    }

    for (int i = 0; i < 6; i++) {

      Serial.print("Enter ");
      Serial.print(txt[i]);
      Serial.print(": ");

      while (!Serial.available()) {
          ; // wait for user input
      }

      str = Serial.readString();  // read user input
      newDate[i] = str.toInt();   // convert user input to number and save to array

      Serial.println(newDate[i]); // show user input
    }

    // update RTC
    rtc.adjust(DateTime(newDate[0], newDate[1], newDate[2], newDate[3], newDate[4], newDate[5]));
    Serial.println("RTC Updated!");
}

String Scheduler::printDateTime(DateTime datetime)
{
    char value[30];

    int ss = datetime.second();
    int mm = datetime.minute();
    int hh = datetime.hour();
    int DD = datetime.dayOfTheWeek();
    int dd = datetime.day();
    int MM = datetime.month();
    int yyyy = datetime.year();

    sprintf(value, "%02d-%02d-%04d %02d:%02d:%02d",
            dd, MM, yyyy, hh, mm, ss);
    
    return String(value);
}

String Scheduler::printTime(DateTime datetime)
{
    char value[6];

    int year = datetime.year();
    int month = datetime.month();
    int day = datetime.day();
    int mm = datetime.minute();
    int hh = datetime.hour();

    sprintf(value, "%02d/%02d/%02d %02d:%02d",
            year, month, day, hh, mm);
    
    return String(value);
}

void Scheduler::setAlarm() {
    DateTime now = rtc.now();
    int closestFutureIndex = -1;

    if(sizeof(schedules) == 0) {
        return;
    }

    int rows = sizeof(schedules) / sizeof(schedules[0]);

    // find the closest future timestamp
    for(int i = 0; i < rows; i++) {
        DateTime s (schedules[i][0], schedules[i][1], schedules[i][2], schedules[i][3], schedules[i][4], schedules[i][5]);
        if(s > now && (closestFutureIndex == -1 || s < DateTime(schedules[closestFutureIndex][0], schedules[closestFutureIndex][1], schedules[closestFutureIndex][2], schedules[closestFutureIndex][3], schedules[closestFutureIndex][4], schedules[closestFutureIndex][5]))) {
            closestFutureIndex = i;
        }
    }

    if(closestFutureIndex == -1) {
        Serial.println("no future timestamps found");
        isAlarmSet = false;
        return;
    }

    nextAlarm = DateTime(schedules[closestFutureIndex][0], schedules[closestFutureIndex][1], schedules[closestFutureIndex][2], schedules[closestFutureIndex][3], schedules[closestFutureIndex][4], schedules[closestFutureIndex][5]);
    Serial.print("Alarm set: ");
    Serial.println(printDateTime(nextAlarm));
    isAlarmSet = true;
}

bool Scheduler::checkAlarm() {
    if(!isAlarmSet) {
        setAlarm();
    }

    if(!isAlarmSet) {
        return false;
    }

    return rtc.now() >= nextAlarm;
}

String Scheduler::printNow() {
    return printTime(rtc.now());
}

void Scheduler::clearAlarm() {
    isAlarmSet = false;
    Serial.println("alarm cleared");
}

String Scheduler::printAlarm() {
    if(isAlarmSet) {
        return printTime(nextAlarm);
    }else {
        return "N/A";
    }
}

DateTime Scheduler::now() {
    return rtc.now();
}

const int PIN_TEST_BUTTON = 7;
const unsigned long TEST_BUTTON_DEBOUNCE_MILLISEC = 500UL;
const int stepsPerRevolution = 2048;   //From your motor data sheet, number of steps per revolution
const unsigned long ALARM_POLL_MILLISEC = 1000UL;
const unsigned long MAIN_LOOP_MILLISEC = 200UL;

unsigned long testButtonLastClickTs = 0UL;
unsigned long alarmPollLastTs = 0UL;
int delayCount = 1;    //A counter to keep track of which delay is in use, 1 or 2


Stepper myStepper(stepsPerRevolution, 8, 9, 10, 11);   //Initialize the stepper library on pins 8 through to 11
Servo servo;
Scheduler rtc;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  servo.attach(6);

  // test button
  pinMode(PIN_TEST_BUTTON,INPUT_PULLUP);

  Serial.println("Hello");
  rtc.init();

  myStepper.setSpeed(5);
}

// set time
void loop2() {
  // updateLCD();
  String dateTime = rtc.printNow();
  // lcd.setCursor(0, 0);
  // lcd.print(dateTime);

  if (Serial.available()) {
    char input = Serial.read();
    if (input == 'u') rtc.setTime();  // update RTC time
    if(input == 't') Serial.println(rtc.printNow());
  }
}

void loop()
{
    unsigned long mainLoopStartTs = millis();

    if (digitalRead(PIN_TEST_BUTTON) == LOW) {
        if(millis() - testButtonLastClickTs > TEST_BUTTON_DEBOUNCE_MILLISEC) {
            Serial.println("button pressed");
            feed();
            testButtonLastClickTs = millis();
        }
    }

    // scheduler
    if(millis() - alarmPollLastTs > ALARM_POLL_MILLISEC) {
        if(rtc.checkAlarm()) {
            feed();
            rtc.clearAlarm();
        }
        alarmPollLastTs = millis();
    }

    unsigned long now = millis();
    if(now < mainLoopStartTs + MAIN_LOOP_MILLISEC) {
        delay(mainLoopStartTs + MAIN_LOOP_MILLISEC - now);
    }
}

void feed() {
    nextWell();
    tap();
}

void nextWell() {
    //146 steps is 1/14th of a full rotation. Since we have 14 wells, this will index the feeder one space.
    myStepper.step(730);           

    //Turn the motor off when not in use to preserve energy and prevent it from heating up
    digitalWrite(8, LOW);          
    digitalWrite(9, LOW);
    digitalWrite(10, LOW);
    digitalWrite(11, LOW);
}

void tap() {
    servo.write(120); //move
    delay(60000);
    servo.write(90); //stop
}
