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
//JsonArray payloads;

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
  else if (typeTag == "DB")
    return EmotiBit::DataType::DEBUG;
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
  Serial.println("After attachbutton: ");
  configTime(0, 0, ntpServer);
  Serial.println("After configTime: ");

  lastCapture = 0;

  //payloads = jsonPayloadDoc.to<JsonArray>();
  Serial.println("After convert payloads: ");
}

void loop()
{
  Serial.println("Before emotibit update: ");
  emotibit.update();
  Serial.println("After emotibit update: ");
  lastCapture += millis();
  
  Serial.println("After set lastCapture: ");
  // allocate the memory for the document
  const size_t CAPACITY = JSON_OBJECT_SIZE(1);
  
  StaticJsonDocument<1024> doc;
  
  //StaticJsonDocument<1024> jsonPayloadDoc; // 1024 * 64

  //JsonObject payload = doc.createObject();

  doc[String("DeviceID")] = fathymDeviceID;

  doc["DeviceType"] = "emotibit";

  doc["Version"] = "1";
  Serial.println("After setting deviceid and type/version: ");

  JsonObject payloadDeviceData = doc.createNestedObject("DeviceData");

  JsonObject payloadSensorReadings = doc.createNestedObject("SensorReadings");

  JsonObject payloadSensorMetadata = doc.createNestedObject("SensorMetadata");

  JsonObject payloadSensorMetadataMicrosAdjustments = payloadSensorMetadata.createNestedObject("MicrosAdjustments");

  JsonObject payloadSensorMetadataRoot = payloadSensorMetadata.createNestedObject("_");

  epochTime = getTime();

  long microsAdjustmentStart = micros();

  payloadDeviceData["Timestamp"] = String(epochTime);

  Serial.println("After timestamps: ");

  for (String typeTag : fathymReadings) {     
    Serial.println("Inside For loop: ");
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
  //JsonArray payloads = doc.as<JsonArray>();

  //if(payloads != NULL){
    //payloads.add(doc.to<JsonArray>());
  //}
  

  //bool isMemoryAllocated = payloads.size() >= 250;

  //bool isCaptureInterval = (lastCapture + captureInterval) >= 0;

  //if (isCaptureInterval || isMemoryAllocated){
    //for (int i = 0; i < payloads.size(); i++){
      char messagePayload[MESSAGE_MAX_LEN];

      //JsonObject payloadSend = payloads[i];

      // serialize the payload for sending
      serializeJson(doc, messagePayload);

      Serial.println(messagePayload);

      EVENT_INSTANCE* message = Esp32MQTTClient_Event_Generate(messagePayload, MESSAGE);

      Esp32MQTTClient_SendEventInstance(message);
    //}

    //lastCapture = 0;

    //payloads.clear();

    //jsonPayloadDoc.clear();
  //}

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
  //Serial.print("AFter deserialize: ");
  JsonArray readingValues = doc["Fathym"]["Readings"].as<JsonArray>();
  //Serial.println("After reading values: ");
  //int size = static_cast<int>(readingValues.size());
  //Serial.println(size);
  const char* readings[25];

  //Serial.println(readingValues.size());
  copyArray(readingValues, readings);
  
  Serial.println("After copy array: ");
  //readingValues.copyTo(readings);
  //Serial.println(sizeof fathymReadings / sizeof fathymReadings[0]);

  //Serial.println(sizeof readings[0]);

  //Serial.println(sizeof readingValues / sizeof readingValues[0]);
  //Serial.println(
  for(int i = 0; i < (readingValues.size()); i++){
    strcpy(fathymReadings[i], readings[i]);    
  }

  //Serial.print("After copying readings: ");  
  //}

  if (doc.isNull()) {
    Serial.println(F("Failed to parse config file"));
    return false;
  }

  fathymConnectionStringPtr = doc["Fathym"]["ConnectionString"].as<String>();
  
  //Serial.print("After conn: ");

  fathymDeviceID = doc["Fathym"]["DeviceID"].as<String>();

  //Serial.print("After deviceid: ");

  readingsInterval = doc["Fathym"]["ReadingInterval"] | 10;

  //Serial.print("After reading interval: ");

  readingsInterval = doc["Fathym"]["CaptureInterval"] | 5000;

  //Serial.print("After capture interval: ");

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
