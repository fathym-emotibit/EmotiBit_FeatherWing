#include "Esp32MQTTClient.h"
#include "EmotiBit.h"
#include "time.h"

#define SerialUSB SERIAL_PORT_USBVIRTUAL // Required to work in Visual Micro / Visual Studio IDE
#define MESSAGE_MAX_LEN (1024 * 250) // Set to a little short of max size of IoT Hub Messages
const uint32_t SERIAL_BAUD = 115200; //115200

EmotiBit emotibit;
const size_t dataSize = EmotiBit::MAX_DATA_BUFFER_SIZE;
float data[dataSize];
static bool hasIoTHub = false;
unsigned long epochTime;
const char* ntpServer = "pool.ntp.org";
String fathymConnectionStringPtr;
String fathymDeviceID;
char fathymReadings[25][2] = {{}};
int readingsInterval;
//char metadataTypeTags[2];
int captureInterval;
long lastCapture;
StaticJsonDocument<16384> jsonPayloadDoc; // 1024 * 16
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

EmotiBit::DataType loadDataTypeFromTypeTag(String typeTag) {
  if (typeTag == "AX"){
    EmotiBit::DataType dataType {EmotiBit::DataType::ACCELEROMETER_X};
    return dataType;
  } 
  else if (typeTag == "AY"){
    EmotiBit::DataType dataType {EmotiBit::DataType::ACCELEROMETER_Y};
    return dataType;
  }
  else if (typeTag == "AZ"){
    EmotiBit::DataType dataType {EmotiBit::DataType::ACCELEROMETER_Z};
    return dataType;
  }
  else if (typeTag == "GX"){
    EmotiBit::DataType dataType {EmotiBit::DataType::GYROSCOPE_X};
    return dataType;
  }
  else if (typeTag == "GY"){
    EmotiBit::DataType dataType {EmotiBit::DataType::GYROSCOPE_Y};
    return dataType;
  }
  else if (typeTag == "GZ"){
    EmotiBit::DataType dataType {EmotiBit::DataType::GYROSCOPE_Z};
    return dataType;
  }
  else if (typeTag == "MX"){
    EmotiBit::DataType dataType {EmotiBit::DataType::MAGNETOMETER_X};
    return dataType;
  }
  else if (typeTag == "MY"){
    EmotiBit::DataType dataType {EmotiBit::DataType::MAGNETOMETER_Y};
    return dataType;
  }
  else if (typeTag == "MZ"){
    EmotiBit::DataType dataType {EmotiBit::DataType::MAGNETOMETER_Z};
    return dataType;
  }
  else if (typeTag == "EA"){
    EmotiBit::DataType dataType {EmotiBit::DataType::EDA};
    return dataType;
  }
  else if (typeTag == "EL"){
    EmotiBit::DataType dataType {EmotiBit::DataType::EDL};
    return dataType;
  }
  else if (typeTag == "ER"){
    EmotiBit::DataType dataType {EmotiBit::DataType::EDR};
    return dataType;
  }
  else if (typeTag == "H0"){
    EmotiBit::DataType dataType {EmotiBit::DataType::HUMIDITY_0};
    return dataType;
  }
  else if (typeTag == "T0"){
    EmotiBit::DataType dataType {EmotiBit::DataType::TEMPERATURE_0};
    return dataType;
  }
  else if (typeTag == "TH"){
    EmotiBit::DataType dataType {EmotiBit::DataType::THERMOPILE};
    return dataType;
  }
  else if (typeTag == "PI"){
    EmotiBit::DataType dataType {EmotiBit::DataType::PPG_INFRARED};
    return dataType;
  }
  else if (typeTag == "PR"){
    EmotiBit::DataType dataType {EmotiBit::DataType::PPG_RED};
    return dataType;
  }
  else if (typeTag == "PG"){
    EmotiBit::DataType dataType {EmotiBit::DataType::PPG_GREEN};
    return dataType;
  }
  else if (typeTag == "DB"){
    EmotiBit::DataType dataType {EmotiBit::DataType::DEBUG};
    return dataType;
  } 
}

void setup()
{
  Serial.begin(SERIAL_BAUD);
  Serial.println("Serial started");
  delay(2000);  // short delay to allow user to connect to serial, if desired

  emotibit.setup();

  if (!loadConfigFile(emotibit._configFilename)) {
    Serial.println("SD card configuration file parsing failed.");
    Serial.println("Create a file 'config.txt' with the following JSON:");
    Serial.println("{\"WifiCredentials\": [{\"ssid\": \"SSSS\", \"password\" : \"PPPP\"}],\"Fathym\":{\"ConnectionString\": \"xxx\", \"DeviceID\": \"yyy\"}}");
  }
  
  const char* connStr = fathymConnectionStringPtr.c_str();
  
  if (!Esp32MQTTClient_Init((const uint8_t*)connStr, true))
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

  payloads = jsonPayloadDoc.to<JsonArray>();
}

