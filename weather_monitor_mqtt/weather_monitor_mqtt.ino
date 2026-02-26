#include <Adafruit_GFX.h>       // Adafruit GFX
#include <Adafruit_ST7789.h>    // Adafruit ST7789 display
#include "Adafruit_MAX1704X.h"  // Batery chip
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h> 

#include <WiFi.h>
#include <ESP32Time.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_sleep.h>
#include "DHT.h"

#include "secrets.h"


// ------------------- DeepSleep ------------------

const int DP_time = 58 * 1000000;  // Time to wake up in Seconds to miliseconds
RTC_DATA_ATTR int first_init = 0;

const int wk_pin_button = 12;

//---------------------------- DISPLAY --------------------------------

Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(240, 135);


//---------------- WiFi Credentials ------------------

struct Network {
  const char* ssid;
  const char* password;
};

struct Network networks[] = {
  { WIFI_SSID_1, WIFI_PASSWORD_1 },
  { WIFI_SSID_2, WIFI_PASSWORD_2 },
};

RTC_DATA_ATTR int last_network = 0;

WiFiClient espClient;

int wifi_tries = 10;


//-------------- MQTT ------------------
// Parameters defined in secrets.h file

PubSubClient client(espClient);


//----------------------------- DHT22 Sensor  -----------------------------

#define DHT_PIN 13      // GPIO sensor
#define DHT_TYPE DHT22  // Sensor model

DHT dht(DHT_PIN, DHT_TYPE);


//------- Batery -----------

Adafruit_MAX17048 bat;


//------------- Time -------------

const char* ntpServer = "time1.google.com";
const long gmtOffset_sec = 1 * 3600;  //  Timezone offset in seconds
const int daylightOffset_sec = 0;


void synchronizeTime(int del) {
  // Configurar y sincronizar la hora desde un servidor NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);  // Configurar el servidor NTP
  delay(del);

  // Esperar a que se establezca la hora
  while (!time(nullptr)) {
    delay(del);
    Serial.println("Esperando sincronización de tiempo...");
  }

  Serial.println("Hora sincronizada correctamente");
}


