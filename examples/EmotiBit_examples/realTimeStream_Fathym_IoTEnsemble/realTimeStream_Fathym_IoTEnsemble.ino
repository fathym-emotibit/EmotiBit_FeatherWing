#include "Esp32MQTTClient.h"
#include "EmotiBit.h"
#include "time.h"

#define SerialUSB SERIAL_PORT_USBVIRTUAL // Required to work in Visual Micro / Visual Studio IDE
#define MESSAGE_MAX_LEN (1024 * 250)     // Set to a little short of max size of IoT Hub Messages
const uint32_t SERIAL_BAUD = 2000000;    // 115200

EmotiBit emotibit;
const size_t dataSize = EmotiBit::MAX_DATA_BUFFER_SIZE;
float data[dataSize];
static bool hasIoTHub = false;
unsigned long epochTime;
const char *ntpServer = "pool.ntp.org";
String fathymConnectionStringPtr;
String fathymDeviceID;
char fathymReadings[25][3] = {{}};
int readingsInterval;
char metadataTypeTags[3];
int captureInterval;
long lastCapture;
StaticJsonBuffer<262144> jsonPayloadBuffer; // 1024 * 256
JsonArray payloads;

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

EmotiBit::DataType loadDataTypeFromTypeTag(String typeTag)
{
  if (typeTag == "AX")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::ACCELEROMETER_X};
    return dataType;
  }
  else if (typeTag == "AY")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::ACCELEROMETER_Y};
    return dataType;
  }
  else if (typeTag == "AZ")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::ACCELEROMETER_Z};
    return dataType;
  }
  else if (typeTag == "GX")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::GYROSCOPE_X};
    return dataType;
  }
  else if (typeTag == "GY")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::GYROSCOPE_Y};
    return dataType;
  }
  else if (typeTag == "GZ")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::GYROSCOPE_Z};
    return dataType;
  }
  else if (typeTag == "MX")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::MAGNETOMETER_X};
    return dataType;
  }
  else if (typeTag == "MY")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::MAGNETOMETER_Y};
    return dataType;
  }
  else if (typeTag == "MZ")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::MAGNETOMETER_Z};
    return dataType;
  }
  else if (typeTag == "EA")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::EDA};
    return dataType;
  }
  else if (typeTag == "EL")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::EDL};
    return dataType;
  }
  else if (typeTag == "ER")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::EDR};
    return dataType;
  }
  else if (typeTag == "H0")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::HUMIDITY_0};
    return dataType;
  }
  else if (typeTag == "T0")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::TEMPERATURE_0};
    return dataType;
  }
  else if (typeTag == "TH")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::THERMOPILE};
    return dataType;
  }
  else if (typeTag == "PI")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::PPG_INFRARED};
    return dataType;
  }
  else if (typeTag == "PR")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::PPG_RED};
    return dataType;
  }
  else if (typeTag == "PG")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::PPG_GREEN};
    return dataType;
  }
  else if (typeTag == "BV")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::BATTERY_VOLTAGE};
    return dataType;
  }
  else if (typeTag == "BP")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::BATTERY_PERCENT};
    return dataType;
  }
  else if (typeTag == "DO")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::DATA_OVERFLOW};
    return dataType;
  }
  else if (typeTag == "DC")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::DATA_CLIPPING};
    return dataType;
  }
  else if (typeTag == "DB")
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::DEBUG};
    return dataType;
  }
  else
  {
    EmotiBit::DataType dataType{EmotiBit::DataType::DEBUG};
    return dataType;
  }
}

void setup()
{
  Serial.begin(SERIAL_BAUD);
  Serial.println("Serial started");
  delay(2000); // short delay to allow user to connect to serial, if desired

  emotibit.setup();

  if (!loadConfigFile(emotibit._configFilename))
  {
    Serial.println("SD card configuration file parsing failed.");
    Serial.println("Create a file 'config.txt' with the following JSON:");
    Serial.println("{\"WifiCredentials\": [{\"ssid\": \"SSSS\", \"password\" : \"PPPP\"}],\"Fathym\":{\"ConnectionString\": \"xxx\", \"DeviceID\": \"yyy\", \"ReadingInterval\": 10, \"Readings\": [\"AX\", \"AY\", \"AZ\", \"TH\", \"PI\", \"PR\", \"PG\", \"BP\"], \"CaptureInterval\": 5000}}");
    Serial.println("Replace the values in the Readings array with the readings you would like to capture.");
  }

  const char *connStr = fathymConnectionStringPtr.c_str();

  if (!Esp32MQTTClient_Init((const uint8_t *)connStr, true))
  {
    hasIoTHub = false;
    Serial.println("Initializing IoT hub failed.");
    return;
  }

  hasIoTHub = true;

  // Attach callback functions
  emotibit.attachShortButtonPress(&onShortButtonPress);
  emotibit.attachLongButtonPress(&onLongButtonPress);

  configTime(0, 0, ntpServer);

  lastCapture = 0;

  payloads = jsonPayloadBuffer.createArray();
}

