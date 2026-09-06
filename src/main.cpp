#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <math.h>

// ============================================================
// BHOOMI-NETr
// ESP32-S3 Physical Sensor Node
//
// Phase-1 data contract:
//
// {
//   "node_id": "NODE_01",
//   "timestamp": "...",
//   "tilt_x": 0.0,
//   "tilt_y": 0.0,
//   "vibration": 0.0,
//   "distance": 0.0
// }
//
// ESP32 responsibilities:
//   1. Read MPU6050
//   2. Calculate tilt
//   3. Calculate vibration metric
//   4. Read HC-SR04 distance
//   5. Send JSON to backend over Wi-Fi
//
// Backend responsibilities:
//   1. Store sensor data
//   2. Calculate displacement
//   3. Analyse risk
//   4. Generate warnings
//   5. Display dashboard
// ============================================================


// ============================================================
// NODE CONFIGURATION
// ============================================================

const char* NODE_ID = "NODE_01";


// ============================================================
// WIFI CONFIGURATION
// ============================================================
//
// DO NOT commit real credentials to GitHub.
// Replace these locally before flashing the physical ESP32.
// ============================================================

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";


// ============================================================
// SUPABASE CONFIGURATION
// ============================================================
//
// This is the same Edge Function used by the existing
// Bhoomi-NETr simulator.
//
// Replace SUPABASE_ANON_KEY locally before flashing.
//
// IMPORTANT:
// Never commit the real key to a public GitHub repository.
// ============================================================

const char* SUPABASE_ENDPOINT =
    "https://bnnrikqlhvcqpuqsujsf.supabase.co/functions/v1/sensor-data";

const char* SUPABASE_ANON_KEY =
    "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImJubnJpa3FsaHZjcXB1cXN1anNmIiwicm9sZSI6ImFub24iLCJpYXQiOjE3ODg1ODc4OTAsImV4cCI6MjEwNDE2Mzg5MH0.sUK2HXu-_2u6AaEdNiyaVb9HXS_Or1pVB58rUReYGdI";


// ============================================================
// PIN DEFINITIONS
// ============================================================

// MPU6050
#define SDA_PIN 8
#define SCL_PIN 9

// HC-SR04
#define TRIG_PIN 5
#define ECHO_PIN 6

// MPU6050 I2C address
#define MPU6050_ADDR 0x68


// ============================================================
// TIMING
// ============================================================

const unsigned long SENSOR_INTERVAL_MS = 1000;

unsigned long lastSensorTime = 0;


// ============================================================
// MPU6050 RAW DATA
// ============================================================

int16_t AcX;
int16_t AcY;
int16_t AcZ;

int16_t GyX;
int16_t GyY;
int16_t GyZ;

int16_t TempRaw;


// ============================================================
// MPU6050 CONVERTED DATA
// ============================================================

float AccX = 0.0;
float AccY = 0.0;
float AccZ = 0.0;

float GyroX = 0.0;
float GyroY = 0.0;
float GyroZ = 0.0;

float Temperature = 0.0;


// ============================================================
// DERIVED DATA
// ============================================================

float tiltX = 0.0;
float tiltY = 0.0;

float vibration = 0.0;


// ============================================================
// HC-SR04 DATA
// ============================================================

float distance = -1.0;


// ============================================================
// VIBRATION STATE
// ============================================================

float previousAccelerationMagnitude = 1.0;


// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

void connectWiFi();

bool readMPU6050();

float readDistance();

void calculateTilt();

void calculateVibration();

String getTimestamp();

void printSensorData();

void sendSensorData();


// ============================================================
// SETUP
// ============================================================

