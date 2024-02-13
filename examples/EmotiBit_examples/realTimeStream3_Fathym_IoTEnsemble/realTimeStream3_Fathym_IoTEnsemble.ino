#include "Esp32MQTTClient.h"
#include "EmotiBit.h"
#include "time.h"
#include "EmotiBitVersionController.h"
#include "EmotiBitVariants.h"
// #include "EmotiBitNvmController.h"
// #include <Wire.h>
TaskHandle_t Task0;
TaskHandle_t Task1;

#define SerialUSB SERIAL_PORT_USBVIRTUAL                                 // Required to work in Visual Micro / Visual Studio IDE
#define BATCH_SIZE (10)                                                  // The number of messages to batch into a single call
#define HUB_MESSAGE_MAX_LEN (1000 * 64)                                  // Set to max size of IoT Hub Messages (256 KB)
#define PAYLOAD_MAX_SIZE (HUB_MESSAGE_MAX_LEN / BATCH_SIZE)              // The max size of a single payload ~5kb
#define PAYLOADS_MAX_SIZE (HUB_MESSAGE_MAX_LEN - (PAYLOAD_MAX_SIZE * 3)) // The maximum size of all collected payloads
const uint32_t SERIAL_BAUD = 2000000;                                    // 115200

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

  // create a task that executes the Task0code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(Task0code, "Task0", 10000, NULL, 1, &Task0, 0);
  // create a task that executes the Task0code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(Task1code, "Task1", 10000, NULL, 1, &Task1, 1);
}

void loop()
{
  // nothing to do here, everything happens in the Task1Code and Task2Code functions
  Serial.println("Loop");
  delay((int)random(1000, 3000));
}

void Task0code(void *pvParameters)
{
  Serial.print("Task0 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    Serial.println("Core 0 processing");
    delay((int)random(100, 1000));
  }
}

void Task1code(void *pvParameters)
{
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    Serial.println("Core 1 processing");
    delay((int)random(100, 1000));
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
