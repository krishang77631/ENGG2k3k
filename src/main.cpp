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
#define ENCODEROUTPUT 700
const int ENCODER_PIN = 22;

volatile unsigned long encoderValue = 0;
void IRAM_ATTR onEncoderRise() { encoderValue++; }

unsigned long prevMillis = 0;
const unsigned long intervalMs = 1000;

/* =========================
   Boat Sensors (ultrasonic)
   ========================= */
int  boatTriggerPins[2] = {2, 33};
int  boatEchoPins[2]    = {4, 25};
const long detectDistance = 10; // cm

/* =========================
   Traffic Lights
   ========================= */
// NOTE: currently only 1 module per side wired → iterate 1 side safely.
// If you later add the second module, set NUM_SIDES = 2 and fill the second pin values.
const int NUM_SIDES = 1;

int boatRedPins[2]    = {19}; // second slot currently uninitialised
int boatYellowPins[2] = {18};
int boatGreenPins[2]  = {5};

int roadRedPins[2]    = {32};
int roadYellowPins[2] = {13};
int roadGreenPins[2]  = {12};

/* =========================
   System State
   ========================= */
String bridgeState      = "Closed";   // Closed, Opening, Open, Closing, Stopped
String boatSensorState  = "Clear";    // Detected, Clear
String systemStatus     = "Ready";    // Ready, Moving, Stopped

/* =========================
   Helpers
   ========================= */
void setBoatLights(bool red, bool yellow, bool green) {
  for (int i = 0; i < NUM_SIDES; i++) {
    digitalWrite(boatRedPins[i],    red);
    digitalWrite(boatYellowPins[i], yellow);
    digitalWrite(boatGreenPins[i],  green);
  }
}

void setRoadLights(bool red, bool yellow, bool green) {
  for (int i = 0; i < NUM_SIDES; i++) {
    digitalWrite(roadRedPins[i],    red);
    digitalWrite(roadYellowPins[i], yellow);
    digitalWrite(roadGreenPins[i],  green);
  }
}

/* =========================
   Motion & State -> Lights (Opposite logic)
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
   PROGMEM HTML (no page reloads)
   ========================= */
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Bridge Control Panel</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; padding: 40px; background:#f7f7f7; }
    #bridge-state { font-size: 22px; margin-bottom: 20px; font-weight: bold; }
    #bridge-visual { width: 180px; height: 180px; background: #ccc; border-radius: 10px; margin: 0 auto 30px;
                     display:flex; align-items:center; justify-content:center; font-size:18px; color:#333;
                     box-shadow: 0 0 10px rgba(0,0,0,0.2); }
    button { border: none; color: white; font-size: 16px; border-radius: 20px; padding: 15px 40px; cursor: pointer; margin: 10px; }
    #toggle-btn { background-color: #79e35c; }
    #stop-btn { background-color: #ff4c4c; }
  </style>
</head>
<body>
  <h2 id="bridge-state">Bridge State: Loading...</h2>
  <div id="bridge-visual">Bridge</div>
  <button id="toggle-btn">Raise/Lower Bridge</button>
  <button id="stop-btn">Emergency Stop</button>

  <script>
    async function updateState() {
      try {
        const res = await fetch('/state');
        const state = await res.text();
        document.getElementById('bridge-state').textContent = 'Bridge State: ' + state;
        const box = document.getElementById('bridge-visual');
        if (state.includes('Open'))      box.style.backgroundColor = '#6ee86e';
        else if (state.includes('Closed')) box.style.backgroundColor = '#f36a6a';
        else if (state.includes('Opening') || state.includes('Closing') || state.includes('Moving'))
          box.style.backgroundColor = '#ffd966';
        else                              box.style.backgroundColor = '#ccc';
      } catch (e) { console.error(e); }
    }

    document.getElementById("toggle-btn").onclick = async () => {
      await fetch("/toggle");
      updateState();
    };

    document.getElementById("stop-btn").onclick = async () => {
      await fetch("/stop");
      updateState();
    };

    setInterval(updateState, 2000);
    updateState();
  </script>
</body>
</html>
)rawliteral";

/* =========================
   Web Routes
   ========================= */
void handleRoot() { server.send_P(200, "text/html", HTML_PAGE); }

void handleState() {
  server.send(200, "text/plain", bridgeState);
}

void handleToggle() {
  if (bridgeState == "Closed" && systemStatus == "Ready") {
    motorOpen();
  } else if (bridgeState == "Open" && systemStatus == "Ready") {
    motorClose();
  }
  server.send(200, "text/plain", "OK");
}

void handleStop() {
  // Immediately stop motor output for safety
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  ledcWrite(pwmChannel, 0);
  systemStatus = "Stopped";
  bridgeState  = "Stopped";
  // Make all lights RED as a clear fail-safe
  setBoatLights(true,  false, false);
  setRoadLights(true,  false, false);
  server.send(200, "text/plain", "STOPPED");
}

void setupRoutes() {
  server.on("/", handleRoot);
  server.on("/state", handleState);
  server.on("/toggle", handleToggle);
  server.on("/stop", handleStop);

  // keep your direct routes too (optional but handy)
  server.on("/open",  [](){ motorOpen();  server.send(200, "text/plain", "OPENING");  });
  server.on("/close", [](){ motorClose(); server.send(200, "text/plain", "CLOSING"); });
  server.on("/boatDetected", []() { boatSensorState = "Detected"; server.send(200, "text/plain", "Boat Detected"); });
  server.on("/boatPassed",   []() { boatSensorState = "Clear";    server.send(200, "text/plain", "Boat Cleared");  });
}

/* =========================
   Setup
   ========================= */
void setup() {
  Serial.begin(115200);

  // Encoder
  pinMode(ENCODER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN), onEncoderRise, RISING);

  // Motor setup
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(enable1Pin, OUTPUT);
  ledcSetup(pwmChannel, freq, resolution);
  ledcAttachPin(enable1Pin, pwmChannel);

  // Boat lights
  for (int i = 0; i < NUM_SIDES; i++) {
    pinMode(boatRedPins[i], OUTPUT);
    pinMode(boatYellowPins[i], OUTPUT);
    pinMode(boatGreenPins[i], OUTPUT);
  }

  // Road lights
  for (int i = 0; i < NUM_SIDES; i++) {
    pinMode(roadRedPins[i], OUTPUT);
    pinMode(roadYellowPins[i], OUTPUT);
    pinMode(roadGreenPins[i], OUTPUT);
  }

  // Initial states
  setBoatLights(true,  false, false); // Boat RED
  setRoadLights(false, false, true);  // Road GREEN

  // Boat sensors
  for (int i = 0; i < 2; i++) {
    pinMode(boatTriggerPins[i], OUTPUT);
    pinMode(boatEchoPins[i], INPUT);
  }

  // Wi-Fi / HTTP
  WiFi.softAP(ssid, password);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
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
