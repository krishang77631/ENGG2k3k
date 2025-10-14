#include <WiFi.h>
#include <WebServer.h>

/* =========================
   Wi-Fi / Web Server
   ========================= */
const char* ssid     = "ESP32_BridgeG30";
const char* password = "LeSyedJames";
WebServer server(80);

/* =========================
   Motor (L298N)
   ========================= */
const int motor1Pin1 = 27;   // IN1
const int motor1Pin2 = 26;   // IN2
const int enable1Pin = 14;   // ENA (PWM)

const int pwmChannel  = 0;
const int freq        = 30000;
const int resolution  = 8;

/* =========================
   Encoder (FIT0186)
   ========================= */
// FIT0186 gearbox encoder: ~700 pulses per output-shaft revolution (A rising)
#define ENCODEROUTPUT 700
const int ENCODER_PIN = 22;  // Channel A wire from encoder

volatile unsigned long encoderValue = 0;  // pulses counted in current window
void IRAM_ATTR onEncoderRise() { encoderValue++; }

// 1-second window
unsigned long prevMillis = 0;
const unsigned long intervalMs = 1000;

/* =========================
   Boat Sensors (ultrasonic)
   ========================= */
int  boatTriggerPins[2] = {2, 33};
int  boatEchoPins[2]    = {4, 25};
const long detectDistance = 10; // cm

/* =========================
   Boat Traffic Lights (modules per side)
   ========================= */
int boatRedPins[2]    = {19};
int boatYellowPins[2] = {18};
int boatGreenPins[2]  = {5};

/* =========================
   Road Traffic Lights (modules per side: RED/YELLOW/GREEN)
   =========================
   Give the ROAD lights their own pins so they can act opposite to the BOAT lights.
*/
int roadRedPins[2]    = {32};
int roadYellowPins[2] = {13};
int roadGreenPins[2]  = {12}; // If RX0 causes issues, change '3' to another spare output

/* =========================
   System State
   ========================= */
String bridgeState      = "Closed";   // Closed, Opening, Open, Closing
String boatSensorState  = "Clear";    // Detected, Clear
String systemStatus     = "Ready";    // Ready, Moving, Stopped

/* =========================
   Helpers
   ========================= */
void setBoatLights(bool red, bool yellow, bool green) {
  for (int i = 0; i < 2; i++) {
    digitalWrite(boatRedPins[i],    red);
    digitalWrite(boatYellowPins[i], yellow);
    digitalWrite(boatGreenPins[i],  green);
  }
}

void setRoadLights(bool red, bool yellow, bool green) {
  for (int i = 0; i < 2; i++) {
    digitalWrite(roadRedPins[i],    red);
    digitalWrite(roadYellowPins[i], yellow);
    digitalWrite(roadGreenPins[i],  green);
  }
}

/* =========================
   Motion & State -> Lights (Opposite logic)
   - Opening/Closing: Boat = YELLOW, Road = RED
   - Open:            Boat = GREEN,  Road = RED
   - Closed:          Boat = RED,    Road = GREEN
   ========================= */
void motorOpen() {
  systemStatus = "Moving";
  bridgeState  = "Opening";

  setBoatLights(false, true,  false); // Boat YELLOW while moving
  setRoadLights(true,  false, false); // Road RED while moving

  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  ledcWrite(pwmChannel, 255);
  delay(5000);
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  ledcWrite(pwmChannel, 0);

  bridgeState  = "Open";
  setBoatLights(false, false, true);  // Boat GREEN when open
  setRoadLights(true,  false, false); // Road stays RED when open
  systemStatus = "Ready";
}

void motorClose() {
  systemStatus = "Moving";
  bridgeState  = "Closing";

  setBoatLights(false, true,  false); // Boat YELLOW while moving
  setRoadLights(true,  false, false); // Road RED while moving

  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, HIGH);
  ledcWrite(pwmChannel, 255);
  delay(5000);
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  ledcWrite(pwmChannel, 0);

  bridgeState  = "Closed";
  setBoatLights(true,  false, false); // Boat RED when closed
  setRoadLights(false, false, true);  // Road GREEN when closed
  systemStatus = "Ready";
}

