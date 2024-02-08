#include "Esp32MQTTClient.h"
#include "EmotiBit.h"
#include "time.h"
#include "EmotiBitVersionController.h"
#include "EmotiBitVariants.h"
// #include "EmotiBitNvmController.h"
// #include <Wire.h>

#define SerialUSB SERIAL_PORT_USBVIRTUAL // Required to work in Visual Micro / Visual Studio IDE
#define MESSAGE_MAX_LEN 1024             // Set to a little short of max size of IoT Hub Messages
const uint32_t SERIAL_BAUD = 2000000;    // 115200

EmotiBit emotibit;
EmotiBitVersionController emotibitVersionController;
EmotiBitVersionController::EmotiBitVersion emotibitVersion;

TaskHandle_t ReadTask;
TaskHandle_t CaptureTask;

unsigned long epochTime;
const char *ntpServer = "pool.ntp.org";
String version;

const size_t dataSize = EmotiBit::MAX_DATA_BUFFER_SIZE;
float data[dataSize];
String fathymDeviceID;
char fathymReadings[18][3] = {{}};
int readingsInterval;

int captureInterval;
String fathymConnectionStringPtr;

void setup()
{
  Serial.begin(SERIAL_BAUD);
  Serial.println("Serial started");
  delay(2000); // short delay to allow user to connect to serial, if desired

  version = EmotiBitVersionController::getHardwareVersion(emotibitVersion);

  // Capture the calling ino into firmware_variant information
  String inoFilename = __FILE__;
  inoFilename = (inoFilename.substring((inoFilename.indexOf(".")), (inoFilename.lastIndexOf("\\")) + 1));

  emotibit.setup(inoFilename);

  emotibit.attachShortButtonPress(&onShortButtonPress);
  emotibit.attachLongButtonPress(&onLongButtonPress);

  if (!loadConfigFile(emotibit._configFilename))
  {
    Serial.println("SD card configuration file parsing failed.");
    Serial.println("Create a file 'config.txt' with the following JSON:");
    Serial.println("{\"WifiCredentials\": [{\"ssid\": \"SSSS\", \"password\" : \"PPPP\"}],\"Fathym\":{\"ConnectionString\": \"xxx\", \"DeviceID\": \"yyy\"}}");
  }

  configTime(0, 0, ntpServer);

  xTaskCreatePinnedToCore(ReadTaskRunner, "ReadTask", 10000, NULL, 1, &ReadTask, 0);
  xTaskCreatePinnedToCore(CaptureTaskRunner, "CaptureTask", 10000, NULL, 1, &CaptureTask, 1);
}

void loop()
{
}

void onShortButtonPress()
{
  // toggle wifi on/off
  if (emotibit.getPowerMode() == EmotiBit::PowerMode::NORMAL_POWER)
  {
    emotibit.setPowerMode(EmotiBit::PowerMode::WIRELESS_OFF);
    Serial.println("PowerMode::WIRELESS_OFF");
  }
  else
  {
    emotibit.setPowerMode(EmotiBit::PowerMode::NORMAL_POWER);
    Serial.println("PowerMode::NORMAL_POWER");
  }
}

void onLongButtonPress()
{
  emotibit.sleep();
}

void ReadTaskRunner(void *pvParameters)
{
  Serial.print("ReadTask running on core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    emotibit.update();

    ReadTaskLoop(pvParameters);

    delay(readingsInterval);
  }
}

void ReadTaskLoop(void *pvParameters)
{
  Serial.print("ReadTask loop running");

  StaticJsonDocument<1024> payload;

  payload["DeviceID"] = fathymDeviceID;

  payload["DeviceType"] = "emotibit";

  JsonObject payloadDeviceData = payload.createNestedObject("DeviceData");

  JsonObject payloadSensorReadings = payload.createNestedObject("SensorReadings");

  JsonObject payloadSensorMetadata = payload.createNestedObject("SensorMetadata");

  epochTime = getTime();

  payloadDeviceData["Timestamp"] = String(epochTime);

  long loopStartMillis = millis();

  for (String typeTag : fathymReadings)
  {
    // Serial.println("Inside For loop: ");
    if (typeTag != NULL)
    {
      enum EmotiBit::DataType dataType = loadDataTypeFromTypeTag(typeTag);

      uint32_t timestamp;
      size_t dataAvailable = emotibit.readData((EmotiBit::DataType)dataType, &data[0], dataSize, timestamp);

      if (dataAvailable > 0)
      {
        long elapsedMillis = timestamp - loopStartMillis;

        Serial.print("Data Available for ");
        Serial.println(typeTag);
        Serial.print("\tDA: ");
        Serial.println(dataAvailable);
        Serial.print("\tTimestamp: ");
        Serial.println(timestamp);
        Serial.print("\tElapsedMillis: ");
        Serial.println(elapsedMillis);

        JsonArray payloadSensorTypeReadings = payloadSensorReadings.createNestedArray(typeTag);

        for (size_t i = 0; i < dataAvailable && i < dataSize; i++)
        {
          Serial.print("Reading for ");
          Serial.print(typeTag);
          Serial.println(": ");

          JsonObject reading = payloadSensorTypeReadings.createNestedObject();
          
          reading["Data"] = data[i];

          Serial.print("Time math: ");
          Serial.println(String((i + 1) / dataAvailable));
          Serial.print("elapsedMillis: ");
          Serial.println(String(elapsedMillis));

          reading["Millis"] = ((i + 1) / dataAvailable) * elapsedMillis;

          serializeJson(reading, Serial);
        }

        payloadSensorReadings[typeTag] = String(data[0]);
      }
    }
  }

  serializeJson(payload, Serial);
}

