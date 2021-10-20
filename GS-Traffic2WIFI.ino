// Changelog Softwareversion 2.2: Added experimental Client-Mode for Wifi and TCP-Bridging
//                                Added Dualcolor LED on GPIO 18 and 5 for Status of the module and/or Traffic-Warning
//                                Added baud rates: 115.200 and 230.400
// Known issues:
//    - Webconfig: Resetsettings does soemtimes not work with some browsers, reloading the page will solve the issure.
// Changelog Softwareversion 2.1: Added extracting data from $gpgsa and $pgrmz messages
// Known issues:
// Changelog Softwareversion 2.0: Added Webserver for Status und Config, Added basic tweaks for Faking GDL90 and NMEA (no Code for faking GDL90 included)
// Changelog Softwareversion 1.1: Tweak for disabling default-gateway
// Changelog Softwareversion 1.0: First stable version - airborne tested :)
// Instructions for Resettings: Put PIN19 to GND at Startup, after a reboot defaults are restores

// Debug-Messages on serial
const int debug = 0; //0=Off, 1=Many, 2=More, 3=All

// IP-Configuration
IPAddress ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress netmask(255, 255, 255, 0);
const char * udpAddress = "192.168.1.255";

// Define additional PINs
#define RESETPIN 19
#define LEDPINGREEN 18
#define LEDPINRED 5


// Define Softserials
#define NUM_COM 1
#define SERIAL_PARAM0 SERIAL_8N1
#define SERIAL0_RXPIN 21
#define SERIAL0_TXPIN 1
#define bufferSize 1024
#define MAX_NMEA_CLIENTS 6

#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include "GS-Traffic2WIFI-webservercontent.h"

Preferences preferences;
WiFiUDP udp;

char ssid[50];
char wpakey[50];
uint8_t chipid[6];
String gprmc="";
String gpgsa="";
String pgrmz="";
String pflau="";
int extractstream=0;
int trafficwarnled=0;


HardwareSerial* COM[NUM_COM] = {&Serial};

// Define Web-Server, without TCP-Port
WebServer webserver(0);

// Define TCP-Servers
WiFiServer server_0(0); // Changed SERIAL0_TCP_PORT to 0, for defining later in setup
WiFiServer *server[NUM_COM] = {&server_0};
WiFiClient TCPClient[NUM_COM][MAX_NMEA_CLIENTS];

// Define Buffers
uint8_t buf1[NUM_COM][bufferSize];
uint16_t i1[NUM_COM] = {0};
uint8_t buf2[NUM_COM][bufferSize];
uint16_t i2[NUM_COM] = {0};
uint8_t BTbuf[bufferSize];
uint16_t iBT = 0;

// Defining Serial-indata-String
String inserial;


