/*
 * ESP32/NodeMCU Test Code for Smart Lock Backend Integration
 * API-based command system (no WebSocket)
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// WiFi credentials
#include "credentials.h"
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Server configuration
const char* serverURL = SERVER_URL;
const char* hardwareId = HARDWARE_ID;

#define LED_PIN 2        // Built-in LED (GPIO2)
#define LED_PIN2 16      // Second built-in LED (GPIO16)

// Connection status
bool wifiConnected = false;
bool backendConnected = false;

// Timing variables
unsigned long lastHeartbeat = 0;
unsigned long lastStatusUpdate = 0;
unsigned long lastCommandCheck = 0;
unsigned long lastLEDUpdate = 0;
unsigned long ledBlinkTime = 0;
bool ledState = false;
const unsigned long heartbeatInterval = 30000; // 30 seconds
const unsigned long statusInterval = 30000; // 1 minute
const unsigned long commandInterval = 1000; // 5 seconds

// Lock state
bool isLocked = true;

// Function declarations
void sendHeartbeat();
void sendStatusUpdate();
void checkForCommands();
String extractCommandId(String response);
void confirmCommand(String commandId, String status);
void lockDoor();
void unlockDoor();
void updateStatusLED();
void handleSerialCommands();
void printStatus();
void runFullTest();

void setup() {
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // LED OFF initially
  digitalWrite(LED_PIN2, HIGH); // LED OFF initially
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(500);
    Serial.print(".");
  }
  
  wifiConnected = true;
  digitalWrite(LED_PIN, LOW); // LED ON when connected
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  Serial.println("ESP32 Smart Lock initialized!");
  Serial.println("API-based system - polling for commands every 5 seconds");
}

void loop() {
  if (millis() - lastHeartbeat > heartbeatInterval) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }
  
  if (millis() - lastStatusUpdate > statusInterval) {
    sendStatusUpdate();
    lastStatusUpdate = millis();
  }
  
  if (millis() - lastCommandCheck > commandInterval) {
    checkForCommands();
    lastCommandCheck = millis();
  }
  
  updateStatusLED();
  handleSerialCommands();
  
  delay(100);
}

void sendHeartbeat() {
  WiFiClient client;
  HTTPClient http;
  
  http.begin(client, String(serverURL) + "/api/hardware/heartbeat");
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"hardware_id\":\"" + String(hardwareId) + "\"}";
  int httpCode = http.POST(payload);
  
  if (httpCode == 200) {
    backendConnected = true;
    String response = http.getString();
    Serial.println("Heartbeat: " + response);
    
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
  } else {
    backendConnected = false;
    Serial.println("Heartbeat failed: " + String(httpCode));
  }
  
  http.end();
}

void sendStatusUpdate() {
  WiFiClient client;
  HTTPClient http;
  
  http.begin(client, String(serverURL) + "/api/hardware/status");
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"hardware_id\":\"" + String(hardwareId) + "\",\"is_locked\":" + (isLocked ? "true" : "false") + "}";
  int httpCode = http.POST(payload);
  
  if (httpCode == 200) {
    Serial.println("Status updated");
  } else {
    Serial.println("Status update failed: " + String(httpCode));
  }
  
  http.end();
}

void checkForCommands() {
  WiFiClient client;
  HTTPClient http;
  
  http.begin(client, String(serverURL) + "/api/hardware/command");
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"hardware_id\":\"" + String(hardwareId) + "\"}";
  int httpCode = http.POST(payload);
  
  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("Command response: " + response); // ADD THIS DEBUG LINE
    
     if (response.indexOf("unlock") > -1 && response.indexOf("command") > -1) {
      String commandId = extractCommandId(response);
      Serial.println("UNLOCK command received!");
      unlockDoor();
      confirmCommand(commandId, "completed");
    }
    else if (response.indexOf("lock") > -1 && response.indexOf("command") > -1 && response.indexOf("unlock") == -1) {
      String commandId = extractCommandId(response);
      Serial.println("LOCK command received!");
      lockDoor();
      confirmCommand(commandId, "completed");
    }
  } else {
    Serial.println("Command check failed: " + String(httpCode)); // ADD THIS
  }
  
  http.end();
}

String extractCommandId(String response) {
  int start = response.indexOf("\"command_id\":") + 13;
  int end = response.indexOf(",", start);
  if (end == -1) end = response.indexOf("}", start);
  return response.substring(start, end);
}

void confirmCommand(String commandId, String status) {
  if (commandId.length() == 0) return;
  
  WiFiClient client;
  HTTPClient http;
  
  http.begin(client, String(serverURL) + "/api/hardware/confirm");
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"command_id\":" + commandId + ",\"status\":\"" + status + "\"}";
  int httpCode = http.POST(payload);
  
  if (httpCode == 200) {
    Serial.println("Command confirmed: " + commandId);
  }
  
  http.end();
}

void lockDoor() {
  isLocked = true;
  Serial.println("🔒 Door LOCKED");
  
  for(int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  
  sendStatusUpdate();
}

void unlockDoor() {
  isLocked = false;
  Serial.println("🔓 Door UNLOCKED");
  
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  
  sendStatusUpdate();
}

void updateStatusLED() {
  unsigned long currentTime = millis();
  if (currentTime - lastLEDUpdate > 2000) {
    if (wifiConnected && backendConnected) {
      digitalWrite(LED_PIN, LOW); // LED ON = all good
    } else {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Blink = issues
    }
    lastLEDUpdate = currentTime;
  }
  
  // Lock status on LED_PIN2 - Slow blink = LOCKED, Fast blink = UNLOCKED
  if (currentTime - ledBlinkTime > (isLocked ? 1000 : 300)) {
    ledState = !ledState;
    digitalWrite(LED_PIN2, ledState ? LOW : HIGH);
    ledBlinkTime = currentTime;
  }
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    
    if (command == "lock") {
      lockDoor();
    } else if (command == "unlock") {
      unlockDoor();
    } else if (command == "status") {
      printStatus();
    } else if (command == "test") {
      runFullTest();
    }
  }
}

void printStatus() {
  Serial.println("\n=== Device Status ===");
  Serial.println("Hardware ID: " + String(hardwareId));
  Serial.println("WiFi: " + String(wifiConnected ? "✓ Connected" : "✗ Disconnected"));
  Serial.println("Backend: " + String(backendConnected ? "✓ Connected" : "✗ Disconnected"));
  Serial.println("IP Address: " + WiFi.localIP().toString());
  Serial.println("Lock Status: " + String(isLocked ? "🔒 LOCKED" : "🔓 UNLOCKED"));
  Serial.println("Server URL: " + String(serverURL));
  Serial.println("====================");
}

void runFullTest() {
  Serial.println("\n=== Running Full System Test ===");
  sendHeartbeat();
  sendStatusUpdate();
  checkForCommands();
  
  Serial.println("\nAPI-based system working!");
  Serial.println("Commands: 'status', 'lock', 'unlock', 'test'");
  Serial.println("=====================================");
}