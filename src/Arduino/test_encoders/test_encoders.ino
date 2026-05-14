#include <Wire.h>
#include "Adafruit_seesaw.h"
#include <Keyboard.h>
#include <SPI.h>
#include <Ethernet.h>

#define TRIG_PIN_1 20
#define ECHO_PIN_1 21

#define SS_SWITCH        24
#define SEESAW_BASE_ADDR 0x36

const uint8_t Nencoders = 5;

// Network config
byte mac[] = { 0x00, 0x03, 0x7B, 0x20, 0xFE, 0xED };
byte ip[]  = { 192, 168, 1, 50 };

EthernetServer server(2101);

Adafruit_seesaw encoders[Nencoders];

bool found_encoders[Nencoders] = { false };
int32_t encoder_positions[Nencoders] = { 0 };

int16_t w = 0;
int16_t x = 0;
int16_t y = 0;
int16_t z = 0;

int getDistance(int triggerPin, int echoPin) {
  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  int distance = duration * 0.034 / 2;
  // The value 0.034 corresponds to the speed of sound in centimeters per microsecond through air.
  // Divided by 2, because the signal goes out and comes back. durration * 0.17 would also work
  return distance;
}

void setup() {
  Serial.begin(9600);
  pinMode(TRIG_PIN_1, OUTPUT);
  pinMode(ECHO_PIN_1, INPUT);

  Ethernet.begin(mac, ip);
  delay(1000);
  server.begin();

  randomSeed(analogRead(0));

  Wire.begin();

  while (!Serial) {
    delay(5);
  }

  for (uint8_t enc = 0; enc < Nencoders; enc++) {
    uint8_t addr = SEESAW_BASE_ADDR + enc;

    if (!encoders[enc].begin(addr)) {
      found_encoders[enc] = false;
    } else {
      encoders[enc].pinMode(SS_SWITCH, INPUT_PULLUP);
      encoders[enc].setEncoderPosition(0);
      encoder_positions[enc] = 0;

      encoders[enc].setGPIOInterrupts((uint32_t)1 << SS_SWITCH, 1);
      encoders[enc].enableEncoderInterrupt();

      found_encoders[enc] = true;
    }
  }

  Serial.print("encoders found at addresses: ");

  for (uint8_t enc = 0; enc < Nencoders; enc++) {
    if (found_encoders[enc]) {
      Serial.print("0x");
      Serial.print(SEESAW_BASE_ADDR + enc, HEX);
      Serial.print(" ");
    }
  }

  Serial.println();
}

void cls() {
  for (uint8_t enc = 0; enc < Nencoders; enc++) {
    if (found_encoders[enc]) {
      encoders[enc].setEncoderPosition(0);
      encoder_positions[enc] = 0;
    }
  }

  w = 0;
  x = 0;
  y = 0;
  z = 0;
}

void oncod() {
  for (uint8_t enc = 0; enc < Nencoders; enc++) {
    if (!found_encoders[enc]) {
      continue;
    }

    int32_t new_position = encoders[enc].getEncoderPosition();

    if (encoder_positions[enc] != new_position) {
      encoder_positions[enc] = new_position;
    }
  }

  w = (int16_t)encoder_positions[0];
  x = (int16_t)encoder_positions[1];
  y = (int16_t)encoder_positions[2];
  z = (int16_t)encoder_positions[3];
}

uint16_t readDWord(int addr) {
  union {
    float f;
    uint32_t i;
  } u;

  switch (addr) {
    // A0 -> D156/D157
    case 157:
      u.f = analogRead(A0);
      return (u.i >> 16) & 0xFFFF;

    case 156:
      u.f = analogRead(A0);
      return u.i & 0xFFFF;

    // A1 -> D160/D161
    case 161:
      u.f = analogRead(A1);
      return (u.i >> 16) & 0xFFFF;

    case 160:
      u.f = analogRead(A1);
      return u.i & 0xFFFF;

    // A2 -> D164/D165
    case 165:
      u.f = analogRead(A2);
      return (u.i >> 16) & 0xFFFF;

    case 164:
      u.f = analogRead(A2);
      return u.i & 0xFFFF;

    case 666:
      return digitalRead(21);

    // Encoder values for HMI
    case 669:
      return (uint16_t)w;

    case 670:
      return (uint16_t)x;

    case 671:
      return (uint16_t)y;

    case 672:
      return (uint16_t)z;

    case 682:
       return (getDistance(TRIG_PIN_1,ECHO_PIN_1)) ; 

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

byte xorBcc(byte *data, int len) {
  byte bcc = 0;

  for (int i = 0; i < len; i++) {
    bcc ^= data[i];
  }

  return bcc;
}

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

  c.write(tx, p);
}

void handle(EthernetClient c) {
  byte buf[64];
  int len = 0;
  unsigned long lastRx = millis();

  while (c.connected() && (millis() - lastRx < 3000)) {
    oncod();

    while (c.available()) {
      lastRx = millis();

      byte b = c.read();

      if (len < 64) {
        buf[len++] = b;
      }

      if (b == 0x0D) {
        if (len >= 14 && buf[0] == 0x05 && buf[4] == 'R' && buf[5] == 'D') {
          char a[5] = {
            (char)buf[6],
            (char)buf[7],
            (char)buf[8],
            (char)buf[9],
            0
          };

          char l[3] = {
            (char)buf[10],
            (char)buf[11],
            0
          };

          int addr = atoi(a);
          int nbytes = strtol(l, NULL, 16);

          replyReadD(c, addr, nbytes);
        }

        len = 0;
      }
    }
  }

  c.stop();
}

void loop() {
  oncod();

  /*
  if (Serial.available()) {
    char c = Serial.read();

    if (c == 'c') {
      cls();
    }
  }
  */

  EthernetClient client = server.available();

  if (client) {
    handle(client);
  }
}