// Extracting and saving Parts of NMEA-Data
void extractdata(String nmeax)
{
  int str_len = nmeax.length() + 1;
  char nmea[str_len];
  nmeax.toCharArray(nmea, str_len);
  // Debug string to Serial
  if (debug > 0)
  {
    Serial.print("Extracting: ");
    Serial.println(nmea);
  }
  char *nmeaa = NULL;
  char *bufptr = nmea;
  nmeaa = strsep(&bufptr, ",");

  String messagetype = nmeaa;
  // Extract Data, if Message is $GPRMC
  if (messagetype == "$GPRMC")
  {
    int inmeaa = 0;
    gprmc = "";
    String msg = nmeaa;
    while (nmeaa != NULL)
    {
      msg=nmeaa;
      if (inmeaa == 1)
      {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Time of message\",";
        gprmc += "\"v\":\"";
        if (nmeaa != ",")
        {
          gprmc += nmeaa;
        }
        gprmc += "\",";
        gprmc += "\"h\":\"";
        if (msg != "")
        {
        gprmc += nmeaa[0];
        gprmc += nmeaa[1];
        gprmc += ":";
        gprmc += nmeaa[2];
        gprmc += nmeaa[3];
        gprmc += ":";
        gprmc += nmeaa[4];
        gprmc += nmeaa[5];
        gprmc += " UTC";
        }
        gprmc += "\"},";
      }
      if (inmeaa == 2)
      {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Position state\",";
        gprmc += "\"v\":\"";
        gprmc += nmeaa;
        gprmc += "\",";
        gprmc += "\"h\":\"";
        if (msg == "A")
        {
          gprmc += "VALID";
        } else
        {
          gprmc += "NOT VALID";
        }
        gprmc += "\"},";
      }
      if (inmeaa == 3)
      {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Latitude\",";
        gprmc += "\"v\":\"";
        gprmc += nmeaa;
        gprmc += "\",";
        gprmc += "\"h\":\"";
        gprmc += nmeaa;
        gprmc += "\"},";
      }
      if (inmeaa == 4)
      {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Latitude direction\",";
        gprmc += "\"v\":\"";
        gprmc += nmeaa;
        gprmc += "\",";
        gprmc += "\"h\":\"";
        gprmc += nmeaa;
        gprmc += "\"},";
      }
      if (inmeaa == 5)
      {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Longitude \",";
        gprmc += "\"v\":\"";
        gprmc += nmeaa;
        gprmc += "\",";
        gprmc += "\"h\":\"";
        gprmc += nmeaa;
        gprmc += "\"},";
      }
      if (inmeaa == 6)
      {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Longitude direction \",";
        gprmc += "\"v\":\"";
        gprmc += nmeaa;
        gprmc += "\",";
        gprmc += "\"h\":\"";
        gprmc += nmeaa;
        gprmc += "\"},";
      }
      if (inmeaa == 7)
      {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Speed over Ground \",";
        gprmc += "\"v\":\"";
        gprmc += nmeaa;
        gprmc += "\",";
        gprmc += "\"h\":\"";
        if (msg != "")
        {
        gprmc += nmeaa;
        gprmc += " kts";
        }
        gprmc += "\"},";
      }
      if (inmeaa == 9)
      {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Date \",";
        gprmc += "\"v\":\"";
        gprmc += nmeaa;
        gprmc += "\",";
        gprmc += "\"h\":\"";
        if (msg != "")
        {
        gprmc += nmeaa[4];
        gprmc += nmeaa[5];
        gprmc += "/";
        gprmc += nmeaa[2];
        gprmc += nmeaa[3];
        gprmc += "/";
        gprmc += nmeaa[0];
        gprmc += nmeaa[1];
        }
        gprmc += "\"},";
      }

      nmeaa = strsep(&bufptr, ",");

      inmeaa++;
    }
    // Debug full string to Serial
    if (debug > 0)
    {
      Serial.print("Result of extracting $GPRMC: ");
      Serial.println(gprmc);
    }
  }

  // Extract Data, if Message is $gpgsa
  if (messagetype == "$GPGSA")
  {
    int inmeaa = 0;
    gpgsa = "";
    String msg = nmeaa;
    while (nmeaa != NULL)
    {
      msg=nmeaa;
      if (inmeaa == 1)
      {
        gpgsa += "{\"t\":\"$GPGSA\",\"p\":\"Fixmode\",";
        gpgsa += "\"v\":\"";
        if (nmeaa != ",")
        {
          gpgsa += nmeaa;
        }
        gpgsa += "\",";
        gpgsa += "\"h\":\"";
        if (nmeaa != ",")
        {
          gpgsa += nmeaa;
        }
        gpgsa += "\"},";
      }

      if (inmeaa == 2)
      {
        gpgsa += "{\"t\":\"$GPGSA\",\"p\":\"Fix\",";
        gpgsa += "\"v\":\"";
        if (nmeaa != ",")
        {
          gpgsa += nmeaa;
        }
        gpgsa += "\",";
        gpgsa += "\"h\":\"";
        if (nmeaa != ",")
        {
          gpgsa += nmeaa;
        }
        gpgsa += "\"},";
      }  

    if (inmeaa == 3)
      {
        gpgsa += "{\"t\":\"$GPGSA\",\"p\":\"GPS Satelites\",";
        gpgsa += "\"v\":\"";
        if (nmeaa != ",")
        {
          gpgsa += nmeaa;
        }
        gpgsa += "\",";
        gpgsa += "\"h\":\"";
        if (nmeaa != ",")
        {
          gpgsa += nmeaa;
        }
        gpgsa += "\"},";
      }  

    if (inmeaa == 15)
      {
        gpgsa += "{\"t\":\"$GPGSA\",\"p\":\"Position dilution of precision\",";
        gpgsa += "\"v\":\"";
        if (nmeaa != ",")
        {
          gpgsa += nmeaa;
        }
        gpgsa += "\",";
        gpgsa += "\"h\":\"";
        if (nmeaa != ",")
        {
          gpgsa += nmeaa;
        }
        gpgsa += "\"},";
      }  

    if (inmeaa == 16)
      {
        gpgsa += "{\"t\":\"$GPGSA\",\"p\":\"Horizontal dilution of precision\",";
        gpgsa += "\"v\":\"";
        if (nmeaa != ",")
        {
          gpgsa += nmeaa;
        }
        gpgsa += "\",";
        gpgsa += "\"h\":\"";
        if (nmeaa != ",")
        {
          gpgsa += nmeaa;
        }
        gpgsa += "\"},";
      }  

    if (inmeaa == 17)
      {
        gpgsa += "{\"t\":\"$GPGSA\",\"p\":\"Vertical dilution of precision\",";
        gpgsa += "\"v\":\"";
        if (nmeaa != ",")
        {
          gpgsa += nmeaa;
        }
        gpgsa += "\",";
        gpgsa += "\"h\":\"";
        if (nmeaa != ",")
        {
          gpgsa += nmeaa;
        }
        gpgsa += "\"},";
      }  

    
      nmeaa = strsep(&bufptr, ",");

      inmeaa++;
    }
    // Debug full string to Serial
    if (debug > 0)
    {
      Serial.print("Result of extracting $GPGSA: ");
      Serial.println(gpgsa);
    }
  }

 // Extract Data, if Message is $pgrmz
  if (messagetype == "$PGRMZ")
  {
    int inmeaa = 0;
    pgrmz = "";
    String msg = nmeaa;
    while (nmeaa != NULL)
    {
      msg=nmeaa;
      if (inmeaa == 1)
      {
        pgrmz += "{\"t\":\"$PGRMZ\",\"p\":\"Altitude\",";
        pgrmz += "\"v\":\"";
        if (nmeaa != ",")
        {
          pgrmz += nmeaa;
        }
        pgrmz += "\",";
        pgrmz += "\"h\":\"";
        if (nmeaa != ",")
        {
          pgrmz += nmeaa;
        }
        pgrmz += "ft\"},"; 
      }
    
      nmeaa = strsep(&bufptr, ",");

      inmeaa++;
    }
    // Debug full string to Serial
    if (debug > 0)
    {
      Serial.print("Result of extracting $PGRMZ: ");
      Serial.println(pgrmz);
    }
  }

 // Extract Data, if Message is $pflau
  if (messagetype == "$PFLAU")
  {
    int inmeaa = 0;
    pflau = "";
    String msg = nmeaa;
    while (nmeaa != NULL)
    {
      msg=nmeaa;
      if (inmeaa == 5)
      {
        pflau += "{\"t\":\"$PFLAU\",\"p\":\"Alarmlevel\",";
        pflau += "\"v\":\"";
        if (nmeaa != ",")
        {
          pflau += nmeaa;
        }
        pflau += "\",";
        pflau += "\"h\":\"";
        if (nmeaa != ",")
        {
          pflau += nmeaa;

          // Generate Warning to LED based on configuation
          if (trafficwarnled>0)
          {
            if (msg.toInt()>=trafficwarnled)
            {
                 digitalWrite(LEDPINRED, HIGH);
            } else
            {
                 digitalWrite(LEDPINRED, LOW);
            }
             
          }
        }
        pflau += "\"},"; 
      }
    
      nmeaa = strsep(&bufptr, ",");

      inmeaa++;
    }
    // Debug full string to Serial
    if (debug > 0)
    {
      Serial.print("Result of extracting PFLAU: ");
      Serial.println(pflau);
    }
  }
  
  
}