void loop()
{
  emotibit.update();

  lastCapture += millis();
  
  // allocate the memory for the document
  const size_t CAPACITY = JSON_OBJECT_SIZE(1);
  
  StaticJsonDocument<1024> doc;
  
  //JsonObject payload = doc.createObject();

  doc[String("DeviceID")] = fathymDeviceID;

  doc["DeviceType"] = "emotibit";

  doc["Version"] = "1";

  JsonObject payloadDeviceData = doc.createNestedObject("DeviceData");

  JsonObject payloadSensorReadings = doc.createNestedObject("SensorReadings");

  JsonObject payloadSensorMetadata = doc.createNestedObject("SensorMetadata");

  JsonObject payloadSensorMetadataMicrosAdjustments = payloadSensorMetadata.createNestedObject("MicrosAdjustments");

  JsonObject payloadSensorMetadataRoot = payloadSensorMetadata.createNestedObject("_");

  epochTime = getTime();

  long microsAdjustmentStart = micros();

  payloadDeviceData["Timestamp"] = String(epochTime);

  for (String typeTag : fathymReadings) {     
    enum EmotiBit::DataType dataType = loadDataTypeFromTypeTag(typeTag);
    size_t dataAvailable = emotibit.readData((EmotiBit::DataType)dataType, &data[0], dataSize);
    //Serial.println("Data Availalbe Size: ");
    //Serial.println(dataAvailable);
    //Serial.println("Reading Types: ");
    //Serial.println(typeTag);
    //Serial.println(String(data[dataAvailable - 1]));

    if (dataAvailable > 0)
    {
      payloadSensorReadings[typeTag] = String(data[dataAvailable - 1]);

      payloadSensorMetadataMicrosAdjustments[typeTag] = micros() - microsAdjustmentStart;
      //payloadSensorReadings[typeTag] = String(data[0]);
    }
  }
  //static DigitalFilter filterBattVolt(DigitalFilter::FilterType::IIR_LOWPASS, ((BASE_SAMPLING_FREQ) / (BATTERY_SAMPLING_DIV)) / (_samplesAveraged.battery), 0.01);

  float battVolt = emotibit.readBatteryVoltage();

  //battVolt = filterBattVolt.filter(battVolt);

  float battPcent = emotibit.getBatteryPercent(battVolt);

  payloadSensorMetadata["BatteryVoltage"] = battVolt;

  payloadSensorMetadata["BatteryPercentage"] = battPcent;

  payloadSensorMetadata["MACAddress"] = emotibit.getFeatherMacAddress();

  //payloadSensorMetadata["EmotibitVersion"] = emotibit.detectEmotiBitVersion();

  //payloadSensorMetadata["HardwareVersion"] = emotibit.getHardwareVersion();

  payloads.add(doc.to<JsonArray>());

  bool isMemoryAllocated = payloads.size() >= 250;

  bool isCaptureInterval = (lastCapture + captureInterval) >= 0;

  if (isCaptureInterval || isMemoryAllocated){
    for (int i = 0; i < payloads.size(); i++){
      char messagePayload[MESSAGE_MAX_LEN];

      JsonObject payloadSend = payloads[i];

      // serialize the payload for sending
      serializeJson(doc, messagePayload);

      Serial.println(messagePayload);

      EVENT_INSTANCE* message = Esp32MQTTClient_Event_Generate(messagePayload, MESSAGE);

      Esp32MQTTClient_SendEventInstance(message);
    }

    lastCapture = 0;

    payloads.clear();

    jsonPayloadDoc.clear();
  }

  delay(readingsInterval);
}

// Loads the configuration from a file
bool loadConfigFile(const char *filename) {
  // Open file for reading
  File file = SD.open(filename);

  if (!file) {
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
  //JsonObject root = jsonBuffer.parseObject(file);

  JsonArray readingValues = doc["Fathym"]["Readings"].as<JsonArray>();
  
  const char* readings[25];

  copyArray(readingValues, readings);
  
  //readingValues.copyTo(readings);

  //Serial.println(
  for(int i = 0; i < (sizeof readings / sizeof readings[0]); i++){
    strcpy(fathymReadings[i], readings[i]);
  }
    
  //}

  if (doc.isNull()) {
    Serial.println(F("Failed to parse config file"));
    return false;
  }

  fathymConnectionStringPtr = doc["Fathym"]["ConnectionString"].as<String>();
  
  fathymDeviceID = doc["Fathym"]["DeviceID"].as<String>();

  readingsInterval = doc["Fathym"]["ReadingInterval"] | 10;

  readingsInterval = doc["Fathym"]["CaptureInterval"] | 5000;

  // Close the file (File's destructor doesn't close the file)
  // ToDo: Handle multiple credentials

  file.close();
  return true;
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
