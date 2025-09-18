#include <WiFi.h>
#include <WebServer.h>

// ===== WiFi Setup =====
const char* ssid = "ESP32_Bridge";
const char* password = "12345678";

WebServer server(80);

// ===== Motor Pins =====
int motor1Pin1 = 27; // IN1
int motor1Pin2 = 26; // IN2
int enable1Pin = 14; // ENA (PWM)
const int pwmChannel = 0;
const int freq = 30000;
const int resolution = 8;

// ===== Boat Sensors (ultrasonic) =====
int boatTriggerPins[2] = {2, 33};
int boatEchoPins[2]    = {4, 25};
const long detectDistance = 10; // cm

// ===== Boat Traffic Lights (1 module per side) =====
int boatRedPins[2]    = {19, 32};
int boatYellowPins[2] = {18, 13};
int boatGreenPins[2]  = {5, 12};

// ===== System States =====
String bridgeState = "Closed";      // Closed, Opening, Open, Closing
String boatSensorState = "Clear";   // Detected, Clear
String systemStatus = "Ready";      // Ready, Moving

// ===== Helper Functions =====
void setBoatLights(bool red, bool yellow, bool green) {
  for (int i = 0; i < 2; i++) {
    digitalWrite(boatRedPins[i], red);
    digitalWrite(boatYellowPins[i], yellow);
    digitalWrite(boatGreenPins[i], green);
  }
}

void motorOpen() {
  systemStatus = "Moving";
  bridgeState = "Opening";
  setBoatLights(false, true, false); // yellow
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  ledcWrite(pwmChannel, 255);
  delay(5000); // 5 seconds motor
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  ledcWrite(pwmChannel, 0);
  bridgeState = "Open";
  setBoatLights(false, false, true); // green
  systemStatus = "Ready";
}

void motorClose() {
  systemStatus = "Moving";
  bridgeState = "Closing";
  setBoatLights(false, true, false); // yellow
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, HIGH);
  ledcWrite(pwmChannel, 255);
  delay(5000); // 5 seconds motor
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  ledcWrite(pwmChannel, 0);
  bridgeState = "Closed";
  setBoatLights(true, false, false); // red
  systemStatus = "Ready";
}

long readBoatDistance(int triggerPin, int echoPin) {
  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000); // timeout 30ms
  if (duration == 0) return -1; // no echo
  long distance = duration * 0.034 / 2; // cm
  return distance;
}

// ===== Web UI =====
String htmlPage() {
  String page = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Bridge Control Panel</title>";
  page += "<style>body{font-family:Arial;text-align:center;background:#f5f5f5;}button{padding:10px 20px;margin:5px;font-size:16px;} .open{background:green;color:white;}.close{background:red;color:white;}.stop{background:orange;color:white;}</style></head><body>";
  page += "<h1>Bridge Control Panel</h1>";
  page += "<h3>Status</h3>";
  page += "<p>Bridge State: <b>" + bridgeState + "</b></p>";
  page += "<p>Boat Sensor: <b>" + boatSensorState + "</b></p>";
  page += "<p>System Status: <b>" + systemStatus + "</b></p>";
  page += "<hr><h3>Bridge Controls</h3>";
  page += "<button class='open' onclick=\"location.href='/open'\">Open Bridge</button>";
  page += "<button class='stop' onclick=\"location.href='/stop'\">Stop</button>";
  page += "<button class='close' onclick=\"location.href='/close'\">Close Bridge</button>";
  page += "<hr><h3>Sensor Overrides</h3>";
  page += "<button onclick=\"location.href='/boatDetected'\">Trigger Boat Detected</button>";
  page += "<button onclick=\"location.href='/boatPassed'\">Trigger Boat Passed</button>";
  page += "</body></html>";
  return page;
}

void handleRoot() { server.send(200, "text/html", htmlPage()); }

void setupRoutes() {
  server.on("/", handleRoot);
  server.on("/open", []() { motorOpen(); server.sendHeader("Location","/"); server.send(303); });
  server.on("/close", []() { motorClose(); server.sendHeader("Location","/"); server.send(303); });
  server.on("/stop", []() { systemStatus = "Stopped"; server.sendHeader("Location","/"); server.send(303); });
  server.on("/boatDetected", []() { boatSensorState = "Detected"; server.sendHeader("Location","/"); server.send(303); });
  server.on("/boatPassed", []() { boatSensorState = "Clear"; server.sendHeader("Location","/"); server.send(303); });
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);

  // Motor pins
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(enable1Pin, OUTPUT);
  ledcSetup(pwmChannel, freq, resolution);
  ledcAttachPin(enable1Pin, pwmChannel);

  // Boat lights
  for (int i = 0; i < 2; i++) {
    pinMode(boatRedPins[i], OUTPUT);
    pinMode(boatYellowPins[i], OUTPUT);
    pinMode(boatGreenPins[i], OUTPUT);
    setBoatLights(true, false, false); // start red
  }

  // Boat sensors
  for (int i = 0; i < 2; i++) {
    pinMode(boatTriggerPins[i], OUTPUT);
    pinMode(boatEchoPins[i], INPUT);
  }

  // Start WiFi AP
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  setupRoutes();
  server.begin();
  Serial.println("HTTP server started");
}

// ===== Loop =====
void loop() {
  server.handleClient();

  // Automatic sensor-driven bridge
  bool boatDetected = false;
  for (int i = 0; i < 2; i++) {
    long dist = readBoatDistance(boatTriggerPins[i], boatEchoPins[i]);
    if (dist > 0 && dist <= detectDistance) {
      boatDetected = true;
      break;
    }
  }

  if (boatDetected && bridgeState == "Closed" && systemStatus == "Ready") {
    boatSensorState = "Detected";
    motorOpen();
    delay(2000); // short pause before closing
    motorClose();
    boatSensorState = "Clear";
  }
}