void loop()
{
  emotibit.update();

  lastCapture += millis();

  // allocate the memory for the document
  const size_t CAPACITY = JSON_OBJECT_SIZE(1);

  StaticJsonBuffer<1024> doc;

  JsonObject &payload = doc.createObject();

  payload[String("DeviceID")] = fathymDeviceID;

  payload["DeviceType"] = "emotibit";

  payload["Version"] = "1";

  JsonObject &payloadDeviceData = payload.createNestedObject("DeviceData");

  payloadDeviceData["Timestamp"] = String(epochTime);

  JsonObject &payloadSensorReadings = payload.createNestedObject("SensorReadings");

  JsonObject &payloadSensorMetadata = payload.createNestedObject("SensorMetadata");

  JsonObject &payloadSensorMetadataMicrosAdjustments = payloadSensorMetadata.createNestedObject("MicrosAdjustments");

  JsonObject &payloadSensorMetadataRoot = payloadSensorMetadata.createNestedObject("_");

  epochTime = getTime();

  long microsAdjustmentStart = micros();

  for (String typeTag : fathymReadings)
  {
    enum EmotiBit::DataType dataType = loadDataTypeFromTypeTag(typeTag);
    size_t dataAvailable = emotibit.readData((EmotiBit::DataType)dataType, &data[0], dataSize);

    if (dataAvailable > 0)
    {
      payloadSensorReadings[typeTag] = String(data[dataAvailable - 1]);

      payloadSensorMetadataMicrosAdjustments[typeTag] = micros() - microsAdjustmentStart;
    }
  }

  payloadSensorMetadata["BatteryVoltage"] = emotibit.readBatteryVoltage();

  payloads.add(&payload);

  // TODO: How to run on separate core when present. will loop1 method work?
  {
    // TODO: Enable more precise memory allocation with V6 of ArduinoJson
    // long payloadsMemoryUsage = payloads.memoryUsage();

    // bool isMemoryAllocated = payloadsMemoryUsage >= (MESSAGE_MAX_LEN - (1024 * 5));
    bool isMemoryAllocated = payloads.size() >= 250;

    bool isCaptureInterval = (lastCapture + captureInterval) >= 0;

    if (isCaptureInterval || isMemoryAllocated)
    {
      // May need to change this to support batches, but from what i can tell MQTT doesn't support batches

      for (int i = 0; i < payloads.size(); i++)
      {
        char messagePayload[MESSAGE_MAX_LEN];

        JsonObject &payloadSend = payloads.at(i);

        // serialize the payloads for sending
        payloads.printTo(payloadSend);

        Serial.println(messagePayload);

        EVENT_INSTANCE *message = Esp32MQTTClient_Event_Generate(messagePayload, MESSAGE);

        Esp32MQTTClient_SendEventInstance(message);
      }

      lastCapture = 0;

      payloads = jsonPayloadBuffer.createArray();
    }
  }

  delay(readingsInterval);
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
  StaticJsonBuffer<1024> jsonBuffer;

  // Parse the root object
  JsonObject &root = jsonBuffer.parseObject(file);

  JsonArray &readingValues = root["Fathym"]["Readings"].as<JsonArray>();

  const char *readings[11];

  readingValues.copyTo(readings);

  for (int i = 0; i < (sizeof readings / sizeof readings[0]); i++)
  {
    strcpy(fathymReadings[i], readings[i]);
  }

  if (!root.success())
  {
    Serial.println(F("Failed to parse config file"));
    return false;
  }

  fathymConnectionStringPtr = root["Fathym"]["ConnectionString"].as<String>();

  fathymDeviceID = root["Fathym"]["DeviceID"].as<String>();

  readingsInterval = root["Fathym"]["ReadingInterval"] | 10;

  captureInterval = root["Fathym"]["CaptureInterval"] | 5000;

  // Close the file (File's destructor doesn't close the file)
  // ToDo: Handle multiple credentials

  file.close();
  return true;
}

// Function that gets current epoch time
unsigned long getTime()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    // Serial.println("Failed to obtain time");
    return (0);
  }
  time(&now);
  return now;
}
