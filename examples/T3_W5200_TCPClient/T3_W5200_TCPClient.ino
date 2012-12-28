#include <stdlib.h>
#include <W5200.h>
#include <Ethernet.h>
#include <mac.h>

byte ownIP[] = { 192, 168, 42, 250 };
byte server[] = { 192, 168, 42, 10 };
EthernetClient client;

void setup() {
  delay(1000);
  Serial.begin(57600);
  
  Serial.println("Reading MAC from hardware...");
  read_mac();
  
  Serial.print("MAC: ");
  print_mac();
  Serial.println();
  
  Serial.print("Initializing W5200... ");
  W5200.init();
  Serial.println("done");
  
  Ethernet.begin(mac, ownIP);
  
  Serial.println("connecting...");

  if (client.connect(server, 80)) {
    Serial.println("connected");
    client.println("GET / HTTP/1.0");
    client.println();
  } else {
    Serial.println("connection failed");
  }
}

void loop() {
    if (client.available()) {
    char c = client.read();
    Serial.print(c);
  }

  if (!client.connected()) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
    for(;;)
      ;
  }
}

