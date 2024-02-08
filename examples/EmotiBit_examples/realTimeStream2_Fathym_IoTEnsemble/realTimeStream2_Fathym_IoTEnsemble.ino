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

Emotibit emotibit;
const size_t dataSize = EmotiBit::MAX_DATA_BUFFER_SIZE;
float data[dataSize];

TaskHandle_t ReadTask;
TaskHandle_t CaptureTask;

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

  xTaskCreatePinnedToCore(ReadTaskRunner, "Task0", 10000, NULL, 1, &ReadTask, 0);
  // create a task that executes the Task0code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(CaptureTaskRunner, "CaptureTask", 10000, NULL, 1, &CaptureTask, 1);
}

void loop()
{
}

void ReadTaskRunner(void *pvParameters)
{
  Serial.print("ReadTask running on core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    emotibit.update();

    ReadTaskLoop(pvParameters);

    delay(10);
  }
}

void ReadTaskLoop(void *pvParameters)
{
  Serial.print("ReadTask loop running");
  enum EmotiBit::DataType dataType = loadDataTypeFromTypeTag("PG");
  uint32_t timestamp;
  size_t dataAvailable = emotibit.readData((EmotiBit::DataType)dataType, &data[0], dataSize, timestamp);

  if (dataAvailable > 0)
  {
    Serial.print("Reading for ");
    Serial.println(typeTag);
    Serial.print("\tDA: ");
    Serial.println(dataAvailable);
    Serial.print("\tDataSize: ");
    Serial.println(data.size());

    for (size_t i = 0; i < dataAvailable && i < dataSize; i++)
    {
      Serial.print("Reading for ");
      Serial.print(typeTag);
      Serial.print(" - ");
      Serial.print(i);
      Serial.print(" - ");
      Serial.print(timestamp);
      Serial.print(" - ");
      Serial.println(data[i]);
    }
  }
}

void CaptureTaskRunner(void *pvParameters)
{
  Serial.print("CaptureTask running on core ");
  Serial.println(xPortGetCoreID());

  for (;;)
  {
    Serial.println("CaptureTask processing");
    delay(10);
  }
}
