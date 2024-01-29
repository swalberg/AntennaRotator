// Import required libraries
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266mDNS.h>
#include <TinyXML.h>
#include <WebSerial.h>
#include <WiFiUdp.h>
#include "RunningAverage.h"
#include "pages.h"



const char* hostname = "rotator";
const int MAX_UDP_PACKET_SIZE = 255;
const char* PARAM_OUTPUT = "output";
const char* PARAM_STATE = "state";
const int runningAverageSize = 25;
int desiredHeading = 0;
bool moving = false;
unsigned long startedMovingAtMillis;
const unsigned long watchdogTimeoutMillis = 90 * 1000;  // timeout in ms

WiFiUDP RLInfo;
unsigned int localUdpPort = 12060;
char incomingPacket[MAX_UDP_PACKET_SIZE + 1];
TinyXML xml;
uint8_t buffer[150];  // For XML decoding

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// The running average of the measurement, in AD units
RunningAverage runningAverage(runningAverageSize);

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  // ESP8266 wierdism... there's a shadow EEPROM we manipulate
  EEPROM.begin(256);

  // Relay control for CW/CCW
  pinMode(12, OUTPUT);
  pinMode(14, OUTPUT);

  moving = false;
  // try to connect to wireless
  WiFi.begin(getWifiSSID().c_str(), getWifiPassword().c_str());
  if (testWifi()) {
    Serial.println("Succesfully Connected!!!");
    setupOta();
    setupRoutes();
    WebSerial.begin(&server);
    WebSerial.msgCallback(receiveSerialMessage);
    server.begin();
    // Listener from RL
    RLInfo.begin(localUdpPort);
    // prime the running average
    for (int i = 0; i < runningAverageSize; i++) {
      runningAverage.addValue(analogRead(A0));
      delay(25);
    }
    // set up xml processing
    xml.init((uint8_t*)buffer, sizeof(buffer), &RL_XML_callback);

  } else {  // not configured or can't connect
    Serial.println("Turning the HotSpot On");
    launchConfiguration();
  }

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());
}

int counter = 0;
int measurementTimer = 0;

void loop() {
  counter++;
  measurementTimer++;

  if (measurementTimer > 1000) {
    measurementTimer = 0;
    // Keep a running average
    runningAverage.addValue(analogRead(A0));
    ArduinoOTA.handle();
  }
  handleRLInfo();

  if (counter > 10000) {
    counter = 0;

    if (!moving) {
      return;
    }

    // watchdog timer
    if (millis() > startedMovingAtMillis + watchdogTimeoutMillis) {
      WebSerial.println("Watchdog timer expired!");
      allStop();
      return;
    }

    int heading = potToHeading(currentAverage());

    Serial.printf("%d - %d is %d\n", heading, desiredHeading, heading - desiredHeading);
    if (abs(heading - desiredHeading) > 5) {
      if (heading > desiredHeading) {
        Serial.println("go left");
        digitalWrite(12, HIGH);
        digitalWrite(14, LOW);
      } else {
        Serial.println("go right");
        digitalWrite(12, LOW);
        digitalWrite(14, HIGH);
      }
    } else {
      allStop();
      Serial.println("All stop!");
    }
  }
}

// Web serial callback for input. Doesn't do anything for now
void receiveSerialMessage(uint8_t* data, size_t len) {
  WebSerial.println("Received Web Data...");
  String d = "";
  for (int i = 0; i < len; i++) {
    d += char(data[i]);
  }
  WebSerial.println(d);
}


// Stop the motor
void allStop() {
  digitalWrite(12, LOW);
  digitalWrite(14, LOW);
  WebSerial.println("All Stop!");
  moving = false;
}

