#include <Arduino.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>

#define MODE_OFF	0
#define MODE_COLOR	1
#define MODE_SPOT	2
#define MODE_FADE_COLOR	3

#define FADE_UP		0
#define FADE_DOWN	1

//
// change this
//
#define LEDS 100
const char *ssid = "your-ssid";
const char *password = "your-password";
const char *mdnsname = "wifi-led-stripes";

unsigned char ledStrip[LEDS][4];
unsigned char mode = MODE_COLOR;
unsigned long lastMillis;
unsigned int spotPosition = 0;
unsigned int spotSpeed = 50;
unsigned int spotMaxBrightness = 6;
unsigned int fadeSpeed = 50;
bool fadeIn = false;
bool fadeOut = false;
bool fadeDirection = FADE_UP;

ESP8266WebServer server(80);

unsigned char setBrightness(unsigned char level);
unsigned char getBrightness(unsigned char level);
void lightsOn();
void fadeColor();
void fadeSpot();
void setSpot(unsigned int position, unsigned int maxBrightness);
void handleRoot();
void handleColor();
void handleSpot();
void handleNotFound();
String html();

void setup(void) {
	// basic setup
	Serial.begin(115200);
	delay(10);
	Serial.println();

	// ledstrip setup
	SPI.begin();
	SPI.setBitOrder(MSBFIRST);
	for (int i = 0; i < LEDS; i++) {
		ledStrip[i][0] = setBrightness(10);
		ledStrip[i][1] = 0x00;
		ledStrip[i][2] = 0x00;
		ledStrip[i][3] = 0x00;
	}
	lightsOn();

	// wifi setup
	WiFi.begin(ssid, password);
	Serial.print("Connecting ...");
	while (WiFi.status() != WL_CONNECTED) {
		delay (250);
		Serial.print(".");
	}
	Serial.println();
	Serial.print("Connected to: ");
	Serial.println(ssid);
	Serial.print("IP-Address: ");
	Serial.println(WiFi.localIP());

	/// mdns setup
	if (MDNS.begin(mdnsname)) {
		Serial.print("mDNS responder started: ");
		Serial.print(mdnsname);
		Serial.println(".local");
	} else {
		Serial.println("Error setting up MDNS responder!");
	}

	// http server setup
	server.on("/", HTTP_GET, handleRoot);
	server.on("/color", HTTP_GET, handleColor);
	server.on("/spot", HTTP_GET, handleSpot);
	server.onNotFound(handleNotFound);
	server.begin();
	Serial.println("HTTP server started");
}

void loop(void) {
	server.handleClient();
	MDNS.update();
	switch (mode) {
		case MODE_FADE_COLOR:
			fadeColor();
			break;
		case MODE_SPOT:
			if (spotSpeed != 0)
				fadeSpot();
			break;
		default:
			break;
	}

}

unsigned char setBrightness(unsigned char level) {
	// set levels from 0 to 31
	return (0xe0 | level);
}

unsigned char getBrightness(unsigned char level) {
	return (0x1f & level);
}

void lightsOn() {
	for (int i = 0; i <= 3; i++) {
		SPI.transfer(0x00);
	}
	for (int i = 0; i < LEDS; i++) {
		SPI.transfer(ledStrip[i][0]);
		SPI.transfer(ledStrip[i][1]);
		SPI.transfer(ledStrip[i][2]);
		SPI.transfer(ledStrip[i][3]);
	}
	for (int i = 0; i < LEDS/2; i++) {
		SPI.transfer(0x01);
	}
}

void fadeColor() {
	if (lastMillis + fadeSpeed > millis()) {
		return;
	}

	lastMillis = millis();
	unsigned char currentBrightness = getBrightness(ledStrip[0][0]);

	if (fadeDirection == FADE_UP) {
		currentBrightness++;
		if (currentBrightness == 31) {
			fadeDirection = FADE_DOWN;
		}
	}

	if (fadeDirection == FADE_DOWN) {
		currentBrightness--;
		if (currentBrightness == 0) {
			fadeDirection = FADE_UP;
			unsigned char blue = random(256);
			unsigned char green = random(256);
			unsigned char red = random(256);
			for (int i = 0; i < LEDS; i++) {
				ledStrip[i][1] = blue;
				ledStrip[i][2] = green;
				ledStrip[i][3] = red;
			}
		}
	}
	for (int i = 0; i < LEDS; i++) {
		ledStrip[i][0] = setBrightness(currentBrightness);
	}
	lightsOn();
}

void fadeSpot() {
	if (lastMillis + spotSpeed > millis()) {
		return;
	}
	lastMillis = millis();

	if (fadeOut) {
		for (int i = 0; i < LEDS; i++) {
			unsigned char currentBrightness = getBrightness(ledStrip[i][0]);
			if (currentBrightness > 0) {
				ledStrip[i][0] = setBrightness(currentBrightness - 1);
			}
		}
		if (getBrightness(ledStrip[LEDS - 1][0]) == 0) {
			fadeOut = false;
			fadeIn = true;
		}
		lightsOn();
		return;
	}

	if (fadeIn) {
		for (int i = 0; i < LEDS; i++) {
			unsigned char currentBrightness = getBrightness(ledStrip[i][0]);
			ledStrip[i][0] = setBrightness(currentBrightness + 1);
			if (currentBrightness == 0) {
				break;
			}
		}
		if (getBrightness(ledStrip[0][0]) == spotMaxBrightness) {
			spotPosition = 0;
			fadeIn = false;
		}
		lightsOn();
		return;
	}

	spotPosition++;
	if (spotPosition == LEDS) {
		fadeOut = true;
	}

	setSpot(spotPosition, spotMaxBrightness);
}

