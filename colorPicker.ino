#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

#include "FastLED.h"
#define NUM_LEDS 6
CRGB leds[NUM_LEDS];

//SSID and Password of your WiFi router
const char* ssid     = "ssid";
const char* password = "password";

// Set web server port number to 80
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

/////////////////////////////////////////////////////
// COLOR MODE 
/////////////////////////////////////////////////////

uint32_t currentColor;

enum COLOR_MODE
    {
        BREATH,
        RAINBOW,
        STATIC
    };

COLOR_MODE colorMode = COLOR_MODE::STATIC;    

const float maxBrightness = 64.0f;
const float minBrightness = 0.0f;
const float brightInterval = 1.0f;

float currentBrightness = maxBrightness;

// BREATH
enum states
    {
        HOLD,
        DOWN,
        UP
    };
    
states _state = states::DOWN;

const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
    <head>
        <title>Color Picker</title>
        <meta charset='UTF-8'>
        <script src='https://rawcdn.githack.com/NC22/HTML5-Color-Picker/fd2cdd0e6ac8843b26e62e2152df091ee9542cb8/html5kellycolorpicker.min.js'></script>
        <style>
            html {font-family:Helvetica;display:inline-block;margin:0px auto;text-align:center;}
            .colorPicker {position:absolute;top:10%;margin:auto;width:100vw;}
            .controls {display:flex;align-items:center;justify-content:center;}
            .radio {flex-direction: column;align-items: center;}
        </style>
    </head>
    <body>  
        <div class='colorPicker'>
            <canvas id='picker'></canvas>
            <br>
            <input id='color' value='#54aedb' name='colorPicker'>
            <br>
            <br>
            <div class="controls">
              <label class="radio">
                <input checked type="radio" name="colorMode" value="STATIC">
                Static
              </label>
              <label class="radio">
                <input type="radio" name="colorMode" value="RAINBOW">
                Rainbow
              </label>
              <label class="radio">
                <input type="radio" name="colorMode" value="BREATH">
                Breath
              </label>
            </div>
            <br>
            <div class="slidecontainer">
              <input type="range" min="1" max="255" value="64" class="slider" id="myRange">
            </div>
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
            colorMode();
            brightnessSlider();
            colerPicker();
            function colerPicker() {
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
            }    
            function colorMode() {
              var radios = document.querySelectorAll('input[type=radio][name="colorMode"]');
              function colorModeChangeHandler(event) {
                connection.send("M" + this.value);
              }
              Array.prototype.forEach.call(radios, function(radio) {
                radio.addEventListener('change', colorModeChangeHandler);
              });
            }    
            function brightnessSlider() {
              var slider = document.getElementById("myRange");
              slider.oninput = function() {
                connection.send("B" + this.value);
              }
            }
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

void HandleLED(uint32_t color) {
  int r = color >> 16;
  int g = color >> 8 & 0xFF;
  int b = color & 0xFF;
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
        currentColor = (uint32_t) strtol((const char *) &payload[1], NULL, 16);   // decode rgb data
        HandleLED(currentColor);
      } else if (payload[0] == 'M'){
        setMode(payload[1]);
      } else if (payload[0] == 'B') { 
        Serial.printf("Brightness: %s\n", payload);
      }
      break;
  }
}


void MODE_breath() {
    switch (_state) {
      case states::UP:
          currentBrightness += brightInterval;
        break;
      case states::DOWN:
          currentBrightness -= brightInterval;
        break;
      case states::HOLD:
          currentBrightness -=  (brightInterval / 10.0f);
        break;
      default:
        break;  
    }

    if (currentBrightness >= maxBrightness) {
      _state = states::HOLD;
    } else if (currentBrightness <= (maxBrightness - 50.0f) && _state == states::HOLD) {
      _state = states::DOWN;
    }else if (currentBrightness <= minBrightness) {
      _state = states::UP;
    }
    
    FastLED.setBrightness( currentBrightness );
    FastLED.show();
  }

void MODE_rainbow() {
    static uint8_t colorIndex = 0;
    colorIndex = colorIndex + 1; /* motion speed */
    for( int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette( RainbowStripeColors_p, colorIndex, currentBrightness, LINEARBLEND);
        colorIndex += 3;
        FastLED.delay(300);
    }
    FastLED.show();
  }  

void setMode(char indicator) {
  if (indicator == 'B') {
    colorMode = COLOR_MODE::BREATH;
    } 
  else if (indicator == 'R') {
    colorMode = COLOR_MODE::RAINBOW;
    }
  else {
    colorMode = COLOR_MODE::STATIC;
    }   
  }

void loop() {
  webSocket.loop();  
  server.handleClient();
  switch (colorMode) {
      case COLOR_MODE::BREATH:
          MODE_breath();
        break;
      case COLOR_MODE::RAINBOW:
          MODE_rainbow();
        break;
      default:
        break;  
    }
  FastLED.delay(100);
}