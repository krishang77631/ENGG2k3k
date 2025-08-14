#include "WiFi.h"
#include "WebServer.h"
#include "AsyncUDP.h"

const char *ssid = "ESP32_Bridge";
const char *password = "12345678";

WebServer server(80);
AsyncUDP udp;

// Bridge states
String bridgeState = "Closed";
String boatSensor = "Clear";
String trafficSensor = "Clear";
String systemStatus = "Ready";

// === HTML Page ===
String htmlPage() {
  String page = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Bridge Control Panel</title>";
  page += "<style>body{font-family:Arial;text-align:center;background:#f5f5f5;}button{padding:10px 20px;margin:5px;font-size:16px;}";
  page += ".open{background:green;color:white;}.close{background:red;color:white;}.stop{background:orange;color:white;}";
  page += "</style></head><body>";
  page += "<h1>Bridge Control Panel</h1>";
  page += "<h3>Status</h3>";
  page += "<p>Bridge State: <b>" + bridgeState + "</b></p>";
  page += "<p>Boat Sensor: <b>" + boatSensor + "</b></p>";
  page += "<p>Traffic Sensor: <b>" + trafficSensor + "</b></p>";
  page += "<p>System Status: <b>" + systemStatus + "</b></p>";
  page += "<hr>";
  page += "<h3>Bridge Controls</h3>";
  page += "<button class='open' onclick=\"location.href='/open'\">Open Bridge</button>";
  page += "<button class='stop' onclick=\"location.href='/stop'\">Stop</button>";
  page += "<button class='close' onclick=\"location.href='/close'\">Close Bridge</button>";
  page += "<hr>";
  page += "<h3>Sensor Overrides</h3>";
  page += "<button onclick=\"location.href='/boatDetected'\">Trigger Boat Detected</button>";
  page += "<button onclick=\"location.href='/boatPassed'\">Trigger Boat Passed</button>";
  page += "<button onclick=\"location.href='/trafficClear'\">Trigger Traffic Clear</button>";
  page += "</body></html>";
  return page;
}

// === Handlers ===
void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void setBridgeState(String state) {
  bridgeState = state;
  Serial.println("Bridge State: " + state);
}

void setBoatSensor(String state) {
  boatSensor = state;
  Serial.println("Boat Sensor: " + state);
}

void setTrafficSensor(String state) {
  trafficSensor = state;
  Serial.println("Traffic Sensor: " + state);
}

// Routes
void setupRoutes() {
  server.on("/", handleRoot);

  server.on("/open", []() {
    setBridgeState("Opening");
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/close", []() {
    setBridgeState("Closing");
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/stop", []() {
    setBridgeState("Stopped");
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/boatDetected", []() {
    setBoatSensor("Detected");
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/boatPassed", []() {
    setBoatSensor("Clear");
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/trafficClear", []() {
    setTrafficSensor("Clear");
    server.sendHeader("Location", "/");
    server.send(303);
  });
}

void setup() {
  Serial.begin(115200);

  // Start AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Start Web Server
  setupRoutes();
  server.begin();
  Serial.println("HTTP server started");

  // Start UDP server
  if (udp.listen(1234)) {
    Serial.println("UDP Listening on port 1234");
    udp.onPacket([](AsyncUDPPacket packet) {
      Serial.print("From: ");
      Serial.print(packet.remoteIP());
      Serial.print(":");
      Serial.print(packet.remotePort());
      Serial.print(", Length: ");
      Serial.print(packet.length());
      Serial.print(", Data: ");
      Serial.write(packet.data(), packet.length());
      Serial.println();
      packet.printf("Got %u bytes of data", packet.length());
    });
  }
}

void loop() {
  server.handleClient();
}
