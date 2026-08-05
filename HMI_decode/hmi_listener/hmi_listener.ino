#include <SPI.h>
#include <Ethernet.h>

// UNO R4 Minima + W5500 shield
// Fake IDEC PLC at 192.168.1.50:2101.
// Every read returns zero. Writes are acknowledged but not stored.
// Serial output lists the registers requested by the HMI.

constexpr uint8_t ETH_CS = 10;
constexpr uint8_t SD_CS = 4;
constexpr uint16_t PORT = 2101;
constexpr size_t BUF_SIZE = 600;

byte mac[] = {0x00, 0x03, 0x7B, 0x20, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 50);
IPAddress dns(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

EthernetServer server(PORT);
uint8_t request[BUF_SIZE];
uint8_t reply[BUF_SIZE];
size_t requestLength = 0;

uint8_t hexValue(uint8_t c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

uint8_t hexByte(const uint8_t *p) {
  return static_cast<uint8_t>((hexValue(p[0]) << 4) | hexValue(p[1]));
}

uint8_t bcc(const uint8_t *data, size_t length) {
  uint8_t value = 0;
  for (size_t i = 0; i < length; ++i) value ^= data[i];
  return value;
}

void addHex(uint8_t value, size_t &length) {
  static const char DIGITS[] = "0123456789ABCDEF";
  reply[length++] = DIGITS[(value >> 4) & 0x0F];
  reply[length++] = DIGITS[value & 0x0F];
}

void sendReply(EthernetClient &client, size_t length) {
  addHex(bcc(reply, length), length);
  reply[length++] = '\r';
  client.write(reply, length);
}

void startAck(size_t &length) {
  length = 0;
  reply[length++] = 0x06;

  if (request[1] == 'F' && request[2] == 'F') {
    reply[length++] = '0';
    reply[length++] = '0';
  } else {
    reply[length++] = request[1];
    reply[length++] = request[2];
  }

  reply[length++] = '0';
}

void printRegister(const char *operation) {
  Serial.print(operation);
  Serial.print("  ");
  Serial.write(request[5]);
  Serial.write(request[6]);
  Serial.write(request[7]);
  Serial.write(request[8]);
  Serial.write(request[9]);
}

void handleRead(EthernetClient &client, size_t length) {
  size_t replyLength;
  startAck(replyLength);
  printRegister("READ");

  if (length >= 12) {
    uint8_t byteCount = hexByte(&request[10]);
    Serial.print("  bytes=");
    Serial.println(byteCount);

    for (uint16_t i = 0; i < byteCount; ++i) {
      addHex(0, replyLength);
    }
  } else {
    Serial.println("  bit");
    reply[replyLength++] = '0';
  }

  sendReply(client, replyLength);
}

void handleWrite(EthernetClient &client, size_t length) {
  size_t replyLength;
  startAck(replyLength);
  printRegister("WRITE");

  if (length >= 12) {
    Serial.print("  bytes=");
    Serial.println(hexByte(&request[10]));
  } else if (length >= 11) {
    Serial.print("  value=");
    Serial.write(request[10]);
    Serial.println();
  } else {
    Serial.println();
  }

  sendReply(client, replyLength);
}

void handleRequest(EthernetClient &client) {
  // The final two characters before CR are the request BCC.
  size_t length = requestLength >= 2 ? requestLength - 2 : requestLength;

  if (length < 10 || request[0] != 0x05) {
    Serial.println("Malformed request");
    return;
  }

  if (request[4] == 'R') {
    handleRead(client, length);
  } else if (request[4] == 'W') {
    handleWrite(client, length);
  } else {
    Serial.print("OTHER command=");
    Serial.println(static_cast<char>(request[4]));
  }
}

void serviceClient(EthernetClient &client) {
  requestLength = 0;

  Serial.print("Client connected: ");
  Serial.println(client.remoteIP());

  while (client.connected()) {
    while (client.available()) {
      int value = client.read();
      if (value < 0) break;

      if (value == '\r') {
        handleRequest(client);
        requestLength = 0;
      } else if (requestLength < BUF_SIZE) {
        request[requestLength++] = static_cast<uint8_t>(value);
      } else {
        requestLength = 0;
        Serial.println("Request too large; discarded");
      }
    }
  }

  client.stop();
  Serial.println("Client disconnected");
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  // Deselect the SD card found on common W5500 shields.
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  Ethernet.init(ETH_CS);
  Ethernet.begin(mac, ip, dns, gateway, subnet);
  server.begin();

  Serial.println();
  Serial.println("IDEC HMI request listener");
  Serial.print("Listening at ");
  Serial.print(Ethernet.localIP());
  Serial.print(':');
  Serial.println(PORT);
}

void loop() {
  EthernetClient client = server.available();
  if (client) serviceClient(client);
}

