#include <Wire.h>
#include <ESP32Servo.h>
#include <Adafruit_PWMServoDriver.h>
Adafruit_PWMServoDriver driver = Adafruit_PWMServoDriver();
Servo servo16;
Servo servo17;

#define servoMIN 100
#define servoMAX 600

void rotateServo(int, int, int = 500);

void setup() {
  servo16.attach(3);
  servo17.attach(5);

  Serial.begin(9600);
  driver.begin();
  driver.setPWMFreq(60);
}

void loop() {
  rotateServo(11, 0, 3);
  rotateServo(11, 180, 3);
}

void rotateServo(int servoNumber, int angle, int waiting) {
  if (angle >= 0 && angle <= 180) {
    if (servoNumber <= 15) {
      int PWM = map(angle, 0, 180, servoMIN, servoMAX);
      driver.setPWM(servoNumber, 0, PWM);
    } else if (servoNumber == 16) {
      servo16.write(angle + 5);
    } else if (servoNumber == 17) {
      servo17.write(angle);
    }

    Serial.println("Servo " + String(servoNumber) + ": " + angle);
    delay(waiting * 1000);
  }
}