#include <ArduinoJson.h>
#include <Arduino.h>
#include "settings.h"

#include <Adafruit_DHT.h>
#include <LaCrosse_TX23.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

#include <stdio.h>
#include <stdarg.h>

#define FIRMWARE_VERSION "1.0"

SYSTEM_MODE(SEMI_AUTOMATIC);

struct Reading
{
    int version;
    float batteryVoltage;
    float panelVoltage;
    bool anemometerRead;
    float windSpeed;
    int windDirection;
    bool dhtRead;
    float dhtTemperature;
    float humidity;
    bool bmpRead;
    float bmpTemperature;
    float pressure;
};

/* Connections */
#define BAROMETER_CHIP_SELECT 1
#define ANEMOMETER 1
#define PANEL_VOLTAGE 1
#define LED D7 //Builtin LED

#define DHT_IN D6

//This pin is switched high to drive a transistor that powers sensors
#define SENSOR_POWER C0

/* Sensors */
#define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321
#define DHT_INIT 1000
DHT dht(DHT_IN, DHTTYPE);

Adafruit_BMP280 bmp280(BAROMETER_CHIP_SELECT);
LaCrosse_TX23 laCrosseTX23(ANEMOMETER);
FuelGauge gauge;

Settings settings;
#define diagnosticMode settings.diagnositicCycles
unsigned long duration;

char* serializeToJson(JsonObject& json)
{
    int length = json.measureLength() + 1;
    char* jsonString = (char*)malloc(length);
    jsonString[length] = NULL;
    json.printTo(jsonString, length);
    return jsonString;
}

void deviceSetup()
{
    duration = millis();

    //Turn off the status LED to save power
    RGB.control(true); 
    RGB.color(0, 0, 0);

    //Load saved settings;
    settings = *loadSettings();

    if(diagnosticMode)
    {
        pinMode(LED, OUTPUT);
        digitalWrite(LED, HIGH);
        settings.diagnositicCycles--;

        saveSettings(&settings);
    }

    pinMode(PANEL_VOLTAGE, INPUT);
    pinMode(SENSOR_POWER, OUTPUT);

    digitalWrite(SENSOR_POWER, HIGH);
    dht.begin();
    bmp280.begin();
}
STARTUP(deviceSetup());

void onSettingsUpdate(const char* event, const char* data)
{
    digitalWrite(LED, HIGH);
    settings = *deserialize(data);
    saveSettings(&settings);

    JsonObject& settingsJson = serialize(&settings);
    char* settingsEcho = serializeToJson(settingsJson);

    Serial.print("SETTINGS UPDATE: ");
    Serial.println(settingsEcho);

    Particle.publish("Settings", settingsEcho, 60, PRIVATE);
    Particle.process();

    digitalWrite(LED, LOW);
}

void setup()
{
    Serial.begin(115200);

    JsonObject& settingsJson = serialize(&settings);
    Serial.print("SETTINGS: ");
    settingsJson.printTo(Serial);

    Particle.subscribe("Settings", onSettingsUpdate, MY_DEVICES);
    Particle.connect();
}

char messageBuffer[255];
char* serialize(Reading *reading)
{
    char* buffer = messageBuffer;
    buffer += sprintf(buffer, "%d:%f:%f|", reading->version, reading->batteryVoltage, reading->panelVoltage);
    if(reading->bmpRead)
    {
        buffer += sprintf(buffer, "b%f:%f", reading->bmpTemperature, reading->pressure);
    }
    if(reading->dhtRead)
    {
        buffer += sprintf(buffer, "d%f:%f", reading->dhtTemperature, reading->humidity);
    }
    if(reading->anemometerRead)
    {
        buffer += sprintf(buffer, "a%f:%d", reading->windSpeed, reading->windDirection);
    }
    return messageBuffer;
}

bool readAnemometer(Reading *reading)
{
    return laCrosseTX23.read(reading->windSpeed, reading->windDirection);
}

bool readVoltage(Reading *reading)
{
    reading->batteryVoltage = gauge.getVCell();
    reading->panelVoltage = 1023.0 / (float)analogRead(PANEL_VOLTAGE);
    return true;
}

bool readDht(Reading *reading)
{
    reading->dhtTemperature = dht.getTempCelcius();
    reading->humidity = dht.getHumidity();
    return !isnan(reading->dhtTemperature) && !isnan(reading->humidity);
}

bool readBmp280(Reading *reading)
{
    digitalWrite(BAROMETER_CHIP_SELECT, HIGH);
    reading->bmpTemperature = bmp280.readTemperature();
    reading->pressure = bmp280.readPressure();
    digitalWrite(BAROMETER_CHIP_SELECT, LOW);
    return !isnan(reading->bmpTemperature) && !isnan(reading->pressure) && reading->pressure > 0;    
}

void loop()
{
    Reading reading;
   
    //read data
    reading.version = settings.version;
    if(!(reading.anemometerRead = readAnemometer(&reading))) 
    {
        Serial.println("ERROR: Could not read anemometer");
    }
    readVoltage(&reading);
    if(!(reading.bmpRead = readBmp280(&reading)))
    {
        Serial.println("ERROR: BMP280 temp/pressure sensor");
    }
    if(!(reading.dhtRead = readDht(&reading)))
    {
        Serial.println("ERROR: DHT22 temp/humidity sensor");
    }

    //If the DHT did not produce a value, keep trying it for a certain amount of time
    if(!reading.dhtRead)
    {
        Serial.println("Retrying DHT22 ");
        unsigned long currentRuntime = millis() - duration;
        while(currentRuntime < DHT_INIT && !(reading.dhtRead = readDht(&reading)))
        {
            Serial.print(".");
            delay(10);
        }
        Serial.println();
    }
    digitalWrite(SENSOR_POWER, LOW);    

    //send serialized reading to the cloud    
    char* publishedReading = serialize(&reading);    
    Serial.println(publishedReading);
    Particle.publish("Reading", publishedReading, 60, PRIVATE);

    //Allow particle to process before going into deep sleep
    Particle.process();
    
    if(diagnosticMode)
    {        
        Serial.print("DIAGNOSTIC COUNT ");
        Serial.println(diagnosticMode);
        digitalWrite(LED, LOW);
    }

    void (*sleepAction)();
    const char* sleepMessage;
    if(settings.useDeepSleep)
    {
        sleepMessage = "DEEP";
        sleepAction = []()
        {
            System.sleep(SLEEP_MODE_SOFTPOWEROFF, settings.sleepTime);
        };
    }
    else
    {
        sleepMessage = "LIGHT";
        sleepAction = []()
        {
            delay(settings.sleepTime * 1000);
            deviceSetup();
        };
    }

    Serial.print(sleepMessage);
    Serial.print(" SLEEP ");
    Serial.println(settings.sleepTime);

    Serial.print("DURATION ");
    duration = millis() - duration;
    Serial.println(duration);
    sleepAction();
}
