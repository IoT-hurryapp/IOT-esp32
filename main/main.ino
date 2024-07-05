#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "DHT.h"


const char* ssid = "Redmi Note 12";
const char* password = "A000GGGG";

const char* mqtt_server = "46.101.128.142";
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

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(MQ135PIN, INPUT);
  pinMode(DSM501APIN, INPUT);
  
 
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);


  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("Connected to MQTT");
    } else {
      Serial.print("Failed to connect, rc=");
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

  delay(2000);

 
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float f = dht.readTemperature(true);

  
  
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }


  int mq135Value = analogRead(MQ135PIN);

  unsigned long duration = pulseIn(DSM501APIN, LOW);
  float concentration = duration / 1000.0;

 
  StaticJsonDocument<200> jsonDoc;
  jsonDoc["id"] = "1";
  jsonDoc["humidity"] = static_cast<int>(h);;
  jsonDoc["temperature_c"] = static_cast<int>(t);
  jsonDoc["temperature_f"] = static_cast<int>(f);

  jsonDoc["mq135_value"] = mq135Value;
  
  jsonDoc["dust_concentration"] = concentration;

  char jsonBuffer[512];
  serializeJson(jsonDoc, jsonBuffer);

  // Publish JSON data to MQTT
  client.publish(mqtt_topic, jsonBuffer);

  // Print data to Serial Monitor
  Serial.print("JSON Data: ");
  Serial.println(jsonBuffer);
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
