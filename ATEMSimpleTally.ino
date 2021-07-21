#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

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

ESP8266WebServer server(80);

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
  if (client.connect(atemip, 9990)) {
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
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
    
  WiFi.begin(WIFI_SID, WIFI_PWD);

  WiFi.hostname("esp8266tally");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    c++;
    if (c>30)
      return -1;
  }

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

  if (!client.connected()) {
    s+="<div>Not connected!</div>";
  } else {
    s+="<div>Connected to: "+atemIP+"</div>";
  }

  s+="<div><ol>";

  for(int i=0;i<outputs;i++) {
    s+="<li>";
    s+=String(output[i]);
    s+="</li>";
  }
  
  s+"</ol></div><div>";

  s+="<form action=\"/route\" method=\"get\">";
  s+="<div>DST: <input type=\"text\" name=\"d\" value=\"1\"></div>";
  s+="<div>SRC: <input type=\"text\" name=\"s\" value=\"3\"></div>";
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
  
  Serial.println(WiFi.localIP().toString());
  buf[0]=0;
  
  server.on("/", httpIndex);
  server.on("/input", httpGetSetInput);
  server.on("/route", httpSetRoute);

  Serial.println("WiFi: Failed"); 
  int n = MDNS.queryService("blackmagic", "tcp");
  if (n == 0) {
    Serial.println("Not found, using default ATEM IP");
  } else {
    Serial.println("ATEM service found");    
    Serial.print("IP: ");
    Serial.println(MDNS.IP(0));
    
    atemIP=String(MDNS.IP(0).toString());
  }

  server.begin();

  if (MDNS.begin("esp8266tally")) {
    Serial.println("MDNS responder started");
  }

  Serial.println("Ready");
  
  delay(2000);

  // And ESP-01 LED is shared with serial so no output after this on it
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
    
    break;
  }
}

void ping()
{
  // Stupid keep alive ping
  pinger++;
  if (pinger>60000) {
    pinger=0;
    client.print("\n");
    Serial.println("PING");
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
  if (output[2]==myID && output[1]!=myID) {
    unsigned long cm = millis();
    if (cm-pm >= 100) {
      pm=cm;
      ppls=(ppls==LOW) ? HIGH : LOW;
    }
    digitalWrite(LED_BUILTIN, ppls);
  } else if (output[1]==myID) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Lost WiFi, reconnecting...");
    wifiSetup();
  }
  
  if (!client.connected()) {
    Serial.println("Lost ATEM, reconnecting...");
    connectATEM(atemIP);
  }

  readClient();
  setLedState();
  
  server.handleClient();
  
  MDNS.update();
  
  ping();
  
}
