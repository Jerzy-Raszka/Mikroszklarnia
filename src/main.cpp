#include <Arduino.h>
#include <DHT.h>
#include <ESP32Time.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "time.h"

#define DHTPIN 15
#define DHTTYPE DHT22
#define EARTH 34
#define WATER 32
#define LEDS 2
#define LEDW 13
#define FAN 16
#define PUMP 17
#define viewButton 33
#define upButton 25
#define downButton 26
#define changeButton 27

const char* ntpServer = "ntp1.tp.pl";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

DHT dht(DHTPIN, DHTTYPE);
ESP32Time rtc(3600);
hw_timer_t *Fan_timer = NULL;
hw_timer_t *Loop_timer = NULL;
LiquidCrystal_I2C lcd(0x27, 16, 2);
float temperature, humidity, earth, water, setTemperature = 27.0, setHumidity = 85.0;
String date, pomiar;
int godz;
bool res, upButtonFlag = false, downButtonFlag = false, viewButtonFlag = true, change = true, view = true, measure = true;
long PumpTimeON;
unsigned long button_time = 0;  
unsigned long last_button_time = 0; 
AsyncWebServer server(80);
struct tm timeinfo;

void IRAM_ATTR viewChange() {
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
  view = !view;
  viewButtonFlag = true;
  last_button_time = button_time;
  }
}
void IRAM_ATTR upChange() {
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
  upButtonFlag = true;
  last_button_time = button_time;
  }
}
void IRAM_ATTR downChange() {
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
  downButtonFlag = true;
  last_button_time = button_time;
  }
}
void IRAM_ATTR Change() {
  button_time = millis();
  if (button_time - last_button_time > 250)
  {
  change = !change;
  last_button_time = button_time;
  }
}

void IRAM_ATTR onTimerFan() {
  digitalWrite(FAN, HIGH); 
  timerWrite(Loop_timer, 0);
}

void IRAM_ATTR onTimerLoop() { measure = true; }

void readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void getRTC(){

 if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

}

void setup() {
  WiFi.mode(WIFI_AP_STA);
  Serial.begin(921600);
  WiFiManager wm;
  res = wm.autoConnect("AutoConnectAP","password");
  if(!res){
    Serial.println("Failed to connect");
  } 
  else{   
    Serial.println("connected...yeey :)");
  }
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SD, "/pomiar.txt","text/json");
  });
  server.begin();
  dht.begin();
  lcd.init();
  lcd.backlight();
  pinMode(DHTPIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(EARTH, INPUT);
  pinMode(WATER, INPUT);
  pinMode(LEDS, OUTPUT);
  pinMode(LEDW, OUTPUT);
  pinMode(FAN, OUTPUT);
  pinMode(PUMP, OUTPUT);
  pinMode(viewButton, INPUT_PULLUP);
  pinMode(upButton, INPUT_PULLUP);
  pinMode(downButton, INPUT_PULLUP);
  pinMode(changeButton, INPUT_PULLUP);
  attachInterrupt(viewButton, viewChange, FALLING);
  attachInterrupt(upButton, upChange, FALLING);
  attachInterrupt(downButton, downChange, FALLING);
  attachInterrupt(changeButton, Change, FALLING);
  analogReadResolution(10);
  Fan_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(Fan_timer, &onTimerFan, true);
  timerAlarmWrite(Fan_timer, 43200000000, true);
  timerAlarmEnable(Fan_timer);
  Loop_timer = timerBegin(1, 80, true);
  timerAttachInterrupt(Loop_timer, &onTimerLoop, true);
  timerAlarmWrite(Loop_timer, 60000000, true); // 900000000 - 15 minut  60 000 000 - 1 minuta
  timerAlarmEnable(Loop_timer);
  if(!SD.begin()){
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
  Serial.println("No SD card attached");
    return;
  }
}

void loop() {
  if (measure == true) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    getRTC();
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    earth = analogRead(EARTH);
    water = digitalRead(WATER);
    char dateServer[50];
    strftime(dateServer,50, "%A, %B %d %Y %H:%M:%S", &timeinfo);
    date = String(dateServer);
    char timeHour[3];
    strftime(timeHour,3, "%H", &timeinfo);
    godz = int(timeHour);
    //Zapalanie ledów o okreslonej godzinie dnia 16:00-24:00
    if (godz >= 15 && godz < 23) {
      digitalWrite(LEDS, HIGH);
    } else {
      digitalWrite(LEDS, LOW);
    }
    // Zapalenie diody jeśli zbiornik wody jest pusty
    if (water == 0.0) {
      digitalWrite(LEDW, HIGH);
    } else {
      digitalWrite(LEDW, LOW);
    }
    // Zapalanie wiatraka jeśli temperatura jest temperatura ponad 27 stopni lub
    // wilgotnosc powyzej 85%
    if (temperature > setTemperature || humidity > setHumidity) {
      digitalWrite(FAN, HIGH);
    } else {
      digitalWrite(FAN, LOW);
    }
    // Zapalanie pompy przy odczycie wilgotności gleby >650
    if (earth > 450) {
      digitalWrite(PUMP, HIGH);
      PumpTimeON = rtc.getEpoch();

    } else {
      digitalWrite(PUMP, LOW);
    }
    pomiar = ("\n Godzina: "+date+" | Ustalona wilgotność powietrza: "+String(setHumidity)+"% | Ustalona temperatura: "+String(setTemperature)+"\u2103"" | Wilgotność powietrza: "+String(humidity)+"% | Temperatura: "+String(temperature)+"\u2103"" | Stan zbiornika: "+String(water)+" | Wilgotność gleby: "+String(earth)+" | Wiatrak: "+String(digitalRead(FAN))+" | Ledy: "+String(digitalRead(LEDS))+" | Pompa: "+String(digitalRead(PUMP))+" ");
    Serial.println(pomiar);
    if(SD.exists("/pomiar.txt")==0){
      writeFile(SD, "/pomiar.txt", "Pomiary miniszklarni 0-urządzenie wyłączone/zbiornik wymaga napełnienia 1-urządzenie włączone/zbiornik pełny"); // stworzenie pliku
    }
    appendFile(SD, "/pomiar.txt",  pomiar.c_str());
    measure = false;

    }

  if ((rtc.getEpoch() - PumpTimeON) > 60) {
    digitalWrite(PUMP, LOW);
  }
  if(viewButtonFlag == true){
    if(view == true){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.printf("UT:%.0f\xDF""CZT:%.1f\xDF""C",setTemperature,temperature);
      lcd.setCursor(0,1);
      lcd.printf("UW:%.0f%%ZW:%.1f%%",setHumidity,humidity);
    }
    else{
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.printf("      IP:");
      lcd.setCursor(0,1);
      lcd.printf("%s",WiFi.localIP().toString().c_str());
    } 
    viewButtonFlag = false;
  }
  if(upButtonFlag == true){
    if(change == true){
      setTemperature = setTemperature+1;
    }
    else{
      setHumidity = setHumidity+1;
    }
    upButtonFlag = false;
    viewButtonFlag = true;
  }
  if(downButtonFlag == true){
    if(change == true){
      setTemperature = setTemperature-1;
    }
    else{
      setHumidity = setHumidity-1;
    }
    downButtonFlag = false;
    viewButtonFlag = true;
  }
  
}
