#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

//Start Customization Variables

#define WifiLEDInterval 200
#define mDNSLEDInterval 1500

#define GREENPIN 5
#define REDPIN 4
#define BLUEPIN 0

#define LEDPIN BUILTIN_LED
#define LEDON LOW


enum status {
  WifiWait,
  MDNSWait,
  Ready
};

status currentStatus = WifiWait;

bool serverInitialized = false;
bool wifiInitialized = false;

// End System and Switch states

// Status LED Setup
unsigned long previousMillis = 0;
int ledState = LEDON;
// End of Status LED Setup

// WiFi,mDNS,Webserver setup
const char* ssid = "SSID";
const char* password = "PASSWORD";
const char* mdns_hostname = "RGB";

ESP8266WebServer server(80);
// End of WiFi,mDNS,Webserver setup

//webpage to serve
String webpage = ""
"<!DOCTYPE html><html><head><title>RGB control</title><meta name='mobile-web-app-capable' content='yes' />"
"<meta name='viewport' content='width=device-width' /></head><body style='margin: 0px; padding: 0px;'>"
"<canvas id='colorspace'></canvas></body>"
"<script type='text/javascript'>"
"(function () {"
" var canvas = document.getElementById('colorspace');"
" var ctx = canvas.getContext('2d');"
" function drawCanvas() {"
" var colours = ctx.createLinearGradient(0, 0, window.innerWidth, 0);"
" for(var i=0; i <= 360; i+=10) {"
" colours.addColorStop(i/360, 'hsl(' + i + ', 100%, 50%)');"
" }"
" ctx.fillStyle = colours;"
" ctx.fillRect(0, 0, window.innerWidth, window.innerHeight);"
" var luminance = ctx.createLinearGradient(0, 0, 0, ctx.canvas.height);"
" luminance.addColorStop(0, '#ffffff');"
" luminance.addColorStop(0.05, '#ffffff');"
" luminance.addColorStop(0.5, 'rgba(0,0,0,0)');"
" luminance.addColorStop(0.95, '#000000');"
" luminance.addColorStop(1, '#000000');"
" ctx.fillStyle = luminance;"
" ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);"
" }"
" var eventLocked = false;"
" function handleEvent(clientX, clientY) {"
" if(eventLocked) {"
" return;"
" }"
" function colourCorrect(v) {"
" return Math.round(1023-(v*v)/64);"
" }"
" var data = ctx.getImageData(clientX, clientY, 1, 1).data;"
" var params = ["
" 'r=' + colourCorrect(data[0]),"
" 'g=' + colourCorrect(data[1]),"
" 'b=' + colourCorrect(data[2])"
" ].join('&');"
" var req = new XMLHttpRequest();"
" req.open('POST', '?' + params, true);"
" req.send();"
" eventLocked = true;"
" req.onreadystatechange = function() {"
" if(req.readyState == 4) {"
" eventLocked = false;"
" }"
" }"
" }"
" canvas.addEventListener('click', function(event) {"
" handleEvent(event.clientX, event.clientY, true);"
" }, false);"
" canvas.addEventListener('touchmove', function(event){"
" handleEvent(event.touches[0].clientX, event.touches[0].clientY);"
"}, false);"
" function resizeCanvas() {"
" canvas.width = window.innerWidth;"
" canvas.height = window.innerHeight;"
" drawCanvas();"
" }"
" window.addEventListener('resize', resizeCanvas, false);"
" resizeCanvas();"
" drawCanvas();"
" document.ontouchmove = function(e) {e.preventDefault()};"
" })();"
"</script></html>";

//////////////////////////////////////////////////////////////////////////////////////////////////


// Color Variables for the strip
int red = 0;
int green = 0;
int blue = 0;


// Toggle Status LED
void toggleLed() {
   ledState = !ledState;
   digitalWrite(LEDPIN, ledState);
}

