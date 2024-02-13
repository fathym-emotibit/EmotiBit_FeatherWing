#include "Esp32MQTTClient.h"
#include "EmotiBit.h"
#include "time.h"
#include "EmotiBitVersionController.h"
#include "EmotiBitVariants.h"
// #include "EmotiBitNvmController.h"
// #include <Wire.h>

#define SerialUSB SERIAL_PORT_USBVIRTUAL                                 // Required to work in Visual Micro / Visual Studio IDE
#define BATCH_SIZE (10)                                                  // The number of messages to batch into a single call
#define HUB_MESSAGE_MAX_LEN (1000 * 64)                                 // Set to max size of IoT Hub Messages (256 KB)
#define PAYLOAD_MAX_SIZE (HUB_MESSAGE_MAX_LEN / BATCH_SIZE)              // The max size of a single payload ~5kb
#define PAYLOADS_MAX_SIZE (HUB_MESSAGE_MAX_LEN - (PAYLOAD_MAX_SIZE * 3)) // The maximum size of all collected payloads
const uint32_t SERIAL_BAUD = 2000000;    // 115200

EmotiBit emotibit;
EmotiBitVersionController emotibitVersionController;
EmotiBitVersionController::EmotiBitVersion emotibitVersion;
String version;

TaskHandle_t ReadTask;
TaskHandle_t CaptureTask;

StaticJsonDocument<1024> config;
unsigned long epochTime;
const char *ntpServer = "pool.ntp.org";

StaticJsonDocument<1024> lastLoopStartMillisDoc;
JsonObject lastLoopStartMillis;
StaticJsonDocument<HUB_MESSAGE_MAX_LEN> payloadsDoc;
JsonArray payloads = payloadsDoc.to<JsonArray>();

const size_t dataSize = EmotiBit::MAX_DATA_BUFFER_SIZE;
float data[dataSize];
String fathymDeviceID;
char fathymReadings[18][3] = {{}};
int readingsInterval;

int captureInterval;
long captureTracking = 0;
String fathymConnectionStringPtr;
long lastCapture = 0;

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

  const char *connStr = fathymConnectionStringPtr.c_str();

  if (!Esp32MQTTClient_Init((const uint8_t *)connStr, true))
  {
    Serial.println("Initializing IoT hub failed.");
    return;
  }

  configTime(0, 0, ntpServer);

  loadLastLoopStartMillis();

  xTaskCreatePinnedToCore(ReadTaskRunner, "ReadTask", 10000, NULL, 2, &ReadTask, 0);
  delay(500);
  xTaskCreatePinnedToCore(CaptureTaskRunner, "CaptureTask", 10000, NULL, 2, &CaptureTask, 1);
  delay(500);

  Serial.println("#################################");
  Serial.println("# Open Biotech Real Time Stream #");
  Serial.println("#################################");
}

void loop()
{
  vTaskDelete(NULL);
  // TODO: Device Health Monitoring, cloud-to-device message handling, device twin syncing?
}

void ReadTaskRunner(void *pvParameters)
{
  Serial.print("ReadTask running on core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    Serial.println("ReadTask loop running");

    emotibit.update();

    //ReadTaskLoop();

    Serial.print("ReadTask loop complete, delaying for ");
    Serial.println(readingsInterval);

    delay(readingsInterval);
  }
}

void ReadTaskLoop()
{
  StaticJsonDocument<PAYLOAD_MAX_SIZE> payload;

  payload["DeviceID"] = fathymDeviceID;

  payload["DeviceType"] = "emotibit";

  JsonObject payloadDeviceData = payload.createNestedObject("DeviceData");

  JsonObject payloadSensorReadings = payload.createNestedObject("SensorReadings");

  epochTime = getTime();

  payloadDeviceData["Timestamp"] = String(epochTime);

  for (String typeTag : fathymReadings)
  {
    if (typeTag != NULL)
    {
      Serial.print("Reading type ");
      Serial.println(typeTag);

      enum EmotiBit::DataType dataType = loadDataTypeFromTypeTag(typeTag);

      long loopStartMillis = lastLoopStartMillis[typeTag];

      uint32_t timestamp;
      size_t dataAvailable = emotibit.readData((EmotiBit::DataType)dataType, &data[0], dataSize, timestamp);

      lastLoopStartMillis[typeTag] = timestamp;

      if (dataAvailable > 0 && loopStartMillis > 0)
      {
        Serial.print(dataAvailable);
        Serial.print(" data record(s) available reading type ");
        Serial.println(typeTag);

        long elapsedMillis = timestamp - loopStartMillis;

        JsonArray payloadSensorTypeReadings = payloadSensorReadings.createNestedArray(typeTag);

        for (size_t i = 0; i < dataAvailable && i < dataSize; i++)
        {
          Serial.print("Reading data record ");
          Serial.print(i);
          Serial.print(" for ");
          Serial.print(typeTag);
          Serial.println(": ");

          JsonObject reading = payloadSensorTypeReadings.createNestedObject();
          
          reading["Data"] = data[i];

          reading["Millis"] = (float(i + 1) / float(dataAvailable)) * float(elapsedMillis);

          serializeJson(reading, Serial);
          Serial.println("");
        }
      }
    }
  }

  JsonObject payloadSensorMetadata = payload.createNestedObject("SensorMetadata");

  float battVolt = emotibit.readBatteryVoltage();

  payloadSensorMetadata["BatteryPercentage"] = emotibit.getBatteryPercent(battVolt);

  payloadSensorMetadata["MACAddress"] = emotibit.getFeatherMacAddress();

  payloadSensorMetadata["EmotibitVersion"] = version;

  Serial.println("Queuing payload for capture: ");

  //  Ensure payload is as small as possible before adding to capture set
  //payload.shrinkToFit();

  Serial.print("Payload Memory Usage: ");
  Serial.println(payload.memoryUsage());

  payloads.add(payload);

  serializeJson(payload, Serial);
  Serial.println("");
}

