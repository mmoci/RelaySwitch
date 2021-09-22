/*******************************************/
/* Relay Relay With MQTT on ESP8266 board */
/*******************************************/

#include "ESP8266WiFi.h"
#include "PubSubClient.h"

#define RELAY_PIN 3

// Number of MQTT retiries 
const int RETRIES = 100;

// WiFi and MQTT configuration variables
const char* WIFI_SSID = "my_ssid";
const char* WIFI_PASSWORD = "my_password";
const char* MQTT_SERVER = "mqtt_broker_ip_address";
const int   MQTT_PORT = 1883;
const char* MQTT_USERNAME = "mqtt_username";
const char* MQTT_PASSWORD = "mqtt_password";
const char* MQTT_CLIENT_NAME = "mqtt/topic/name";

// WiFi Static IP example
IPAddress staticIP(192,168,0,100);
IPAddress gateway(192,168,0,0);
IPAddress subnet(255,255,255,0);
IPAddress dns1(83,139,103,3);
IPAddress dns2(83,139,121,8);

// WiFi and MQTT clients
WiFiClient wifiClient;
PubSubClient mqtt_client(wifiClient);

// Relay structure
struct Relay
{  
  // Relay Availability Status
  enum Availability 
  {
    OFFLINE,
    ONLINE
  };

  // Relay State
  enum State
  {
    OFF,
    ON
  };  
  
  Relay(Availability availability, State state)
    : availability(availability),
      state(state){}  
  
  private:
  Availability availability;
  State state; 

  public:
  void turn_onOff()
  {
    if(state == ON)
    {
      digitalWrite(RELAY_PIN, HIGH);
      delay(10);
    }
    else
    {
      digitalWrite(RELAY_PIN, LOW);
      delay(10);
    }
    Serial.println("Turn " + stateToStr(state) + " relay");
  }

  // Getters and Setters
  void setAvailability(Availability availability)
  {
    this->availability = availability;
  }
  
  Availability getAvailability()
  {
    return availability;
  }

  void setState(State state)
  {
    this->state = state;
  }

  State getState()
  {
    return state;
  }

  String stateToStr(State state)
  {
    switch(state)
    {
      case OFF: return "OFF";
      case ON: return "ON";
      default: return "";
    }
  }

  String availabilityToStr(Availability availability)
  {
    switch(availability)
    {
      case OFFLINE: return "offline";
      case ONLINE: return "online";
      default: return "";
    }
  }
};

Relay relay(Relay::OFFLINE, Relay::OFF);

/*************************************/
/* Main setup() - executed only once */
/*************************************/
void setup() 
{  
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  
  setup_wifi();
  //setup_OTA_update();
  mqtt_client.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt_client.setCallback(mqtt_receive); 
  delay(1000);
}

/***************/
/* Main loop() */
/***************/
void loop() 
{
  mqtt_connect();
  mqtt_client.loop();
  //ArduinoOTA.handle(); //not used on ESP01 board
  delay(100);  
}

