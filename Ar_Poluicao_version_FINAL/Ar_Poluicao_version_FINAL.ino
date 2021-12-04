#include <MICS6814.h>
#include <DSM501.h>
#include "RTClib.h"
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>

#define PIN_CO  A0
#define PIN_NO2 A1
#define PIN_NH3 A2

#define DSM501_PM10 7
#define DSM501_PM25 1

DSM501 dsm501(DSM501_PM10, DSM501_PM25);
MICS6814 gas(PIN_CO, PIN_NO2, PIN_NH3);

const int buttonPinW = 13;
const int buttonPinB = 12;
int buttonPinB_prev;
int buttonPinW_prev;
int lcdState = 0;
int changeAQI = -1;

unsigned long starttime;
unsigned long sampletime_ms = 120000;// 10s ;

float pm25_ = 0, no2_ = 0, co_ = 0, no2b_ = 0;

String resultAQI_Names[6] = { "GOOD", "MODERATE", "UNHEALTHY-", "UNHEALTHY+", "UNHEALTHY++", "HAZARDOUS" };
String resultAQI_Nome[5] = { "BOA", "MODERADA", "RUIM", "MUITO RUIM", "PESSIMA" };
String saveText;

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2);
RTC_DS3231 rtc;

void getReadData() {
  pm25_ = dsm501.getParticleWeight(1);
  no2_ = gas.measure(NO2) * 1000;
  no2b_ = no2_ * 12.187 * 46.0055 / (273.15 + rtc.getTemperature()); //µg/m3 = (ppb)*(12.187)*(M) / (273.15 + °C)
  co_ = gas.measure(CO);
}

void PM_25() {
  print_LCD("PM2.5", "ug/m3", pm25_, 0);
  buttonOnOff();
}

void CO_() {
  print_LCD("CO", "ppm", co_, 1);
  buttonOnOff();
}

void NO2_() {
  if (changeAQI == -1) {
    print_LCD("NO2", "ppb", no2_, 2);
    buttonOnOff();
  }
}

void NO2B_() {
  if (changeAQI == 1) {
    print_LCD("NO2", "ug/m3", no2b_, 3);
    buttonOnOff();
  }
}

void saveData() {
  saving();
  uploadText(0, pm25_);
  uploadText(1, co_);
  uploadText(2, no2_);
  uploadText(3, no2b_);
  dsm501.update();
  starttime = millis();
}

void setup() {
  Serial.begin(9600);

  pinMode(buttonPinW, INPUT_PULLUP);
  buttonPinW_prev = digitalRead(buttonPinW);

  pinMode(buttonPinB, INPUT_PULLUP);
  buttonPinB_prev = digitalRead(buttonPinB);

  lcd.init();
  lcd.backlight();

  loading();
  dsm501.begin(MIN_WIN_SPAN);
  gas.calibrate();
  startDSM501_MICS6814();
  loading();

  startRTC();
  Wire.begin();
  starttime = millis();
}

void loop() {
  getReadData();
  while (!((millis() - starttime) >= sampletime_ms)) {
    CO_();
    PM_25();
    NO2_();
    NO2B_();
  }
  saveData();
}

void loading() {
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("Loading");
  delay(1000);
}

void saving() {
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("Saving");
  delay(1000);
}

void startDSM501_MICS6814() {
  clearLine1();
  for (int i = 1; i <= 60; i++) {
    delay(1000); // 1s
    lcd.setCursor(1, 0);
    lcd.print(i);
    lcd.setCursor(4, 0);
    lcd.print(" s wait 60s");
  }
}

