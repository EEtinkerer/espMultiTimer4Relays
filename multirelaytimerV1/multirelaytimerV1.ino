#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <time.h>

#define RLY1_PIN 16
#define RLY2_PIN 5
#define RLY3_PIN 4

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PWORD";
const char* hostName = "RelayTimers";
//uncomment for static IP
//IPAddress local_IP(192, 168, 1, 184);
//IPAddress gateway(192, 168, 1, 1);
//IPAddress subnet(255, 255, 255, 0);
//IPAddress primaryDNS(8, 8, 8, 8);
//IPAddress secondaryDNS(8, 8, 4, 4);

AsyncWebServer server(80);

struct Timer {
  int hourOn;
  int minuteOn;
  int hourOff;
  int minuteOff;
  bool state;
};

Timer timers[3] = {
  {0, 0, 0, 0, false},
  {0, 0, 0, 0, false},
  {0, 0, 0, 0, false}
};

bool manualRelay[3] = {false, false, false};
int relayPins[3] = {RLY1_PIN, RLY2_PIN, RLY3_PIN};

void saveSettings() {
  DynamicJsonDocument doc(1024);
  for (int i = 0; i < 3; i++) {
    JsonObject timer = doc.createNestedObject("timer" + String(i + 1));
    timer["hourOn"] = timers[i].hourOn;
    timer["minuteOn"] = timers[i].minuteOn;
    timer["hourOff"] = timers[i].hourOff;
    timer["minuteOff"] = timers[i].minuteOff;
    timer["state"] = timers[i].state;
    timer["manual"] = manualRelay[i];
  }
  File file = SPIFFS.open("/timers.json", "w");
  if (!file) {
    //Serial.println("Failed to open timers.json for writing");
    return;
  }
  serializeJson(doc, file);
  file.close();
  //Serial.println("Settings saved to SPIFFS");
}

void loadSettings() {
  File file = SPIFFS.open("/timers.json", "r");
  if (!file) {
    //Serial.println("Failed to open timers.json for reading, using default settings");
    saveSettings();  // Save default settings if file doesn't exist
    return;
  }
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    //Serial.println("Failed to parse timers.json, using default settings");
    saveSettings();  // Save default settings if parsing fails
    return;
  }
  for (int i = 0; i < 3; i++) {
    JsonObject timer = doc["timer" + String(i + 1)];
    timers[i].hourOn = timer["hourOn"];
    timers[i].minuteOn = timer["minuteOn"];
    timers[i].hourOff = timer["hourOff"];
    timers[i].minuteOff = timer["minuteOff"];
    timers[i].state = timer["state"];
    manualRelay[i] = timer["manual"];
  }
  file.close();
  //Serial.println("Settings loaded from SPIFFS");
  for (int i = 0; i < 3; i++) {
    //Serial.printf("Timer %d settings: ON %02d:%02d, OFF %02d:%02d, State %s, Manual %s\n", 
        ///          i, timers[i].hourOn, timers[i].minuteOn, timers[i].hourOff, timers[i].minuteOff, 
            //     timers[i].state ? "Enabled" : "Disabled", manualRelay[i] ? "ON" : "OFF");
  }
}

