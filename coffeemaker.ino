#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,123,251);


EthernetUDP Udp;

// Syslog configuration
#define localUdpPort 8888
#define syslogPort 10514
IPAddress syslogServer(192, 168, 123, 14);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);


// Const for ID Generation
char hexadecimal[16] = {'A', 'B', 'C', 'D', 'E', 'F', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};


// PIN for relay control
int RELAY_PIN = 2;

// Variable for Syslog messages
char line1[100];


// HTCPCP Codes

char* HTTP_200 = "200 OK";
char* HTTP_406 = "406 Not Acceptable";
char* HTTP_418 = "418 I'm a teapot";
char* HTTP_500 = "500 Internal Server Error";
char* HTTP_503 = "503 Service Unavailable";


// SYSLOG_LEVELS
int EMERG = 0;
int ALERT = 1;
int CRIT = 2;
int ERR = 3;
int WARN = 4;
int NOTICE = 5;
int INFO = 6;
int DEBUG = 7;

// Brew Status
String brew_status = "NO_COFFEE";
String currentId = "";


void do_brew() {
  digitalWrite(RELAY_PIN, HIGH);
}

void do_when() {
  digitalWrite(RELAY_PIN, LOW);
}

String generateId() {
  char msg[7];
  for (int cnt = 0; cnt < 6; cnt++) {
    msg[cnt] = hexadecimal[random(0,16)];
  }
  msg[6] = '\0';
  return msg;
}

void http_response(EthernetClient client, char* http_status, char* reason, String response) {

  client.print("HTTP/1.1 ");
  client.print(http_status);
  client.print('\n');
  //Serial.println(msg);
  client.print("Content-Type: text/json");
  client.print("\n");
  client.println("Connection: close");  // the connection will be closed after completion of the response
  client.println();
  client.print("{\"reason\": \"");
  client.print(reason);
  client.print("\", \"response\": \"");
  client.print(response);
  client.print("\", \"brew_id\": \"");
  client.print(currentId.c_str());
  client.print("\"}\n");

}

void http_when(EthernetClient client) {
      if (brew_status == "COFFEE_READY" || brew_status == "NO_COFFEE") {
        // send a standard http response header
        http_response(client, HTTP_406, "Not Brewing", "NOT_ACCEPTABLE");
        sendSyslogMessage(WARN, "406|WHEN|NOT_ACCEPTABLE||Not Brewing");
      } else {
        brew_status = "COFFEE_READY";
        do_when();
        http_response(client, HTTP_200, "Brew Stopped", "BREW_STOPPED");
        sendSyslogMessage(INFO, "200|WHEN|BREW_STOPPED|" + currentId + "|Brew Stopped");
      }

}


void http_propfind(EthernetClient client) {
      http_response(client, HTTP_200, "Coffee Status Requested", brew_status);
      String msg = "200|PROPFIND|" + brew_status + "|" + currentId + "|Coffee Status Requested";
      sendSyslogMessage(INFO, msg);
}

void http_brew(EthernetClient client) {
      if (brew_status == "BREWING") {
        http_response(client, HTTP_406, "Already Brewing", "NOT_ACCEPTABLE");
        sendSyslogMessage(WARN, "406|BREW|NOT_ACCEPTABLE|" + currentId + "|Already Brewing");
      } else {
        brew_status = "BREWING";
        do_brew();
        currentId = generateId();
        http_response(client, HTTP_200, "Brew Started", "BREW_STARTED");
        sendSyslogMessage(INFO, "200|BREW|BREW_STARTED|" + currentId + "|Brew Started");
      }
}

void http_get(EthernetClient client) {
      if (brew_status == "BREWING" ) {
          http_response(client, HTTP_503, "Still Brewing", brew_status);
          sendSyslogMessage(ERR, "503|GET|BREWING|" + currentId + "|Still Brewing");
      } else if (brew_status == "NO_COFFEE") {
          http_response(client, HTTP_503, "No Coffee Available", brew_status);
          sendSyslogMessage(ERR, "503|GET|NO_COFFEE||No Coffee Available");
      } else if (brew_status == "COFFEE_READY") {
          http_response(client, HTTP_200, "Coffee Served", "COFFEE_SERVED");
          sendSyslogMessage(INFO, "200|GET|COFFEE_SERVED|" + currentId + "|Coffee Served");
          brew_status = "NO_COFFEE";
          currentId = "";
      } else {
          http_response(client, HTTP_500, "Unexpected brewing status", "ERROR");
          sendSyslogMessage(ERR, "500|GET|ERROR||Unexpected brewing status");
      }
}

void setup() {
 // Open serial communications and wait for port to open:
  //Serial.begin(9600);
  // while (!Serial) {
  //  ; // wait for serial port to connect. Needed for Leonardo only
  //}

  randomSeed(analogRead(0));

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  //Serial.print("server is at ");
  //Serial.println(Ethernet.localIP());

  Udp.begin(localUdpPort);
  sendSyslogMessage(INFO, "000|BOOT|START||ArcSight CoffeeMaker started");
}


void loop() {
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    //Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    boolean got_header = false;
    String header;
    header[0] = 0;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (isWhitespace(c) && !got_header) {
          got_header = true;
          //Serial.println(header);
        }

        if (!got_header) {
          header = header + c;
        }


        //Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          if (got_header) {
            if (header == "GET") {
              http_get(client);
            } else if (header == "PROPFIND") {
              http_propfind(client);
            } else if (header == "BREW" || header == "POST") {
              http_brew(client);
            } else if (header == "WHEN" || header == "DELETE") {
              http_when(client);
            }
            break;
          }
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    //Serial.println("client disonnected");
  }
}

void sendSyslogMessage(int severity, String message)
{
  /*
   0 Emergency: system is unusable
   1 Alert: action must be taken immediately
   2 Critical: critical conditions
   3 Error: error conditions
   4 Warning: warning conditions
   5 Notice: normal but significant condition
   6 Informational: informational messages
   */

  int pri = (184 + severity);
  String priString = String(pri, DEC);

  String buffer = "<" + priString + ">" + "CoffeeMaker " + message;
  int bufferLength = buffer.length();
  char char1[bufferLength+1];
  for(int i=0; i<bufferLength; i++)
  {
    char1[i]=buffer.charAt(i);
  }
  char1[bufferLength] = '\0';
  Udp.beginPacket(syslogServer, syslogPort);
  Udp.write(char1);
  Udp.endPacket();


}

