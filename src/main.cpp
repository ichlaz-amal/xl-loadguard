#include <Arduino.h>
#include <esp_sara_nbiot.h>
#include <esp_sara_config.h>
#include <HX711.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <Servo.h>

#define SERVO_PIN 15
#define HX711_DT 19
#define HX711_SCK 18

HX711 scale; Servo servo;
LiquidCrystal_I2C lcd(0x27, 20, 4);
esp_sara_client_handle_t *sara;
volatile bool mqtt_connected = false;

// SARA EVENT HANDLER
static esp_err_t sara_event_handle(esp_sara_event_handle_t *event)
{
  const char *TAG = "EVENT";
  esp_sara_client_handle_t *client = event->client;
  switch (event->event_id)
  {
  case SARA_EVENT_SIGNAL_FOUND:
  {
    Serial.printf("Signal found\n");
    if (event->payload_size > 0)
      Serial.printf("Signal strengh %d\n", event->payload[0]);
  }
  break;
  case SARA_EVENT_SIGNAL_NOT_FOUND:
  {
    Serial.printf("Signal not found\n");
  }
  break;
  case SARA_EVENT_ATTACHED:
  {
    Serial.printf("Atttached\n");
    if (event->payload_size > 0)
      Serial.printf("Attached %d\n", event->payload[0]);
  }
  break;
  case SARA_EVENT_DETTACHED:
  {
    Serial.printf("Dettached\n");
    if (event->payload_size > 0)
      Serial.printf("Attached %d\n", event->payload[0]);
  }
  break;
  case SARA_EVENT_MQTT_CONNECTED:
  {
    Serial.printf("MQTT_CONNECTED\n");
    mqtt_connected = true;
    esp_sara_subscribe_mqtt(client, "/test/rx", 1);
  }
  case SARA_EVENT_MQTT_DATA:
  {
    Serial.printf("MQTT_DATA\n");
    Serial.printf("topic: %s mesg: %s\n", event->topic, event->payload);
  }
  break;
  case SARA_EVENT_PUBLISHED:
  {
    Serial.printf("MQTT_PUBLISHED\n");
  }
  break;
  case SARA_EVENT_PUBLISH_FAILED:
  {
    Serial.printf("MQTT_PUBLISHED_FAILED\n");
  }
  break;
  case SARA_EVENT_MQTT_ERR:
  {
    Serial.printf("MQTT ERROR %d %d\n", *(event->payload), *((event->payload + 4)));
  }
  case SARA_EVENT_CME_ERROR:
  {
    Serial.printf("CME ERROR %s\n", (char *)event->payload);
  }
  default:
    break;
  }
  return ESP_OK;
}

void setup()
{
  Serial.begin(115200);
  lcd.begin(); lcd.backlight();

  // HX711 INIT
  scale.begin(HX711_DT, HX711_SCK);
  scale.set_scale(-96650);
  scale.tare();

  // SERVO INIT
  servo.attach(SERVO_PIN);
  servo.write(0);

  // ESP SARA INIT
  pinMode(SARA_UART_DTR_PIN, OUTPUT);
  digitalWrite(SARA_UART_DTR_PIN, LOW);
  pinMode(SARA_UART_RTS_PIN, OUTPUT);
  digitalWrite(SARA_UART_RTS_PIN, LOW);

  esp_sara_client_config_t config = {};
  config.apn = "iotxl";
  config.rat = 8;
  config.use_hex = false;
  config.transport = SARA_TRANSPORT_MQTT;
  config.event_handle = sara_event_handle;

  esp_sara_mqtt_client_config_t mqtt_config = {};
  mqtt_config.client_id = "9600630087010749";
  mqtt_config.host = "mqtt.flexiot.xl.co.id";
  mqtt_config.port = 1883;
  mqtt_config.timeout = 120;
  mqtt_config.clean_session = false;
  mqtt_config.username = "generic_brand_1679-generic_device-v1_2778";
  mqtt_config.password = "1568002849_2778";

  config.transport_config = (esp_sara_transport_config_t *)&mqtt_config;

  sara = esp_sara_client_init(&config);
  esp_sara_start(sara);
}

void loop()
{
  // GET WEIGHT
  float weight = scale.get_units();
  Serial.print("Weight : ");
  Serial.println(weight);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Weight : ");
  lcd.print(weight);

  // CHECK WEIGHT
  if (weight == 0)
  {
    servo.write(0);
  }
  else if (weight > 0.07)
  {
    servo.write(0);
    lcd.setCursor(0, 1);
    lcd.print("Overloaded");
  }
  else
  {
    servo.write(90);
    lcd.setCursor(0, 1);
    lcd.print("Welcome");
  }
  delay(500);

  // PUBLISH WEIGHT
  if (mqtt_connected)
  {
    int csq = 99;
    esp_sara_get_csq(sara, &csq);
    String message = "{\"eventName\":\"loadguard\",\"status\":\"none\",\"weight\":"+ String(weight) + ",\"m\":\"4205236004488867\"}";
    char* msg = (char*)message.c_str();
    esp_sara_publish_mqtt(sara, "/test/tx", (char *)msg, false, 1, 0);
  } delay(10000 / portTICK_PERIOD_MS);
}