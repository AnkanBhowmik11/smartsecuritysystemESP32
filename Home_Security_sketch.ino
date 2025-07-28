#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h> // For easy WiFi setup

// --- WebServer related defines ---
WebServer server(80);

// --- GPIO Pin Definitions ---
const int PIR_PIN = 16;       // PIR sensor output
const int REFLECTIVE_IR_PIN = 17; // Single 3-pin Reflective IR sensor output
const int BUZZER_PIN = 4;     // Passive Buzzer pin
const int LED_EXTERNAL_PIN = 23; // External LED pin

// HC-SR04 Ultrasonic Sensor pins
const int ULTRASONIC_TRIG_PIN = 13;
const int ULTRASONIC_ECHO_PIN = 12;

// --- Global Variables for Sensor States ---
volatile bool pirMotionDetected = false;
unsigned long lastMotionTriggerTime = 0;
const long PIR_COOLDOWN_MS = 5000;

volatile bool irCurrentlyDetectingObject = false; // Real-time reflective IR status
unsigned long lastReflectiveIrChangeTime = 0;
const unsigned long IR_DEBOUNCE_DELAY_MS = 100;

volatile int irObjectCount = 0; // Total objects detected by reflective IR since reset

bool ultrasonicProximityClose = false;
long ultrasonicDistanceCm = 0;
const int PROXIMITY_THRESHOLD_CM = 5; // ALERT if object is within 5 cm

// --- Alarm Latching and Control ---
bool alarmLatched = false; // NEW: Flag to keep alarm on once triggered
bool manualAlarmOverride = false; // For direct manual activation/silencing from web portal

// --- Blinking LED Variables ---
unsigned long lastBlinkTime = 0;
const long BLINK_INTERVAL_MS = 250; // Blink every 250 milliseconds (0.25 seconds)
bool ledState = LOW; // Current state of the blinking LED

// --- Function Prototypes for Web Handling ---
void handleRoot();
void handleMotionStatus();
void handleCounts();
void handleDistance();
void handleManualAlertOn();
void handleManualAlertOff();
void handleResetCounts();
void handleNotFound();

// --- Interrupt Service Routines (ISRs) ---
void IRAM_ATTR detectMotionISR() {
  unsigned long currentTime = millis();
  if (currentTime - lastMotionTriggerTime > PIR_COOLDOWN_MS) {
    pirMotionDetected = true;
    lastMotionTriggerTime = currentTime;
    Serial.println("PIR: MOTION DETECTED!");
  }
}

void IRAM_ATTR reflectiveIrISR() {
  unsigned long currentTime = millis();
  if (currentTime - lastReflectiveIrChangeTime > IR_DEBOUNCE_DELAY_MS) {
    bool currentIrState = digitalRead(REFLECTIVE_IR_PIN);
    if (currentIrState == LOW) { // Object Detected (assuming active LOW)
      if (!irCurrentlyDetectingObject) {
        irObjectCount++; // Increment count only on a new detection event
        Serial.printf("Reflective IR: Object Detected! Total Detections: %d\n", irObjectCount);
      }
      irCurrentlyDetectingObject = true;
    } else { // Object No Longer Detected (HIGH)
      irCurrentlyDetectingObject = false;
    }
    lastReflectiveIrChangeTime = currentTime;
  }
}

