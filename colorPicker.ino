#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

#include "FastLED.h"
#define NUM_LEDS 3
CRGB leds[NUM_LEDS];

int color = 0x008000;

//SSID and Password of your WiFi router
const char* ssid     = "ssid";
const char* password = "password";

// Set web server port number to 80
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
    <head>
        <title>Color Picker</title>
        <meta charset='UTF-8'>
        <script src='https://rawcdn.githack.com/NC22/HTML5-Color-Picker/fd2cdd0e6ac8843b26e62e2152df091ee9542cb8/html5kellycolorpicker.min.js'></script>
        <style>
            .button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}
            html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}
            .colorPicker { padding-top: 20px; }
        </style>
       
    </head>
    <body>  
        <div class='colorPicker'>
            <canvas id='picker'></canvas>
            <br>
            <input id='color' value='#54aedb' name='colorPicker'>
        </div>
        <script>
            const url = location.hostname;
            const websocketPort = 81;
            const connection = new WebSocket(`ws://${url}:${websocketPort}`, ['arduino']);    

            connection.onopen = function () {
              connection.send('Connect ' + new Date());
            };    

            connection.onerror = function (error) {
              console.log('WebSocket Error ', error);
            };
            connection.onmessage = function (e) {
              console.log('Server: ', e.data);
            };
            connection.onclose = function () {
              console.log('WebSocket connection closed');
            };

            var picker = new KellyColorPicker({
                place : 'picker',
                input : 'color',
                size : 150,
                userEvents : { 
                    change : function(self) {
                        connection.send(self.getCurColorHex());
                        }  
                    }
                });
        </script>   
    </body>
</html>
)=====";

void setup() {
  FastLED.addLeds<NEOPIXEL, 2>(leds, NUM_LEDS); 
  Serial.begin(115200);
  startWifi();
  startWebSocket(); 
  startServer();
}

void startWifi() {
   WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void startServer() {
  server.begin();
  server.on("/", HandleClient);
  }
  
void startWebSocket() {
  webSocket.begin();
  webSocket.onEvent(HandleWebSocketEvent);
  Serial.println("WebSocket server started.");
}

void HandleClient() {
  server.send(200, "text/html", MAIN_page, sizeof(MAIN_page));
}

void HandleLED(int r, int g, int b) {
  Serial.printf("R: [%i], G: [%i], B: [%i]",r, g, b);
  for(int i = 0; i < NUM_LEDS; i++) {
    leds[i].red = r;
    leds[i].green = g;
    leds[i].blue =  b;
    FastLED.show(); 
  }
}

void HandleWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED:             // if the websocket is disconnected
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {              // if a new websocket connection is established
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      }
      break;
    case WStype_TEXT:                     // if new text data is received
      Serial.println();
      Serial.printf("[%u] get Text: %s\n", num, payload);
      if (payload[0] == '#') {            // we get RGB data
        uint32_t number = (uint32_t) strtol((const char *) &payload[1], NULL, 16);   // decode rgb data
        int r = number >> 16;
        int g = number >> 8 & 0xFF;
        int b = number & 0xFF;
        HandleLED(r,g,b);
      }
      break;
  }
}

void loop() {
  webSocket.loop();  
  server.handleClient();
}
