#include "EmotiBit.h"

#define SerialUSB SERIAL_PORT_USBVIRTUAL // Required to work in Visual Micro / Visual Studio IDE
const uint32_t SERIAL_BAUD = 2000000; //115200

EmotiBit emotibit;

void onShortButtonPress()
{
	// toggle wifi on/off
	if (emotibit.getWiFiMode() == EmotiBit::WiFiMode::NORMAL)
	{
		emotibit.setWiFiMode(EmotiBit::WiFiMode::OFF);
		Serial.println("WiFiMode::OFF");
	}
	else
	{
		emotibit.setWiFiMode(EmotiBit::WiFiMode::NORMAL);
		Serial.println("WiFiMode::NORMAL");
	}
}

void onLongButtonPress()
{
	emotibit.hibernate();
}

void onDataReady()
{
	// This is a placeholder
	// ToDo: attach this function
}

void setup() 
{
	Serial.begin(SERIAL_BAUD);
	Serial.println("Serial started");
	delay(2000);	// short delay to allow user to connect to serial, if desired
	//while (!Serial);

	emotibit.setup(EmotiBit::Version::V02H);

	// Attach callback functions
	emotibit.attachToShortButtonPress(&onShortButtonPress);
	emotibit.attachToLongButtonPress(&onLongButtonPress);
}

void loop() 
{
	emotibit.update();
}