#include <Wire.h>
#include <ESP32Servo.h>
#include <Adafruit_PWMServoDriver.h>
#include <WiFi.h>
#include <WebServer.h>
#include <math.h>
 
Adafruit_PWMServoDriver driver = Adafruit_PWMServoDriver();
Servo servo16;
Servo servo17;


#define servoMIN 100
#define servoMAX 620

int speed = 50;
int mode = 0; //0 = stop, 1 = forward, 2 = backward, 3 = right, 4 = left
bool is_standing = 1;

unsigned long previousMillis = 0;
const long interval = 500;

const char *ssid = "hexa_1";
const char *password = "6655443322";

IPAddress localIP(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

String SendHTML()
{
  String html = "<!DOCTYPE html> <html>\n";
  html += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  html += "<title>Hex Pod Control</title>\n";
  html += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  html += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
  html += ".button {display: inline-block;width: 120px;background-color: #3498db;border: none;color: white;padding: 13px 15px;text-decoration: none;font-size: 25px;margin: 10px;cursor: pointer;border-radius: 4px;}\n";
  html += ".button-forward {background-color: #3498db;}\n";
  html += ".button-backward {background-color: #e74c3c;font-size: 24px}\n";
  html += ".button-left {background-color: #2ecc71;}\n";
  html += ".button-right {background-color: #f39c12;}\n";
  html += ".button-stop {background-color: #000;}\n";
  html += ".slider {width: 80%; margin: 20px auto;}\n";
  html += ".value {font-size: 20px; color: #555;}\n";
  html += "</style>\n";
  html += "</head>\n";
  html += "<body>\n";
  html += "<h1>Hex Pod Control</h1>\n";
  html += "<div class=\"button button-forward\" id=\"forward\">Forward</div>\n";
  html += "<br>\n";
  html += "<div class=\"button button-left\" id=\"left\">Left</div>";
  html += "<div class=\"button button-stop\" id=\"stop\">Stop</div> <div class=\"button button-right\" id=\"right\">Right</div>\n";
  html += "<br>\n";
  html += "<div class=\"button button-backward\" id=\"backward\">Backward</div>\n";
  html += "<br>\n";
  html += "<input type=\"range\" class=\"slider\" id=\"mySlider\" min=\"0\" max=\"100\" value=\"50\">\n";
  html += "<div class=\"value\" id=\"sliderValue\">50%</div>\n";
  html += "<script>\n";
  html += "var slider = document.getElementById('mySlider');\n";
  html += "var output = document.getElementById('sliderValue');\n";
  html += "output.innerHTML = slider.value + '%';\n";
  html += "slider.oninput = function() {\n";
  html += "  var value = this.value;\n";
  html += "  output.innerHTML = value + '%';\n";
  html += "  // Send XML request to update speed\n";
  html += "  var xhttp = new XMLHttpRequest();\n";
  html += "  xhttp.onreadystatechange = function() {\n";
  html += "    if (this.readyState == 4 && this.status == 200) {\n";
  html += "      // Parse XML response and update speed\n";
  html += "      var xmlDoc = this.responseXML;\n";
  html += "      var speedNode = xmlDoc.getElementsByTagName('speed')[0];\n";
  html += "      var updatedSpeed = speedNode.childNodes[0].nodeValue;\n";
  html += "      // Update the speed variable with the received value\n";
  html += "      speed = parseInt(updatedSpeed);\n";
  html += "    }\n";
  html += "  };\n";
  html += "  xhttp.open('GET', '/setSlider?value=' + value, true);\n";
  html += "  xhttp.send();\n";
  html += "}\n";
  html += "document.getElementById('forward').addEventListener('click', function() { sendButtonAction('forward'); });\n";
  html += "document.getElementById('backward').addEventListener('click', function() { sendButtonAction('backward'); });\n";
  html += "document.getElementById('left').addEventListener('click', function() { sendButtonAction('left'); });\n";
  html += "document.getElementById('right').addEventListener('click', function() { sendButtonAction('right'); });\n";
  html += "document.getElementById('stop').addEventListener('click', function() { sendButtonAction('stop'); });\n";
  html += "function sendButtonAction(action) {\n";
  html += "  var xhttp = new XMLHttpRequest();\n";
  html += "  xhttp.open('GET', '/' + action, true);\n";
  html += "  xhttp.send();\n";
  html += "}\n";
  html += "</script>\n";
  html += "</body>\n";
  html += "</html>\n";
  return html;
}

// Prototypes
void rotateServo(int, int, float = 1);
void forward(float=50);
void backward(float=50);
void turn_right(float=50);
void turn_left(float=50);
void stand();
void handle_rssi();
double* localization();
void handle_forward();
void handle_backward();
void handle_right();
void handle_left();
void handle_stop();
void handleSetSlider();
void onClientConnect();
void implement_mode_action();
void handle_NotFound();

void setup() {
  servo16.attach(4);
  servo17.attach(5);

  Serial.begin(9600);
  driver.begin();
  driver.setPWMFreq(60);

  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(localIP, gateway, subnet);

  server.on("/", HTTP_GET, onClientConnect);
  server.on("/forward", HTTP_GET, handle_forward);
  server.on("/backward", HTTP_GET, handle_backward);
  server.on("/right", HTTP_GET, handle_right);
  server.on("/left", HTTP_GET, handle_left);
  server.on("/stop", HTTP_GET, handle_stop); 
  server.on("/setSlider", HTTP_GET, handleSetSlider);

  delay(100);

  server.begin();
  Serial.println("server is on");

  stand();
}

struct Point {
    double x;
    double y;
};

Point trilaterate(Point p1, double d1, Point p2, double d2, Point p3, double d3) {
    Point result;

    double diff[2] = {(p2.x - p1.x, 2), (p2.y - p1.y, 2)};
    double h = sqrt(diff[0]*diff[0] + diff[1]*diff[1]);
    double i[2] = {diff[0] / h, diff[1] / h};

    double ey[2] = {p3.x - p1.x, p3.y - p1.y};
    double j = i[0]*ey[0] + i[1]*ey[1];

    double k[2] = {ey[0] - j*i[0], ey[1] - j*i[1]};
    double l = sqrt(k[0]*k[0] + k[1]*k[1]);

    
    double x = p1.x + (d1*d1 - d2*d2 + h*h) / (2*h) * i[0] + (d1*d1 - d3*d3 + l*l) / (2*l) * k[0];
    double y = p1.y + (d1*d1 - d2*d2 + h*h) / (2*h) * i[1] + (d1*d1 - d3*d3 + l*l) / (2*l) * k[1];

    result.x = x;
    result.y = y;

    return result;
}

void loop()
{
  server.handleClient();
  implement_mode_action();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) 
  {
    previousMillis = currentMillis;

    // handle_rssi();
    double* distance = localization();
    Serial.println((distance[0]));
    Serial.println((distance[1]));
    Serial.println((distance[2]));
    Serial.println((distance[3]));
  }
  Point knownPoint1 = {0, 0};
  Point knownPoint2 = {6, 0};
  Point unknownPoint = trilaterate(, distances[0], knownPoint2, distances[1], knownPoint2, distance2);

    // Print the result
    Serial.print("Unknown Point: (");
    Serial.print(unknownPoint.x);
    Serial.print(", ");
    Serial.print(unknownPoint.y);
    Serial.println(")");
}



