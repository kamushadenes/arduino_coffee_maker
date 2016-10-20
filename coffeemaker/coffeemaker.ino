#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 125, 222);


EthernetUDP Udp;

// Syslog configuration
#define localUdpPort 8888
#define syslogPort 10516
IPAddress syslogServer(192, 168, 125, 231);

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
String event = "";
String msg = "No Coffee Available";
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

void http_response(EthernetClient client, char* http_status, String reason, String response) {

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
        event = "NOT_ACCEPTABLE";
        msg = "Not Brewing";
        http_response(client, HTTP_406, msg, event);
        sendSyslogMessage(WARN, "CEF:0|HPE Brazil SecLab|Coffee Maker|1.0|0|" + event + "|5|msg=" + msg + " requestMethod=WHEN");
      } else {
        brew_status = "COFFEE_READY";
        do_when();
        event = "BREW_STOPPED";
        msg = "Brew Stopped";
        http_response(client, HTTP_200, msg, event);
        sendSyslogMessage(INFO, "CEF:0|HPE Brazil SecLab|Coffee Maker|1.0|0|" + event + "|5|externalId=" + currentId + " msg=" + msg + " requestMethod=WHEN");
      }

}


void http_propfind(EthernetClient client) {
      event = brew_status;
      msg = "Coffee Status Requested";
      http_response(client, HTTP_200, msg, event);
      String msg = "CEF:0|HPE Brazil SecLab|Coffee Maker|1.0|0|" + event + "|5|externalId=" + currentId + " msg=" + msg + " requestMethod=PROPFIND";
      
      sendSyslogMessage(INFO, msg);
}

void http_brew(EthernetClient client) {
      if (brew_status == "BREWING") {
        event = "NOT_ACCEPTABLE";
        msg = "Already Brewing";
        http_response(client, HTTP_406, msg, event);
        sendSyslogMessage(WARN, "CEF:0|HPE Brazil SecLab|Coffee Maker|1.0|0|" + event + "|5|externalId=" + currentId + " msg=" + msg + " requestMethod=BREW");
        
      } else {
        brew_status = "BREWING";
        do_brew();
        currentId = generateId();
        event = "BREW_STARTED";
        msg = "Brew Started";
        http_response(client, HTTP_200, msg, event);
        sendSyslogMessage(INFO, "CEF:0|HPE Brazil SecLab|Coffee Maker|1.0|0|" + event + "|5|externalId=" + currentId + " msg=" + msg + " requestMethod=BREW");
        
      }
}

void http_get(EthernetClient client) {
      if (brew_status == "BREWING" ) {
          event = brew_status;
          msg = "Still Brewing";
          http_response(client, HTTP_503, msg, event);
          sendSyslogMessage(ERR, "CEF:0|HPE Brazil SecLab|Coffee Maker|1.0|0|" + event + "|5|externalId=" + currentId + " msg=" + msg + " requestMethod=GET");
      } else if (brew_status == "NO_COFFEE") {
          event = brew_status;
          msg = "No Coffee Available";
          http_response(client, HTTP_503, msg, event);
          sendSyslogMessage(ERR, "CEF:0|HPE Brazil SecLab|Coffee Maker|1.0|0|" + event + "|5|msg=" + msg + " requestMethod=GET");
      } else if (brew_status == "COFFEE_READY") {
          event = "COFFEE_SERVED";
          msg = "Coffee Served";
          http_response(client, HTTP_200, msg, event);
          sendSyslogMessage(INFO, "CEF:0|HPE Brazil SecLab|Coffee Maker|1.0|0|" + event + "|5|externalId=" + currentId + " msg=" + msg + " requestMethod=GET");
          brew_status = "NO_COFFEE";
          currentId = "";
      } else {
          event = "ERROR";
          msg = "Unexpected brewing status";
          http_response(client, HTTP_500, msg, event);
          sendSyslogMessage(ERR, "CEF:0|HPE Brazil SecLab|Coffee Maker|1.0|0|" + event + "|5|msg=" + msg + " requestMethod=GET");

          
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
  sendSyslogMessage(INFO, "CEF:0|HPE Brazil SecLab|Coffee Maker|1.0|0|START|5|msg=ArcSight CoffeeMaker started requestMethod=BOOT");
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