void setSpot(unsigned int position, unsigned int maxBrightness) {
	ledStrip[position][0] = setBrightness(maxBrightness);

	for (int i = position + 1; i < LEDS; i++) {
		int lastBrightness = getBrightness(ledStrip[i - 1][0]);
		int newBrightness = 0;
		if (lastBrightness > 0) {
			newBrightness = lastBrightness - 1;
		}
		ledStrip[i][0] = setBrightness(newBrightness);
	}

	for (int i = position - 1; i >= 0; i--) {
		int lastBrightness = getBrightness(ledStrip[i + 1][0]);
		int newBrightness = 0;
		if (lastBrightness > 0) {
			newBrightness = lastBrightness - 1;
		}
		ledStrip[i][0] = setBrightness(newBrightness);
	}
	lightsOn();
}

void handleRoot() {
	if (server.hasArg("action") && server.arg("action") == "Aus") {
		mode = MODE_OFF;
		for (int i = 0; i < LEDS; i++) {
			ledStrip[i][0] = setBrightness(0);
		}
		lightsOn();
	}
	if (server.hasArg("action") && server.arg("action") == "Farbwechsel") {
		mode = MODE_FADE_COLOR;
	}
	server.send(200, "text/html", html());
}

void handleColor() {
	mode = MODE_COLOR;
	char c_blue[3] = { server.arg("colorpicker").charAt(5), server.arg("colorpicker").charAt(6), '\0' };
	char c_green[3] = { server.arg("colorpicker").charAt(3), server.arg("colorpicker").charAt(4), '\0' };
	char c_red[3] = { server.arg("colorpicker").charAt(1), server.arg("colorpicker").charAt(2), '\0' };
	unsigned char blue = (int)strtol(c_blue, NULL, 16); 
	unsigned char green = (int)strtol(c_green, NULL, 16); 
	unsigned char red = (int)strtol(c_red, NULL, 16); 
	unsigned char brightness = server.arg("brightness").toInt();
	for (int i = 0; i < LEDS; i++) {
		ledStrip[i][0] = setBrightness(brightness);
		ledStrip[i][1] = blue;
		ledStrip[i][2] = green;
		ledStrip[i][3] = red;
	}
	lightsOn();
	server.send(200, "text/html", html());
}

void handleSpot() {
	mode = MODE_SPOT;

	spotPosition = server.arg("position").toInt();
	spotSpeed = server.arg("speed").toInt();
	spotMaxBrightness = server.arg("brightness").toInt();

	if (spotSpeed != 0) {
		spotSpeed = map(spotSpeed,1,100,100,10);
	}

	setSpot(spotPosition, spotMaxBrightness);
	server.send(200, "text/html", html());
}

void handleNotFound() {
	server.sendHeader("Location", "/", true);
	server.send(307, "text/plain", ""); 
}

String html() {
	//
	// Color-Picker
	//

	// get rgb value
	String hexValue = "#";
	char red[3];
	char green[3];
	char blue[3];
	sprintf(red, "%02x", ledStrip[0][3]);
	sprintf(green, "%02x", ledStrip[0][2]);
	sprintf(blue, "%02x", ledStrip[0][1]);
	hexValue = hexValue + red + green + blue;

	//
	// Spot-Light
	//

	// speed back mapping
	String spotSpeedMap = "0";
	if (spotSpeed != 0) {
		spotSpeedMap = String(map(spotSpeed,100,10,1,100));
	}

	String ptr = R"=====(
<!DOCTYPE html>
<html>
	<head>
		<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
		<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=0" />
		<title>LED-Control</title>
		<style>
			body {
				background-color: antiquewhite;
				font-family: sans-serif;
			}
			div.settings {
				display:grid;
				grid-template-columns: max-content max-content;
				grid-gap:5px;
			}
			div.settings label { text-align:right; }
			div.settings label:after { content: ":"; }
		</style>
	</head>
	<body>
		<h1>LED-Control</h1>
		<form action="/" method="GET">
			<div class="settings">
				<input type="submit" name="action" value="Aus">
				<input type="submit" name="action" value="Farbwechsel">
			</div>
		</form>
		<h1>Color-Picker</h1>
		<form action="/color" method="GET">
			<div class="settings">
				<label for="brightness">Helligkeit</label><input type="range" name="brightness" value=")====="; ptr += String(getBrightness(ledStrip[0][0])); ptr += R"=====(" min="0" max="31" step="1.0" onchange="this.form.submit()">
				<label for="colorpicker">Farbe</label><input type="color" name="colorpicker" value=")====="; ptr += hexValue; ptr += R"=====(" onchange="this.form.submit()">
			</div>
		</form>
		<h1>Spot-Light</h1>
		<form action="/spot" method="GET">
			<div class="settings">
				<label for="brightness">Helligkeit</label><input type="range" name="brightness" value=")====="; ptr += String(spotMaxBrightness); ptr += R"=====(" min="0" max="31" step="1.0" onchange="this.form.submit()">
				<label for="speed">Geschwindigkeit</label><input type="range" name="speed" value=")====="; ptr += String(spotSpeedMap); ptr += R"=====(" min="0" max="100" step="1.0" onchange="this.form.submit()">
				<label for="position">Position</label><input type="range" name="position" value=")====="; ptr += String(spotPosition); ptr += R"=====(" min="0" max=")====="; ptr += String(LEDS - 1); ptr += R"=====(" step="1.0" onchange="this.form.submit()">
			</div>
		</form>
	</body>
</html>
)=====";
	return ptr;
}
