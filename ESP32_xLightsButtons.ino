//CAPACATIVE TOUCH FUNCTION CODE getTouchState:
//The interrupt is executed on every touch (flag is set to true) This flag is queried in the loop() function.
//If it is true, The flag is first set to false again (the touch was pressed was finally detected). The
//time of the "successful" touch input is stored for the next comparison: sinceLastTouchT0 = millis().
//Then a function (touchDelayComp(sinceLastTouchT0) query whether enough time has passed since the last touch.
//If the function returns false, the if-query is finished and nothing happens. If the function returns true the 
//the SendData function is executed

#include <WiFi.h>
#include <WiFiManager.h>
//#include <FS.h>
#include <Arduino.h>
//#include <ArduinoJson.h>
//#include <SPIFFS.h>
#include "Configure.h"

//============[ Touch variables ] ==========
const uint8_t threshold = 30;
bool touchedT0 = false; // Touch Sensor 0 GPIO4
bool touchedT3 = false; // Touch Sensor 3 GPI015

const long touchDelay = 350; //ms
int value = 0;

volatile unsigned long sinceLastTouchT0 = 0;
volatile unsigned long sinceLastTouchT3 = 0;

bool touchDelayComp(unsigned long);

void IRAM_ATTR T0wasActivated() {
  touchedT0 = true;
}
void IRAM_ATTR T3wasActivated() {
  touchedT3 = true;
}


//==========[ Push Button variables]===========
short pressed[100]; // an array keeping state of the buttons between loops
short buttons; // number of buttons
int Processed = 0;
unsigned long elapseTime;


//==========[ WiFi Manager variables]===========
//bool shouldSaveConfig = false;

const char* host = SERVER_IP; //xSchedule server


void setup() {
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  delay(1000);

  //setupSpiffs();

  //=========[WiFi Manager config]===================
  WiFiManager wifiManager;
  //wifiManager.setSaveConfigCallback(saveConfigCallback);
  //wifiManager.resetSettings();

  wifiManager.setSTAStaticIPConfig(IPAddress(192,168,100,96), IPAddress(192,168,100,1), IPAddress(255,255,255,0));

  if (!wifiManager.autoConnect("xButtonConnectAP")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);

    ESP.restart();   // if we still have not connected restart and try all over again
    delay(5000);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //=============[ Push Button config ]==========

  // work out the number of buttons
  buttons = BUTTONS;
  if (buttons > sizeof(pressed) / sizeof(short))
  {
    buttons = sizeof(pressed) / sizeof(short);
#ifdef DEBUG
    Serial.print("Buttons limited to ");
    Serial.println(buttons);
#endif
  }
  //touch listener 
  touchAttachInterrupt(T0, T0wasActivated, threshold);
  touchAttachInterrupt(T3, T3wasActivated, threshold);

  // setup the power pin
  pinMode(POWERPIN, OUTPUT);
  digitalWrite(POWERPIN, HIGH);

  // set up the button pressed pin
  pinMode(PRESSPIN, OUTPUT);


  // setup our input pins
  for (int i = 0; i < buttons; i++)
  {
    pinMode(pins[i], INPUT_PULLUP);
    pressed[i] = LOW;
  }

  //=======[ Save params to config file ]========

  //save the custom parameters to FS
//  if (shouldSaveConfig = true) {
//    Serial.println("saving config");
//    DynamicJsonDocument doc(1024);
//
//    doc["ip"]          = WiFi.localIP().toString();
//    doc["gateway"]     = WiFi.gatewayIP().toString();
//    doc["subnet"]      = WiFi.subnetMask().toString();
//
//    File configFile = SPIFFS.open("/config.json", "w");
//    if (!configFile) {
//      Serial.println("failed to open config file for writing");
//    }
//
//    if (serializeJson(doc, configFile) == 0) {
//      Serial.println(F("Failed to write to file"));
//    }
//    configFile.close();
//    shouldSaveConfig = false;
//  }

  Serial.begin(SERIALRATE);
  delay(10);

}


void loop() {

  getButtonPress();  //Process push button event triggers

  getTouchState();  //Process touch event triggers

}



