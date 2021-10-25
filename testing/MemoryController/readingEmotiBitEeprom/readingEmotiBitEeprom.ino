/*
  Author: Nitin Nair(nitin@emotibit.com)

  This example can be used to test Read/Write functionality of the EEPROM 23AA02 on EMotiBit V04.

  - Usage
    - Uncomment the #define TEST_SYNC_RW to read/Write in async mode in this example.
	- The MemoryController class has been designed to work with an ISR. defining TEST_SYNC_RW enables testing without the ISR

 */

#include <Wire.h>
#include "wiring_private.h"
#include "EmotiBitVersionController.h"
#include "EmotiBitMemoryController.h"


struct SampleData
{
	float fData[5] = {1.23f,2.34f,3.45f,1.2345678f};
	uint8_t iData[10] = {0,1,2,3,4,5,6,7,8,9};
	String sData = "SampleData";
}sampleData;

void printData(SampleData* sampleData);
EmotiBitMemoryController emotibitMemoryController;
void readFromEeprom(EmotiBitMemoryController::DataType datatype);
void setup()
{
	Serial.begin(115200);
	while (!Serial.available())
	{
		Serial.println("enter a character to continue");
		delay(500);
	}
	Serial.read();
	
	TwoWire emotibit_i2c(&sercom1, 11, 13);
	emotibit_i2c.begin();
	pinPeripheral(11, PIO_SERCOM);
	pinPeripheral(13, PIO_SERCOM);
	emotibit_i2c.setClock(100000);

	Serial.println("Test for Reading EmotiBit EEPROM using memory controller");
	EmotiBitVersionController emotibitVersionController;
	

	EmotiBitVersionController::EmotiBitVersion hwVersion;
	hwVersion = emotibitVersionController.detectEmotiBitVersion(&emotibit_i2c, 0x50);
	emotibitMemoryController.init(emotibit_i2c, hwVersion);
	emotibitMemoryController.setHwVersion(hwVersion);

	Serial.print("Size of SampleData: "); Serial.println(sizeof(SampleData));
	Serial.println("Writing to the EEPROM");

	SampleData* sampData;
	sampData = &sampleData;
	uint8_t status;
	uint8_t dataFormatVersion = 0;
	// Writing into the Variant info space
	status = emotibitMemoryController.requestToWrite(EmotiBitMemoryController::DataType::VARIANT_INFO, dataFormatVersion, sizeof(SampleData),(uint8_t*)sampData);

	// writing into the EDA data space
	status = emotibitMemoryController.requestToWrite(EmotiBitMemoryController::DataType::EDA, dataFormatVersion+1, sizeof(SampleData), (uint8_t*)sampData, true);  // synchronous write

	if (status != 0)
	{
		Serial.println("Something went wrong");
	}
	else
	{
		Serial.println("EEPROM updated wiith contents");
	}

	Serial.println("Reading VARIANT_INFO from EEPROM");
	delay(1000);
	readFromEeprom(EmotiBitMemoryController::DataType::VARIANT_INFO);

	Serial.println("Reading EDA data from EEPROM");
	delay(1000);
	readFromEeprom(EmotiBitMemoryController::DataType::EDA);



	Serial.println("Reached end of code");
}

void readFromEeprom(EmotiBitMemoryController::DataType datatype)
{
	SampleData* sampData = nullptr;
	uint8_t* eepromData = nullptr;
	uint8_t size = 0;
	uint8_t status;
	status = emotibitMemoryController.requestToRead(datatype, eepromData, size, true);
	if (status == 0)
	{
		Serial.println("read successfull");
	}
	else
	{
		Serial.print("Read Error: "); Serial.println(status);
	}


	Serial.print("Data size read from EEPROM: "); Serial.println(size);
	sampData = (SampleData*)eepromData;
	printData(sampData);

	Serial.println("\nDeleting pointer");
	delete[] eepromData;
	eepromData = nullptr;
	sampData = nullptr;
}

void printData(SampleData* sampleData)
{
	for (int i = 0; i < 5; i++)
	{
		Serial.print("Float "); Serial.print(i); Serial.print(": "); 
		Serial.println(sampleData->fData[i], 7);
	}
	for (int i = 0; i < 10; i++)
	{
		Serial.print("uint8 "); Serial.print(i); Serial.print(": ");
		Serial.println(sampleData->iData[i]);
	}
	Serial.print("String: "); Serial.print(sampleData->sData);
}

void loop()
{

}