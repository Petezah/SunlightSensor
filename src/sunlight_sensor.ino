/*
 Name:		sunlight_sensor.ino
 Created:	4/24/2016 3:01:09 PM
 Author:	Peter Dunshee

 For Feather M0 Wifi and Adafruit Analog Light Sensor
 Connect sensor output to Analog Pin 0
 Connect 3.3v to VCC and GND to GND
*/

//#include <Adafruit_WINC1500Udp.h>
//#include <Adafruit_WINC1500SSLClient.h>
//#include <Adafruit_WINC1500Server.h>
//#include <Adafruit_WINC1500MDNS.h>
//#include <Adafruit_WINC1500Client.h>
#include <Adafruit_SleepyDog.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ASFCore.h>
#include <WiFi101.h>

#include "config.h" // See comment in "sample-config.h"

// Wifi
#define WINC_CS		8
#define WINC_IRQ	7
#define WINC_RST	4
#define WINC_EN		2

int status = WL_IDLE_STATUS;

// Sensors / buttons
const int sensorPin = A0;    // select the input pin for the potentiometer

const int buttonPinA = 9;
const int buttonPinB = 6;
const int buttonPinC = 5;

#define VBATPIN A7

const float rawRange = 1024; // 3.3v
const float logRange = 5.0; // 3.3v = 10^5 lux

// Set up the wifi client
WiFiClient client;

// Setup a feed called 'sunlight-intensity' for publishing changes.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
const char LIGHT_SENSOR_FEED[] PROGMEM = AIO_USERNAME "/feeds/sunlight-intensity";
const char LIGHT_SENSOR_BATTERY_FEED[] PROGMEM = AIO_USERNAME "/feeds/sunlight-intensity-battery";
const char LIGHT_SENSOR_RESET_FEED[] PROGMEM = AIO_USERNAME "/feeds/sunlight-intensity-reset";

// Store the MQTT server, client ID, username, and password in flash memory. 
 // This is required for using the Adafruit MQTT library. 
const char MQTT_SERVER[] PROGMEM = AIO_SERVER;
// Set a unique MQTT client ID using the AIO key + the date and time the sketch 
// was compiled (so this should be unique across multiple devices for a user, 
// alternatively you can manually set this to a GUID or other random value). 
const char MQTT_CLIENTID[] PROGMEM = __TIME__ AIO_USERNAME;
const char MQTT_USERNAME[] PROGMEM = AIO_USERNAME;
const char MQTT_PASSWORD[] PROGMEM = AIO_KEY;

Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, AIO_SERVERPORT, MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD);
Adafruit_MQTT_Publish lightSensorFeed = Adafruit_MQTT_Publish(&mqtt, LIGHT_SENSOR_FEED);
Adafruit_MQTT_Publish lightSensorBatteryFeed = Adafruit_MQTT_Publish(&mqtt, LIGHT_SENSOR_BATTERY_FEED);
Adafruit_MQTT_Publish lightSensorResetFeed = Adafruit_MQTT_Publish(&mqtt, LIGHT_SENSOR_RESET_FEED);

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

void setup()
{
	WiFi.setPins(WINC_CS, WINC_IRQ, WINC_RST);

	Watchdog.enable(20000); // Reset if we crash; wait at least 20 seconds

	Serial.begin(9600);
	Serial.println("Adafruit Analog Light Sensor Test");
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
	display.display();

	pinMode(buttonPinA, INPUT);
	pinMode(buttonPinB, INPUT);
	pinMode(buttonPinC, INPUT);

	delay(200);

#ifdef WINC_EN
	pinMode(WINC_EN, OUTPUT);
	digitalWrite(WINC_EN, HIGH);
#endif

	display.clearDisplay();
	display.setCursor(0, 0);
	display.setTextColor(WHITE);
	display.print("Light sensor -- Init Wifi");
	display.display();

	// Check for presence of breakout
	if (WiFi.status() == WL_NO_SHIELD)
	{
		display.print("WINC1500 not present");
		display.display();
		while (true); // do not continue
	}

	WiFi.lowPowerMode(); // M2M_PS_H_AUTOMATIC go into power save mode when possible!

	// Ok to proceed
	display.println(" -- OK!");
	display.display();
	delay(500);
}