// Reset to "Default-Settings"
void defaultsettings()
{
  esp_efuse_read_mac(chipid);
  sprintf(ssid, "GS-Traffic-%02x%02x", chipid[4], chipid[5]);
  sprintf(wpakey, "%02x%02x%02x%02x", chipid[2], chipid[3], chipid[4], chipid[5]);
  String tmpssid = ssid;
  String tmpwpakey = wpakey;
  preferences.begin("GS-Traffic", false);
  preferences.clear();
  preferences.putString("ssid", tmpssid);
  preferences.putString("wpakey", tmpwpakey);
  preferences.putInt("extractstream", 0);
  preferences.putInt("defgw", 0);
  preferences.putInt("redpw", 0);
  preferences.putInt("baudrate", 19200);
  preferences.putInt("tcpport", 2000);
  preferences.putInt("udpport", 4000);
  preferences.putInt("fakenmea", 0);
  preferences.putInt("fakegdl90", 0);
  preferences.putInt("clientmode", 0);
  preferences.putInt("trafficwarnled", 0);
  preferences.end();
  webserver.send(200, "text/html", "Settings restored to default...rebooting..." );
  delay(500);
  ESP.restart();
}


// Apply new settings
void applysettings()
{
  preferences.begin("GS-Traffic", false);
  preferences.clear();
  preferences.putString("ssid", webserver.arg("ssid"));
  preferences.putString("wpakey", webserver.arg("wpakey"));
  preferences.putInt("baudrate", webserver.arg("baudrate").toInt());
  preferences.putInt("tcpport", webserver.arg("tcpport").toInt());
  preferences.putInt("udpport", webserver.arg("udpport").toInt());
  preferences.putInt("trafficwarnled", webserver.arg("trafficwarnled").toInt());
  if (webserver.arg("extractstream") == "on")
  {
    preferences.putInt("extractstream", 1);
  } else
  {
    preferences.putInt("extractstream", 0);
  }
  if (webserver.arg("defgw") == "on")
  {
    preferences.putInt("defgw", 1);
  } else
  {
    preferences.putInt("defgw", 0);
  }
  if (webserver.arg("redpw") == "on")
  {
    preferences.putInt("redpw", 1);
  } else
  {
    preferences.putInt("redpw", 0);
  }
  if (webserver.arg("fakenmea") == "on")
  {
    preferences.putInt("fakenmea", 1);
  } else
  {
    preferences.putInt("fakenmea", 0);
  }
  if (webserver.arg("fakegdl90") == "on")
  {
    preferences.putInt("fakegdl90", 1);
  } else
  {
    preferences.putInt("fakegdl90", 0);
  }
  if (webserver.arg("clientmode") == "on")
  {
    preferences.putInt("clientmode", 1);
  } else
  {
    preferences.putInt("clientmode", 0);
  }
  
  preferences.end();
  webserver.send(200, "text/html", "<html><head><meta http-equiv=refresh content=\"2;url=http://192.168.1.1\"/></head><body>Settings saved...rebooting...</body></html>" );
  delay(500);
  ESP.restart();
}


