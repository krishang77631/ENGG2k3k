#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

#define SERVO_PIN 13   // Servo control pin (change if needed)
#define LED_BUILTIN 2   // Optional onboard LED

const char* ssid = "Kogan_8E73_2.4G";
const char* password = "bZmRXZrZprZrt742";

WebServer server(80);
Servo bridgeServo;

bool servoOpen = false;  // servo state

void handleRoot() {
  String html = "<html><body style='text-align:center;font-family:sans-serif;'>";
  html += "<h1>ESP32 Bridge Control</h1>";
  html += "<button onclick=\"location.href='/on'\" style='font-size:20px;padding:10px;'>Open Bridge</button>";
  html += "<button onclick=\"location.href='/off'\" style='font-size:20px;padding:10px;margin-left:10px;'>Close Bridge</button>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleOn() {
  servoOpen = true;
  bridgeServo.write(90);  // Open position
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleOff() {
  servoOpen = false;
  bridgeServo.write(0);   // Closed position
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);

  bridgeServo.attach(SERVO_PIN);
  bridgeServo.write(0);  // start closed

  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  int retryCount = 0;
  const int maxRetries = 30;
  while (WiFi.status() != WL_CONNECTED && retryCount < maxRetries) {
    delay(500);
    Serial.print(".");
    retryCount++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Failed to connect to Wi-Fi.");
  }

  server.on("/", handleRoot);
  server.on("/on", handleOn);
  server.on("/off", handleOff);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