void handle_rssi()
{
  int numNetworks = WiFi.scanNetworks();

  Serial.println("--------------------------------------------------");

  for (int i = 0; i < numNetworks; i++)
  {
    Serial.println(WiFi.SSID(i) + " | RSSI: " + String(WiFi.RSSI(i)));
  }
}


double* localization() {
  double* distance = new double[4];

  double car_1 = 0;
  double car_2 = 0;
  double car_3 = 0;
  double hexa_2 = 0;

  int numNetworks = WiFi.scanNetworks();
  Serial.println("--------------------------------------------------");
  for (int i = 0; i < numNetworks; i++) {
    Serial.println(WiFi.SSID(i) + " | RSSI: " + String(WiFi.RSSI(i)));
    if (String(WiFi.SSID(i)) == "car_1") {
      double s = (-59 - WiFi.RSSI(i)) / (10.0 * 3);
      distance[0] = pow(10.0, s);
      car_1 = distance[0];
      Serial.println("distance from " + WiFi.SSID(i) + " : " + String(distance[0]));
    } else if (String(WiFi.SSID(i)) == "car_2") {
      double s = (-59 - WiFi.RSSI(i)) / (10.0 * 3);
      distance[1] = pow(10.0, s);
      car_2 = distance[1];
      Serial.println("distance from " + WiFi.SSID(i) + " : " + String(distance[1]));
    } else if (String(WiFi.SSID(i)) == "car_3") {
      double s = (-59 - WiFi.RSSI(i)) / (10.0 * 3);
      distance[2] = pow(10.0, s);
      car_3 = distance[2];
      Serial.println("distance from " + WiFi.SSID(i) + " : " + String(distance[2]));
    } else if (String(WiFi.SSID(i)) == "hexa2") {
      double s = (-59 - WiFi.RSSI(i)) / (10.0 * 3);
      distance[3] = pow(10.0, s);
      hexa_2 = distance[3];
      Serial.println("distance from " + WiFi.SSID(i) + " : " + String(distance[3]));
    }
  }
  return distance;
}


