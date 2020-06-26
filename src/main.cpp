#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TM1637Display.h>
#include <math.h>
#include <../secrets.h>

#define READINGS_PER_PUBLISH 10
#define MQTT_TOPIC "beer/temp/reading"
#define ONE_WIRE_BUS D2
#define INTERVAL 1000

#define DISPLAY_TOPIC "beer/display/#"
#define DISPLAY_BRIGHTNESS_TOPIC "beer/display/brightness"
#define DISPLAY_TOPIC_UPDATE "beer/display/topic"

#define DISPLAY_CLK D6
#define DISPLAY_DIO D5

unsigned long previousMillis = millis() + INTERVAL;
int displayNumber;
String display_topic_value;

WiFiClient espClient;
PubSubClient client(espClient);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);

void updateDisplay()
{
  display.showNumberDec(displayNumber, false, 4, 0);
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived[");
  Serial.print(topic);
  Serial.println("] ");

  String message;
  for (unsigned int i = 0; i < length; i++)
  {
    message = message + (char)payload[i];
  }

  Serial.print("Message: ");
  Serial.println(message);

  if (String(topic) == String(DISPLAY_BRIGHTNESS_TOPIC))
  {
    if (message == "off")
    {
      display.setBrightness(0, false);
    }
    else
    {
      int brightness = (int)payload[0] - '0';
      Serial.print("Setting brightness to ");
      Serial.println(brightness, true);
      display.setBrightness(brightness);
    }
  }

  if (String(topic) == display_topic_value)
  {
    displayNumber = message.toFloat() * 100;
  }

  if (String(topic) == String(DISPLAY_TOPIC_UPDATE))
  {
    Serial.print("Updating display topic: ");
    Serial.println(message);
    display_topic_value = message;
    displayNumber = 0;
  }

  updateDisplay();
}

void connectToNetwork()
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to " + String(WIFI_SSID));
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("Connected to network");
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("fermenter.temp.esp8266.1", MQTT_USERNAME, MQTT_PASSWORD))
    {
      Serial.println("connected");
      client.subscribe(DISPLAY_TOPIC, 1);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

float averageTemperature()
{
  float temperatureC = 0;
  float sum = 0;
  for (int i = 0; i < READINGS_PER_PUBLISH; i++)
  {
    temperatureC = sensors.getTempCByIndex(0);
    sum = sum + temperatureC;
  }

  temperatureC = sum / READINGS_PER_PUBLISH;

  return temperatureC;
}

void setup()
{
  Serial.begin(115200);
  Serial.println();

  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);

  display.setBrightness(4);

  connectToNetwork();

  sensors.begin();

  reconnect();
}

void preLoop()
{
  if (!client.connected())
  {
    reconnect();
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    connectToNetwork();
  }
}

void loop()
{
  preLoop();

  unsigned long currentMillis = millis();
  if (currentMillis > previousMillis + INTERVAL)
  {
    previousMillis = currentMillis;
    sensors.requestTemperatures();
    float temperatureC = averageTemperature();

    Serial.print(temperatureC);
    Serial.println("ºC");

    String payload = "{ \"temp\":" + String(temperatureC) + ", \"deviceId\": \"" + String(WiFi.macAddress()) + ".esp8266\"}";
    client.publish(MQTT_TOPIC, payload.c_str());
  }

  client.loop();
}