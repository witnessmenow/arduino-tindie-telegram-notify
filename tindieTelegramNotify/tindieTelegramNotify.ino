/*******************************************************************
    Notify a Telegram chat when you get a new Tindie order

    By Brian Lough
    YouTube: https://www.youtube.com/brianlough
    Tindie: https://www.twitch.tv/brianlough
    Twitter: https://twitter.com/witnessmenow
 *******************************************************************/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include <SPI.h>
#include "FS.h"

#include <TindieApi.h>
// https://github.com/witnessmenow/arduino-tindie-telegram-notify

#include <UniversalTelegramBot.h>
// https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot


#include <ArduinoJson.h>
// Library used for parsing Json from the API responses
// NOTE: There is a breaking change in the 6.x.x version,
// install the 5.x.x version instead
// Search for "Arduino Json" in the Arduino Library manager
// https://github.com/bblanchon/ArduinoJson

//------- Replace the following! ------

char ssid[] = "SSID";         // your network SSID (name)
char password[] = "password"; // your network password

#define TINDIE_API_KEY "1234567890654rfscFfsdfdsffd"
// API Key can be retrieved from here
// https://www.tindie.com/profiles/update/
//
// NOTE: Please be very careful with this API key as people will be
// able to retrieve customer information (address, phone numbers, emails etc)
// if they get access to it

#define TINDIE_USER "brianlough"

#define TELEGRAM_BOT_TOKEN "XXXXXXXXX:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"

#define TELEGRAM_CHAT_ID "123456789"
// Use IdBot to get the chat ID

//------- ---------------------- ------

#define TELEGRAM_FINGERPRINT "BB:DC:45:2A:07:E3:4A:71:33:40:32:DA:BE:81:F7:72:6F:4A:2B:6B"

#define TELEGRAM_TINDIE_CONFIG "telegramTindie.json"

WiFiClientSecure client;
TindieApi tindie(client, TINDIE_USER, TINDIE_API_KEY);
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, client);

long lastOrderNumber;
int totalOrders;

unsigned long delayBetweenRequests = 60000; // Time between requests (1 minute)
unsigned long requestDueTime;               //time when request due

void setup()
{

  Serial.begin(115200);

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount FS");
    return;
  }

  loadConfig();

  // Set WiFi to station mode and disconnect from an AP if it was Previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Attempt to connect to Wifi network:
  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  // Only avaible in ESP8266 V2.5 RC1 and above
  client.setFingerprint(TINDIE_FINGERPRINT);



  // If you want to enable some extra debugging
  //tindie._debug = true;
  //bot._debug = true;
}

bool loadConfig() {
  File configFile = SPIFFS.open(TELEGRAM_TINDIE_CONFIG, "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  lastOrderNumber = json["lastOrderNumber"].as<long>();
  totalOrders = json["totalOrders"].as<int>();

  return true;
}

bool saveConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["lastOrderNumber"] = lastOrderNumber;
  json["totalOrders"] = totalOrders;

  File configFile = SPIFFS.open(TELEGRAM_TINDIE_CONFIG, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  return true;
}

bool sendTelegramMessage(OrderInfo orderInfo) {
  String message = "New order from " + String(orderInfo.shipping_country) + "!\n\n";
  message += "They bought " + String(orderInfo.number_of_products) + " different product(s) for a total of $" + String(orderInfo.total_subtotal) + ".\n\n";
  
  int numProductInfo = orderInfo.number_of_products;
  if (orderInfo.number_of_products > TINDIE_MAX_PRODUCTS_IN_ORDER)
  {
    numProductInfo = TINDIE_MAX_PRODUCTS_IN_ORDER;
  }
  for (int i = 0; i < numProductInfo; i++)
  {
    message += "- " + String(orderInfo.products[i].quantity) + " no. " + String(orderInfo.products[i].product) + " " + String(orderInfo.products[i].options) + "\n";
  }

  message += "\n";
  
  char url[50];
  sprintf(url, TINDIE_ORDER_URL_FORMAT, orderInfo.number);
  message += String(url);

  client.setFingerprint(TELEGRAM_FINGERPRINT);
  //Serial.print(message);
  bool responseStatus = bot.sendMessage(TELEGRAM_CHAT_ID, message, "Markdown");

  //Set it back to the Tindie fingerprint
  client.setFingerprint(TINDIE_FINGERPRINT);

  return responseStatus;
}

void loop()
{

  if (millis() > requestDueTime)
  {
    //Serial.print("Free Heap: ");
    //Serial.println(ESP.getFreeHeap());

    Serial.println("Checking Tindie");
    OrderInfo orderInfo = tindie.getOrderInfo(0, 0); // offset 0, shipped status 0 (false)
    if (!orderInfo.error) {
      if (orderInfo.number != 0) {
        if (orderInfo.number != lastOrderNumber) {
          Serial.println("New Order!");
          if (sendTelegramMessage(orderInfo)) {
            Serial.println("Sent succesfully!");
            lastOrderNumber = orderInfo.number;
            saveConfig();
          }
        } else {
          Serial.println("Already processed this order");
        }
      } else {
        Serial.println("No unshipped orders");
      }
    } else {
      Serial.println("Error getting info");
    }

    requestDueTime = millis() + delayBetweenRequests;
  }
}