// Set Strip 1 colors
void updateStrip() {
    Serial.println("Updating strip");
    analogWrite(REDPIN, red);
    analogWrite(BLUEPIN, blue);
    analogWrite(GREENPIN, green);
}

void handleRoot() {
// Serial.println("handle root..");
red = 1024 - server.arg(0).toInt(); // read RGB arguments
green = 1024 - server.arg(1).toInt();
blue = 1024 - server.arg(2).toInt();

updateStrip();

Serial.print("Red =");
 Serial.println(red); // for TESTING
 Serial.print("green =");
 Serial.println(green); // for TESTING
 Serial.print("Blue =");
 Serial.println(blue); // for TESTING
server.send(200, "text/html", webpage);
}



void fade(int pin) {

    for (int u = 0; u < 1024; u++) {
      analogWrite(pin, 1023 - u);
      delay(1);
    }
    for (int u = 0; u < 1024; u++) {
      analogWrite(pin, u);
      delay(1);
    }
}

void testRGB() { // fade in and out of Red, Green, Blue

Serial.println("Testing the lights");
red =1023;
green = 1023;
blue = 1023;
updateStrip();

fade(REDPIN); // R
fade(GREENPIN); // G
fade(BLUEPIN); // B
}

//////////////////////////////////////////////////////////////////////////////////////////////////


void setup() {
  // Set Pin Modes
  pinMode(LEDPIN, OUTPUT);
  pinMode(REDPIN, OUTPUT);
  pinMode(GREENPIN, OUTPUT);
  pinMode(BLUEPIN, OUTPUT);

  Serial.begin(115200);

  testRGB();
}

void loop() {
  // Check if Wifi Disconnected
  if(wifiInitialized && serverInitialized) {
    if(WiFi.status() != WL_CONNECTED) {
      server.stop();
      serverInitialized = false;
      wifiInitialized = false;
      currentStatus = WifiWait;
    }
  }

  // WifiWait State, waiting for Wifi to initialize and connect
  if(currentStatus == WifiWait) {
    // If Wifi is not Initialized start the WiFi connection
    if(!wifiInitialized) {
      WiFi.begin(ssid, password);
      wifiInitialized = true;
    }
    // When the Wifi Connects move to the MDNSWait State
    if(WiFi.status() == WL_CONNECTED) {
      Serial.println("Wifi Connected !");
      Serial.print("IP :");
      Serial.println(WiFi.localIP());
      currentStatus = MDNSWait;
    } else {
      // Waiting for Wifi to connect, flash LED accordingly
      unsigned long currentMillis = millis();
      unsigned long elapsedTime = currentMillis - previousMillis;
      if(elapsedTime >= WifiLEDInterval) {
        previousMillis = currentMillis;
        toggleLed();
      }
    }
  }

  // MDNSWait Sate, waiting for MDNS to initialize
  if(currentStatus == MDNSWait) {
    // When the MDNS initializes move to Ready State
    if (MDNS.begin(mdns_hostname)) {
      currentStatus = Ready;
      Serial.print("mDNS Up. Should be accessible via ");
      Serial.print(mdns_hostname);
      Serial.println(".local");
    } else {
      // Waiting for MDNS to Init, flash LED accordingly
      unsigned long currentMillis = millis();
      unsigned long elapsedTime = currentMillis - previousMillis;
      if(elapsedTime >= MDNSWait) {
        previousMillis = currentMillis;
        toggleLed();
      }
    }
  }

  // Ready State, start webserver if not initialized and handle clients
  if(currentStatus == Ready) {
    if(serverInitialized == false) {
      //Initialize Webserver
      digitalWrite(LEDPIN, LEDON);
      ledState = LEDON;

      // Setup Server Routes
      server.on("/", handleRoot);

      // Start Server
      server.begin();

      // Update Color Values
      updateStrip();

      serverInitialized = true;
    } else {
      server.handleClient();
    }
  }

}