long readBoatDistance(int triggerPin, int echoPin) {
  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return -1;
  long distance = duration * 0.034f / 2.0f;
  return distance;
}

/* =========================
   Web UI
   ========================= */
String htmlPage() {
  String page = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Bridge Control Panel</title>";
  page += "<style>body{font-family:Arial;text-align:center;background:#f5f5f5;}button{padding:10px 20px;margin:5px;font-size:16px;} .open{background:green;color:white;}.close{background:red;color:white;}.stop{background:orange;color:white;}</style></head><body>";
  page += "<h1>Bridge Control Panel</h1>";
  page += "<h3>Status</h3>";
  page += "<p>Bridge State: <b>" + bridgeState + "</b></p>";
  page += "<p>Boat Sensor: <b>" + boatSensorState + "</b></p>";
  page += "<p>System Status: <b>" + systemStatus + "</b></p>";
  // Indicators from current outputs
  page += "<p>Boat Lights: <b>" 
       + String((digitalRead(boatRedPins[0]) || digitalRead(boatRedPins[1])) ? "RED" :
                (digitalRead(boatYellowPins[0]) || digitalRead(boatYellowPins[1])) ? "YELLOW" : "GREEN")
       + "</b></p>";
  page += "<p>Road Lights: <b>" 
       + String((digitalRead(roadRedPins[0]) || digitalRead(roadRedPins[1])) ? "RED" :
                (digitalRead(roadYellowPins[0]) || digitalRead(roadYellowPins[1])) ? "YELLOW" : "GREEN")
       + "</b></p>";
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
  server.on("/stop", []() { 
    systemStatus = "Stopped";
    // Immediately stop motor output for safety
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, LOW);
    ledcWrite(pwmChannel, 0);
    server.sendHeader("Location","/");
    server.send(303); 
  });
  server.on("/boatDetected", []() { boatSensorState = "Detected"; server.sendHeader("Location","/"); server.send(303); });
  server.on("/boatPassed",   []() { boatSensorState = "Clear";    server.sendHeader("Location","/"); server.send(303); });
}

/* =========================
   Setup
   ========================= */
void setup() {
  Serial.begin(115200);

  // Encoder (Channel A only)
  pinMode(ENCODER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN), onEncoderRise, RISING);

  // Motor setup
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
  }

  // Road lights
  for (int i = 0; i < 2; i++) {
    pinMode(roadRedPins[i], OUTPUT);
    pinMode(roadYellowPins[i], OUTPUT);
    pinMode(roadGreenPins[i], OUTPUT);
  }

  // Initial states: bridge Closed → road GREEN, boat RED
  setBoatLights(true,  false, false); // Boat RED
  setRoadLights(false, false, true);  // Road GREEN

  // Boat sensors
  for (int i = 0; i < 2; i++) {
    pinMode(boatTriggerPins[i], OUTPUT);
    pinMode(boatEchoPins[i], INPUT);
  }

  // Wi-Fi / HTTP
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  setupRoutes();
  server.begin();
  Serial.println("HTTP server started");

  encoderValue = 0;
  prevMillis = millis();
}

/* =========================
   Loop
   ========================= */
void loop() {
  server.handleClient();

  // --- RPM calculation every 1s ---
  unsigned long now = millis();
  if (now - prevMillis >= intervalMs) {
    prevMillis = now;

    unsigned long pulses = encoderValue;
    encoderValue = 0;

    float rpm = (float)pulses * 60.0f / ENCODEROUTPUT;
    Serial.print(pulses);
    Serial.print(" pulses / ");
    Serial.print(ENCODEROUTPUT);
    Serial.print(" PPR x 60 = ");
    Serial.print(rpm, 1);
    Serial.println(" RPM");
  }

  // --- Automatic boat detection ---
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
    delay(5000);
    motorClose();
    boatSensorState = "Clear";
  }
}