void getButtonPress() {

  if ((millis() - elapseTime) > 10) {  // a small delay helps deal with key bounce

    bool ispressed = false; // keep track if any button press so the press indicator LED can be turned on

    // check each button
    for (int i = 0; i < BUTTONS; i++)
    {
      // read it
      short v = digitalRead(pins[i]);

      // if it is pressed immediately light the led
      if (v == LOW)
      {
        digitalWrite(PRESSPIN, HIGH);
        ispressed = true;

      }

      // if the button has changed state
      if (pressed[i] != v)
      {
#ifdef DEBUG
        Serial.print("Pin changed state: ");
        Serial.print(pins[i]);
        Serial.print(" : ");
        Serial.println(v);
#endif
        if (v == LOW)
        {
          // button is newly pressed
          if (pins[i] == 12) {
            SendData("/xScheduleCommand?Command=PressButton&Parameters=Next");
            Serial.println(pins[i]);
          }
          if (pins[i] == 14) {
            SendData ("/xScheduleCommand?Command=PressButton&Parameters=Back");
            Serial.println(pins[i]);
          }
        }
        pressed[i] = v;
      }
    }

    // if no button is pressed clear the pressed LED
    if (!ispressed)
    {
      digitalWrite(PRESSPIN, LOW);
      Serial.println("Nothing");
    }
    elapseTime = millis();
  }

}

void getTouchState() {

  if (touchedT0)
  {
    touchedT0 = false;
    if (touchDelayComp(sinceLastTouchT0))
    {
#ifdef DEBUG
      //Reduce value when Touch0 is trigered
      value = value - 1;
      Serial.print("T0: "); Serial.println(value);
#endif
      SendData("/xScheduleCommand?Command=PressButton&Parameters=Next");
      sinceLastTouchT0 = millis();
    }
  }

  if (touchedT3)
  {
    touchedT3 = false;
    if (touchDelayComp(sinceLastTouchT3))
    {
#ifdef DEBUG
      //incriment values when Touch3 is triggered
      value = value + 1;
      Serial.print("T3: "); Serial.println(value);
#endif
      SendData ("/xScheduleCommand?Command=PressButton&Parameters=Back");
      sinceLastTouchT3 = millis();
    }
  }

}

bool touchDelayComp(unsigned long lastTouch)
{
  if (millis() - lastTouch < touchDelay) return false;
  return true;
}


void SendData (String urlrequest) {
  String weburl = urlrequest;
  Serial.print("connecting to ");
  Serial.println(host);

  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = WEBPORT;
  Serial.println(host + httpPort );
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }

  Serial.print("Requesting URL: ");
  Serial.println(weburl);

  // This will send the request to the server
  client.print(String("GET ") + weburl + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");
  delay(10);

  // Read all the lines of the reply from server and print them to Serial
  while (client.available()) {
    String line = client.readStringUntil('\r');
    Serial.print(line);
    int hsdev = line.indexOf('ok');
    if (hsdev > -1) {
      Serial.println("");
      Serial.println("Correct response received");
      Serial.println("");
      Processed = 1;
      Serial.println("Button press sucessfully sent");
    } else {
      Processed = 0;
    }
  }
  //json response if no failures:
  //{"result":"ok","reference":"","command":"PressButton"}
  Serial.println();
  Serial.println("closing connection");

}

//callback notifying us of the need to save config
//void saveConfigCallback () {
//  Serial.println("Should save config");
//  shouldSaveConfig = false;
//}
//
//void setupSpiffs() {
//  //clean FS, for testing
//  // SPIFFS.format();
//
//  //read configuration from FS json
//  Serial.println("mounting FS...");
//
//  if (SPIFFS.begin()) {
//    Serial.println("mounted file system");
//    if (SPIFFS.exists("/config.json")) {
//      //file exists, reading and loading
//      Serial.println("reading config file");
//      File configFile = SPIFFS.open("/config.json", "r");
//      if (configFile) {
//        Serial.println("opened config file");
//        size_t size = configFile.size();
//        // Allocate a buffer to store contents of the file.
//        std::unique_ptr<char[]> buf(new char[size]);
//
//        configFile.readBytes(buf.get(), size);
//        DynamicJsonDocument doc(1024);
//
//        DeserializationError error = deserializeJson(doc, configFile);
//        if (error) {
//          Serial.println(F("Failed to read file, using default configuration"));
//        } else {
//
//          Serial.println("\nparsed json");
//
//          if (doc["ip"]) {
//            Serial.println("setting custom ip from config");
//            strcpy(static_ip, doc["ip"]);
//            strcpy(static_gw, doc["gateway"]);
//            strcpy(static_sn, doc["subnet"]);
//            Serial.println(static_ip);
//          } else {
//            Serial.println("no custom ip in config");
//          }
//        }
//      }
//    }
//  } else {
//    Serial.println("failed to mount FS");
//  }
//  //end read
//}
