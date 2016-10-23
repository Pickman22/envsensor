#include <stdio.h>
#include "SparkFunBME280.h"
#include "ESP8266WiFi.h"
#include "PubSubClient.h"
#include "Ticker.h"
#include "network_info.h"

/*** WiFi definitions ***/
const char  network_name[] = NETWORK_NAME;
const char network_pass[] = NETWORK_PASSWORD;
WiFiClient wifi_client;

/*** Application definitions ***/
#define JSON_BUF_SIZE 128
#define UPDATE_RATE 600.
#define HANDLE_CONNECTION_RATE 5.
const uint8_t trying_pattern = 0x0F;
const uint8_t success_pattern = 0x2A;
char json_body[JSON_BUF_SIZE] = "{\"id\":\"atm_data\",\"reg\":\"Wloo-Iowa\",\"t\":%d.%d,\"h\":%d.%d,\"p\":%d.%d, \"v\":%d}";

Ticker publishTicker;
Ticker connectionTicker;

ADC_MODE(ADC_VCC);

/*** MQTT Definitions ***/
const char mqtt_client_id[] = "ElRobotista";
const char mqtt_server[] = "iot.eclipse.org";
const uint32_t mqtt_port =  1883;
const char mqtt_topic[] = "feedback_systems_sensors";
PubSubClient mqtt_client(wifi_client);

/*** Hardware definitions ***/
const uint8_t LED = 5;
BME280 sensor;

void heartbeat(void)
{
  static uint8_t tmp;
  digitalWrite(LED, tmp);
  tmp ^= 0x01;
}

#ifndef LOW_ENERGY_MODE
void flash_pattern(uint8_t stat)
{
    for(uint8_t tmp = 0; tmp < 8; tmp++) {
    digitalWrite(LED, stat & (0x01 << tmp));
    delay(50);
  }
}
#else
void flash_pattern(uint8_t stat) {
  /* Do not flash led to save energy. */
  return;
}
#endif

void connect_wifi(void)
{
  uint8_t tmp = 0;
  WiFi.begin(network_name, network_pass);
  while (WiFi.status() != WL_CONNECTED) {
     flash_pattern(trying_pattern);
  }
  flash_pattern(success_pattern);
}

void connect_mqtt(void)
{
  if(WiFi.status() != WL_CONNECTED) {
    return;
  }
  while(!mqtt_client.connected()) {
    flash_pattern(trying_pattern);
    if(mqtt_client.connect(mqtt_client_id)) {
      flash_pattern(success_pattern);
      mqtt_client.publish(mqtt_topic, "Initializing system...");
    }
  }
}

void configure_sensor(void)
{
  flash_pattern(trying_pattern);
  sensor.settings.commInterface = I2C_MODE;
  sensor.settings.I2CAddress = 0x77;
  sensor.settings.runMode = NORMAL_MODE;
  sensor.settings.filter = 0;
  sensor.settings.tempOverSample = 1;
  sensor.settings.pressOverSample = 1;
  sensor.settings.humidOverSample = 1;
  sensor.settings.tStandby = STANDBY_1000_MS;
  sensor.begin();
  flash_pattern(success_pattern);
}

void int_frac_from_float(float val, int32_t* _int, uint32_t* _frac)
{
  *_int = int32_t(val);
  *_frac = uint32_t(100 * (val - *_int));
}

void read_sensor(BME280* sens, float* temp, float* humid, float* pres)
{
  *temp = sens->readTempC();
  *humid = sens->readFloatHumidity();
  *pres = sens->readFloatPressure() / 100.;
}

void publish_sensor_data(void)
{
  float temp, humid, pres;
  char json_char[JSON_BUF_SIZE] = {0};
  int32_t temp_integer, humid_integer, pres_integer;
  uint32_t temp_fraction, humid_fraction, pres_fraction;
  uint32_t vcc;

  if((WiFi.status() != WL_CONNECTED) || !mqtt_client.connected()) {
    return;
  }

  digitalWrite(LED, HIGH);
  read_sensor(&sensor, &temp, &humid, &pres);
  vcc = ESP.getVcc();

  int_frac_from_float(temp, &temp_integer, &temp_fraction);
  int_frac_from_float(pres, &pres_integer, &pres_fraction);
  int_frac_from_float(humid, &humid_integer, &humid_fraction);

  sprintf(json_char, json_body, temp_integer, temp_fraction, humid_integer, humid_fraction, pres_integer, pres_fraction, vcc);
  mqtt_client.publish(mqtt_topic, json_char);
  digitalWrite(LED, LOW);
}

void handle_connection(void)
{
  if(WiFi.status() != WL_CONNECTED) {
    connect_wifi();
  }
  if(!mqtt_client.connected()) {
    connect_mqtt();
  }
  if((WiFi.status() == WL_CONNECTED) && mqtt_client.connected()) {
    mqtt_client.loop();
  }
}

void setup(void)
{
  pinMode(LED, OUTPUT);
  connect_wifi();
  mqtt_client.setServer(mqtt_server, mqtt_port);
  connect_mqtt();
  configure_sensor();
  publish_sensor_data();
  publishTicker.attach(UPDATE_RATE, publish_sensor_data);
  connectionTicker.attach(HANDLE_CONNECTION_RATE, handle_connection);
}

void loop(void) {}