void setup() {

    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("       BHOOMI-NETr SENSOR NODE");
    Serial.println("          ESP32-S3 / PHASE 1");
    Serial.println("========================================");


    // --------------------------------------------------------
    // HC-SR04
    // --------------------------------------------------------

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    digitalWrite(TRIG_PIN, LOW);


    // --------------------------------------------------------
    // MPU6050
    // --------------------------------------------------------

    Wire.begin(SDA_PIN, SCL_PIN);

    Serial.println();
    Serial.println("Checking MPU6050...");

    Wire.beginTransmission(MPU6050_ADDR);

    byte error = Wire.endTransmission();

    if (error == 0) {

        Serial.println("MPU6050 detected.");

        // Wake MPU6050
        Wire.beginTransmission(MPU6050_ADDR);

        Wire.write(0x6B);
        Wire.write(0x00);

        Wire.endTransmission();

    } else {

        Serial.println("ERROR: MPU6050 not detected.");
        Serial.println("Check:");
        Serial.println("  SDA -> GPIO 8");
        Serial.println("  SCL -> GPIO 9");
        Serial.println("  VCC");
        Serial.println("  GND");
    }


    // --------------------------------------------------------
    // Wi-Fi
    // --------------------------------------------------------

    connectWiFi();


    // --------------------------------------------------------
    // NTP
    // --------------------------------------------------------
    //
    // Timestamp is optional in the backend, but using NTP
    // allows the physical node to provide a valid ISO timestamp.
    // --------------------------------------------------------

    configTime(
        0,
        0,
        "pool.ntp.org",
        "time.nist.gov"
    );


    Serial.println();
    Serial.println("Initialization complete.");
    Serial.println();

    lastSensorTime = millis();
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop() {

    // --------------------------------------------------------
    // Reconnect Wi-Fi if necessary
    // --------------------------------------------------------

    if (WiFi.status() != WL_CONNECTED) {

        Serial.println();
        Serial.println("Wi-Fi disconnected.");

        connectWiFi();
    }


    // --------------------------------------------------------
    // Read sensors at fixed interval
    // --------------------------------------------------------

    if (millis() - lastSensorTime >= SENSOR_INTERVAL_MS) {

        lastSensorTime = millis();


        // MPU6050
        bool mpuOK = readMPU6050();


        // HC-SR04
        distance = readDistance();


        if (mpuOK) {

            calculateTilt();

            calculateVibration();
        }


        // ----------------------------------------------------
        // Local serial output
        // ----------------------------------------------------

        printSensorData();


        // ----------------------------------------------------
        // Send data to backend
        // ----------------------------------------------------

        if (WiFi.status() == WL_CONNECTED) {

            sendSensorData();
        }
    }
}


// ============================================================
// CONNECT TO WI-FI
// ============================================================

void connectWiFi() {

    if (WiFi.status() == WL_CONNECTED) {
        return;
    }


    Serial.println();
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(WIFI_SSID);


    WiFi.mode(WIFI_STA);

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );


    unsigned long startTime = millis();


    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - startTime < 15000
    ) {

        delay(500);

        Serial.print(".");
    }


    Serial.println();


    if (WiFi.status() == WL_CONNECTED) {

        Serial.println("Wi-Fi connected.");

        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

    } else {

        Serial.println("Wi-Fi connection failed.");
        Serial.println("Sensor readings will continue locally.");
    }
}


// ============================================================
// READ MPU6050
// ============================================================

bool readMPU6050() {

    Wire.beginTransmission(MPU6050_ADDR);

    Wire.write(0x3B);

    if (Wire.endTransmission(false) != 0) {

        Serial.println("MPU6050 communication error.");

        return false;
    }


    Wire.requestFrom(
        (uint8_t)MPU6050_ADDR,
        (size_t)14,
        true
    );


    if (Wire.available() < 14) {

        Serial.println("MPU6050 data unavailable.");

        return false;
    }


    // --------------------------------------------------------
    // Accelerometer
    // --------------------------------------------------------

    AcX =
        (Wire.read() << 8) |
        Wire.read();

    AcY =
        (Wire.read() << 8) |
        Wire.read();

    AcZ =
        (Wire.read() << 8) |
        Wire.read();


    // --------------------------------------------------------
    // Temperature
    // --------------------------------------------------------

    TempRaw =
        (Wire.read() << 8) |
        Wire.read();


    // --------------------------------------------------------
    // Gyroscope
    // --------------------------------------------------------

    GyX =
        (Wire.read() << 8) |
        Wire.read();

    GyY =
        (Wire.read() << 8) |
        Wire.read();

    GyZ =
        (Wire.read() << 8) |
        Wire.read();


    // --------------------------------------------------------
    // Convert accelerometer
    // ±2g range
    // --------------------------------------------------------

    AccX = AcX / 16384.0;

    AccY = AcY / 16384.0;

    AccZ = AcZ / 16384.0;


    // --------------------------------------------------------
    // Convert gyroscope
    // ±250°/s range
    // --------------------------------------------------------

    GyroX = GyX / 131.0;

    GyroY = GyY / 131.0;

    GyroZ = GyZ / 131.0;


    // --------------------------------------------------------
    // Temperature
    // --------------------------------------------------------

    Temperature =
        (TempRaw / 340.0) +
        36.53;


    return true;
}


