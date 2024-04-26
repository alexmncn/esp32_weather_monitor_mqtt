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