void handle_NotFound()
{
  server.send(404, "text/plain", "Not found");
}

void onClientConnect()
{
  Serial.println("Client Connected");
  server.send(200, "text/html", SendHTML());
}

void implement_mode_action()
{
  if(mode == 0 && is_standing == 0)
  {
    stand();
    is_standing = 1;
  }
  else if(mode == 1)
  {
    forward(speed);
    is_standing = 0;
  }
  else if(mode == 2)
  {
    backward(speed);
    is_standing = 0;
  }
  else if(mode == 3)
  {
    turn_right(speed);
    is_standing = 0;
  }
  else if(mode == 4)
  {
    turn_left(speed);
    is_standing = 0;
  }
}

void handle_stop()
{
  Serial.println("Stop");
  stand();
  mode = 0;
  server.send(200, "text/xml", "<response><message>Stop</message></response>");
}

void handle_forward()
{
  Serial.println("Forward");
  mode = 1;
  forward(speed);
  server.send(200, "text/xml", "<response><message>Forward</message></response>");
}

void handle_backward()
{
  Serial.println("Backward");
  mode = 2;
  backward(speed);
  server.send(200, "text/xml", "<response><message>Backward</message></response>");
}

void handle_left()
{
  Serial.println("Left");
  mode = 4;
  turn_left(speed);
  server.send(200, "text/xml", "<response><message>Left</message></response>");
}

void handle_right()
{
  Serial.println("Right");
  mode = 3;
  turn_right(speed);
  server.send(200, "text/xml", "<response><message>Right</message></response>");
}

void handleSetSlider() {
  if (server.hasArg("value")) {
    speed = server.arg("value").toInt();
  }
  String response = "<response><speed>" + String(speed) + "</speed></response>";
  server.send(200, "text/xml", response);
}

void rotateServo(int servoNumber, int angle, float waiting) {
  if (angle >= 0 && angle <= 180) {
    if (servoNumber <= 15) {
      if (servoNumber == 0 || servoNumber == 1 || servoNumber == 5 || servoNumber == 6 || servoNumber == 7 || servoNumber == 11 || servoNumber == 12 || servoNumber == 13) {
        int PWM = map(angle, 0, 180, servoMAX, servoMIN);
        driver.setPWM(servoNumber, 0, PWM);
      } else if (servoNumber == 2 || servoNumber == 8 || servoNumber == 14) {
        int PWM = map(angle, 0, 180, servoMIN, servoMAX);
        driver.setPWM(servoNumber, 0, PWM);
      } else  // we will remove this later!
      {
        int PWM = map(angle, 0, 180, servoMIN, servoMAX);
        driver.setPWM(servoNumber, 0, PWM);
      }

    } else if (servoNumber == 16) {
      servo16.write(angle);
    } else if (servoNumber == 17) {
      servo17.write(180 - angle);
    }

    // Serial.println("Servo " + String(servoNumber) + ": " + angle);
    delay(waiting * 1000);
  }
}

void rise_femur(int leg, int angle)
{
  if (leg == 0) {
    rotateServo(1, angle - 31, 0);
  }
  if (leg == 1) {
    rotateServo(4, angle - 26, 0);
  }
  if (leg == 2) {
    rotateServo(7, angle - 26, 0);
  }
  if (leg == 3) {
    rotateServo(10, angle - 26, 0);
  }
  if (leg == 4) {
    rotateServo(13, angle - 26, 0);
  }
  if (leg == 5) {
    rotateServo(16, angle - 26, 0);
  }
}

void rise_tibia(int leg, int angle)
{
  if (leg == 0) {
    rotateServo(2, angle, 0);
  }
  if (leg == 1) {
    rotateServo(5, angle, 0);
  }
  if (leg == 2) {
    rotateServo(8, angle, 0);
  }
  if (leg == 3) {
    rotateServo(11, angle, 0);
  }
  if (leg == 4) {
    rotateServo(14, angle, 0);
  }
  if (leg == 5) {
    rotateServo(17, angle, 0);
  }
}

void rise_leg(int leg, int angle) {
  rise_femur(leg, angle);
  rise_tibia(leg, 180 - angle);
}

void stand()
{
  rise_tibia(0, 0);
  rise_tibia(1, 4);
  rise_tibia(2, 15);
  rise_tibia(3, 90);
  rise_tibia(4, 10);
  rise_tibia(5, 0);
}