// ============================================================
// CALCULATE TILT
// ============================================================
//
// Accelerometer-based tilt.
// Appropriate for the Phase-1 demonstration when the node
// is relatively stationary.
//
// Units: degrees
// ============================================================

void calculateTilt() {

    tiltX =
        atan2(
            AccX,
            sqrt(
                AccY * AccY +
                AccZ * AccZ
            )
        )
        * 180.0 /
        PI;


    tiltY =
        atan2(
            AccY,
            sqrt(
                AccX * AccX +
                AccZ * AccZ
            )
        )
        * 180.0 /
        PI;
}


// ============================================================
// CALCULATE VIBRATION
// ============================================================
//
// Phase-1 demonstration metric.
//
// MPU6050 accelerometer measures gravity as well as dynamic
// acceleration. A stationary sensor has approximately 1g total
// acceleration.
//
// Therefore:
//     vibration ≈ deviation from 1g
//
// This is a prototype metric and is NOT a validated
// structural/mine-safety vibration threshold.
// ============================================================

void calculateVibration() {

    float accelerationMagnitude =
        sqrt(
            AccX * AccX +
            AccY * AccY +
            AccZ * AccZ
        );


    // Difference from static 1g condition
    float dynamicAcceleration =
        fabs(
            accelerationMagnitude -
            1.0
        );


    // Change between consecutive samples
    float sampleChange =
        fabs(
            accelerationMagnitude -
            previousAccelerationMagnitude
        );


    // Use the larger response
    vibration =
        max(
            dynamicAcceleration,
            sampleChange
        );


    // Remove tiny sensor noise
    if (vibration < 0.01) {

        vibration = 0.0;
    }


    previousAccelerationMagnitude =
        accelerationMagnitude;
}


// ============================================================
// READ HC-SR04
// ============================================================
//
// Returns distance in centimetres.
// Returns -1 if no echo is received.
// ============================================================

float readDistance() {

    digitalWrite(
        TRIG_PIN,
        LOW
    );

    delayMicroseconds(2);


    digitalWrite(
        TRIG_PIN,
        HIGH
    );

    delayMicroseconds(10);


    digitalWrite(
        TRIG_PIN,
        LOW
    );


    unsigned long duration =
        pulseIn(
            ECHO_PIN,
            HIGH,
            30000
        );


    if (duration == 0) {

        return -1.0;
    }


    float measuredDistance =
        duration *
        0.0343 /
        2.0;


    return measuredDistance;
}


// ============================================================
// GET TIMESTAMP
// ============================================================

String getTimestamp() {

    struct tm timeInfo;


    if (
        !getLocalTime(
            &timeInfo,
            1000
        )
    ) {

        // Timestamp is optional in the backend.
        return "";
    }


    char buffer[32];


    strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%dT%H:%M:%SZ",
        &timeInfo
    );


    return String(buffer);
}


// ============================================================
// SEND SENSOR DATA
// ============================================================

