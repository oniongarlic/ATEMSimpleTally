#ifdef ESP32
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include <WebServer.h>
#else
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#endif

// Very simple ESP-01 ESP8266 ATEM Mini Tally

// Setup WiFi and default ATEM IP in setup.h
// Make a copy setup.h.sample to setup.h and adjust for your setup
//
#include "setup.h"

/**
 *
OUTPUT LABELS:
0 Output
1 Program
2 Preview
 */

int myID=MY_INPUT;
int preview=0;
int pinger=0;

WiFiClient client;

#ifdef ESP32
WebServer server(80);

#define LED_BUILTIN (2)

#else
ESP8266WebServer server(80);
#endif

char buf[100];
int i=0;
int state=0;

// XXX: Count these from the connection
int outputs=3;
int output[16];

String atemIP=ATEM_IP;

int connectATEM(String atemip)
{
  Serial.println("Connecting to ATEM");
  if (client.connect(atemip.c_str(), 9990)) {
    Serial.println("connected");
    return 0;
  } else {
    Serial.println("connection failed");
    delay(2000);
  }
  return -1;
}

int wifiSetup()
{
  int c=0;
    
  WiFi.mode(WIFI_STA);   
  WiFi.begin(WIFI_SID, WIFI_PWD);

  // WiFi.hostname("esp8266tally");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    Serial.printf(" %d ", WiFi.status());
    c++;
    if (c>30)
      return -1;
  }

  Serial.println(WiFi.localIP().toString());
  WiFi.setAutoReconnect(true);

  return 0;
}

void setRouting(int dest, int src)
{
  client.print("VIDEO OUTPUT ROUTING:");
  client.print("\n");

  client.print(dest);
  client.print(" ");
  client.print(src);
  client.print("\n");

  client.print("\n");
}

void httpIndex() {
  String s = "<html><h1>Tally</h1>";

  s+="<div>Input: <b>"+String(myID)+"</b></div>";

  s+="<form action=\"/setid\" method=\"post\">";
  s+="<div>ID: <input type=\"text\" name=\"i\" value=\""+String(myID)+"\"></div>";  
  s+="<div><input type=\"submit\" value=\"Change\"></div>";
  s+="</form>";

  if (!client.connected()) {
    s+="<div>Not connected!</div>";
  } else {
    s+="<div>Connected to: "+atemIP+"</div>";
  }

  s+="<table><tr><th>Output</th><th>Input</th></tr>";

  for(int i=0;i<outputs;i++) {
    s+="<tr>";
    s+="<td>"+String(i)+"</td>";
    s+="<td>"+String(output[i])+"</td>";
    s+="</tr>";
  }
  
  s+"</table><div>";

  s+="<form action=\"/route\" method=\"post\">";
  s+="<div>DST: <input type=\"text\" name=\"d\" value=\"1\"></div>";
  s+="<div>SRC: <input type=\"text\" name=\"s\" value=\""+String(output[2])+"\"></div>";
  s+="<div><input type=\"submit\" value=\"Set\"></div>";
  s+="</form>";
  
  server.send(200, "text/html", s);  
}

void httpGetSetInput() {
  String input=server.arg("i");

  if (input.length()>0) {
    myID=input.toInt();
  }
  server.send(200, "text/plain", "My input is: " + String(myID));
}

void httpSetMyInput() {
  int i;
  String ti=server.arg("i");

  if (ti.length()==0) {
    server.send(500, "text/plain", "Invalid request");
    return;
  }

  i=ti.toInt();
  // xxx
  if (i<0 || i>16) {
    server.send(404, "text/plain", "Invalid input");     
    return;
  }

  myID=i;
  server.send(200, "text/plain", "My input is now "+String(myID));
}

void httpSetRoute() {
  int dst,src;
  String tsrc=server.arg("s");
  String tdst=server.arg("d");

  if (tsrc.length()==0 || tdst.length()==0) {
    server.send(500, "text/plain", "Invalid request");
    return;
  }

  src=tsrc.toInt();
  dst=tdst.toInt();

  // XXX
  if (src<0 || src>16 || dst<0 || dst>3) {
    server.send(404, "text/plain", "Invalid source or destination");
    return;
  }

  setRouting(dst, src);
  
  server.send(200, "text/plain", "Routed "+String(src)+ " to "+String(dst));
}

void setup() {
  delay(1000);
  
  Serial.begin(115200);

  while (wifiSetup()<0) {
    Serial.println("WiFi: Failed");
    delay(2000);
  }
    
  buf[0]=0;
  
  server.on("/", httpIndex);
  server.on("/input", httpGetSetInput);
  server.on("/route", httpSetRoute);
  server.on("/setid", httpSetMyInput);

  if (MDNS.begin("atemtally")) {
    Serial.println("MDNS responder started");

    //
    MDNS.addService("http", "tcp", 80);

    Serial.println("Querying network for ATEM");
    int n = MDNS.queryService("blackmagic", "tcp");
    if (n == 0) {
      Serial.println("Nothing found, using default ATEM IP");
    } else {
      Serial.println("ATEM found");    
      Serial.print("IP: ");
      Serial.println(MDNS.IP(0));
    
      atemIP=String(MDNS.IP(0).toString());
    }
  }

  server.begin();

  Serial.println("Ready");
  
  delay(2000);

  // On a ESP-01 LED is shared with serial GPIO so no output after this on it!
  pinMode(LED_BUILTIN, OUTPUT);
}

void check()
{
  if (state==0 && strcmp(buf, "VIDEO OUTPUT ROUTING:")==0) {
    state=1;
    return;
  } else if (strlen(buf)==0) {
    state=0;
  }
  switch (state) {
    case 0:
    
    break;
    case 1:
    char *s1=buf;
    char *s2=strchr(buf, ' ');
    int d=atoi(s1);
    int s=atoi(s2);

    output[d]=s;

    Serial.print(d);
    Serial.print("=");
    Serial.println(s);
    
    break;
  }
}

// Keep alive ping
//
void ping()
{
  static unsigned long pm=0;
  unsigned long cm = millis();
  
  if (cm-pm >= 2000) {
    pm=cm;
    client.print("\n");    
  }
}

void readClient()
{
  // VIDEO OUTPUT ROUTING:
  if (client.available()) {
    char c = client.read();
    if (c!='\n' && i<99) {
      buf[i]=c;
      i++;      
    } else {
      buf[i]=0;
      //Serial.println(buf);
      check();
      i=0;
    }    
  }
}

unsigned long pm=0;
int ppls=HIGH;

void setLedState()
{
  // Blink if preview
  // On if program
  // Off otherwise
  //

  if (!client.connected()) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(300);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    return;
  }
  
  if (output[2]==myID && output[1]!=myID) {
    unsigned long cm = millis();
    if (cm-pm >= 100) {
      pm=cm;
      ppls=(ppls==LOW) ? HIGH : LOW;
    }
    digitalWrite(LED_BUILTIN, ppls);
  } else if (output[1]==myID || output[0]==myID) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf(" %d ", WiFi.status());
    Serial.println("Lost WiFi, waiting for auto reconnecting...");
    delay(500);
    return;
  }
  
  if (!client.connected()) {
    Serial.println("Lost ATEM, reconnecting...");
    connectATEM(atemIP);
  }

  if (client.connected()) {
    readClient();
  }
  
  setLedState();
  
  server.handleClient();
#ifndef ESP32  
  MDNS.update();
#endif
  
  ping();  
}