void forward(float movement_speed)
{
  movement_speed = (movement_speed - 0) * (0.17 - 0.73) / (100 - 0) + 0.73;

  rise_tibia(0, 0);
  rise_tibia(1, 4);
  rise_tibia(2, 15);
  rise_tibia(3, 90);
  rise_tibia(4, 10);
  rise_tibia(5, 0);

  //----------------------------
  // Lean on other Legs
  rise_femur(1, 175);
  rise_femur(2, 165);
  rise_femur(5, 170);
  delay(50);
  //Rise
  rise_femur(0, 200);
  rise_femur(3, 200);
  rise_femur(4, 200);

  delay(movement_speed * 500);
  
  //----------------------------
  //Rotate
  rotateServo(3, 120, 0);
  rotateServo(6, 120, 0);
  rotateServo(15, 120, 0);

  //Rotate Back
  rotateServo(0, 60, 0);
  rotateServo(9, 60, 0);
  rotateServo(12, 90, 0);
  
  delay(movement_speed * 1000);

  //----------------------------
  //Stand
    rise_femur(0, 175);
    rise_femur(3, 180);
    rise_femur(4, 175);
    delay(50);
    rise_femur(1, 180);
    rise_femur(2, 175);
    rise_femur(5, 175);
    
    delay(movement_speed * 100);

  //----------------------------
  // Lean on other Legs
  rise_femur(0, 160);
  rise_femur(3, 155);
  rise_femur(4, 165);
  delay(50);
  //Rise
  rise_femur(1, 200);
  rise_femur(2, 200);
  rise_femur(5, 200);

  delay(movement_speed * 500);

  //----------------------------
  //Rotate
  rotateServo(9, 90, 0);
  delay(28);
  rotateServo(0, 100, 0);
  rotateServo(12, 120, 0);
  rise_tibia(4, 0);

  //Rotate Back
  rotateServo(3, 90, 0);
  rotateServo(6, 90, 0);
  rotateServo(15, 90, 0);

  delay(movement_speed * 1000);

  //----------------------------
  //Stand
    rise_femur(1, 180);
    rise_femur(2, 175);
    rise_femur(5, 175);
    delay(50);
    rise_femur(0, 175);
    rise_femur(3, 180);
    rise_femur(4, 175);
    
    delay(movement_speed * 100);
}

void turn_right(float movement_speed)
{
  movement_speed = (movement_speed - 0) * (0.17 - 0.73) / (100 - 0) + 0.73;

  rise_tibia(0, 0);
  rise_tibia(1, 4);
  rise_tibia(2, 15);
  rise_tibia(3, 90);
  rise_tibia(4, 10);
  rise_tibia(5, 0);

  //----------------------------
  // Lean on other Legs
  rise_femur(1, 175);
  rise_femur(2, 165);
  rise_femur(5, 170);
  delay(50);
  //Rise
  rise_femur(0, 200);
  rise_femur(3, 200);
  rise_femur(4, 200);

  delay(movement_speed * 500);
  
  //----------------------------
  //Rotate
  rotateServo(3, 60, 0); //120
  rotateServo(6, 120, 0); //120
  rotateServo(15, 60, 0);//120

  //Rotate Back
  rotateServo(0, 60, 0);
  rotateServo(9, 60, 0);
  rotateServo(12, 90, 0);
  
  delay(movement_speed * 1000);

  //----------------------------
  //Stand
    rise_femur(0, 175);
    rise_femur(3, 180);
    rise_femur(4, 175);
    delay(50);
    rise_femur(1, 180);
    rise_femur(2, 175);
    rise_femur(5, 175);
    
    delay(movement_speed * 100);

  //----------------------------
  // Lean on other Legs
  rise_femur(0, 160);
  rise_femur(3, 160);
  rise_femur(4, 165);
  delay(50);
  //Rise
  rise_femur(1, 200);
  rise_femur(2, 200);
  rise_femur(5, 200);

  delay(movement_speed * 500);

  //----------------------------
  //Rotate
  rotateServo(9, 100, 0); //90
  delay(28);
  rotateServo(0, 100, 0);
  rotateServo(12, 120, 0);
  rise_tibia(4, 0);

  //Rotate Back
  rotateServo(3, 90, 0);
  rotateServo(6, 90, 0);
  rotateServo(15, 90, 0);

  delay(movement_speed * 1000);

  //----------------------------
  //Stand
    rise_femur(1, 180);
    rise_femur(2, 175);
    rise_femur(5, 175);
    delay(50);
    rise_femur(0, 175);
    rise_femur(3, 180);
    rise_femur(4, 175);
    
    delay(movement_speed * 100); 
}