void sendSensorData() {

    // --------------------------------------------------------
    // Validate HC-SR04
    // --------------------------------------------------------

    if (distance < 0) {

        Serial.println(
            "Skipping server upload: invalid distance."
        );

        return;
    }


    // --------------------------------------------------------
    // Build timestamp
    // --------------------------------------------------------

    String timestamp =
        getTimestamp();


    // --------------------------------------------------------
    // Build JSON
    // --------------------------------------------------------

    String json = "{";


    json += "\"node_id\":\"";
    json += NODE_ID;
    json += "\",";


    if (timestamp.length() > 0) {

        json += "\"timestamp\":\"";
        json += timestamp;
        json += "\",";
    }


    json += "\"tilt_x\":";
    json += String(
        tiltX,
        2
    );

    json += ",";


    json += "\"tilt_y\":";
    json += String(
        tiltY,
        2
    );

    json += ",";


    json += "\"vibration\":";
    json += String(
        vibration,
        3
    );

    json += ",";


    json += "\"distance\":";
    json += String(
        distance,
        2
    );


    json += "}";


    // --------------------------------------------------------
    // Print outgoing payload
    // --------------------------------------------------------

    Serial.println();
    Serial.println(
        "========== OUTGOING JSON =========="
    );

    Serial.println(json);

    Serial.println(
        "==================================="
    );


    // --------------------------------------------------------
    // HTTPS client
    // --------------------------------------------------------
    //
    // For Phase-1 prototype testing, certificate validation
    // is disabled.
    //
    // This should NOT be considered production-grade TLS
    // configuration.
    // --------------------------------------------------------

    WiFiClientSecure client;

    client.setInsecure();


    HTTPClient http;


    if (
        !http.begin(
            client,
            SUPABASE_ENDPOINT
        )
    ) {

        Serial.println(
            "ERROR: Could not initialize HTTPS connection."
        );

        return;
    }


    // --------------------------------------------------------
    // Headers
    // --------------------------------------------------------

    http.addHeader(
        "Content-Type",
        "application/json"
    );


    http.addHeader(
        "Authorization",
        String("Bearer ") +
        SUPABASE_ANON_KEY
    );


    http.addHeader(
        "apikey",
        SUPABASE_ANON_KEY
    );


    // --------------------------------------------------------
    // POST
    // --------------------------------------------------------

    Serial.println(
        "Sending sensor data..."
    );


    int httpCode =
        http.POST(json);


    Serial.print(
        "HTTP response: "
    );

    Serial.println(httpCode);


    // --------------------------------------------------------
    // Response
    // --------------------------------------------------------

    if (httpCode > 0) {

        String response =
            http.getString();


        Serial.println(
            "Server response:"
        );

        Serial.println(response);

    } else {

        Serial.print(
            "HTTP request failed: "
        );

        Serial.println(
            http.errorToString(
                httpCode
            )
        );
    }


    http.end();
}


// ============================================================
// PRINT SENSOR DATA
// ============================================================

void printSensorData() {

    Serial.println();
    Serial.println(
        "----------- SENSOR DATA -----------"
    );


    // MPU6050
    Serial.println("MPU6050:");

    Serial.print("Acceleration X: ");
    Serial.print(AccX, 3);
    Serial.println(" g");

    Serial.print("Acceleration Y: ");
    Serial.print(AccY, 3);
    Serial.println(" g");

    Serial.print("Acceleration Z: ");
    Serial.print(AccZ, 3);
    Serial.println(" g");


    Serial.print("Gyroscope X: ");
    Serial.print(GyroX, 2);
    Serial.println(" deg/s");

    Serial.print("Gyroscope Y: ");
    Serial.print(GyroY, 2);
    Serial.println(" deg/s");

    Serial.print("Gyroscope Z: ");
    Serial.print(GyroZ, 2);
    Serial.println(" deg/s");


    // Derived values
    Serial.print("Tilt X: ");
    Serial.print(tiltX, 2);
    Serial.println(" deg");

    Serial.print("Tilt Y: ");
    Serial.print(tiltY, 2);
    Serial.println(" deg");


    Serial.print("Vibration: ");
    Serial.print(vibration, 3);
    Serial.println(" g");


    // Temperature
    Serial.print("MPU Temperature: ");
    Serial.print(Temperature, 2);
    Serial.println(" C");


    // HC-SR04
    Serial.print("Distance: ");

    if (distance >= 0) {

        Serial.print(
            distance,
            2
        );

        Serial.println(" cm");

    } else {

        Serial.println(
            "Out of range / No echo"
        );
    }


    Serial.println(
        "-----------------------------------"
    );
}