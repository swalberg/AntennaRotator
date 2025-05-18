String foundNetworks;

String getWifiSSID() {
  String ssid = "";
  for (int i = 0; i < 32; ++i) {
    ssid += char(EEPROM.read(i));
  }
  Serial.print("SSID: ");
  Serial.println(ssid);
  return ssid;
}

String getWifiPassword() {
  String password = "";
  for (int i = 32; i < 96; ++i) {
    password += char(EEPROM.read(i));
  }
  Serial.print("Password: ");
  Serial.println(password);
  return password;
}

// If there's no value (default 255) in the first position, we've never been configured
bool isConfigured() {
  return EEPROM.read(0) != 255;
}

bool testWifi(void) {
  int c = 0;
  if (!isConfigured()) {
    return false;
  }

  Serial.println("Waiting for Wifi to connect");
  while (c < 20) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connect timed out, opening AP");
  return false;
}

void launchConfiguration() {
  inSetupMode = true;
  createWebServer();
  server.begin();
  setupAP();
}

void setupAP(void) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.printf("There are %d networks\n", n);
  if (n == 0) {
    Serial.println("no networks found");
    foundNetworks = "No networks found";
  } else {
    foundNetworks = "<ol>";
    for (int i = 0; i < n; i++) {
      // Print SSID and RSSI for each network found
      foundNetworks += "<li>";
      foundNetworks += WiFi.SSID(i);
      foundNetworks += " (";
      foundNetworks += WiFi.RSSI(i);

      foundNetworks += ")";
      foundNetworks += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
      foundNetworks += "</li>";
    }
    foundNetworks += "</ol>";
  }

  delay(100);
  WiFi.softAP("rotator", "");
}

void createWebServer() {

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    IPAddress ip = WiFi.softAPIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);

    String content;

    content = "<!DOCTYPE HTML>\r\n<html>Hello from ESP8266 at ";
    content += ipStr;
    content += "!!!<p>";
    content += foundNetworks;
    content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";
    content += "</html>";
    request->send(200, "text/html", content);
  });

  server.on("/setting", HTTP_GET, [](AsyncWebServerRequest* request) {
    String qsid;
    String qpass;

    if (request->hasParam("ssid") && request->hasParam("pass")) {
      qsid = request->getParam("ssid")->value();
      qpass = request->getParam("pass")->value();


      Serial.println("clearing eeprom");
      for (int i = 0; i < 96; ++i) {
        EEPROM.write(i, 0);
      }
      Serial.println(qsid);
      Serial.println("");
      Serial.println(qpass);
      Serial.println("");

      Serial.println("writing eeprom ssid:");
      for (int i = 0; i < qsid.length(); i++) {
        EEPROM.write(i, qsid[i]);
        Serial.print("Wrote: ");
        Serial.println(qsid[i]);
      }
      Serial.println("writing eeprom pass:");
      for (int i = 0; i < qpass.length(); i++) {
        EEPROM.write(32 + i, qpass[i]);
        Serial.print("Wrote: ");
        Serial.println(qpass[i]);
      }
      EEPROM.commit();
      request->send(200, "application/json", "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}");
      ESP.reset();
    } else {
      request->send(409, "application/json", "{\"Error\":\"Missing a ssid and password\"}");
    }
  });
}