void setup() {
  Serial.begin(115200);
  //Serial.flush();
  //Serial.println("\n\n---Starting Main Sketch---");

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    //Serial.println("STA Failed to configure");
  }
  
  WiFi.hostname(hostName);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  if (!SPIFFS.begin()) {
    //Serial.println("Failed to mount file system");
    return;
  } else {
    //Serial.println("SPIFFS mounted successfully");
  }

  loadSettings();

  for (int i = 0; i < 3; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }
  //Serial.println("Output pins set to low");

  configTime(-8 * 3600, 1 * 3600, "pool.ntp.org", "time.nist.gov");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", "text/html");
    //Serial.println("Served index.html");
  });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    String message = "";
    if (request->hasParam("timer")) {
      int timerId = request->getParam("timer")->value().toInt();
      if (timerId >= 0 && timerId < 3) {
        timers[timerId].hourOn = request->getParam("hourOn")->value().toInt();
        timers[timerId].minuteOn = request->getParam("minuteOn")->value().toInt();
        timers[timerId].hourOff = request->getParam("hourOff")->value().toInt();
        timers[timerId].minuteOff = request->getParam("minuteOff")->value().toInt();
        timers[timerId].state = request->getParam("state")->value() == "true";
        saveSettings();
        message = "Timer settings saved.";
        //Serial.printf("Timer %d settings updated: ON %02d:%02d, OFF %02d:%02d, State %s\n", 
          //            timerId, timers[timerId].hourOn, timers[timerId].minuteOn, 
            //          timers[timerId].hourOff, timers[timerId].minuteOff, 
                      timers[timerId].state ? "Enabled" : "Disabled");
      } else {
        message = "Invalid timer ID.";
        //Serial.println("Invalid timer ID.");
      }
    } else if (request->hasParam("manual")) {
      int relayId = request->getParam("manual")->value().toInt();
      if (relayId >= 0 && relayId < 3) {
        manualRelay[relayId] = request->getParam("state")->value() == "true";
        digitalWrite(relayPins[relayId], manualRelay[relayId] ? HIGH : LOW);
        saveSettings();
        message = "Manual relay control saved.";
        //Serial.printf("Manual control for Relay %d: %s\n", relayId, manualRelay[relayId] ? "ON" : "OFF");
      } else {
        message = "Invalid relay ID.";
        //Serial.println("Invalid relay ID.");
      }
    }
    request->send(200, "text/plain", message);
  });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(1024);
    for (int i = 0; i < 3; i++) {
      JsonObject timer = doc.createNestedObject("timer" + String(i + 1));
      timer["hourOn"] = timers[i].hourOn;
      timer["minuteOn"] = timers[i].minuteOn;
      timer["hourOff"] = timers[i].hourOff;
      timer["minuteOff"] = timers[i].minuteOff;
      timer["state"] = timers[i].state;
      doc["manual" + String(i + 1)] = manualRelay[i];
      doc["status" + String(i + 1)] = digitalRead(relayPins[i]) == HIGH ? "ON" : "OFF";
    }
    //Serial.println("Settings data prepared for sending.");
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  server.begin();
  //Serial.println("HTTP server started");
}

void loop() {
  static unsigned long lastCheckTime = 0;
  const unsigned long checkInterval = 1000; // Check every second

  unsigned long currentMillis = millis();
  if (currentMillis - lastCheckTime >= checkInterval) {
    lastCheckTime = currentMillis;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      int currentHour = timeinfo.tm_hour;
      int currentMinute = timeinfo.tm_min;

     // Serial.printf("Current Time: %02d:%02d\n", currentHour, currentMinute);

      for (int i = 0; i < 3; i++) {
        if (!manualRelay[i] && timers[i].state) {
          bool shouldTurnOn = (currentHour > timers[i].hourOn || 
                               (currentHour == timers[i].hourOn && currentMinute >= timers[i].minuteOn)) &&
                              (currentHour < timers[i].hourOff || 
                               (currentHour == timers[i].hourOff && currentMinute <= timers[i].minuteOff));

          //Serial.printf("Checking Timer %d: Current Time %02d:%02d, ON %02d:%02d, OFF %02d:%02d, Should Turn On: %s\n", 
                        //i, currentHour, currentMinute, timers[i].hourOn, timers[i].minuteOn, timers[i].hourOff, timers[i].minuteOff, 
                        //shouldTurnOn ? "Yes" : "No");
                        
          digitalWrite(relayPins[i], shouldTurnOn ? HIGH : LOW);
          //Serial.printf("Relay %d turned %s by timer\n", i, shouldTurnOn ? "ON" : "OFF");
        }
      }
    } else {
      //Serial.println("Failed to obtain time");
    }
  }
}