void startRTC() {
  if (! rtc.begin()) {
    Serial.println("DS3231 não encontrado");
    while (1);
  }
  if (rtc.lostPower()) {
    Serial.println("DS3231 OK!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  delay(100);
}

void buttonOnOff() {
  if (digitalRead(buttonPinW) == HIGH && buttonPinW_prev == LOW) { // && ((millis() - starttime ) >= 500)  && lastButtonState == HIGH
    lcdState = (lcdState == HIGH) ? LOW : HIGH;
    if (lcdState == HIGH) {
      lcd.clear();
      lcd.noBacklight();
    }
    else
      lcd.backlight();
  }
  if (digitalRead(buttonPinB) == HIGH && buttonPinB_prev == LOW) {
    changeAQI = changeAQI * -1;
  }
  buttonPinW_prev = digitalRead(buttonPinW);
  buttonPinB_prev = digitalRead(buttonPinB);
}

void connetionSlave(int op) {
  Wire.beginTransmission(4);
  Wire.write(saveText.c_str());
  Wire.write(op);
  Wire.endTransmission();
  delay(500);
}

void print_LCD(String LABEL, String TYPE, float CON, int op) {

  buttonOnOff();

  int help = -1;
  int RES = 0;
  if (changeAQI == -1)
    RES = getAQI(op, CON);
  else
    RES = getAQI_BR(2, CON);

  if (lcdState == LOW) {
    lcd.clear();
    if (op == 1)
      help = 3;
    if (op == 2)
      help = 2;
    lcd.setCursor(help, 0);
    lcd.print(LABEL);
    lcd.setCursor(6, 0);
    lcd.print(CON);
    if (op == 2)
      help = 12;
    else
      help = 11;
    lcd.setCursor(help, 0);
    lcd.print(TYPE);
    if (changeAQI == -1)
      getResultAQI(RES);
    else
      getResultAQI_BR(RES);
  }
  delay(2000);
}

void uploadText(int op, float CON) {
  int RES, RES_B;

  RES = getAQI(op, CON);
  RES_B = getAQI_BR(op, CON);

  if (op == 2) {
    RES_B = -1;
  }

  if (op == 3) {
    RES = -1;
    RES_B = getAQI_BR(2, CON);
  }

  formatText(RES, RES_B, CON);
  connetionSlave(op);
}

void formatText(int RES, int RES_B, float CON) {
  DateTime now = rtc.now();
  String tCON = String(CON);
  String tRES, tRES_B;
  tCON.replace(".", ",");

  tRES_B = String(RES_B);
  tRES = String(RES)  + ";";

  if (RES == -1) {
    tRES = "";
  }

  if (RES_B == -1) {
    tRES_B = "";
  }

  char dateBuffer[12];
  sprintf(dateBuffer, "%02u:%02u", now.hour(), now.minute());

  saveText = String(now.day(), DEC) + "/" + String(now.month(), DEC) + "/" + String(now.year() - 2000 , DEC) + " "
             + dateBuffer + ";" + tCON + ";" + tRES + tRES_B;
}

void getResultAQI(float getAQI) {
  clearLine2();

  if (getAQI >= 0 && getAQI <= 50) {
    lcdPrint(resultAQI_Names[0], 5, 1);
  }
  if (getAQI > 50 && getAQI <= 100) {
    lcdPrint(resultAQI_Names[1], 4, 1);
  }
  if (getAQI > 100 && getAQI <= 150) {
    lcdPrint(resultAQI_Names[2], 3, 1);
  }
  if (getAQI > 150 && getAQI <= 200) {
    lcdPrint(resultAQI_Names[3], 4, 1);
  }
  if (getAQI > 200 && getAQI <= 300 ) {
    lcdPrint(resultAQI_Names[4], 3, 1);
  }
  if (getAQI > 300) {
    lcdPrint(resultAQI_Names[5], 4, 1);
  }
}

void getResultAQI_BR(float getAQI) {
  clearLine2();

  if (getAQI >= 0 && getAQI <= 40) {
    lcdPrint(resultAQI_Nome[0], 5, 1);
  }
  if (getAQI >= 41 && getAQI <= 80) {
    lcdPrint(resultAQI_Nome[1], 4, 1);
  }
  if (getAQI >= 81 && getAQI <= 120) {
    lcdPrint(resultAQI_Nome[2], 3, 1);
  }
  if (getAQI >= 121 && getAQI <= 200) {
    lcdPrint(resultAQI_Nome[3], 4, 1);
  }
  if (getAQI > 200) {
    lcdPrint(resultAQI_Nome[4], 3, 1);
  }
}

void lcdPrint(String text, int column, int line) {
  lcd.setCursor(column, line);
  lcd.print(text);
}

void clearLine1() {
  lcd.setCursor (0, 0);
  for (int i = 0; i < 16; ++i)
  {
    lcd.write(' ');
  }
}

void clearLine2() {
  lcd.setCursor (0, 1);
  for (int i = 0; i < 16; ++i)
  {
    lcd.write(' ');
  }
}

float calcAQI(float I_high, float I_low, float C_high, float C_low, float C) {
  return (I_high - I_low) * (C - C_low) / (C_high - C_low) + I_low;
}

int getAQI(int sensor, float density) {
  //int d10 = (int)(density * 10);
  float d10 = density;

  if ( sensor == 0 ) { //2.5
    if (d10 <= 0) {
      return 0;
    }
    else if (d10 <= 12.0) {
      return calcAQI(50, 0, 12.0, 0, d10);
    }
    else if (d10 <= 35.4) {
      return calcAQI(100, 51, 35.4, 12.1, d10);
    }
    else if (d10 <= 55.4) {
      return calcAQI(150, 101, 55.4, 35.5, d10);
    }
    else if (d10 <= 150.4) {
      return calcAQI(200, 151, 150.4, 55.5, d10);
    }
    else if (d10 <= 250.4) {
      return calcAQI(300, 201, 250.4, 150.5, d10);
    }
    else if (d10 <= 350.4) {
      return calcAQI(400, 301, 350.4, 250.5, d10);
    }
    else if (d10 <= 500.4) {
      return calcAQI(500, 401, 500.4, 350.5, d10);
    }
    else {
      return 1001;
    }
  }
  if ( sensor == 1 ) { // CO
    if (d10 <= 0) {
      return 0;
    }
    else if (d10 <= 4.4) {
      return calcAQI(50, 0, 4.4, 0, d10);
    }
    else if (d10 <= 9.4) {
      return calcAQI(100, 51, 9.4, 4.5, d10);
    }
    else if (d10 <= 12.4) {
      return calcAQI(150, 101, 12.4, 9.5, d10);
    }
    else if (d10 <= 15.4) {
      return calcAQI(200, 151, 15.4, 12.5, d10);
    }
    else if (d10 <= 30.4) {
      return calcAQI(300, 201, 30.4, 15.5, d10);
    }
    else if (d10 <= 40.4) {
      return calcAQI(400, 301, 40.4, 30.5, d10);
    }
    else if (d10 <= 50.4) {
      return calcAQI(500, 401, 50.4, 40.5, d10);
    }
    else {
      return 1001;
    }
  }
  if ( sensor == 2 ) { // NO2
    if (d10 <= 0) {
      return 0;
    }
    else if (d10 <= 53) {
      return calcAQI(50, 0, 53, 0, d10);
    }
    else if (d10 <= 100) {
      return calcAQI(100, 51, 100, 54, d10);
    }
    else if (d10 <= 360) {
      return calcAQI(150, 101, 360, 101, d10);
    }
    else if (d10 <= 649) {
      return calcAQI(200, 151, 649, 361, d10);
    }
    else if (d10 <= 1249) {
      return calcAQI(300, 201, 1249, 650, d10);
    }
    else if (d10 <= 1649) {
      return calcAQI(400, 301, 1649, 1250, d10);
    }
    else if (d10 <= 2049) {
      return calcAQI(500, 401, 2049, 1650 , d10);
    }
    else {
      return 1001;
    }
  }
}

int getAQI_BR(int sensor, float density) {
  float d10 = density;

  if ( sensor == 0 ) { //2.5
    if (d10 <= 0) {
      return 0;
    }
    else if (d10 <= 25) {
      return calcAQI(40, 0, 25, 0, d10);
    }
    else if (d10 <= 50) {
      return calcAQI(80, 41, 50, 25.01, d10);
    }
    else if (d10 <= 75) {
      return calcAQI(120, 81, 75, 50.01, d10);
    }
    else if (d10 <= 125) {
      return calcAQI(200, 121, 125, 75.01, d10);
    }
    else if (d10 <= 500) {
      return calcAQI(500, 201, 500, 125.01, d10);
    }
    else {
      return 1001;
    }
  }
  if ( sensor == 1 ) { // CO
    if (d10 <= 0) {
      return 0;
    }
    else if (d10 <= 9) {
      return calcAQI(40, 0, 9, 0, d10);
    }
    else if (d10 <= 11) {
      return calcAQI(80, 41, 11, 9.01, d10);
    }
    else if (d10 <= 13) {
      return calcAQI(120, 81, 13, 11.01, d10);
    }
    else if (d10 <= 15) {
      return calcAQI(200, 121, 15, 13.01, d10);
    }
    else if (d10 <= 50) {
      return calcAQI(300, 201, 50, 15.01, d10);
    }
    else {
      return 1001;
    }
  }
  if ( sensor == 2 ) { // NO2 (ug/m3)
    if (d10 <= 0) {
      return 0;
    }
    else if (d10 <= 200) {
      return calcAQI(40, 0, 200, 0, d10);
    }
    else if (d10 <= 240) {
      return calcAQI(80, 41, 240, 200.01, d10);
    }
    else if (d10 <= 320) {
      return calcAQI(120, 81, 320, 240.01, d10);
    }
    else if (d10 <= 1130) {
      return calcAQI(200, 121, 1130, 320.01, d10);
    }
    else if (d10 <= 2000) {
      return calcAQI(300, 201, 2000, 1130.01, d10);
    }
    else {
      return 1001;
    }
  }
}
