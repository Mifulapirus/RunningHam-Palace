/****************************************
* RunningHam Wheel
* Version = 0.1
* Author: Angel Hernandez
* Contact @mifulapirus
*         @RunnerHam
*
* Attached Sensors:
*  - Hall: A3144 on pin D7
*****************************************/
#include <SoftwareSerial.h>
#include <ESP8266.h>
#include <EEPROMex.h>

/*******************************
* Pin definitions
*******************************/
#define SDA_PIN        4
#define SCL_PIN        4

#define WIFI_RX_PIN    3
#define WIFI_TX_PIN    2
#define WIFI_RST_PIN   6

#define HALL_PIN       7

#define GREEN_LED_PIN        A0
#define YELLOW_LED_PIN       A1
#define RED_LED_PIN          A2
#define SESSION_RUNNING_PIN  YELLOW_LED_PIN

#define EEPROM_ERASE_PIN     8

/*******************************
* General variables
*******************************/
#define FIRMWARE_VERSION   "0.1"
#define DEVICE_CATEGORY    "WS" 
#define DEVICE_NAME        "WS_1"
long timerStartedTime = 0;
long timerLastUpdate   = 0;
long timerValue       = 10000;         //10 seconds
boolean timerStarted = false;

/*****************************
* Other devices
*****************************/

/*******************************
* WiFi Stuff
*******************************/
#define SSID "YOUR SSID"       //EDIT this with the name of your wifi Network
#define PASS "YOUR PASS"       //EDIT this with the password of your wifi network
//---------------------------//
#define BAUD 9600
ESP8266 wifi(WIFI_RX_PIN, WIFI_TX_PIN, WIFI_RST_PIN, BAUD);

/****************************
* Control Commands
****************************/


/****************************
* Configuration Commands
****************************/

/*******************************
* ThingSpeak
********************************/
String UpdateKey = "THINGSPEAK_KEY";                //EDIT this with your own ThingSpeak key
String TwitterApiKey = "THINGSPEAK_TWITTER_KEY";    //EDIT this with your own ThingSpeak Twitter key
//---------------------------//
String GET = "GET /update?key=" + UpdateKey;
String SpeedField = "&field1=";
String MaxSpeedField = "&field2=";
String DistanceField = "&field3=";
#define UpdateIP "184.106.153.149" // thingspeak.com
long ThingSpeakLastUpdate = 0;
long ThingSpeakRegularUpdateDelay = 60000*60;  //1 hour
long ThingSpeakMinUpdateDelay = 10000;         //10 seconds
long ThingSpeakUpdateDelay = ThingSpeakRegularUpdateDelay;
String TwitterURL = "api.thingspeak.com";
String TwitterGet = "GET /apps/thingtweet/1/statuses/update?api_key=" + TwitterApiKey + "&status=";

/******************************
* Wheel variables
******************************/
float WheelLength = 0.43;    //EDIT this with the length of your wheel in m  
//---------------------------//
bool previousHall = false;
long SpinCounter = 0;
float LastSpeed = 0;         //Speed in m/s
long LastSpinTime = 0;
float SessionDistance = 0;
float TotalDistance = 0;    //distance in m
float CurrentSpeed = 0.0;
float MaxSpeed = 0;
float MaxSessionSpeed = 0;
boolean RunningSession = false;
int MaxElapsedTime = 20;    //Maximum time between spins in Seconds

/******************************
* Eeprom variables
******************************/
int MaxSpeedEepromAddress = 0;
int TotalDistanceEepromAddress = 4;
long EepromUpdateDelay = 60000*60;
long EepromLastUpdate = 0;


/************************************
* Setup and initializations
************************************/  
void setup() {
  //Set input pins
  pinMode(HALL_PIN, INPUT);
  pinMode(EEPROM_ERASE_PIN, INPUT);

  //Set output pins
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  
  //Initialize navite serial port
  Serial.begin(9600);
  digitalWrite(RED_LED_PIN, HIGH);
  delay(1000);
  wifi.wifiLongMessage.reserve(150);
  wifi.listen();
  bootUp(); 
}

/************************************
* General Functions
************************************/
void loop() {
  CheckHall();
  CheckIfRunningSession();
  
  //Update values on ThingSpeak
  PeriodicUpdateThingSpeak();
}