// Sending JSON (gs.json) to Client
void sendjson() {
  preferences.begin("GS-Traffic", false);

  String response = "{";
  response += "\"version\":\"";
  response += "2.2";

  char mac[50] = "";
  esp_efuse_read_mac(chipid);
  sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",  chipid[0], chipid[1], chipid[2], chipid[3], chipid[4], chipid[5]);

  response += "\",\"mac\":\"";
  response += mac;

  response += "\",\"ssid\":\"";
  response += preferences.getString("ssid", "");

  response += "\",\"wpakey\":\"";
  response += preferences.getString("wpakey", "");

  response += "\",\"extractstream\":";
  response += preferences.getInt("extractstream", 0);

  response += ",\"defgw\":";
  response += preferences.getInt("defgw", 0);

  response += ",\"redpw\":";
  response += preferences.getInt("redpw", 0);

  response += ",\"tcpport\":";
  response += preferences.getInt("tcpport", 0);

  response += ",\"udpport\":";
  response += preferences.getInt("udpport", 0);

  response += ",\"baudrate\":";
  response += preferences.getInt("baudrate", 0);

  response += ",\"fakenmea\":";
  response += preferences.getInt("fakenmea", 0);

  response += ",\"fakegdl90\":";
  response += preferences.getInt("fakegdl90", 0);

  response += ",\"clientmode\":";
  response += preferences.getInt("clientmode", 0);

  response += ",\"trafficwarnled\":";
  response += preferences.getInt("trafficwarnled", 0);
  
  response += ",\"cap\":[";
  response += gprmc;
  response += gpgsa;
  response += pgrmz;  

  response += "{\"t\":\"\",\"p\":\"\",\"v\":\"\",\"h\":\"\"}"; // Dummy as placeholder

  
  response += "]";

  response += "}";
  webserver.send(200, "text/json", response);
}

// Generating CRC16-Table for GDL90
int Crc16Table[256];
void crcInit( void )
{
  unsigned int i, bitctr, crc;
  for (i = 0; i < 256; i++)
  {
    crc = (i << 8);
    for (bitctr = 0; bitctr < 8; bitctr++)
    {
      crc = (crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0);
    }
    Crc16Table[i] = crc;
  }
}

