/*
This example demonstrates the use of the IMU data captured using the EmotiBit to display Emoji on the charlie plex wing.
The charlieplex wing is stacked on the Emotibit using stacking headers.
*/


#include "EmotiBit.h"
#include "EmojiBit.h"

#define SerialUSB SERIAL_PORT_USBVIRTUAL // Required to work in Visual Micro / Visual Studio IDE
const uint32_t SERIAL_BAUD = 2000000; //115200

EmotiBit emotibit;
const size_t dataSize = EmotiBit::MAX_DATA_BUFFER_SIZE;
float data[dataSize];

// Initializing Charlieplex wing
EmojiBit matrix = EmojiBit();

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
	emotibit.hibernate();
}

void setup() 
{
	Serial.begin(SERIAL_BAUD);
	Serial.println("Serial started");
	delay(2000);	// short delay to allow user to connect to serial, if desired

	emotibit.setup(EmotiBit::Version::V02H);

	// Attach callback functions
	emotibit.attachShortButtonPress(&onShortButtonPress);
	emotibit.attachLongButtonPress(&onLongButtonPress);
	
	if (!matrix.begin()) {
		Serial.println("IS31 not found");
		while (1);
	}
	Serial.println("IS31 Found!");
}

void loop()
{
	//Serial.println("emotibit.update()");
	emotibit.update();
	
	// Choose which data channel to read from EmotiBit
	size_t dataAvailable = emotibit.readData(EmotiBit::DataType::ACCELEROMETER_Y, &data[0], dataSize);
	if (dataAvailable > 0)
	{
		// Hey cool, I got some data! Maybe I can light up my shoes whenever I get excited!

		// print the data to view in the serial plotter
		bool printData = true;
		if (printData)
		{
			for (size_t i = 0; i < dataAvailable && i < dataSize; i++)
			{
				// Note that dataAvailable can be larger than dataSize
				Serial.println(data[i]);
				if (data[i] <= -0.9) // choose a threshold to activate display
				{
					//matrix.clear();
					// Check out the ;ist of EMojis in EmojiTemplates.h
					matrix.drawEmoji(EMOJI::HEART, 1);
				}
				else
				{
					matrix.clear();
				}
			}
		}
	}
}