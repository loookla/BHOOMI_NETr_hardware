#include <Arduino.h>
#include <Wire.h>

// ================================
// PIN DEFINITIONS
// ================================

// MPU6050 I2C pins
#define SDA_PIN 8
#define SCL_PIN 9

// HC-SR04 pins
#define TRIG_PIN 5
#define ECHO_PIN 6

// MPU6050 I2C Address
#define MPU6050_ADDR 0x68


// ================================
// MPU6050 VARIABLES
// ================================

int16_t AcX, AcY, AcZ;
int16_t GyX, GyY, GyZ;
int16_t TempRaw;

float AccX, AccY, AccZ;
float GyroX, GyroY, GyroZ;
float Temperature;


// ================================
// FUNCTION DECLARATIONS
// ================================

void readMPU6050();
float readDistance();


// ================================
// SETUP
// ================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32-S3 Structural Monitoring");
  Serial.println("Initializing...");
  Serial.println("================================");


  // --------------------------------
  // HC-SR04 Setup
  // --------------------------------

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(TRIG_PIN, LOW);


  // --------------------------------
  // MPU6050 Setup
  // --------------------------------

  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("Checking MPU6050...");

  Wire.beginTransmission(MPU6050_ADDR);

  byte error = Wire.endTransmission();

  if (error == 0) {

    Serial.println("MPU6050 detected!");

    // Wake up MPU6050
    Wire.beginTransmission(MPU6050_ADDR);

    Wire.write(0x6B);  // Power management register
    Wire.write(0x00);  // Wake up

    Wire.endTransmission();

  } else {

    Serial.println("ERROR: MPU6050 not detected!");
    Serial.println("Check SDA, SCL, VCC and GND.");

  }

  delay(1000);

}


// ================================
// LOOP
// ================================

void loop() {

  // Read MPU6050
  readMPU6050();


  // Read HC-SR04 distance
  float distance = readDistance();


  // =================================
  // SERIAL OUTPUT
  // =================================

  Serial.println();
  Serial.println("----------- SENSOR DATA -----------");


  // MPU6050 ACCELERATION

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


  // MPU6050 GYROSCOPE

  Serial.print("Gyroscope X: ");
  Serial.print(GyroX, 2);
  Serial.println(" deg/s");

  Serial.print("Gyroscope Y: ");
  Serial.print(GyroY, 2);
  Serial.println(" deg/s");

  Serial.print("Gyroscope Z: ");
  Serial.print(GyroZ, 2);
  Serial.println(" deg/s");


  // TEMPERATURE

  Serial.print("MPU Temperature: ");
  Serial.print(Temperature, 2);
  Serial.println(" C");


  // HC-SR04

  Serial.print("Distance: ");

  if (distance >= 0) {

    Serial.print(distance);
    Serial.println(" cm");

  } else {

    Serial.println("Out of range / No echo");

  }


  Serial.println("-----------------------------------");


  delay(500);

}


// ================================
// READ MPU6050
// ================================

void readMPU6050() {

  Wire.beginTransmission(MPU6050_ADDR);

  Wire.write(0x3B);

  Wire.endTransmission(false);


  Wire.requestFrom(
    (uint8_t)MPU6050_ADDR,
    (size_t)14,
    true
  );


  if (Wire.available() < 14) {
    Serial.println("MPU6050 read error!");
    return;
  }


  // Acceleration

  AcX = (Wire.read() << 8) | Wire.read();
  AcY = (Wire.read() << 8) | Wire.read();
  AcZ = (Wire.read() << 8) | Wire.read();


  // Temperature

  TempRaw = (Wire.read() << 8) | Wire.read();


  // Gyroscope

  GyX = (Wire.read() << 8) | Wire.read();
  GyY = (Wire.read() << 8) | Wire.read();
  GyZ = (Wire.read() << 8) | Wire.read();


  // =================================
  // CONVERT RAW VALUES
  // =================================

  // Accelerometer range: +/- 2g

  AccX = AcX / 16384.0;
  AccY = AcY / 16384.0;
  AccZ = AcZ / 16384.0;


  // Gyroscope range: +/- 250 degrees/sec

  GyroX = GyX / 131.0;
  GyroY = GyY / 131.0;
  GyroZ = GyZ / 131.0;


  // Temperature

  Temperature = (TempRaw / 340.0) + 36.53;

}


// ================================
// READ HC-SR04 DISTANCE
// ================================

float readDistance() {

  // Ensure trigger is LOW

  digitalWrite(TRIG_PIN, LOW);

  delayMicroseconds(2);


  // Send trigger pulse

  digitalWrite(TRIG_PIN, HIGH);

  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);


  // Read echo pulse

  unsigned long duration = pulseIn(
    ECHO_PIN,
    HIGH,
    30000
  );


  // No echo received

  if (duration == 0) {

    return -1;

  }


  // Calculate distance in centimeters

  float distance = duration * 0.0343 / 2.0;


  return distance;

}