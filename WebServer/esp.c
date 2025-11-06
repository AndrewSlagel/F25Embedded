#include <WiFi.h>

const char* ssid = "MyAltice 1484fb";//Your Wifi Name
const char* password = "402-orchid-629";//Your Wifi Password

const char* host = "192.168.1.13"; // laptop IP
const uint16_t port = 5000;

WiFiClient client;
const int ledPin = 2;

String buffer = ""; // buffer for incoming TCP commands

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");

  connectToServer();
}

void loop() {
  // Reconnect if disconnected
  if (!client.connected()) {
    connectToServer();
    delay(1000);
    return;
  }

  // Read all available bytes from TCP
  while (client.available()) {
    char c = client.read();
    if (c == '\n') {
      buffer.trim();
      if (buffer == "LED_ON") {
        digitalWrite(ledPin, HIGH);
        Serial.println("LED ON (from laptop)");
      } else if (buffer == "LED_OFF") {
        digitalWrite(ledPin, LOW);
        Serial.println("LED OFF (from laptop)");
      } else if (buffer.length() > 0) {
        Serial.print("Unknown command: ");
        Serial.println(buffer);
      }
      buffer = ""; // clear buffer after processing
    } else {
      buffer += c; // accumulate characters
    }
  }

  // Forward Serial input to Python server
  while (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0 && client.connected()) {
      client.println(input);
      Serial.print("Sent to laptop: ");
      Serial.println(input);
    }
  }

  delay(10);
}

void connectToServer() {
  Serial.print("Connecting to server ");
  Serial.print(host);
  Serial.print(":");
  Serial.println(port);

  while (!client.connected()) {
    if (client.connect(host, port)) {
      Serial.println("Connected to Python server!");
      break;
    } else {
      Serial.println("Connection failed. Retrying...");
      delay(1000);
    }
  }
}