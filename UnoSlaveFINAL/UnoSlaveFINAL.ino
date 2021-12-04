#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <EEPROM.h>

File myFile;
RTC_DS3231 rtc;

String fileNames[4] = { "25", "CO", "NO2", "NO2b" }, folderName = "";
char saveChar;
String saveText = "";
int op, num = -1;

void setup()
{
  startRTC();
  sdStart();
  makeDir();
  Wire.begin(4);
  Wire.onReceive(receiveEvent);
  Serial.begin(9600);
}

void loop()
{
  delay(100);
}

void receiveEvent()
{
  while (1 < Wire.available())
  {
    saveChar = Wire.read();
    saveText = saveText + saveChar;
  }
  op = Wire.read();
  Serial.println(saveText);
  saveTextFile(saveText, op);
  saveText = "";
}

void saveTextFile(String text, int op) {
  String title = folderName + "/" + fileNames[op] + "_RES.txt" ;
  myFile = SD.open(title, FILE_WRITE);
  if (myFile) {
    myFile.println(text);
    while (myFile.available()) {
      Serial.write(myFile.read());
    }
    myFile.close();
  }
  else {
    Serial.println("Error opening " + title);
  }
}

void sdStart() {
  Serial.print("Init SD card...");
  SD.begin(4);
  if (!SD.begin(4)) {
    Serial.println("Init failed!");
    while (1);
  } else
    Serial.print("Erro SD");
  Serial.println("Init done.");
}

void startRTC() {
  Serial.println("Foi");
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


void makeDir() {
  DateTime now = rtc.now();

  if (!(SD.open(String(now.year(), DEC))).available()) {
    SD.mkdir(String(now.year(), DEC));
  }

  if (!(SD.open(String(now.year(), DEC) + "/" + String(now.month(), DEC)).available())) {
    SD.mkdir(String(now.year(), DEC) + "/" + String(now.month(), DEC));
  }

  if (!(SD.open(String(now.year(), DEC) + "/" + String(now.month(), DEC) + "/" + String(now.day(), DEC))).available()) {
    SD.mkdir(String(now.year(), DEC) + "/" + String(now.month(), DEC) + "/" + String(now.day(), DEC));
  }

  num = EEPROM.read(0);
  num++;
  EEPROM.write(0, num);

  folderName = String(now.year(), DEC) + "/" + String(now.month(), DEC) + "/" + String(now.day(), DEC) + "/" + String(num) + "_RES"; // Ex: 2021 -> 11 -> 21 -> RES_0 -> TXT'
  if (!(SD.open(folderName)))
    SD.mkdir(folderName);
}