//Bootup sequence
void bootUp() {
  int _err = -1;
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, HIGH);
  
  _err = wifi.init(SSID, PASS);
  if(_err != NO_ERROR) {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(YELLOW_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
    Serial.print("E: ");
    Serial.println(String(_err));
  }
  
  else{
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(YELLOW_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
    Serial.print("OK: ");
    Serial.println(wifi.IP); 
  } 

  wifi.setTxMode(false);
  
   //Check if we should erase the EEPROM
   if (digitalRead(EEPROM_ERASE_PIN)) {
     EEPROM.updateFloat(MaxSpeedEepromAddress, 0.0);
     EEPROM.updateFloat(TotalDistanceEepromAddress, 0.0);  
     Serial.println("Mem erased");
  }

  //If not, get the saved values
  else {
    MaxSpeed = EEPROM.readFloat(MaxSpeedEepromAddress);
    TotalDistance = EEPROM.readFloat(TotalDistanceEepromAddress);
  }
 
  previousHall = digitalRead(HALL_PIN);
}


/******************************
* EEPROM Functions
******************************/
void EepromUpdate() {
  EEPROM.updateFloat(MaxSpeedEepromAddress, MaxSpeed);
  EEPROM.updateFloat(TotalDistanceEepromAddress, TotalDistance);  
  EepromLastUpdate = millis();
  Serial.println("Mem update");
}

void EepromPeriodicUpdate() {
  if (millis() > EepromLastUpdate + EepromUpdateDelay) {
    EepromUpdate();
  }
}

/****************************
* Wheel Interruption CallBack
*****************************/
void CheckHall() {
  bool currentHall = digitalRead(HALL_PIN);
  if ((previousHall==false) and (currentHall == true)) { //Spin detected
    SpinCount();
  }
  previousHall = currentHall;
}
  
  
  
void SpinCount() {
  long CurrentTime = millis();
  RunningSession = true;
  digitalWrite(GREEN_LED_PIN, HIGH);
  float ElapsedTime = (CurrentTime - LastSpinTime) / 1000.0;  //Elapsed time in seconds
  if (ElapsedTime > 0) {CurrentSpeed = WheelLength / ElapsedTime;}
  SpinCounter++;
  SessionDistance += WheelLength;
  if (CurrentSpeed > MaxSessionSpeed) {MaxSessionSpeed = CurrentSpeed;}
  LastSpinTime = CurrentTime;
  
  digitalWrite(GREEN_LED_PIN, LOW);
}

void CheckIfRunningSession() {
  if (RunningSession == true) {
    long CurrentTime = millis();
    digitalWrite(SESSION_RUNNING_PIN, HIGH);
    float ElapsedTime = (CurrentTime - LastSpinTime) / 1000.0;  //Elapsed time in seconds
           
    //Check if session has finished
    if (ElapsedTime > MaxElapsedTime) {
      //Stop Runnning session
      RunningSession = false;
      //Add Sesion values to total
      TotalDistance += SessionDistance;
      if (MaxSessionSpeed > MaxSpeed) {MaxSpeed = MaxSessionSpeed;}

      UpdateSessionThingSpeakTwitter();
      UpdateThingSpeak();
      
      //Update EEPROM
      EepromUpdate();
      
      //Reset Sesion values
      CurrentSpeed = 0;
      MaxSessionSpeed = 0;
      SessionDistance = 0;
      ThingSpeakUpdateDelay = ThingSpeakRegularUpdateDelay;
      digitalWrite(SESSION_RUNNING_PIN, LOW);
    }
  } 
}

/************************************
* ThingSpeak functions
************************************/
void UpdateThingSpeak() {
  Serial.print("TS update @ ");
  Serial.println(millis());
  
  //Open TCP
  byte _err = wifi.openTCP(UpdateIP, "80", true);
  
  if(_err != NO_ERROR) {
    Serial.print("E: ");
    Serial.println(String(_err));
  }
  
  else{
    Serial.print("OK ");
  }
  delay(100);
  wifi.wifiLongMessage = GET;
  wifi.wifiLongMessage += SpeedField;
  wifi.wifiLongMessage += String(CurrentSpeed);
  wifi.wifiLongMessage += MaxSpeedField;
  wifi.wifiLongMessage += String(MaxSpeed);
  wifi.wifiLongMessage += DistanceField;
  wifi.wifiLongMessage += String(TotalDistance);
  wifi.wifiLongMessage += "\r\n";
  
  _err = wifi.sendLongMessage("SEND OK", true);
  if(_err != NO_ERROR) {
    Serial.print("E: ");
    Serial.println(String(_err));
  }
  
  else{
    wifi.closeTCP();
  }
  ThingSpeakLastUpdate = millis();
}

void PeriodicUpdateThingSpeak() {
  if (millis() > ThingSpeakLastUpdate + ThingSpeakUpdateDelay) {
    UpdateThingSpeak();
  }
}


boolean UpdateSessionThingSpeakTwitter() {
  Serial.print("Tw @ ");
  Serial.println(millis());
  byte _err = wifi.openTCP(TwitterURL, "80", true);

  wifi.wifiLongMessage = TwitterGet; 
  wifi.wifiLongMessage += "Hi!\r\n";
  wifi.wifiLongMessage = TwitterGet; 
  wifi.wifiLongMessage += "I'm done! "; 
  wifi.wifiLongMessage += String(SessionDistance); 
  wifi.wifiLongMessage += "m at "; 
  wifi.wifiLongMessage += String(MaxSessionSpeed); 
  wifi.wifiLongMessage += "m/s";
  wifi.wifiLongMessage += "\r\n";
  
  _err = wifi.sendLongMessage("SEND OK", true);
  if(_err != NO_ERROR) {
    Serial.print("E: ");
    Serial.println(String(_err));
    return false;
  }
  
  else{
    wifi.closeTCP();
  }
  return true;
}