void setupOta() {
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(hostname);

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void setupRoutes() {
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", index_html, processor);
    WebSerial.println("Root page request");
  });
  server.on("/status.json", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", status_json, processor);
  });
  // Send a GET request to <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest* request) {
    String inputMessage1;
    String inputMessage2;
    // GET input1 value on <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(PARAM_OUTPUT) && request->hasParam(PARAM_STATE)) {
      inputMessage1 = request->getParam(PARAM_OUTPUT)->value();
      inputMessage2 = request->getParam(PARAM_STATE)->value();
      digitalWrite(inputMessage1.toInt(), inputMessage2.toInt());
    } else {
      inputMessage1 = "No message sent";
      inputMessage2 = "No message sent";
    }
    Serial.print("GPIO: ");
    Serial.print(inputMessage1);
    Serial.print(" - Set to: ");
    Serial.println(inputMessage2);
    WebSerial.printf("Manual %s %s\n", inputMessage1, inputMessage2);
    request->send(200, "text/plain", "OK");
  });

  server.on("/goto", HTTP_GET, [](AsyncWebServerRequest* request) {
    String heading;
    // GET input1 value on <ESP_IP>/goto?heading=degrees
    if (request->hasParam("heading")) {
      heading = request->getParam("heading")->value();
      goTo(heading.toInt());
    } else {
      heading = "No message sent";
    }
    WebSerial.printf("heading to %s", heading);
    request->send(200, "text/plain", "OK");
  });
  server.on("/health", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "OK");
  });
}

// Sets us up to move to a particular position
void goTo(int heading) {
  WebSerial.printf("Changing heading to %d\n", heading);
  moving = true;
  startedMovingAtMillis = millis();  // Log the time we started
  desiredHeading = heading;
}


// UDP processing of port 12060
void handleRLInfo() {
  int packetSize = RLInfo.parsePacket();
  if (packetSize) {
    int len = RLInfo.read(incomingPacket, MAX_UDP_PACKET_SIZE);
    xml.reset();
    for (int i = 0; i < len; i++) {
      xml.processChar(incomingPacket[i]);
    }
  }
}

void RL_XML_callback(uint8_t statusflags, char* tagName, uint16_t tagNameLen, char* data, uint16_t dataLen) {
  if ((statusflags & STATUS_TAG_TEXT) && !strcasecmp(tagName, "/RotorCommand/TurnTo")) {
    WebSerial.printf("Received UDP Packet to change to %s\n", data);
    goTo(atoi(data));
  }
  if ((statusflags & STATUS_TAG_TEXT) && !strcasecmp(tagName, "/Contest_RotorCommand/TurnTo")) {
    WebSerial.printf("Received UDP Contest Packet to change to %s\n", data);
    goTo(atoi(data));
  }
}

// Replaces placeholders in a page with dynamic info
String processor(const String& var) {
  //Serial.println(var);
  if (var == "BUTTONPLACEHOLDER") {
    String buttons = "";
    buttons += "<h4>Moving Right</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"14\" " + outputState(14) + "><span class=\"slider\"></span></label>";
    buttons += "<h4>Moving Left</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"12\" " + outputState(12) + "><span class=\"slider\"></span></label>";
    return buttons;
  }
  if (var == "DESIRED_DEGREES") {
    return String(desiredHeading);
  }
  if (var == "MOVING") {
    return String(moving ? "Moving" : "Not Moving");
  }
  if (var == "HEADING_DEGREES") {
    if (currentAverage() < 640) {
      return String("Rotator disconnected");
    }
    return String(potToHeading(currentAverage()));
  }
  if (var == "HEADING_RAW") {
    return String(currentAverage());
  }

  return String();  // default
}

String outputState(int output) {
  if (digitalRead(output)) {
    return "checked";
  } else {
    return "";
  }
}
/*
 Returns the current heading in degrees
 */
int potToHeading(int a) {
  if (a > 816) {
    return 0;
  }
  if (a < 640) {
    return 450;
  }
  return map(a, 816, 640, 0, 450);
}

int currentAverage() {
  return runningAverage.getAverage();
}