void turn_left(float movement_speed)
{
  movement_speed = (movement_speed - 0) * (0.17 - 0.73) / (100 - 0) + 0.73;

  rise_tibia(0, 0);
  rise_tibia(1, 4);
  rise_tibia(2, 15);
  rise_tibia(3, 90);
  rise_tibia(4, 10);
  rise_tibia(5, 0);

  //----------------------------
  // Lean on other Legs
  rise_femur(1, 175);
  rise_femur(2, 165);
  rise_femur(5, 170);
  delay(50);
  //Rise
  rise_femur(0, 200);
  rise_femur(3, 200);
  rise_femur(4, 200);

  delay(movement_speed * 500);
  
  //----------------------------
  //Rotate
  rotateServo(3, 120, 0); //120
  rotateServo(6, 60, 0); //120
  rotateServo(15, 120, 0);//120

  //Rotate Back
  rotateServo(0, 60, 0);
  rotateServo(9, 60, 0);
  rotateServo(12, 90, 0);
  
  delay(movement_speed * 1000);

  //----------------------------
  //Stand
    rise_femur(0, 175);
    rise_femur(3, 180);
    rise_femur(4, 175);
    delay(50);
    rise_femur(1, 180);
    rise_femur(2, 175);
    rise_femur(5, 175);
    
    delay(movement_speed * 100);

  //----------------------------
  // Lean on other Legs
  rise_femur(0, 160);
  rise_femur(3, 160);
  rise_femur(4, 165);
  delay(50);
  //Rise
  rise_femur(1, 200);
  rise_femur(2, 200);
  rise_femur(5, 200);

  delay(movement_speed * 500);

  //----------------------------
  //Rotate
  rotateServo(9, 90, 0); //90
  delay(28);
  rotateServo(0, 60, 0);
  rotateServo(12, 60, 0);
  rise_tibia(4, 0);

  //Rotate Back
  rotateServo(3, 90, 0);
  rotateServo(6, 90, 0);
  rotateServo(15, 90, 0);

  delay(movement_speed * 1000);

  //----------------------------
  //Stand
    rise_femur(1, 180);
    rise_femur(2, 175);
    rise_femur(5, 175);
    delay(50);
    rise_femur(0, 175);
    rise_femur(3, 180);
    rise_femur(4, 175);
    
    delay(movement_speed * 100); 
}

void backward(float movement_speed)
{
  movement_speed = (movement_speed - 0) * (0.17 - 0.73) / (100 - 0) + 0.73;

  rise_tibia(0, 0);
  rise_tibia(1, 4);
  rise_tibia(2, 15);
  rise_tibia(3, 90);
  rise_tibia(4, 10);
  rise_tibia(5, 0);

  //----------------------------
  // Lean on other Legs
  rise_femur(1, 175);
  rise_femur(2, 165);
  rise_femur(5, 170);
  delay(50);
  //Rise
  rise_femur(0, 200);
  rise_femur(3, 200);
  rise_femur(4, 200);

  delay(movement_speed * 500);
  
  //----------------------------
  //Rotate
  rotateServo(3, 90, 0);
  rotateServo(6, 90, 0);
  rotateServo(15, 90, 0);

  //Rotate Back
  rotateServo(0, 100, 0); //60
  rotateServo(9, 90, 0); //60
  rotateServo(12, 120, 0);//90
  
  delay(movement_speed * 1000);

  //----------------------------
  //Stand
    rise_femur(0, 175);
    rise_femur(3, 180);
    rise_femur(4, 175);
    delay(50);
    rise_femur(1, 180);
    rise_femur(2, 175);
    rise_femur(5, 175);
    
    delay(movement_speed * 100);

  //----------------------------
  // Lean on other Legs
  rise_femur(0, 160);
  rise_femur(3, 155);
  rise_femur(4, 165);
  delay(50);
  //Rise
  rise_femur(1, 200);
  rise_femur(2, 200);
  rise_femur(5, 200);

  delay(movement_speed * 500);

  //----------------------------
  //Rotate
  rotateServo(9, 60, 0); //90
  delay(28);
  rotateServo(0, 60, 0); //100
  rotateServo(12, 90, 0); //120
  rise_tibia(4, 0);

  //Rotate Back
  rotateServo(3, 120, 0);
  rotateServo(6, 120, 0);
  rotateServo(15, 120, 0);

  delay(movement_speed * 1000);

  //----------------------------
  //Stand
    rise_femur(1, 180);
    rise_femur(2, 175);
    rise_femur(5, 175);
    delay(50);
    rise_femur(0, 175);
    rise_femur(3, 180);
    rise_femur(4, 175);
    
    delay(movement_speed * 100);
}