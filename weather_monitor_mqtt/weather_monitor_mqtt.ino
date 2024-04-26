#include <Adafruit_GFX.h>    // Adafruit GFX
#include <Adafruit_ST7789.h> // Adafruit ST7789 display
#include "Adafruit_MAX1704X.h" // Batery chip
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>

#include <WiFi.h>
#include <ESP32Time.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

#include <secrets.h>

//---------------------------- DISPLAY --------------------------------

Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(240, 135);


//---------------- WiFi Credentials ------------------

struct Network {
  const char* ssid;
  const char* password;
};

struct Network networks[] = {
  {WIFI_SSID_1, WIFI_PASSWORD_1},
  {WIFI_SSID_2, WIFI_PASSWORD_2},
};

WiFiClient espClient;


//----------------------------- DHT22 Sensor  -----------------------------

#define DHT_PIN 13     // GPIO sensor
#define DHT_TYPE DHT22 // Sensor model

DHT dht(DHT_PIN, DHT_TYPE);


//------- Batery -----------

Adafruit_MAX17048 bat;


//------------- Time -------------

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 2*3600;
const int daylightOffset_sec = 0;


void synchronizeTime() {
  // Configurar y sincronizar la hora desde un servidor NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); // Configurar el servidor NTP 

  // Esperar a que se establezca la hora
  while (!time(nullptr)) {
    delay(1000);
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

  // Format the date and time in the format 'DD-MM-YYYY HH:MM:SS'
  char formattedTime[20];
  snprintf(formattedTime, sizeof(formattedTime), "%02d-%02d-%04d %02d:%02d:%02d",
           timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  return String(formattedTime);
}


bool getTempAndHumd(float &temperature, float &humidity) {
  dht.begin();
  // Try to measure data
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // Verify if data is valid
  if (isnan(temp) || isnan(hum)) {
    return false; // Error, no return anything
  } else {
    temperature = temp;
    humidity = hum;
    return true; // Read and return success
  }
}


void detectAndConect() {
  for (const auto& network : networks) {
    WiFi.begin(network.ssid, network.password);
    
    Serial.print("Intentando conectar a la red ");
    Serial.print(network.ssid);
    
    int try_count = 0;
    while (WiFi.status() != WL_CONNECTED && try_count < 10) {
      try_count++;
      Serial.print(".");
      delay(1000);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("Wifi Conectado");
      return; // Sale de la función si la conexión es exitosa
    }
    Serial.println("");
  }
  
  Serial.println("");
  Serial.println("No se pudo conectar a ninguna red.");

}


void setup() {
  Serial.begin(115200);

  delay(1000);

  // SET-UP display --------------------
  // turn on the TFT / I2C power supply
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  
  // Init ST7789 240x135 and parameters
  display.init(135, 240);           
  display.setRotation(3);
  canvas.setFont(&FreeSans12pt7b);
  canvas.setTextColor(ST77XX_WHITE);
  //--------------------------------------
  
  // Wifi
  detectAndConect();

  // Time sync
  synchronizeTime();
  print(getFormattedDateTime());

  // Batery
  bat.begin();

}


void loop() {
  // Read sensor data
  float temperature, humidity;
  getTempAndHumd(temperature, humidity);

  
  // DISPLAY
  canvas.fillScreen(ST77XX_BLACK);
  canvas.setCursor(0, 25);
  canvas.setTextColor(ST77XX_WHITE);
  canvas.print("Temperatura: ");
  canvas.print(temperature);
  canvas.println(" ºC");
  canvas.print("Humedad: ");
  canvas.print(humidity);
  canvas.println(" %");
  canvas.print("Bateria: ");
  canvas.print(bat.cellPercent(), 0);
  canvas.println("%");



  // Show all canva configuration set before
  display.drawRGBBitmap(0, 0, canvas.getBuffer(), 240, 135);
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  // Apagamos wifi
  WiFi.disconnect(true);
  delay(60000);
  return;
}