void CaptureTaskRunner(void *pvParameters)
{
  Serial.print("CaptureTask running on core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    Serial.println("CaptureTask processing");
    delay(captureInterval);
  }
}

// Loads the configuration from a file
bool loadConfigFile(const char *filename)
{
  // Open file for reading
  File file = SD.open(filename);

  if (!file)
  {
    Serial.print("File ");
    Serial.print(filename);
    Serial.println(" not found");
    return false;
  }

  Serial.print("Parsing: ");
  Serial.println(filename);

  // Allocate the memory pool on the stack.
  // Don't forget to change the capacity to match your JSON document.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<1024> doc;

  // Parse the root object
  deserializeJson(doc, file, DeserializationOption::NestingLimit(3));
  // JsonObject root = jsonBuffer.parseObject(file);
  // Serial.print("AFter deserialize: ");
  JsonArray readingValues = doc["Fathym"]["Readings"].as<JsonArray>();
  // Serial.println("After reading values: ");
  // int size = static_cast<int>(readingValues.size());
  // Serial.println(size);
  const char *readings[18];

  Serial.println(readingValues.size());
  copyArray(readingValues, readings);

  // Serial.println("After copy array: ");
  // readingValues.copyTo(readings);
  // Serial.println(sizeof fathymReadings / sizeof fathymReadings[0]);

  // Serial.println(sizeof readings[0]);

  // Serial.println(sizeof readingValues / sizeof readingValues[0]);
  // Serial.println(
  for (int i = 0; i < readingValues.size(); i++)
  {
    strcpy(fathymReadings[i], readings[i]);
  }

  // Serial.print("After copying readings: ");
  // }

  if (doc.isNull())
  {
    Serial.println(F("Failed to parse config file"));
    return false;
  }

  fathymConnectionStringPtr = doc["Fathym"]["ConnectionString"].as<String>();

  // Serial.print("After conn: ");

  fathymDeviceID = doc["Fathym"]["DeviceID"].as<String>();

  // Serial.print("After deviceid: ");

  readingsInterval = doc["Fathym"]["ReadingInterval"] | 10;

  // Serial.print("After reading interval: ");

  captureInterval = doc["Fathym"]["CaptureInterval"] | 5000;

  // Serial.print("After capture interval: ");

  // Close the file (File's destructor doesn't close the file)
  // ToDo: Handle multiple credentials

  file.close();

  return true;
}

EmotiBit::DataType loadDataTypeFromTypeTag(String typeTag)
{
  if (typeTag == "AX")
    return EmotiBit::DataType::ACCELEROMETER_X;
  else if (typeTag == "AY")
    return EmotiBit::DataType::ACCELEROMETER_Y;
  else if (typeTag == "AZ")
    return EmotiBit::DataType::ACCELEROMETER_Z;
  else if (typeTag == "GX")
    return EmotiBit::DataType::GYROSCOPE_X;
  else if (typeTag == "GY")
    return EmotiBit::DataType::GYROSCOPE_Y;
  else if (typeTag == "GZ")
    return EmotiBit::DataType::GYROSCOPE_Z;
  else if (typeTag == "MX")
    return EmotiBit::DataType::MAGNETOMETER_X;
  else if (typeTag == "MY")
    return EmotiBit::DataType::MAGNETOMETER_Y;
  else if (typeTag == "MZ")
    return EmotiBit::DataType::MAGNETOMETER_Z;
  else if (typeTag == "EA")
    return EmotiBit::DataType::EDA;
  else if (typeTag == "EL")
    return EmotiBit::DataType::EDL;
  else if (typeTag == "ER")
    return EmotiBit::DataType::EDR;
  else if (typeTag == "H0")
    return EmotiBit::DataType::HUMIDITY_0;
  else if (typeTag == "T0")
    return EmotiBit::DataType::TEMPERATURE_0;
  else if (typeTag == "TH")
    return EmotiBit::DataType::THERMOPILE;
  else if (typeTag == "PI")
    return EmotiBit::DataType::PPG_INFRARED;
  else if (typeTag == "PR")
    return EmotiBit::DataType::PPG_RED;
  else if (typeTag == "PG")
    return EmotiBit::DataType::PPG_GREEN;
}

// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}
