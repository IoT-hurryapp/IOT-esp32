#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit SH110X.h>

const char* ssid = "Redmi Note 12";
const char* password = "A000GGGG";

const char* mqtt_server = "206.81.29.27";
const int mqtt_port = 1883;
const char* mqtt_user = "test";
const char* mqtt_password = "test";
const char* mqtt_topic = "sensor/data";


#define DHTPIN 32
#define MQ135PIN 34
#define DSM501APIN 14
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64


#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SAMPLE_TIME_MS 2000 //
void setup() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  Serial.begin(115200);
  dht.begin();
  pinMode(MQ135PIN, INPUT);
  pinMode(DSM501APIN, INPUT);


  

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Connecting to WiFi..."));
    display.display();
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Connected to WiFi"));
  display.display();
  Serial.println("Connected to WiFi");


  client.setServer(mqtt_server, mqtt_port);



  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Connecting to MQTT..."));
    display.display();
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("Connected to MQTT");
      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println(F("Connected to MQTT"));
      display.display();
    } else {
      Serial.println("Failed to connect, rc=");
      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.println(F("MQTT Failed to connect"));
      display.display();
      Serial.print(client.state());
      delay(2000);
    }
  }

}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  delay(1000);


  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float f = dht.readTemperature(true);



  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }


  int mq135Value = analogRead(MQ135PIN);

  // Convert concentration to AQI
  int aqi = concentrationToAQI(mq135Value);


  
  unsigned long duration = pulseIn(DSM501APIN, LOW, SAMPLE_TIME_MS * 1000);
  float ratio = duration / (SAMPLE_TIME_MS * 10.0);
  float concentration = 1.1 * pow(ratio, 3) - 3.8 * pow(ratio, 2) + 520 * ratio + 0.62;
  int dustPercentage = calculateAirQualityPercentage(concentration);
  //unsigned long duration = pulseIn(DSM501APIN, LOW);
  //unsigned long maxDuration = 200000;
 // float concentrationPercentage = (float)duration / maxDuration * 100.0;


  StaticJsonDocument<200> jsonDoc;
  jsonDoc["id"] = "1";
  jsonDoc["humidity"] = static_cast<int>(h);
  ;
  jsonDoc["temperature_c"] = static_cast<int>(t);
  jsonDoc["temperature_f"] = static_cast<int>(f);

  jsonDoc["AQI"] = aqi;

  jsonDoc["dust_percentage"] = dustPercentage;

  char jsonBuffer[512];
  serializeJson(jsonDoc, jsonBuffer);

  // Publish JSON data to MQTT
  client.publish(mqtt_topic, jsonBuffer);

  // Print data to Serial Monitor
  Serial.print("JSON Data: ");
  Serial.println(jsonBuffer);
  display.clearDisplay();
  display.setTextSize(1);               // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);  // Draw white text
  display.setCursor(0, 0);              // Start at top-left corner
       // Start at top-left corner

  // Convert values to strings and display
  display.println("Temp: " + String(t));
  display.println("Humidity: " + String(h));

  display.print("AQI: ");
  display.println(aqi); // Display AQI
  display.println("Pollution: " + String(dustPercentage)+"%");

  // Update the display with the buffer
  display.display();
  delay(SAMPLE_TIME_MS);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

int concentrationToAQI(float concentration) {
  if (concentration <= 50) return (concentration / 50) * 50;
  if (concentration <= 100) return ((concentration - 50) / 50) * 50 + 50;
  if (concentration <= 150) return ((concentration - 100) / 50) * 50 + 100;
  if (concentration <= 200) return ((concentration - 150) / 50) * 50 + 150;
  if (concentration <= 300) return ((concentration - 200) / 100) * 100 + 200;
  if (concentration <= 400) return ((concentration - 300) / 100) * 100 + 300;
  return 500;
}
const float GOOD_THRESHOLD = 35.0; // Adjust based on PM2.5 or PM10 standards
const float DANGEROUS_THRESHOLD = 10000; // Adjust based on PM2.5 or PM10 standards

// Function to calculate air quality index as a percentage
int calculateAirQualityPercentage(float concentration) {
  if (concentration <= GOOD_THRESHOLD) {
    return 0; // Good air quality
  } else if (concentration >= DANGEROUS_THRESHOLD) {
    return 100; // Dangerous air quality
  } else {
    // Linear interpolation between good and dangerous thresholds
    return map(concentration, GOOD_THRESHOLD, DANGEROUS_THRESHOLD, 0, 100);
  }
}