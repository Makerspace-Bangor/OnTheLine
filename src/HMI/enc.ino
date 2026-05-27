 //HC-SR04 example sketch
#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include "Adafruit_seesaw.h"

const int trigPin = 6;
const int echoPin = 7;

float d682_float = 0;
float distance = 0.0;
unsigned long duration = 0;
unsigned long lastDistanceUpdate = 0;

byte mac[] = { 0x00, 0x03, 0x7B, 0x20, 0xFE, 0xED };
byte ip[]  = { 192, 168, 1, 50 };

EthernetServer server(2101);

#define SS_SWITCH        24
#define SEESAW_BASE_ADDR 0x36

const uint8_t Nencoders = 5;
Adafruit_seesaw encoders[Nencoders];

bool found_encoders[Nencoders] = { false };
int32_t encoder_positions[Nencoders] = { 0 };

int16_t w = 0;
int16_t x = 0;
int16_t y = 0;
int16_t z = 0;

bool blink_active = false;
unsigned long blink_timer = 0;
int blink_count = 0;
bool led_state = false;

float readDistanceCM() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);

  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) {
    return -1.0;
  }

  return (duration * 0.0343) / 2.0;
}

void updateD682() {
  if (millis() - lastDistanceUpdate < 100) {
    return;
  }

  lastDistanceUpdate = millis();

  float v = readDistanceCM();

if (v > 0.0) {
  d682_float = v;
}
 // this was writen for a word, 
 // need to write a bitshift for 16b float conversion to arduino float
 // Serial.print("duration=");
 // Serial.print(duration);
//  Serial.print(" distance=");
//  Serial.println(distance);
 // Serial.print(" d682=");
 // Serial.println(d682);
}

void blinky(int count) {
  pinMode(LED_BUILTIN, OUTPUT);
  blink_count = count * 2;
  blink_active = true;
  blink_timer = millis();
}

void updateBlink() {
  if (!blink_active) {
    return;
  }

  if (millis() - blink_timer >= 500) {
    blink_timer = millis();
    led_state = !led_state;
    digitalWrite(LED_BUILTIN, led_state);
    blink_count--;

    if (blink_count <= 0) {
      blink_active = false;
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
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
    case 5:
      blinky(5);
      return 0;

    case 157:
      u.f = analogRead(A0);
      return (u.i >> 16) & 0xFFFF;

    case 156:
      u.f = analogRead(A0);
      return u.i & 0xFFFF;

    case 161:
      u.f = analogRead(A1);
      return (u.i >> 16) & 0xFFFF;

    case 160:
      u.f = analogRead(A1);
      return u.i & 0xFFFF;

    case 165:
      u.f = analogRead(A2);
      return (u.i >> 16) & 0xFFFF;

    case 164:
      u.f = analogRead(A2);
      return u.i & 0xFFFF;

    case 666:
      return digitalRead(21);

    case 669:
      return (uint16_t)w;

    case 670:
      return (uint16_t)x;

    case 671:
      return (uint16_t)y;

    case 672:
      return (uint16_t)z;

case 682: {
  union {
    float f;
    uint32_t i;
  } u;

  u.f = d682_float;

  return u.i & 0xFFFF;          // low word
}

case 683: {
  union {
    float f;
    uint32_t i;
  } u;

  u.f = d682_float;

  return (u.i >> 16) & 0xFFFF;  // high word
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

  tx[p++] = 0x06;
  tx[p++] = '0';
  tx[p++] = '0';
  tx[p++] = '0';

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
    updateBlink();
    updateD682();

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

    delay(1);
  }

  c.stop();
}

void setup() {
  Serial.begin(9600);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  digitalWrite(trigPin, LOW);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  Ethernet.begin(mac, ip);
  delay(1000);
  server.begin();

  randomSeed(analogRead(0));

  Wire.begin();

  for (uint8_t enc = 0; enc < Nencoders; enc++) {
    uint8_t addr = SEESAW_BASE_ADDR + enc;

    if (!encoders[enc].begin(addr)) {
      found_encoders[enc] = false;
    } else {
      encoders[enc].pinMode(SS_SWITCH, INPUT_PULLUP);
      encoders[enc].setEncoderPosition(0);
      encoder_positions[enc] = 0;
      encoders[enc].enableEncoderInterrupt();
      found_encoders[enc] = true;
    }
  }

  Serial.println("ready");
}

void loop() {
  oncod();
  updateBlink();
  updateD682();

  EthernetClient client = server.available();

  if (client) {
    handle(client);
  }
}