int resetValue = 0;
void loop()
{
	Watchdog.reset();

	// Ensure the connection to the MQTT server is alive (this will make the first
	// connection and automatically reconnect when disconnected).  See the MQTT_connect
	// function definition further below.
	MQTT_connect();
	Watchdog.reset();

	if (resetValue == 0)
	{
		lightSensorResetFeed.publish((int32_t)1); // 1, we reset
		resetValue++;
	}
	lightSensorResetFeed.publish((int32_t)-1); // -1, running; we will remain at this value

	// read the raw value from the sensor:
	int rawValue = analogRead(sensorPin);
	int buttonA = digitalRead(buttonPinA);
	int buttonB = digitalRead(buttonPinB);
	int buttonC = digitalRead(buttonPinC);

	float measuredvbat = analogRead(VBATPIN);
	measuredvbat *= 2;      // v divided by 2, multiply back
	measuredvbat *= 3.3;    // multiply by ref voltage
	measuredvbat /= 1024.0; // convert to voltage

	display.clearDisplay();
	display.setCursor(0, 0);
	display.setTextColor(WHITE);

	drawData(rawValue, buttonA, buttonB, buttonC, measuredvbat);

	lightSensorFeed.publish(RawToLux(rawValue));
	lightSensorBatteryFeed.publish(measuredvbat);

	Watchdog.reset(); // make sure device stays alive

	display.display();
	delay(5000); // wait 5s to send next messages
}

union ip_string
{
	uint32_t ip;
	uint8_t c[4];
};

void drawData(int rawValue, int buttonA, int buttonB, int buttonC, float vbat)
{
	ip_string ip2str;
	ip2str.ip = WiFi.localIP();

	display.print("Raw = ");
	display.println(rawValue);
	display.print("Lux = ");
	display.println(RawToLux(rawValue));
	display.print(ip2str.c[0]);
	display.print(".");
	display.print(ip2str.c[1]);
	display.print(".");
	display.print(ip2str.c[2]);
	display.print(".");
	display.println(ip2str.c[3]);
	//display.print(" - A ");
	//display.print(buttonA);
	//display.print(" B ");
	//display.print(buttonB);
	//display.print(" C ");
	//display.println(buttonC);
	display.print("Vbat: ");
	display.println(vbat);
}

float RawToLux(int raw)
{
	float logLux = raw * logRange / rawRange;
	return pow(10, logLux);
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
	int8_t ret;

	// attempt to connect to Wifi network:
	while (WiFi.status() != WL_CONNECTED)
	{
		display.clearDisplay();
		display.setCursor(0, 0);
		display.print("Attempting to connect to SSID: ");
		display.println(WLAN_SSID);
		display.display();

		// Connect to WPA/WPA2 network. Change this line if using open or WEP network:
		status = WiFi.begin(WLAN_SSID, WLAN_PASS);

		// wait 10 seconds for connection:
		uint8_t timeout = 10;
		while (timeout && (WiFi.status() != WL_CONNECTED)) {
			Watchdog.reset(); // make sure device stays alive
			timeout--;
			delay(1000);
		}
	}

	// Stop if already connected.
	if (mqtt.connected()) {
		return;
	}

	Watchdog.reset(); // make sure device stays alive
	display.print("Connecting to MQTT... ");
	display.display();

	while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
		Watchdog.reset(); // make sure device stays alive
		display.println(mqttConnectErrorString(ret));
		display.println("Retrying MQTT connection in 5 seconds...");
		display.display();
		mqtt.disconnect();
		delay(5000);  // wait 5 seconds
	}

	Watchdog.reset(); // make sure device stays alive
	display.println("MQTT Connected!");
}

const __FlashStringHelper* mqttConnectErrorString(int8_t code) {
	switch (code) {
	case 1: return F("Wrong protocol");
	case 2: return F("ID rejected");
	case 3: return F("Server unavail");
	case 4: return F("Bad user/pass");
	case 5: return F("Not authed");
	case 6: return F("Failed to subscribe");
	case -1: return F("Connection failed");
	default: return F("Unknown error");
	}
}

char *dtostrf(double value, int width, unsigned int precision, char *result)
{
	int decpt, sign, reqd, pad;
	const char *s, *e;
	char *p;
	s = fcvt(value, precision, &decpt, &sign);
	if (precision == 0 && decpt == 0) {
		s = (*s < '5') ? "0" : "1";
		reqd = 1;
	}
	else {
		reqd = strlen(s);
		if (reqd > decpt) reqd++;
		if (decpt == 0) reqd++;
	}
	if (sign) reqd++;
	p = result;
	e = p + reqd;
	pad = width - reqd;
	if (pad > 0) {
		e += pad;
		while (pad-- > 0) *p++ = ' ';
	}
	if (sign) *p++ = '-';
	if (decpt <= 0 && precision > 0) {
		*p++ = '0';
		*p++ = '.';
		e++;
		while (decpt < 0) {
			decpt++;
			*p++ = '0';
		}
	}
	while (p < e) {
		*p++ = *s++;
		if (p == e) break;
		if (--decpt == 0) *p++ = '.';
	}
	if (width < 0) {
		pad = (reqd + width) * -1;
		while (pad-- > 0) *p++ = ' ';
	}
	*p = 0;
	return result;
}