// --- Ultrasonic Sensor Reading Function ---
long readUltrasonicDistance() {
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH);
  long distanceCm = duration * 0.0343 / 2; // Speed of sound ~0.0343 cm/µs
  return distanceCm;
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Configure Sensor Pins
  pinMode(PIR_PIN, INPUT);
  pinMode(REFLECTIVE_IR_PIN, INPUT_PULLUP); // Use INPUT_PULLUP for reflective IR if it pulls LOW on detect
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Configure External LED Pin
  pinMode(LED_EXTERNAL_PIN, OUTPUT);
  digitalWrite(LED_EXTERNAL_PIN, LOW);

  // HC-SR04 Pins
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);

  // Attach Interrupts
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), detectMotionISR, RISING);
  attachInterrupt(digitalPinToInterrupt(REFLECTIVE_IR_PIN), reflectiveIrISR, CHANGE);

  // WiFiManager setup
  WiFiManager wm;
  // wm.resetSettings(); // Uncomment ONCE to reset WiFi settings for new setup! Then re-comment and re-upload.

  Serial.println("\nAttempting to connect to WiFi or launch config portal...");
  bool res = wm.autoConnect("ESP32_Security_AP", "password123");

  if (!res) {
    Serial.println("Failed to connect to WiFi and config portal timed out or failed.");
    Serial.println("Rebooting in 5 seconds...");
    delay(5000);
    ESP.restart();
  } else {
    Serial.println("Connected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  // --- Web Server Route Handling ---
  server.on("/", handleRoot);
  server.on("/motion_status", handleMotionStatus);
  server.on("/counts", handleCounts);
  server.on("/distance", handleDistance);
  server.on("/alertOn", handleManualAlertOn);
  server.on("/alertOff", handleManualAlertOff);
  server.on("/resetCounts", handleResetCounts);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  // Update Ultrasonic Distance Reading and condition flag
  static unsigned long lastUltrasonicReadTime = 0;
  const long ULTRASONIC_READ_INTERVAL_MS = 200;
  if (millis() - lastUltrasonicReadTime > ULTRASONIC_READ_INTERVAL_MS) {
    ultrasonicDistanceCm = readUltrasonicDistance();
    if (ultrasonicDistanceCm > 1 && ultrasonicDistanceCm <= PROXIMITY_THRESHOLD_CM) {
        if (!ultrasonicProximityClose) {
            Serial.printf("Ultrasonic: OBJECT TOO CLOSE! (%ld cm)\n", ultrasonicDistanceCm);
        }
        ultrasonicProximityClose = true;
    } else {
        ultrasonicProximityClose = false;
    }
    lastUltrasonicReadTime = millis();
  }

  // Reset PIR motion flag after cooldown period
  if (pirMotionDetected && (millis() - lastMotionTriggerTime > PIR_COOLDOWN_MS)) {
      pirMotionDetected = false;
  }

  // --- Check if the alarm trigger conditions are met ---
  // If irObjectCount is >= 1 AND PIR motion is detected AND Ultrasonic distance is close
  if (irObjectCount >= 1 && pirMotionDetected && ultrasonicProximityClose) {
      if (!alarmLatched && !manualAlarmOverride) { // Only latch if not already latched or manually overridden
          alarmLatched = true; // Set alarm to latched state
          Serial.println("--- ALL 3 CONDITIONS MET: ALARM LATCHED! ---");
      }
  }

  // --- Control buzzer and external LED based on final alarm state ---
  // The alarm is active if it's latched OR if manually overridden
  bool finalAlarmActive = alarmLatched || manualAlarmOverride;

  if (finalAlarmActive) {
      digitalWrite(BUZZER_PIN, HIGH);
      // Blinking logic for external LED
      if (millis() - lastBlinkTime >= BLINK_INTERVAL_MS) {
        lastBlinkTime = millis();
        ledState = !ledState;
        digitalWrite(LED_EXTERNAL_PIN, ledState);
      }
  } else {
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_EXTERNAL_PIN, LOW);
      ledState = LOW; // Reset LED state when alarm is off
  }

  delay(1);
}

// --- Web Server Handler Functions ---