/********************************/
/* Used to setup WiFi connecton */
/********************************/
void setup_wifi() 
{  
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  // Set WIFI mode, WIFI_STA - Station (STA) mode is used to get ESP module connected to a WiFi network established by an access point
  WiFi.mode(WIFI_STA);
  WiFi.config(staticIP, gateway, subnet, dns1, dns2); //For statis IP address
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

/*****************************/
/* Used to setup OTA updates */
/*****************************/
//not used on ESP01 board
//void setup_OTA_update() 
//{
//  ArduinoOTA.setHostname("ESP01_living_room");
//  //ArduinoOTA.setPassword("esp32_kitchen");
//
//  ArduinoOTA.onStart([]() {
//    Serial.println("Start OTA");
//  });
//  ArduinoOTA.onEnd([]() {
//    Serial.println("\nEnd OTA");
//  });
//  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
//    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
//  });
//  ArduinoOTA.onError([](ota_error_t error) {
//    Serial.printf("Error[%u]: ", error);
//    if (error == OTA_AUTH_ERROR) Serial.println("Authentication Failed");
//    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
//    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
//    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
//    else if (error == OTA_END_ERROR) Serial.println("End Failed");
//  });
//  ArduinoOTA.begin();
//  Serial.println("OTA ready");
//}

/***************************************************************************/
/* Initial MQTT connection/reconnection, publishing and subscribing topics */
/***************************************************************************/
void mqtt_connect() 
{  
  int retries = 0;
  const int willQoS = 0;
  const boolean willRetain = true;
  char willTopic[40] = {0};
  char willPayload[40] = {0};
  
  while (!mqtt_client.connected()) {
    if(retries < RETRIES)
    {
      Serial.println("Attempting MQTT connection...");
      strcpy(willTopic, MQTT_CLIENT_NAME);
      strcat(willTopic, "availability");
      strcpy(willPayload, "offline");
      if (mqtt_client.connect(MQTT_CLIENT_NAME, MQTT_USERNAME, MQTT_PASSWORD, willTopic, willQoS, willRetain, willPayload)) 
      {
        relay.setAvailability(Relay::ONLINE);
        Serial.println("MQTT client state: " + String(mqtt_client.state()));

        //Subscribe MQTT topics
        mqtt_subscribe ("state/set");

        // Publish MQTT topics
        mqtt_publish ("availability", relay.availabilityToStr(relay.getAvailability()), true);
        mqtt_publish ("state", relay.stateToStr(relay.getState()), true);
      } 
      else 
      {
        relay.setAvailability(Relay::OFFLINE);

        // Publish MQTT topic
        mqtt_publish ("availability", relay.availabilityToStr(relay.getAvailability()), true);
        
        Serial.println("Failed to connect to MQTT server");
        Serial.println("MQTT client state: " + String(mqtt_client.state()));
        Serial.println("Try again in 5 seconds...");
        retries++;
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
    if(retries >= RETRIES)
    {
      relay.setAvailability(Relay::OFFLINE);

      // Publish MQTT topic
      mqtt_publish ("availability", relay.availabilityToStr(relay.getAvailability()), true);
      
      Serial.print("Restarting board, state: " + String(relay.getAvailability()));
      ESP.restart();
    }
  } 
}

/************************************************************/
/* Callback function for receiving subscribed MQTT messages */
/************************************************************/
void mqtt_receive(char* topic, byte* payload, unsigned int length) 
{
  payload[length] = '\0';
  String topicString = String(topic);
  String payloadString = String((char *)payload);
  
  Serial.println("Message arrived [" + topicString + "] payload: " + payloadString);

  switch (relay.getAvailability())
  {
    case Relay::ONLINE:
      if(topicString.equals(String(MQTT_CLIENT_NAME) + "state/set"))
      {
        payloadString.equals("ON") ? relay.setState(Relay::ON) : relay.setState(Relay::OFF);
        relay.turn_onOff();
        mqtt_publish ("state", relay.stateToStr(relay.getState()), true);
        Serial.println("Current state set to " + relay.stateToStr(relay.getState()));
      }
    break;
    default:
      Serial.println("ERROR: MQTT message [" + topicString + "] received in wrong state = " + relay.availabilityToStr(relay.getAvailability()));
    break;  
  }
}

/***************************************/
/* Method for publishing MQTT messages */
/***************************************/
void mqtt_publish (String stringTopic, String stringPayload, boolean retain)
{
  char topic[40] = {0};
  char payload[40] = {0};

  strcpy(topic, MQTT_CLIENT_NAME);
  strcat(topic, stringTopic.c_str());
  strcpy(payload, stringPayload.c_str());

  mqtt_client.publish(topic, payload, retain);
  Serial.println("Publishing topic [" + String(topic) + "] payload: " + String(payload));  
}

/*******************************************/
/* Method for subscribing on MQTT messages */
/*******************************************/
void mqtt_subscribe (String stringTopic)
{
  char topic[40] = {0};

  strcpy(topic, MQTT_CLIENT_NAME);
  strcat(topic, stringTopic.c_str());

  mqtt_client.subscribe(topic);
  Serial.println("Subscribing on topic [" + String(topic) + "]");  
}
