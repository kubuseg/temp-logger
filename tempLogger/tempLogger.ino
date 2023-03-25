#include <RTClib.h>
#include <LowPower.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <SD.h>

#define CLOCK_INTERRUPT_PIN 2
#define thermDATA 5
#define sdGATE 6
#define RSTPIN 7
#define CHIPSELECT 10
#define MOSIPIN 11
#define MISOPIN 12
#define SCKPIN 13
//RTC connections
//SDA:4, SCL:5, VCC:3.3V
//SD connections
//MOSO:12, MOSI:11, SCK:13, CS:10

OneWire oneWire(thermDATA);
DallasTemperature tempSensor(&oneWire);
RTC_DS3231 rtc;
const TimeSpan perBtwMeasur = TimeSpan(0, 0, 1, 0);

float checkTemp()
{
  tempSensor.begin();
  Serial.print("Requesting temperatures..."); 
  tempSensor.requestTemperatures();
  Serial.print("DONE "); 
  float temp = tempSensor.getTempCByIndex(0);
  Serial.print("Temperature is: "); 
  Serial.println(temp);
  Serial.flush();
  return temp;
}

void turnOffSdModule()
{
    SD.end();
    delay(10);
    digitalWrite(sdGATE, LOW);
    delay(10);
    digitalWrite(SCKPIN, HIGH);
    digitalWrite(CHIPSELECT, HIGH);
    digitalWrite(MOSIPIN, HIGH);
    digitalWrite(MISOPIN, HIGH);
    delay(10);
}

void writeToSd(DateTime date, float rtcTemp, float temp)
{
  //Setting file name(format MM-YYYY)
  char fileNameBuff[] = "MM-YYYY";
  char fileName[12];
  (rtc.now().toString(fileNameBuff) + String(".txt")).toCharArray(fileName, 12);
  //Turning on mos transistor
  digitalWrite(sdGATE, HIGH);
  //Turning on communication protocol for sd card module
  SPI.begin();
  //Checking if can begin sd card
  if (!SD.begin()) {
    Serial.println("initialization failed!");
    Serial.flush();
    digitalWrite(RSTPIN, LOW); //Reset arudino 
    return;
  }
  Serial.println("initialization done.");
  File myFile = SD.open(fileName, FILE_WRITE);
  if (myFile) {
    Serial.print("Writing to " + String(fileName) + "...");
    char buff[] = "DD-MM-YY hh:mm, ";
    myFile.print(date.toString(buff)); //wpisywanie temp i daty
    myFile.print(rtcTemp);
    myFile.print(", "); 
    myFile.println(temp);
    myFile.close();
    Serial.println("done.");
  } else {
    // if the file didn't open, print an error:
    Serial.print("error opening ");
    Serial.println(fileName);
    Serial.flush();
    digitalWrite(RSTPIN, LOW); //Reset arudino
    return;
  }
  turnOffSdModule();
}

void setAlarm(DateTime alarmTime)
{
  rtc.clearAlarm(1);
  // stop oscillating signals at SQW Pin
  // otherwise setAlarm1 will fail
  rtc.writeSqwPinMode(DS3231_OFF);
  if(!rtc.setAlarm1(alarmTime, DS3231_A1_Minute)) {
    Serial.println("Error, alarm wasn't set!");
  } else {
    Serial.print("Alarm will happen at ");
    char buff[] = "hh:mm:ss";
    Serial.println(alarmTime.toString(buff));
  }  
  
}

void onAlarm() {}

void setup() {
    //CLKPR = 0x80;
    //CLKPR = 0x01;
    Serial.begin(9600);
    pinMode(sdGATE, OUTPUT); digitalWrite(sdGATE, LOW);
    pinMode(CHIPSELECT, OUTPUT); digitalWrite(CHIPSELECT, HIGH);
    pinMode(MOSIPIN, OUTPUT); digitalWrite(MOSIPIN, HIGH);
    pinMode(MISOPIN, INPUT); digitalWrite(MISOPIN, HIGH);
    pinMode(SCKPIN, OUTPUT); digitalWrite(SCKPIN, HIGH);
    digitalWrite(RSTPIN, HIGH); pinMode(RSTPIN, OUTPUT);
    // initializing the rtc
    if(!rtc.begin()) {
        Serial.println("Couldn't find RTC!");
        Serial.flush();
        while (1) delay(10);
    }

    if(rtc.lostPower()) {
        // this will adjust to the date and time at compilation
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // Making it so, that the alarm will trigger an interrupt
    pinMode(CLOCK_INTERRUPT_PIN, INPUT_PULLUP);
    //Making alarm pin on rtc module interrupt deep sleep
    attachInterrupt(digitalPinToInterrupt(CLOCK_INTERRUPT_PIN), onAlarm, FALLING);
    //we don't need the 32K Pin, so disable it
    rtc.disable32K();
    // set alarm 2 flag to false (so alarm 2 didn't happen so far)
    // if not done, this easily leads to problems, as both register aren't reset on reboot/recompile
    rtc.clearAlarm(2);
    // turn off alarm 2 (in case it isn't off already)
    // again, this isn't done at reboot, so a previously set alarm could easily go overlooked
    rtc.disableAlarm(2);
    setAlarm(rtc.now() + perBtwMeasur);
    //initializing the temperature sensor
    //const uint8_t resolution = 11;
    //tempSensor.setResolution(resolution);
    tempSensor.begin();
}

void loop() {
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); 
    if(rtc.alarmFired(1)) {
    setAlarm(rtc.now() + perBtwMeasur);
    }
    DateTime now = rtc.now(); // Get the current time
    char buff[] = "Alarm triggered at hh:mm:ss DDD, DD MMM YYYY";
    Serial.println(now.toString(buff));
    float rtcTemp = rtc.getTemperature();
    Serial.print("RTC temp is : ");
    Serial.println(rtcTemp);
    float temp = checkTemp();
    //int timeStart = millis();
    writeToSd(now, rtcTemp, temp);
    Serial.flush();
    //Serial.println(String(millis()-timeStart) + String("ms"));
}