// FRC-Calc for GDL90
unsigned int crcCompute(unsigned char *block, unsigned long int length)
{
  unsigned long int i;
  unsigned int crc = 0;
  for (i = 0; i < length; i++)
  {
    crc = Crc16Table[crc >> 8] ^ (crc << 8) ^ block[i];
  }
  return crc;
}



void setup() {
  crcInit();

  
  pinMode(LEDPINGREEN, OUTPUT);
  pinMode(LEDPINRED, OUTPUT);

  digitalWrite(LEDPINGREEN, HIGH);
  delay(200);
  digitalWrite(LEDPINGREEN, LOW);
  delay(200);
  digitalWrite(LEDPINRED, HIGH);
  delay(200);
  digitalWrite(LEDPINRED, LOW);
  delay(200);

  preferences.begin("GS-Traffic", false);

  // Resetting to Defaults, if no SSID is found (ESP32 seems to be blank)
  if (preferences.getString("ssid", "") == "")
  {
    defaultsettings();
  }

  // Starting serial based on configuration
  Serial.begin(preferences.getInt("baudrate", 0));

  // Checking Resetpin by Hardware, if DOWN (GND), Reset to defaults
  pinMode(RESETPIN, INPUT);
  pinMode(RESETPIN, INPUT_PULLUP);
  delay(50);
  if (digitalRead(RESETPIN) == 0)
  {
    Serial.println("Resetting");
    digitalWrite(LEDPINGREEN, HIGH);
    digitalWrite(LEDPINRED, HIGH);
    delay(500);
    defaultsettings();
  }

  String tmpssid;
  String tmpwpakey;
  tmpssid = preferences.getString("ssid", "");
  tmpwpakey = preferences.getString("wpakey", "");
  tmpssid.toCharArray(ssid, 50);
  tmpwpakey.toCharArray(wpakey, 50);
  delay(500);

  COM[0]->begin(preferences.getInt("baudrate", 0), SERIAL_PARAM0, SERIAL0_RXPIN, SERIAL0_TXPIN); // Changed to variable Baudrate

  // No Default-GW-Routing, if disabled (default) and Clientmode is off
  if ((preferences.getInt("defgw", 0) == 0) && (preferences.getInt("clientmode", 0) == 0))
  {
    uint8_t val = 0;
    tcpip_adapter_dhcps_option(TCPIP_ADAPTER_OP_SET, TCPIP_ADAPTER_ROUTER_SOLICITATION_ADDRESS, &val, sizeof(dhcps_offer_t));
  }

  if (preferences.getInt("extractstream", 0) == 1)
  {
    extractstream=1;
  }

  if (preferences.getInt("trafficwarnled", 0) == 1)
  {
    trafficwarnled=preferences.getInt("trafficwarnled", 0);
  }

  if (preferences.getInt("clientmode", 0) == 0)
  {
  // Starting Wifi as AP-Mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, wpakey);
  delay(1000);
  WiFi.softAPConfig(ip, ip, netmask);
  Serial.println("Wifi started as AP");
  Serial.print(ssid);
  Serial.print("@");
  Serial.print(wpakey);
  server[0]->begin(preferences.getInt("tcpport", 0)); //Added variable port instead of initalizing outside setup()
  server[0]->setNoDelay(true);
  } else {
  // Starting Wifi as Client
  WiFi.begin(ssid, wpakey);
  Serial.println("Wifi started as Client");
  Serial.print(ssid);
  Serial.print("@");   
  Serial.print(wpakey);
  Serial.print(": connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());  
  }



  // Reducing Wifi-Power if enabled (not default)
  if (preferences.getInt("redpw", 0) == 1)
  {
    esp_err_t esp_wifi_set_max_tx_power(50);
  }

  // Starting UDP for GDL90
  udp.begin(preferences.getInt("udpport", 0));

  // Configuring Webserver
  webserver.on("/", []() {
    webserver.send(200, "text/html", htmlindex);
  });
  webserver.on("/mini-default.min.css", []() {
    webserver.send(200, "text/css", htmlcss);
  });
  webserver.on(F("/gs.json"), HTTP_GET, sendjson);
  webserver.on(F("/resetsettings"), HTTP_GET, defaultsettings);
  webserver.on(F("/savesettings"), HTTP_GET, applysettings);

  // Starting Webserver
  webserver.begin(80);
}


void loop()
{
  // Handling Webserver
  webserver.handleClient();

  // Full code of AP-Mode
  if (preferences.getInt("clientmode", 0) == 0)
      {
      // Handle TCP-Server
      if (server[0]->hasClient())
      {
        for (byte i = 0; i < MAX_NMEA_CLIENTS; i++) {
          if (!TCPClient[0][i] || !TCPClient[0][i].connected()) {
            if (TCPClient[0][i]) TCPClient[0][i].stop();
            TCPClient[0][i] = server[0]->available();
            continue;
          }
        }
        WiFiClient TmpserverClient = server[0]->available();
        TmpserverClient.stop();
      }
    
      // Checking if Fake-Mode is activated (deactivating serial-wifi-bridging)
      if (preferences.getInt("fakenmea", 0) == 1 || preferences.getInt("fakedl90", 0) == 1)
      {
        // Global delay for both NMEA and GDL90 in Fake-Mode
        delay(400);
    
        // Fakecode for NMEA
        if (preferences.getInt("fakenmea", 0) == 1)
        {
          // Simple faking of an static messages
          for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++)
          {
            if (TCPClient[0][cln])
              TCPClient[0][cln].println("$GPRMC,081834.825,A,5417.91,N,00940.95,E,70.0,120.0,040821,,,*13"); //Fake-GPRMC
              TCPClient[0][cln].println("$GPGSA,A,3,10,16,18,26,,,,,,,,,4.56,3.60,2.80*01"); //Fake-GPGSA
              TCPClient[0][cln].println("$PGRMZ,137,F,2*3F"); //Fake-PGRMZ
          }
          if (extractstream==1) // Extract data vom Stream if option is turned on
          {
              extractdata("$GPRMC,081834.825,A,5417.91,N,00940.95,E,70.0,120.0,040821,,,*13"); //Fake-GPRMC
              delay(5);
              extractdata("$GPGSA,A,3,10,16,18,26,,,,,,,,,4.56,3.60,2.80*01"); //Fake-GPGSA
              delay(5);
              extractdata("$PGRMZ,137,F,2*3F"); //Fake-PGRMZ
    
          }
        }
    
        // Fakecode for GDL90
        if (preferences.getInt("fakegdl90", 0) == 1)
        {
          // removed in this release
        }
    
    
      } else
      {
        // No simulation, doing real serial-wifi-bridging
        if (COM[0] != NULL)
        {
          if (COM[0]->available())
          {
            while (COM[0]->available())
            {
              buf2[0][i2[0]] = COM[0]->read();
              if (i2[0] < bufferSize - 1) i2[0]++;
            }
    
            // Debug received Data to serial
            if (debug > 1)
            {
              Serial.println("Full received data:");
              Serial.write(buf2[0], i2[0]);
            }
    
            if (extractstream==1) // Extract data vom Stream if option is turned on
            {
                  // Put received Data to String for interpreting
                  int serialcount = 0;
                  while (serialcount < i2[0])
                  {
                    // Check for Carriage return, If yes=Break string and push to interpreter
                    if (buf2[0][serialcount] == 10)
                    {
                      if (debug > 1)
                      {
                        Serial.print("Separated message:");
                        Serial.println(inserial);
                      }
                      extractdata(inserial); // Push data to interpreter
                      inserial = ""; // New empty string after new line
                    }
                    else
                    {
                      inserial += (char) buf2[0][serialcount];  // Add data to String, ir not CR
                    }
                    serialcount++;
                  }
            }
    
            for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++)
            {
              if (TCPClient[0][cln])
                TCPClient[0][cln].write(buf2[0], i2[0]);
            }
            i2[0] = 0;
          }
        }
      }
  } else {
  
            // Code for Clientmode
              const uint16_t port = preferences.getInt("tcpport", 0); // Use the configurable TCP-Port in Clientmode, too
              const char * host = "192.168.1.1";
              WiFiClient client;
              if (!client.connect(host, port)) {
                  Serial.println("Connection failed.");
                  return;
              }
            int maxloops = 0;
          
            //wait for the server's reply to become available
            while (!client.available() && maxloops < 1000)
            {
              maxloops++;
            }
            if (client.available() > 0)
            {
             if (debug > 1)
            {
                Serial.println(client.readString());
            }
             // Sending received data on serial
             COM[0]->print(client.readString());

            }
            client.stop();
  }
  


}