void CaptureTaskRunner(void *pvParameters)
{
  Serial.print("CaptureTask running on core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    Serial.print("Calculating CaptureTask loop run with ");

    float allocatedMemory = payloadsDoc.memoryUsage();

    Serial.print("allocated memory ");
    Serial.print(allocatedMemory);

    bool isMemoryAllocated = allocatedMemory >= PAYLOADS_MAX_SIZE;

    Serial.print(" and capture tracking ");
    Serial.println(captureTracking);

    bool isCaptureInterval = captureTracking >= captureInterval;

    if (isCaptureInterval || isMemoryAllocated)
    {
      Serial.print("CaptureTask loop running due to ");

      if (isMemoryAllocated)
      {
        Serial.print("memory allocated");
      }
      else if (isCaptureInterval)
      {
        Serial.print("capture interval");
      }

      //CaptureTaskLoop();

      Serial.println("CaptureTask loop complete");

      captureTracking = 0;

      lastCapture = millis();
    }
    else
    {
      captureTracking = millis() - lastCapture;

      //  Small delay to space out capture tracking checks
      delay(10);
    }
  }
}

void CaptureTaskLoop()
{
  StaticJsonDocument<HUB_MESSAGE_MAX_LEN> payloadCapturesDoc;

  JsonArray payloadCaptures = payloadCapturesDoc.to<JsonArray>();

  //  Fill array for processing captures
  payloadCaptures.set(payloads);

  //  Immediately clear payloads so that new payloads can be read
  payloadsDoc.clear();

  for (JsonVariant payloadCapture : payloadCaptures)
  {
    char messagePayload[PAYLOAD_MAX_SIZE];

    serializeJson(payloadCapture, messagePayload);

    Serial.println("Capturing payload: ");

    EVENT_INSTANCE *message = Esp32MQTTClient_Event_Generate(messagePayload, MESSAGE);

    Serial.println(messagePayload);

    Esp32MQTTClient_SendEventInstance(message);
  
    Serial.println("Payload captured");
  }
}

// Loads the configuration from a file
bool loadConfigFile(const char *filename)
{
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

  deserializeJson(config, file, DeserializationOption::NestingLimit(3));

  JsonArray readingValues = config["Fathym"]["Readings"].as<JsonArray>();

  const char *readings[18];

  Serial.println(readingValues.size());
  copyArray(readingValues, readings);

  for (int i = 0; i < readingValues.size(); i++)
  {
    strcpy(fathymReadings[i], readings[i]);
  }

  if (config.isNull())
  {
    Serial.println(F("Failed to parse config file"));
    return false;
  }

  fathymConnectionStringPtr = config["Fathym"]["ConnectionString"].as<String>();

  fathymDeviceID = config["Fathym"]["DeviceID"].as<String>();

  readingsInterval = config["Fathym"]["ReadingInterval"] | 10;

  captureInterval = config["Fathym"]["CaptureInterval"] | 5000;

  // Close the file (File's destructor doesn't close the file)
  // ToDo: Handle multiple credentials

  file.close();

  Serial.println("Serialized Config: ");
  serializeJson(config, Serial);
  Serial.println("");
  Serial.print("Config memory usage: ");
  Serial.println(config.memoryUsage());

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

void loadLastLoopStartMillis()
{
  Serial.println("Initializing last loop start millis for tracking");

  lastLoopStartMillis = lastLoopStartMillisDoc.to<JsonObject>();

  JsonArray readingValues = config["Fathym"]["Readings"].as<JsonArray>();

  for (JsonVariant readingValue : readingValues)
  {
    lastLoopStartMillis[readingValue.as<String>()] = millis();
  }
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
