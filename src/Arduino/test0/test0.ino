#include <SPI.h>
#include <Ethernet.h>


// Network config
byte mac[] = { 0x00, 0x03, 0x7B, 0x20, 0xFE, 0xED };
byte ip[]  = { 192, 168, 1, 50 };

EthernetServer server(2101);


// Test register values / sensor IO

uint16_t readDWord(int addr) {

  union {
    float f;
    uint32_t i;
  } u;

  switch (addr) {

    // A0 -> D156/D157
    case 157: {
      u.f = analogRead(A0);
      Serial.println(u.f);
      return (u.i >> 16) & 0xFFFF;
    }

    case 156: {
      u.f = analogRead(A0);
      return u.i & 0xFFFF;
    }

    // A1 -> D160/D161
    case 161: {
      u.f = analogRead(A1);
      Serial.println(u.f);
      return (u.i >> 16) & 0xFFFF;
    }

    case 160: {
      u.f = analogRead(A1);
      return u.i & 0xFFFF;
    }

    // A2 -> D164/D165
    case 165: {
      u.f = analogRead(A2);
      Serial.println(u.f);
      return (u.i >> 16) & 0xFFFF;
    }

    case 164: {
      u.f = analogRead(A2);
      return u.i & 0xFFFF;
    }

    case 666:{
        int d666 = digitalRead(21);
        return d666;
        }
    case 570:
      return 26;

    case 3498:
    case 3499:
    case 3500:
      return 0;

    case 158:
      return random(50, 101);

    default:
      return 0;
  }
}

// BCC helpers
byte xorBcc(byte *data, int len) {
  byte bcc = 0;
  for (int i = 0; i < len; i++) {
    bcc ^= data[i];
  }
  return bcc;
}

void printHex(const char *label, const byte *buf, int len) {
  // Serial.print(label);
  // Serial.print(": ");
  //for (int i = 0; i < len; i++) {
  //  if (buf[i] < 16) // Serial.print("0");
    // Serial.print(buf[i], HEX);
    // Serial.print(" ");
  //}
  // Serial.println();
}

// Reply builder
void replyReadD(EthernetClient &c, int addr, int nbytes) {
  byte tx[96];
  int p = 0;

  tx[p++] = 0x06;   // ACK
  tx[p++] = '0';    // reply device 00
  tx[p++] = '0';
  tx[p++] = '0';    // OK discontinued

  int words = nbytes / 2;

  for (int i = 0; i < words; i++) {
    uint16_t v = readDWord(addr + i);

    const char *h = "0123456789ABCDEF";
    tx[p++] = h[(v >> 12) & 0xF];
    tx[p++] = h[(v >> 8)  & 0xF];
    tx[p++] = h[(v >> 4)  & 0xF];
    tx[p++] = h[v & 0xF];
  }

  byte bcc = xorBcc(tx, p);

  const char *h = "0123456789ABCDEF";
  tx[p++] = h[(bcc >> 4) & 0xF];
  tx[p++] = h[bcc & 0xF];
  tx[p++] = '\r';

  printHex("TX", tx, p);
  c.write(tx, p);
}

// Main handler
void handle(EthernetClient c) {
  byte buf[64];
  int len = 0;

  unsigned long lastRx = millis();

  while (c.connected() && (millis() - lastRx < 3000)) {
    while (c.available()) {
      lastRx = millis();

      byte b = c.read();

      if (len < 64) {
        buf[len++] = b;
      }

      if (b == 0x0D) {
        printHex("RX", buf, len);

        if (len >= 14 && buf[0] == 0x05 && buf[4] == 'R' && buf[5] == 'D') {
          char a[5] = {
            (char)buf[6], (char)buf[7],
            (char)buf[8], (char)buf[9], 0
          };

          char l[3] = {
            (char)buf[10], (char)buf[11], 0
          };

          int addr = atoi(a);
          int nbytes = strtol(l, NULL, 16);

          // Serial.print("READ D");
          // Serial.print(addr);
          // Serial.print(" nbytes=");
          // Serial.println(nbytes);

          replyReadD(c, addr, nbytes);
        }

        len = 0;
      }
    }
  }

  c.stop();
}

// Arduino setup
void setup() {
  Serial.begin(9600);

  Ethernet.begin(mac, ip);
  delay(1000);

  server.begin();

  randomSeed(analogRead(0));

  // Serial.print("Listening on ");
  // Serial.print(Ethernet.localIP());
  // Serial.println(":2101");
}


// Loop


void loop() {
  EthernetClient client = server.available();

  if (client) {
    // Serial.println("Client connected");
    handle(client);
    // Serial.println("Client done");
  }
}
