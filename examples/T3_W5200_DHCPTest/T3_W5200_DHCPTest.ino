#include <stdlib.h>
#include <W5200.h>
#include <Ethernet.h>
#include <mac.h>

byte server[] = { 192, 168, 42, 10 }; // where to send http-request to
EthernetClient client;

void setup() {
  delay(1000);
  Serial.begin(57600);
  
  Serial.print("Reading MAC from hardware... ");
  read_mac();
  print_mac();
  Serial.println();
  
  Serial.print("Initializing W5200... ");
  W5200.init();
  Serial.println("done");

  Serial.print("Requesting DHCP IP... ");
  
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }
  
  Serial.print("got ");
  Serial.println(Ethernet.localIP());
  
  Serial.println("Connecting to server... ");

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
    for(;;) {
      delay(300000); // 5min
      Serial.print(Ethernet.maintain(), DEC);
    }
  }
}
