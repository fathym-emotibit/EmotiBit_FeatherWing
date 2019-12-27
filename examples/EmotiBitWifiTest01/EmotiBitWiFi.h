/// EmotiBitWifi
///
/// Supports WiFi communications with the EmotiBit 
///
/// Written by produceconsumerobot Dec 2019
///
/// **** Overview of EmotiBitWiFi communication ****
/// EmotiBit uses 3 separate channels for network advertising, control
/// and data transmission. Exactly what protocols are used for each 
///	of these channels can change, but typically the following are used:
///		- Advertising: UDP on hard-coded EMOTIBIT_WIFI_ADVERTISING_PORT
///		- Control: TCP on port chosen by host 
///		- Data: UDP on port chosen by host 
///	Typical Usage:
///   - in setup():
///		  - Call EmotiBitWiFi.begin(ssid, password) in setup() to setup WiFi+advertising connection
///   - in loop():
///		  - Call EmotiBitWifi.processAdvertising() to handle advertising and connection requests
///     - Call emotibitWifi.readControl(String& controlPacket) to read an incoming control packet
///     - Call emotibitWifi.sendData(String& data) to send data
///	Typical program flow (behind the scenes)
///		- Host messages the network to determine which EmotiBits are present
///			- All EmotiBits respond indicating availability for connection
///		- Host initiates a connection with a specific EmotiBit on a specified port
///		- EmotiBit connects to the control channel and starts sending data on the specified port
///		- Host sends periodic messages to maintain Host-EmotiBit connection
///		- Host initiates disconnection from EmotiBit


#pragma once

#include <SPI.h>
#include <WiFi101.h>
#include <WiFiUdp.h>
#include "EmotiBitComms.h"
#include "EmotiBitPacket.h"

class EmotiBitWiFi {
public:
	//typedef struct Credential
	//{
	//	String ssid = "";
	//	String password = "";
	//};

	uint16_t _advertisingPort = EmotiBitComms::WIFI_ADVERTISING_PORT;	// Port to advertise EmotiBits on the network
	uint16_t _controlPort;								// Port to toggle EmotiBit controls
	uint16_t _dataPort;								// Port to sent EmotiBit data

	IPAddress _hostIp;

	WiFiClient _controlCxn;
	WiFiUDP _advertisingCxn;
	WiFiUDP _dataCxn;

	bool _isConnected = false;
	int _wifiStatus = WL_IDLE_STATUS;
	int _keyIndex = 0;            // your network key Index number (needed only for WEP)
	bool gotIp = false;
	uint8_t _nUdpSends = 1; // Number of times to send the same packet (e.g. for UDPx3 = 3)

	static const uint8_t SUCCESS = 0;
	static const uint8_t FAIL = -1;
	static const uint16_t MAX_SEND_LEN = 512;
	static const uint32_t WIFI_BEGIN_ATTEMPT_DELAY = 5000;
	static const uint32_t WIFI_BEGIN_SWITCH_CRED = 300000;      //Set to 30000 for debug, 300000 for Release
	static const uint32_t SETUP_TIMEOUT = 61500;          //Enough time to run through list of network credentials twice
	static const uint8_t MAX_WIFI_CONNECT_HISTORY = 20; // NO. of wifi connectivity status to remember
	uint16_t CONNECTION_TIMEOUT = 5000;

	char _inPacketBuffer[EmotiBitPacket::MAX_TO_EMOTIBIT_PACKET_LEN + 1]; //buffer to hold incoming packet
	EmotiBitPacket::Header _packetHeader;
#ifdef ARDUINO
	String _receivedAdvertisingMessage = ""; 
	String _receivedControlMessage = "";
#else

#endif

	uint16_t advertisingPacketCounter = 0;
	uint16_t controlPacketCounter = 0;
	uint16_t dataPacketCounter = 0;

	int8_t setup();
	int8_t begin(const char* ssid, const char* pass);
	int8_t end();
	//int8_t update();
	int8_t processAdvertising();
	int8_t connect(IPAddress hostIp, const String&);
	int8_t connect(IPAddress hostIp, uint16_t controlPort, uint16_t dataPort);
	int8_t disconnect();
	uint8_t readControl(String& packet);
	int8_t sendControl(String& message);
	int8_t sendData(String& message);
	int8_t sendAdvertising(const String& message, const IPAddress& ip, const uint16_t& port);

	int8_t sendUdp(WiFiUDP& udp, const String& message, const IPAddress& ip, const uint16_t& port);
};
