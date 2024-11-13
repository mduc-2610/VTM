#include <WiFi.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WebServer.h>

// Pin configuration and constants
#define DHTPIN 23                 // Pin for DHT22
#define DHTTYPE DHT11             // Sensor type DHT11
#define LIGHT_PIN 25              // Pin for light sensor
#define VALVE_PIN 19
const int servoPin = 16;          // Pin for servo control
const int rotationTime = (int) 187 / 2;   // Time to rotate 90 degrees

// WiFi configuration
// const char* ssid = "Hieu Tran";             
// const char* password = "hieu123456789";      

// const char* ssid = "Khanh Van";             
// const char* password = "88888888";

const char* ssid = "Hiếu";
const char* password = "20032003";

const String baseUrl = "https://zvfd6g-3000.csb.app/";
const String statisticsEndpoint = baseUrl + "statistics";
const String servoEndpoint = baseUrl + "servo";

// Thresholds for temperature, humidity, and light
const float TEMP_THRESHOLD = 30.0;      // Temperature threshold (°C)
const float HUM_THRESHOLD = 80.0;       // Humidity threshold (%)
const int LIGHT_THRESHOLD = 500;        // Light threshold (adjust based on actual readings)

// Objects and state variables
Servo myServo;
DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

bool isAuto = false;
bool isOpen = true;
bool isWaterValveOpen = true;
bool canRotate = true;
bool isRotated = false;

void setup() {
    Serial.begin(115200);
    myServo.attach(servoPin, 500, 2500);
    dht.begin();
    pinMode(LIGHT_PIN, INPUT);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Server endpoints
    server.enableCORS();
    server.on("/test", HTTP_GET, []() {
        Serial.println("Received GET request.");
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "text/plain", "Hello from ESP32");
    });

    server.on("/api/getStatus", HTTP_GET, []() {
        String jsonData = "{\"isOpen\": " + String(isOpen) + "}";
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
        server.send(200, "text/plain", jsonData);
    });

    server.on("/api/servo", HTTP_POST, []() {
        String postBody = server.arg("plain");
        // StaticJsonDocument<200> doc;
        // DeserializationError error = deserializeJson(doc, postBody);
        // if (!error) {
        //     isOpen = doc["isOpen"];  // Get isOpen value from the JSON
        //     Serial.println("Received POST request for /api/servo:");
        //     Serial.println("isOpen: " + String(isOpen));
        //     Serial.println(postBody);
        // } else {
        //     Serial.println("Failed to parse JSON");
        // }
        isOpen = !isOpen;
        handleServoManual();
        Serial.println("Received POST request:" + String(isOpen));
        Serial.println(postBody);
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
        server.send(200, "text/plain", postBody);
    });

    server.on("/api/auto", HTTP_POST, [] () {
        isAuto = !isAuto;
        String postBody = server.arg("plain");
        Serial.println("Received POST request: " + String(isAuto));
        Serial.println(postBody);
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
        server.send(200, "text/plain", postBody);
    });

    server.on("/api/waterValve", HTTP_POST, []() {
        String postBody = server.arg("plain");
        // isWaterValveOpen = !isWaterValveOpen;
        handleValveManual();
        Serial.println("Water Valve State Changed: " + String(isWaterValveOpen));
        Serial.println(postBody);
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "text/plain", postBody);
    });

    server.begin();

    // Reset servo to initial position
    myServo.write(90);
    delay(1000);
}

void sendDataToServoAPI(bool isOpen) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(servoEndpoint);
        http.addHeader("Content-Type", "application/json");

        String jsonData = "{\"isOpen\": " + String(isOpen ? "true" : "false") + "}";
        int httpResponseCode = http.POST(jsonData);

        if (httpResponseCode > 0) {
            Serial.printf("Response Code: %d\n", httpResponseCode);
        } else {
            Serial.printf("Error sending data: %s\n", http.errorToString(httpResponseCode).c_str());
        }

        http.end();
    } else {
        Serial.println("WiFi not connected!");
    }
}

void sendDataToStatisticsAPI(float temperature, float humidity, int lightLevel) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(statisticsEndpoint);
        http.addHeader("Content-Type", "application/json");

        String jsonData = "{\"temperature\": " + String(temperature) +
                          ", \"humidity\": " + String(humidity) +
                          ", \"light\": " + String(lightLevel) + "}";
        int httpResponseCode = http.POST(jsonData);

        if (httpResponseCode > 0) {
            Serial.printf("Response Code: %d\n", httpResponseCode);
        } else {
            Serial.printf("Error sending data: %s\n", http.errorToString(httpResponseCode).c_str());
        }

        http.end();
    } else {
        Serial.println("WiFi not connected!");
    }
}

void handleServoManual() {
  Serial.println("Manual");
  if (isOpen) {
    Serial.println("Close");
    myServo.write(180);
    delay(rotationTime);
    myServo.write(90);
  }
  else {
    Serial.println("Open");
    myServo.write(0);
    delay(rotationTime);
    myServo.write(90);
  }
}

void handleValveManual() {
    Serial.println("Manual Valve Control Activated");
    if (isWaterValveOpen) {
        Serial.println("Opening Valve");
        digitalWrite(VALVE_PIN, HIGH);  
    } else {
        Serial.println("Closing Valve");
        digitalWrite(VALVE_PIN, LOW);   
    }
    // isValveOpen = !isValveOpen;
}

void loop() {
    server.handleClient();
    // Read sensor values
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    int lightLevel = analogRead(LIGHT_PIN);

    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("Error reading DHT sensor!");
        return;
    }

    // Log sensor readings
    Serial.println("----------------------------------------");
    // Serial.printf("Temperature: %.1f°C\n", temperature);
    Serial.printf("Humidity: %.1f%%\n", humidity);
    // Serial.printf("Light Level: %d\n", lightLevel);
    // Serial.printf("Can Rotate: %s\n", canRotate ? "Yes" : "No");
    // Serial.printf("Has Rotated: %s\n", isRotated ? "Yes" : "No");

    // Send data to API
    sendDataToStatisticsAPI(temperature, humidity, lightLevel);

    if (isAuto) {
        Serial.println("Threshold servo");
        if ((temperature > TEMP_THRESHOLD && humidity > HUM_THRESHOLD) && canRotate && !isRotated) {
            Serial.println("Threshold exceeded - Rotating servo");
            myServo.write(0);
            delay(rotationTime);
            sendDataToServoAPI(false);
            myServo.write(90);
            isRotated = true;
            canRotate = false;
            Serial.println("Servo rotation complete");
        } else if ((temperature < TEMP_THRESHOLD || humidity < HUM_THRESHOLD ) && !canRotate) {
            Serial.println("Below threshold - Resetting state");
            canRotate = true;
            isRotated = false;
            myServo.write(180);
            delay(rotationTime);
            sendDataToServoAPI(true);
            myServo.write(90);
        }
    }

    // if (isAuto) {
    //     Serial.println("Auto Mode Enabled - Checking Conditions");
    //     if (humidity < HUM_THRESHOLD_VALVE && !isValveOpen) {
    //         Serial.println("Humidity Below Valve Threshold - Opening Valve");
    //         handleValveManual();
    //     } else if (humidity >= HUM_THRESHOLD_VALVE && isValveOpen) {
    //         Serial.println("Humidity Above Valve Threshold - Closing Valve");
    //         handleValveManual();
    //     }
    //     // isValveOpen = !isValveOpen;
    // }
    handleValveManual();
    delay(2000);  
}