void handleRoot() {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 Security</title>";
  html += "<style>";
  html += "body{font-family: Arial; text-align: center; margin: 0; padding: 20px; background-color: #f4f4f4;}";
  html += ".container{max-width: 500px; margin: auto; padding: 20px; border: 1px solid #ddd; border-radius: 8px; background-color: #fff; box-shadow: 0 2px 4px rgba(0,0,0,0.1);}";
  html += "h2{color: #333;}";
  html += ".status-box{background-color:#eee; padding:12px; margin-bottom:10px; border-radius:6px; text-align: left; display: flex; justify-content: space-between; align-items: center;}";
  html += ".status-label{font-weight:bold; color: #555;}";
  html += ".status-value{font-size: 1.2em; color: #007bff;}";
  html += ".motion-value.YES{color: #dc3545;} .motion-value.NO{color: #28a745;}";
  html += ".proximity-value.CLOSE{color: #dc3545;} .proximity-value.SAFE{color: #28a745;}";
  html += ".ir-value.DETECTED{color: #dc3545;} .ir-value.CLEAR{color: #28a745;}";
  html += ".button-group button{padding:10px 15px; margin:5px; background-color:#007bff; color:white; border:none; border-radius:5px; cursor:pointer; font-size:1em; transition: background-color 0.3s ease;}";
  html += ".button-group button:hover{background-color:#0056b3;}";
  html += ".reset-button{background-color:#6c757d;} .reset-button:hover{background-color:#5a6268;}";
  html += "</style>";
  html += "<script>";
  html += "function fetchData() {";
  html += "  fetch('/motion_status').then(response => response.text()).then(data => {";
  html += "    const motionValue = document.getElementById('motionValue');";
  html += "    motionValue.innerText = data === 'true' ? 'YES' : 'NO';";
  html += "    motionValue.className = 'status-value motion-value ' + (data === 'true' ? 'YES' : 'NO');";
  html += "  });";
  html += "  fetch('/counts').then(response => response.json()).then(data => {";
  html += "    document.getElementById('irObjectCount').innerText = data.irObjectCount;";
  html += "    const irStatus = document.getElementById('irStatus');";
  html += "    irStatus.innerText = data.reflectiveIrActive ? 'OBJECT DETECTED!' : 'CLEAR';";
  html += "    irStatus.className = 'status-value ir-value ' + (data.reflectiveIrActive ? 'DETECTED' : 'CLEAR');";
  html += "  });";
  html += "  fetch('/distance').then(response => response.text()).then(data => {";
  html += "    const distanceValue = document.getElementById('distanceValue');";
  html += "    distanceValue.innerText = data + ' cm';";
  html += "    const proximityStatus = document.getElementById('proximityStatus');";
  html += "    if (parseInt(data) > 0 && parseInt(data) <= " + String(PROXIMITY_THRESHOLD_CM) + ") {";
  html += "      proximityStatus.innerText = 'CLOSE!';";
  html += "      proximityStatus.className = 'status-value proximity-value CLOSE';";
  html += "    } else {";
  html += "      proximityStatus.innerText = 'SAFE';";
  html += "      proximityStatus.className = 'status-value proximity-value SAFE';";
  html += "    }";
  html += "  });";
  html += "}";
  html += "setInterval(fetchData, 1000);";
  html += "window.onload = fetchData;";
  html += "</script>";
  html += "</head><body><div class='container'>";
  html += "<h2>Home Security System</h2>";

  html += "<div class='status-box'><span class='status-label'>Motion Detected:</span> <span id='motionValue' class='status-value'>Loading...</span></div>";
  html += "<div class='status-box'><span class='status-label'>Reflective IR:</span> <span id='irStatus' class='status-value'>Loading...</span></div>";
  html += "<div class='status-box'><span class='status-label'>Objects Detected Count:</span> <span id='irObjectCount' class='status-value'>Loading...</span></div>";
  html += "<div class='status-box'><span class='status-label'>Proximity:</span> <span id='distanceValue' class='status-value'>Loading...</span> <span id='proximityStatus' class='status-value'></span></div>";

  html += "<div class='button-group'>";
  html += "<form action=\"/alertOn\" method=\"get\"><button type=\"submit\">Activate Alarm (Buzzer & LED)</button></form>";
  html += "<form action=\"/alertOff\" method=\"get\"><button type=\"submit\">Silence Alarm</button></form>";
  html += "<form action=\"/resetCounts\" method=\"get\"><button type=\"submit\" class='reset-button'>Reset IR Object Count</button></form>";
  html += "</div>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleMotionStatus() {
  server.send(200, "text/plain", pirMotionDetected ? "true" : "false");
}

void handleCounts() {
  String jsonResponse = "{\"irObjectCount\":";
  jsonResponse += irObjectCount;
  jsonResponse += ",\"reflectiveIrActive\":";
  jsonResponse += (irCurrentlyDetectingObject ? "true" : "false");
  jsonResponse += "}";
  server.send(200, "application/json", jsonResponse);
}

void handleDistance() {
  server.send(200, "text/plain", String(ultrasonicDistanceCm));
}

void handleManualAlertOn() {
    manualAlarmOverride = true; // Manual override will force the alarm ON
    alarmLatched = true; // Ensure alarm is latched if manually activated
    Serial.println("Manual Alarm Activated!");
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Alert ON");
}

void handleManualAlertOff() {
    manualAlarmOverride = false; // Turn off manual override
    alarmLatched = false; // Clear latched state
    Serial.println("Manual Alarm Silenced!");
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Alert OFF");
}

void handleResetCounts() {
    irObjectCount = 0; // Reset the count
    Serial.println("IR Object count reset via web.");
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Counts Reset");
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}