String getFormattedDateTime() {
  // Get current time in UNIX
  time_t now = time(nullptr);

  // Convert UNIX time to a local time structure
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  char formattedTime[20];

  // Format the date and time in the format 'DD-MM-YYYY HH:MM:SS'
  snprintf(formattedTime, sizeof(formattedTime), "%04d-%02d-%02d %02d:%02d:%02d",
           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(formattedTime);
}


bool getTempAndHumd(float& temperature, float& humidity) {
  dht.begin();
  // Try to measure data
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // Verify if data is valid
  if (isnan(temp) || isnan(hum)) {
    return false;  // Error, no return anything
  } else {
    temperature = temp;
    humidity = hum;
    return true;  // Read and return success
  }
}


void WifiConnect(struct Network network) {
  WiFi.begin(network.ssid, network.password);

  Serial.print("Intentando conectar a la red ");
  Serial.print(network.ssid);

  int try_count = 0;
  while (WiFi.status() != WL_CONNECTED && try_count < wifi_tries) {
    try_count++;
    Serial.print(".");
    delay(1000);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("Wifi Conectado");

    return;  // Exit function if success
  }
  Serial.println("");
}

void detectAndConnect() {
  int cont = 0;
  for (const auto& network : networks) {
    WifiConnect(network);
    if (WiFi.status() == WL_CONNECTED) {
      last_network = cont;
      return;
    } else {
      cont++;
    }
  }

  Serial.println("");
  Serial.println("No se pudo conectar a ninguna red.");
}


void showDataOnDisplay() {
  float temperature, humidity;
  getTempAndHumd(temperature, humidity);

  // Get the formatted date and time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  String formattedDateTime = getFormattedDateTime();
  // Extract hours and minutes
  String hourMinute = formattedDateTime.substring(11, 16);

  // DISPLAY

  // Background
  canvas.fillScreen(ST77XX_BLACK);

  // Temperature
  canvas.setTextColor(ST77XX_WHITE);
  canvas.setFont(&FreeSansBold24pt7b);
  canvas.setCursor(35, 50);
  canvas.print(temperature);
  canvas.print(" ºC");

  // Humidity
  uint16_t blue = display.color565(54, 187, 233);
  canvas.setTextColor(blue);
  canvas.setFont(&FreeSans12pt7b);
  canvas.setCursor(35, 83);
  canvas.print("Humedad: ");
  canvas.setFont(&FreeSansBold12pt7b);
  canvas.print((int)humidity);
  canvas.println(" %");

  // Show hour and minutes in the bottom left corner
  canvas.setTextColor(ST77XX_WHITE);
  canvas.setFont(&FreeSansBold12pt7b);
  canvas.setCursor(10, 135 - 12);
  canvas.print(hourMinute);

  // Draw a battery indicator
  int batteryLevel = bat.cellPercent();
  int batteryHeight = map(batteryLevel, 0, 100, 0, 25);
  int batteryWidth = map(80, 0, 100, 0, 30 * 0.8);
  int batteryX = canvas.width() - batteryWidth - 10;
  int batteryY = canvas.height() - 25 - 10;
  
  int stackWidth = 11;
  int stackHeight = 6;
  int stackX = batteryX + (batteryWidth - stackWidth) / 2;
  int stackY = batteryY - stackHeight;
  canvas.fillRect(stackX, stackY, stackWidth, stackHeight, ST77XX_WHITE); // Button

  canvas.fillRect(batteryX, batteryY - 3, batteryWidth, 28, ST77XX_WHITE); // Battery outline
  canvas.fillRect(batteryX + 1, batteryY + 24 - batteryHeight, batteryWidth - 2, batteryHeight, ST77XX_GREEN); // Level

  // Show battery percentage
  canvas.setTextColor(ST77XX_WHITE);
  canvas.setFont(&FreeSansBold12pt7b);
  canvas.setCursor(batteryX - 65, batteryY + 20);
  canvas.print(batteryLevel);
  canvas.print("%");

  // Show all canvas configuration set before
  display.drawRGBBitmap(0, 0, canvas.getBuffer(), 240, 135);
  delay(5000);
}


void setup() {
  Serial.begin(115200);
  // Batery
  bat.begin();
  delay(500);
  Serial.println(" ");

  // If wake up by external reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    // Init ST7789 240x135 and parameters
    display.init(135, 240);
    display.setRotation(1);
    pinMode(TFT_BACKLITE, OUTPUT);
    digitalWrite(TFT_BACKLITE, HIGH);

    showDataOnDisplay();
  }

  // First init date and wifi setup
  String time_now;
  if (first_init == 0) {
    // Wifi
    detectAndConnect();

    // Time sync
    synchronizeTime(2000);
    time_now = getFormattedDateTime();
    Serial.println(time_now);
    first_init = 1;
  }

  time_now = getFormattedDateTime();
  if (first_init == 1 && time_now.indexOf("1970") != -1) {
    WifiConnect(networks[last_network]);

    // Time sync
    Serial.println("Intentando restablecer hora");
    synchronizeTime(2000);
  }


  // Configurar temporizador para wake-up
  esp_sleep_enable_timer_wakeup(DP_time);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)wk_pin_button, HIGH);

  // --------- READ and SEND DATA -------

  // Read sensor data
  float temperature, humidity;
  getTempAndHumd(temperature, humidity);

  // Read the battery level
  float battery = (bat.cellPercent());

  // Set the timezone and get the date(Not saved for deepsleep)
  configTime(0, 0, ntpServer); // Normalize UTC
  String currentTime;
  currentTime = getFormattedDateTime();

  DynamicJsonDocument data(256);
  data["sensor"] = MQTT_CLIENT_ID;
  data["temp"] = temperature;
  data["humd"] = humidity;
  data["date"] = currentTime;
  data["battery"] = battery;


  // Serialize the data object in JSON
  char data_sensors[256];
  serializeJson(data, data_sensors);
  Serial.println(data_sensors);

  // Wifi
  if (WiFi.status() != WL_CONNECTED) {
    WifiConnect(networks[last_network]);
  }

  if (WiFi.status() != WL_CONNECTED) {
    detectAndConnect();
  }

  if (WiFi.status() == WL_CONNECTED) {
    client.setServer(MQTT_BROKER, MQTT_PORT);

    // Publish the mqtt message
    client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS);
    if (client.connected()) {
      bool resp = client.publish(DATA_TOPIC, data_sensors, false);
      if (resp) {
        Serial.println("Mensaje publicado correctamente");
      } else {
        Serial.println("Error");
      }
    }
  }

  // Turn off Wifi
  WiFi.disconnect(true);

  Serial.println("Entrando en deep sleep...");
  esp_deep_sleep_start();
}


void loop() {
}