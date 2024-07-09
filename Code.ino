#include <WiFi.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <WebServer.h>

const char* ssid = "嗨";
const char* password = "qwertyuiop";

WiFiServer server(80);
String header;

const int output27 = 27;

MAX30105 particleSensor;

const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;

float beatsPerMinute;
int beatAvg;
float spo2Value;
float heartRate = 0;

unsigned long previousMillis = 0;
const long interval = 100;

String SendHTML(float BPM, float SpO2) {
  String ptr = "<!DOCTYPE html>";
  ptr += "<html>";
  ptr += "<head>";
  ptr += "<title>Pulse Oximeter ESP32</title>"; // ESP32 instead of ESP8266
  ptr += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  ptr += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.7.2/css/all.min.css'>";
  ptr += "<style>";
  ptr += "body { background-color: #fff; font-family: sans-serif; color: #333333; font: 14px Helvetica, sans-serif box-sizing: border-box;}";
  ptr += "#page { margin: 20px; background-color: #fff;}";
  ptr += ".container { height: inherit; padding-bottom: 20px;}";
  ptr += ".header { padding: 20px;}";
  ptr += ".header h1 { padding-bottom: 0.3em; color: #008080; font-size: 45px; font-weight: bold; font-family: Garmond, 'sans-serif'; text-align: center;}";
  ptr += "h2 { padding-bottom: 0.2em; border-bottom: 1px solid #eee; margin: 2px; text-align: left;}";
  ptr += ".header h3 { font-weight: bold; font-family: Arial, 'sans-serif'; font-size: 17px; color: #b6b6b6; text-align: center;}";
  ptr += ".box-full { padding: 20px; border: 1px solid #ddd; border-radius: 1em 1em 1em 1em; box-shadow: 1px 7px 7px 1px rgba(0,0,0,0.4); background: #fff; margin: 20px; width: 300px;}";
  ptr += "@media (max-width: 494px) { #page { width: inherit; margin: 5px auto; } #content { padding: 1px;} .box-full { margin: 8px 8px 12px 8px; padding: 10px; width: inherit;; float: none; } }";
  ptr += "@media (min-width: 494px) and (max-width: 980px) { #page { width: 465px; margin 0 auto; } .box-full { width: 380px; } }";
  ptr += "@media (min-width: 980px) { #page { width: 930px; margin: auto; } }";
  ptr += ".sensor { margin: 12px 0px; font-size: 2.5rem;}";
  ptr += ".sensor-labels { font-size: 1rem; vertical-align: middle; padding-bottom: 15px;}";
  ptr += ".units { font-size: 1.2rem;}";
  ptr += "hr { height: 1px; color: #eee; background-color: #eee; border: none;}";
  ptr += "@keyframes colorChange { 0% { color: #007bff; } 50% { color: #ff6347; } 100% { color: #007bff; } }";
  ptr += ".sensor-labels, .sensor { animation: colorChange 2s infinite; }";
  ptr += "</style>";
  ptr += "</head>";
  ptr += "<body>";
  ptr += "<div id='page'>";
  ptr += "<div class='header'>";
  ptr += "<h1> ESP32 WebServer </h1>"; // ESP32 instead of ESP8266
  ptr += "<h3><a href='https://iotprojectsideas.com'></a></h3>";
  ptr += "</div>";
  ptr += "<div id='content' align='center'>";
  ptr += "<div class='box-full' align='left'>";
  ptr += "<h2>Sensor Readings</h2>";
  ptr += "<div class='sensors-container'>";

  ptr += "<p class='sensor'>";
  ptr += "<i class='fas fa-burn' style='color:#f7347a'></i>";
  ptr += "<span class='sensor-labels'> BPM </span>";
  ptr += String(BPM); 
  ptr += "<sup class='units'></sup>";
  ptr += "</p>";
  
  ptr += "<p class='sensor'>";
  ptr += "<i class='fas fa-burn' style='color:#f7347a'></i>";
  ptr += "<span class='sensor-labels'> SpO2 </span>";
  ptr += String(SpO2, 2); 
  ptr += "<sup class='units'>%</sup>";
  ptr += "</p>";

  ptr += "</div>";
  ptr += "</div>";
  ptr += "</div>";
  ptr += "</div>";
  ptr += "</body>";
  ptr += "</html>";
  return ptr;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 was not found. Please check wiring/power.");
    while (1);
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");

  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);

  pinMode(output27, OUTPUT);
  digitalWrite(output27, LOW);

  WiFi.softAP(ssid, password);

  Serial.println("");
  Serial.println("AP created. IP address: ");
  Serial.println(WiFi.softAPIP());
  server.begin();
}

void loop() {
  const long timeoutTime = 2000;
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    long irValue = particleSensor.getIR();

    if (checkForBeat(irValue) == true) {
      long delta = millis() - lastBeat;
      lastBeat = millis();
      beatsPerMinute = 60 / (delta / 1000.0);

      if (beatsPerMinute < 255 && beatsPerMinute > 20) {
        rates[rateSpot++] = (byte)beatsPerMinute;
        rateSpot %= RATE_SIZE;

        beatAvg = 0;
        for (byte x = 0; x < RATE_SIZE; x++)
          beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
      }
    }

    int32_t ir_dc_value = particleSensor.getIR();
    int32_t red_dc_value = particleSensor.getRed();

    float R = (float)red_dc_value / (float)ir_dc_value;
    if (R < 0.3) {
      spo2Value = 104 - 17 * R;
    }

    heartRate = (float)irValue / 100.0;

    Serial.print("IR=");
    Serial.print(irValue);
    Serial.print(", BPM=");
    Serial.print(beatsPerMinute);
    Serial.print(", Avg BPM=");
    Serial.print(beatAvg);
    Serial.print(", HR=");
    Serial.print(heartRate);
    Serial.print(", SpO2=");
    Serial.print(spo2Value);
    Serial.print(", HR=");
    Serial.print(spo2Value - 50);

    if (irValue < 50000)
      Serial.print(" No finger?");

    Serial.println();
  }

  WiFiClient client = server.available();
  if (client) {
    Serial.println("New client");
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        header += c;
        if (c == '\n') {
          if (header.indexOf("GET /27/on") >= 0) {
            digitalWrite(output27, HIGH);
          } else if (header.indexOf("GET /27/off") >= 0) {
            digitalWrite(output27, LOW);
          }

          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println();
          client.print(SendHTML(beatsPerMinute, spo2Value));
          client.println();
          break;
        }
      }
    }
    header = "";
    client.stop();
    Serial.println("Client disconnected");
  }
}