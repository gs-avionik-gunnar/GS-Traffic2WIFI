// Changelog Softwareversion 4.0 A1:   Public-Testrelease for developing new features. Changed to Arduino IDE 2.xh
//                                      Compiling should work with Arduino IDE 2.3.2, Espressif ESP32 2.0.11, Neopixel 1.3.2 and M5Stack 2.1.1
//                                      ! ! ! THIS RELEASE IS NOT TESTED AND MIGHT BE VERY UNSTABLE ! ! !
//                                      Todo:
//                                          - GDL90 Multiple-Traffic in Queue / Change whole traffic handling
//                                          - Add different Countrycodes for Wifi
//                                          - Add Traffic-Screen and Traffic Warnining via Trafficmod-LEDs
//                                          - Add Store, Filter and Forward (sffmode/sffpflax) Options
//                                          - Add GDL90 Traffic based on NMEA-Traffic
//                                      -----------
//                                      Added: Experimental GDL90 Ownship-Report based on NMEA-Data - SkyDemon only (I think)
//                                      Added: Support for GS-TRAFFIC2WIFIv3-Hardware (Dev) incl. Rotary-Encoder, Screen and Trafficmo-LED-Circle
//                                      Added: Experimental traffic.json to display array of traffic-data
//                                      Added: Some more data to NMEA-Fakecode, which is also used for Extract/Simulation and GDL90 Examples
//                                      Added: Spinning wheel and black overlay when json isn't already loaded and variables are not available
//                                      Added: TCP-Server for debugging purpose
//                                      Added: A lot more status-details of internal processes in gs.json and Status-Page (connected Wifi-Clients, HEAP etc.)
//                                      Added: Restart-Button in WebUI
//                                      Added: Support for second core of ESP32, resulting in more stable connections and performance for new features
//                                      Added: Duo-LED is now showing Firmware-Version as boot-seq on statup
//                                      Modified: Structure of Sourcecode
// Changelog Softwareversion 3.0c:      Fixed: Modified Serial-Code to solve some of the 3.0b bugs
// Changelog Softwareversion 3.0b:      Changed Read-MAC-Function to be compatible to newer ESP32-Libraries.
//                                      Modified: WIFI-Security is now WPA2-PSK for not generating "Unsecure" Warning on Apple-Devices
// Changelog Softwareversion 3.0:       Fixed: Changed Country-Code for WIFI-Mode to DE
//                                      Tested: Tested with 1.0, 2.0 DEV and 2.0 Final Hardware without any problems
// Changelog Softwareversion 3.0 BETA2: Added: Experimental Fake-Mode for Serial-Output is now part of this firmware (replaces dedicated Faker-Firmware for my own development-setup)
//                                      Fixed: Updatecycles will be 0 in WebUI if update <3.0 and no Default-Settings are loaded
//                                      Fixed: Changed Update text from Failure to Unknown-State for updates, which are handled very quick
// Changelog Softwareversion 3.0 BETA:  Fixed: Saving settings will u++se POST instead of GET for changed parameters
//                                      Fixed: Resetsettings will now work all the times
//                                      Fixed: Minor Web-UI Changes for better usability
//                                      Added: Support for 2.0 Hardware (DEV-Devices)
//                                      Added: Lot more details in http://192.168,1.1/gs.json for debugging
//                                      Added: Showing Compile-Date of Firmware in WebUI
//                                      Added virtual Status-LED in WebUI (Works like the real Status-LED)
//                                      Added Orange-LED-Out-Warning if no valid Position is avilable, configurable in Web-UI
//                                      Added New-LED-Bootup-Sequence
//                                      Added experimental Debug-Mode-Activation via Web-UI (mostly usable on 2.0 Devices)
//                                      Rerwritten code for Trafficwarn-LED, more Options in Web-UI
// Changelog Softwareversion 2.3b:      Saving settings will sometimes not work due to new Update-Form of 2.3->Fixed
// Changelog Softwareversion 2.3:       Added Feature to update Firmware via Webgui
//                                      Added tweaks for future hardware-versions
//                                      Added tooltips in WebUI for all config-options
//                                      Increased maximum of simultaneous NMEA-Clients to 10 devices
// Changelog Softwareversion 2.2:       Added experimental Client-Mode for Wifi and TCP-Bridging
//                                      Added Dualcolor LED on GPIO 18 and 5 for Status of the module and/or Traffic-Warning
//                                      Added baud rates: 115.200 and 230.400
// Changelog Softwareversion 2.1:       Added extracting data from $gpgsa and $pgrmz messages
// Changelog Softwareversion 2.0:       Added Webserver for Status und Config, Added basic tweaks for Faking GDL90 and NMEA (no Code for faking GDL90 included)
// Changelog Softwareversion 1.1:       Tweak for disabling default-gateway
// Changelog Softwareversion 1.0:       First stable version - airborne tested :)
// Instructions for Resetting:          Put PIN19 to GND at Startup, after a reboot defaults are restores

static void debuglog(String logstring, int loglevel);

#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <esp_task_wdt.h>
#include <esp_adc_cal.h>
#include <M5Dial.h>
#include "GS-Traffic2WIFI-definitions.h"
#include "GS-Traffic2WIFI-variables.h"
#include "GS-Traffic2WIFI-webservercontent.h"
#include "GS-Traffic2WIFI-gdl90.h"
#include "GS-Traffic2WIFI-tmled.h"


// Generating json of wifi-stations
String Wifistations() {
  String returnstring;
  wifi_sta_list_t stationList;
  esp_wifi_ap_get_sta_list(&stationList);
  returnstring = ",\"wifistations\":";
  returnstring += stationList.num;
  returnstring += ",";
  returnstring += "\"stations\":[";
  for (int i = 0; i < stationList.num; i++) {
    returnstring += "{\"mac\":\"";
    wifi_sta_info_t station = stationList.sta[i];
    for (int j = 0; j < 6; j++) {
      char str[3];
      sprintf(str, "%02x", (int)station.mac[j]);
      returnstring += str;
      if (j < 5) {
        returnstring += ":";
      }
    }
    if (stationList.num == i + 1) {
      returnstring += "\"}";
    } else {
      returnstring += "\"},";
    }
  }
  returnstring += "]";
  return returnstring;
}

int planedb(String trafficid)  // Search for Slot/Space to store Traffic-Information
{
  int result = 100;
  if (trafficid != "") {
    for (int j = 0; j < maxtraffic; j++) {
      if (traffic[j][0] != "PFLAA") {
        result = j;
      }
    }
    for (int i = 0; i < maxtraffic; i++) {
      if (traffic[i][2] == trafficid) {
        result = i;
      }
    }
  }
  return result;
}

int nmea0183_checksum(char *nmea_data) {
  int crc = 0;
  int i;
  for (i = 1; i < strlen(nmea_data); i++) {
    crc ^= nmea_data[i];
  }
  return crc;
}

void outputfakenmea(char *nmea_data, int mode) {
  if (mode == 1)  // Serial-Fakemode
  {
    Serial2.print(nmea_data);
    Serial2.print("*");
    Serial2.println(nmea0183_checksum(nmea_data), HEX);
  }

  if (mode == 0)  // Wifi-Fakemode
  {
    for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
      if (TCPClient[0][cln]) {
        TCPClient[0][cln].print(nmea_data);
        TCPClient[0][cln].print("*");
        TCPClient[0][cln].println(nmea0183_checksum(nmea_data), HEX);
      }
    }
  }

  if (extractstream == 1)  // Extract data from Stream if option is turned on
  {
    extractdata(String(nmea_data) + "*" + String(nmea0183_checksum(nmea_data), HEX));
  }
}

void fakenmea(int mode) {
  fakestartlat = fakestartlat - 0.005;
  fakestartlon = fakestartlon - 0.008;
  fakealt = fakealt + 0.2;

  fake01lat = fake01lat - 3;
  fake01lon = fake01lon + 4;
  fake02lat = fake02lat + 10;
  fake02lon = fake02lon + 6;
  fake03lat = fake03lat - 100;
  fake03lon = fake03lon + 400;

  String opstring;
  opstring = "$GPRMC,185726.800,A,";
  opstring += String(fakestartlat, 3);
  opstring += ",N,00";
  opstring += String(fakestartlon, 3);
  opstring += ",E,43,259,191221,0.0,E,A";
  outputfakenmea(&opstring[0], mode);

  opstring = "$GPGGA,185726.800,";
  opstring += String(fakestartlat, 3);
  opstring += ",N,";
  opstring += String(fakestartlon, 3);
  opstring += ",E,9,10,1.0,";
  opstring += String(fakealt, 3);
  opstring += ",M,50.0,M,1,";
  outputfakenmea(&opstring[0], mode);

  opstring = "$GPGSA,A,3,24,,,,,,,,,,,,1.0,1.0,1.0";
  outputfakenmea(&opstring[0], mode);

  opstring = "$PFLAA,0,";
  opstring += fake01lat;
  opstring += ",";
  opstring += fake01lon;
  opstring += ",220,2,FAKE01,180,,30,-1.4,1";
  outputfakenmea(&opstring[0], mode);

  opstring = "$PFLAA,0,";
  opstring += fake02lat;
  opstring += ",";
  opstring += fake02lon;
  opstring += ",2000,2,FAKE01,90,,60,-1.4,1";
  outputfakenmea(&opstring[0], mode);

  opstring = "$PFLAA,0,";
  opstring += fake03lat;
  opstring += ",";
  opstring += fake03lon;
  opstring += ",500,2,FAKE01,240,,100,-1.4,1";
  outputfakenmea(&opstring[0], mode);



  opstring = "$PFLAU,1,1,2,1,2,-30,2,-32,755";
  outputfakenmea(&opstring[0], mode);
}

void debuglog(String logstring, int loglevel) {
  // Debug to TCP
  if (preferences.getInt("debugtcp", 0) > 0) {
    for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
      if (TCPClient[1][cln])
        TCPClient[1][cln].println(logstring);
    }
  }
  // Debug to serial
  if (debug > 0) {
    Serial.println(logstring);
  }
}


// Sending jquery.min.js to Client
void onJavaScript(void) {
  webserver.setContentLength(30178);  // Length is hardcoded
  webserver.sendHeader(F("Content-Encoding"), F("gzip"));
  webserver.send_P(200, "text/javascript", jquery_min_js_v3_2_1_gz, 30178);  // Length is hardcoded
}

// Extracting and saving Parts of NMEA-Data
void extractdata(String nmeax) {

  // Decrease of Counter when extracting NMEA
  if (trafficwarning > 0) {
    trafficwarning = trafficwarning - 1;
  }
  if (positionwarning > 0) {
    positionwarning = positionwarning - 1;
  }

  int str_len = nmeax.length() + 1;
  char nmea[str_len];
  nmeax.toCharArray(nmea, str_len);
  // Debug string to Serial
  if (debug > 0) {
    Serial.print("Extracting: ");
    Serial.println(nmea);
  }
  // Debug to TCP
  if (preferences.getInt("debugtcp", 0) > 0) {
    for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
      if (TCPClient[1][cln])
        TCPClient[1][cln].println("Extracting");
      TCPClient[1][cln].println(nmea);
    }
  }
  char *nmeaa = NULL;
  char *bufptr = nmea;
  nmeaa = strsep(&bufptr, ",");

  String messagetype = nmeaa;
  // Extract Data, if Message is $GPRMC
  if (messagetype == "$GPRMC") {
    int inmeaa = 0;
    gprmc = "";
    String msg = nmeaa;
    while (nmeaa != NULL) {
      msg = nmeaa;
      if (inmeaa == 1) {
        if (preferences.getInt("sendudpfromnmea", 0) == 1) {
          nmea2gdl90ssmutc = atof(nmeaa);
        }
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Time of message\",";
        gprmc += "\"v\":\"";
        if (nmeaa != ",") {
          gprmc += nmeaa;
        }
        gprmc += "\",";
        gprmc += "\"h\":\"";
        if (msg != "") {
          gprmc += nmeaa[0];
          gprmc += nmeaa[1];
          gprmc += ":";
          gprmc += nmeaa[2];
          gprmc += nmeaa[3];
          gprmc += ":";
          gprmc += nmeaa[4];
          gprmc += nmeaa[5];
          gprmc += " UTC";

          nmea2gdl90time = nmeaa[0];
          nmea2gdl90time += nmeaa[1];
          nmea2gdl90time += ":";
          nmea2gdl90time += nmeaa[2];
          nmea2gdl90time += nmeaa[3];
        }
        gprmc += "\"},";
      }
      if (inmeaa == 2) {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Position state\",";
        gprmc += "\"v\":\"";
        gprmc += nmeaa;
        gprmc += "\",";
        gprmc += "\"h\":\"";
        if (msg == "A") {
          gprmc += "VALID";
          nmea2gdl90satsignal = 1;
          // Generate no POS-Warning based on configuation
          if (positionwarnled > 0) {
            trafficwarning = 0;
          }
        } else {
          gprmc += "NOT VALID";
          nmea2gdl90satsignal = 0;
          // Generate POS-Warning based on configuation
          if (positionwarnled > 0) {
            trafficwarning = preferences.getInt("posresetcycles", 0);
          }
        }
        gprmc += "\"},";
      }
      if (inmeaa == 3) {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Latitude\",";
        gprmc += "\"v\":\"";
        gprmc += nmeaa;
        gprmc += "\",";
        gprmc += "\"h\":\"";
        gprmc += nmeaa;
        gprmc += "\"},";
        // Add data to variables of GDL90
        if (preferences.getInt("sendudpfromnmea", 0) == 1) {
          nmea2gdl90lat = GpsToDecimalDegrees(nmeaa, nmea2gdl90latquad);
        }
      }
      if (inmeaa == 4) {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Latitude direction\",";
        gprmc += "\"v\":\"";
        gprmc += nmeaa;
        gprmc += "\",";
        gprmc += "\"h\":\"";
        gprmc += nmeaa;
        gprmc += "\"},";
        if (preferences.getInt("sendudpfromnmea", 0) == 1) {
          nmea2gdl90latquad = nmeaa;
        }
      }
      if (inmeaa == 5) {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Longitude \",";
        gprmc += "\"v\":\"";
        gprmc += nmeaa;
        gprmc += "\",";
        gprmc += "\"h\":\"";
        gprmc += nmeaa;
        gprmc += "\"},";
        // Add data to variables of GDL90
        if (preferences.getInt("sendudpfromnmea", 0) == 1) {
          nmea2gdl90lon = GpsToDecimalDegrees(nmeaa, nmea2gdl90lonquad);
        }
      }
      if (inmeaa == 6) {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Longitude direction \",";
        gprmc += "\"v\":\"";
        gprmc += nmeaa;
        gprmc += "\",";
        gprmc += "\"h\":\"";
        gprmc += nmeaa;
        gprmc += "\"},";
        if (preferences.getInt("sendudpfromnmea", 0) == 1) {
          nmea2gdl90lonquad = nmeaa;
        }
      }
      if (inmeaa == 7) {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Speed over Ground \",";
        gprmc += "\"v\":\"";
        gprmc += nmeaa;
        gprmc += "\",";
        gprmc += "\"h\":\"";
        if (msg != "") {
          gprmc += nmeaa;
          gprmc += " kts";
          // Add data to variables of GDL90
          if (preferences.getInt("sendudpfromnmea", 0) == 1) {
            nmea2gdl90groundspeed = atof(nmeaa);
          }
        }
        gprmc += "\"},";
      }
      if (inmeaa == 8) {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Course direction \",";
        gprmc += "\"v\":\"";
        gprmc += nmeaa;
        gprmc += "\",";
        gprmc += "\"h\":\"";
        if (msg != "") {
          gprmc += nmeaa;
          gprmc += " deg";
          // Add data to variables of GDL90
          if (preferences.getInt("sendudpfromnmea", 0) == 1) {
            nmea2gdl9track = atof(nmeaa);
          }
        }
        gprmc += "\"},";
      }
      if (inmeaa == 9) {
        gprmc += "{\"t\":\"$GPRMC\",\"p\":\"Date \",";
        gprmc += "\"v\":\"";
        gprmc += nmeaa;
        gprmc += "\",";
        gprmc += "\"h\":\"";
        if (msg != "") {
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
    if (debug > 0) {
      Serial.print("Result of extracting $GPRMC: ");
      Serial.println(gprmc);
    }
    // Debug to TCP
    if (preferences.getInt("debugtcp", 0) > 0) {
      for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
        if (TCPClient[1][cln])
          TCPClient[1][cln].println("Result of extracting $GPRMC: ");
        TCPClient[1][cln].println(gprmc);
      }
    }
  }

  // Extract Data, if Message is $gpgsa
  if (messagetype == "$GPGSA") {
    int inmeaa = 0;
    gpgsa = "";
    while (nmeaa != NULL) {
      if (inmeaa == 1) {
        gpgsa += "{\"t\":\"$GPGSA\",\"p\":\"Fixmode\",";
        gpgsa += "\"v\":\"";
        if (nmeaa != ",") {
          gpgsa += nmeaa;
        }
        gpgsa += "\",";
        gpgsa += "\"h\":\"";
        if (nmeaa != ",") {
          gpgsa += nmeaa;
        }
        gpgsa += "\"},";
      }

      if (inmeaa == 2) {
        gpgsa += "{\"t\":\"$GPGSA\",\"p\":\"Fix\",";
        gpgsa += "\"v\":\"";
        if (nmeaa != ",") {
          gpgsa += nmeaa;
        }
        gpgsa += "\",";
        gpgsa += "\"h\":\"";
        if (nmeaa != ",") {
          gpgsa += nmeaa;
        }
        gpgsa += "\"},";
      }

      if (inmeaa == 3) {
        gpgsa += "{\"t\":\"$GPGSA\",\"p\":\"GPS Satelites\",";
        gpgsa += "\"v\":\"";
        if (nmeaa != ",") {
          gpgsa += nmeaa;
          String temp = nmeaa;
          nmea2gdl90satellites = temp.toInt();
        }
        gpgsa += "\",";
        gpgsa += "\"h\":\"";
        if (nmeaa != ",") {
          gpgsa += nmeaa;
        }
        gpgsa += "\"},";
      }

      if (inmeaa == 15) {
        gpgsa += "{\"t\":\"$GPGSA\",\"p\":\"Position dilution of precision\",";
        gpgsa += "\"v\":\"";
        if (nmeaa != ",") {
          gpgsa += nmeaa;
        }
        gpgsa += "\",";
        gpgsa += "\"h\":\"";
        if (nmeaa != ",") {
          gpgsa += nmeaa;
        }
        gpgsa += "\"},";
      }

      if (inmeaa == 16) {
        gpgsa += "{\"t\":\"$GPGSA\",\"p\":\"Horizontal dilution of precision\",";
        gpgsa += "\"v\":\"";
        if (nmeaa != ",") {
          gpgsa += nmeaa;
        }
        gpgsa += "\",";
        gpgsa += "\"h\":\"";
        if (nmeaa != ",") {
          gpgsa += nmeaa;
        }
        gpgsa += "\"},";
      }

      nmeaa = strsep(&bufptr, ",");
      inmeaa++;
    }
    // Debug full string to Serial
    if (debug > 0) {
      Serial.print("Result of extracting $GPGSA: ");
      Serial.println(gpgsa);
    }
    if (preferences.getInt("debugtcp", 0) > 0) {
      for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
        if (TCPClient[1][cln])
          TCPClient[1][cln].println("Result of extracting $GPGSA: ");
        TCPClient[1][cln].println(gpgsa);
      }
    }
  }

  // Extract Data, if Message is $pgrmz
  if (messagetype == "$PGRMZ") {
    int inmeaa = 0;
    pgrmz = "";
    String msg = nmeaa;
    while (nmeaa != NULL) {
      msg = nmeaa;
      if (inmeaa == 1) {
        pgrmz += "{\"t\":\"$PGRMZ\",\"p\":\"Altitude Barometric\",";
        pgrmz += "\"v\":\"";
        if (nmeaa != ",") {
          pgrmz += nmeaa;
        }
        pgrmz += "\",";
        pgrmz += "\"h\":\"";
        if (nmeaa != ",") {
          pgrmz += nmeaa;
        }
        pgrmz += "ft\"},";
      }

      nmeaa = strsep(&bufptr, ",");

      inmeaa++;
    }
    // Debug full string to Serial
    if (debug > 0) {
      Serial.print("Result of extracting $PGRMZ: ");
      Serial.println(pgrmz);
    }
    // Debug to TCP
    if (preferences.getInt("debugtcp", 0) > 0) {
      for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
        if (TCPClient[1][cln])
          TCPClient[1][cln].println("Result of extracting $PGRMZ: ");
        TCPClient[1][cln].println(pgrmz);
      }
    }
  }


  // Extract Data, if Message is $gpgga
  if (messagetype == "$GPGGA") {
    int inmeaa = 0;
    gpgga = "";
    String msg = nmeaa;
    while (nmeaa != NULL) {
      msg = nmeaa;
      if (inmeaa == 9) {
        gpgga += "{\"t\":\"$GPGGA\",\"p\":\"Altitude MSL\",";
        gpgga += "\"v\":\"";
        if (nmeaa != ",") {
          gpgga += nmeaa;
          // Add data to variables of GDL90
          if (preferences.getInt("sendudpfromnmea", 0) == 1) {
            nmea2gdl90altitide = atof(nmeaa) * 3.2808;  // Converting Altitude m to feet // Test
          }
        }
        gpgga += "\",";
        gpgga += "\"h\":\"";
        if (nmeaa != ",") {
          gpgga += nmeaa;
        }
        gpgga += "m\"},";
      }
      if (inmeaa == 11) {
        gpgga += "{\"t\":\"$GPGGA\",\"p\":\"Altitude geodetic WGS84\",";
        gpgga += "\"v\":\"";
        if (nmeaa != ",") {
          gpgga += nmeaa;
          // Add data to variables of GDL90
          if (preferences.getInt("sendudpfromnmea", 0) == 1) {
            nmea2gdl90altitidegeodetic = atof(nmeaa) * 3.2808;  // Converting Altitude m to feet // Test
          }
        }
        gpgga += "\",";
        gpgga += "\"h\":\"";
        if (nmeaa != ",") {
          gpgga += nmeaa;
        }
        gpgga += "m\"},";
      }

      nmeaa = strsep(&bufptr, ",");

      inmeaa++;
    }
    // Debug full string to Serial
    if (debug > 0) {
      Serial.print("Result of extracting $GPGGA: ");
      Serial.println(gpgga);
    }
    // Debug to TCP
    if (preferences.getInt("debugtcp", 0) > 0) {
      for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
        if (TCPClient[1][cln])
          TCPClient[1][cln].println("Result of extracting $GPGGA: ");
        TCPClient[1][cln].println(gpgga);
      }
    }
  }


  // Extract Data, if Message is $pflau
  if (messagetype == "$PFLAU") {
    int inmeaa = 0;
    pflau = "";
    String msg = nmeaa;
    while (nmeaa != NULL) {
      msg = nmeaa;
      if (inmeaa == 5) {
        pflau += "{\"t\":\"$PFLAU\",\"p\":\"Alarmlevel\",";
        pflau += "\"v\":\"";
        if (nmeaa != ",") {
          pflau += nmeaa;
        }
        pflau += "\",";
        pflau += "\"h\":\"";
        if (nmeaa != ",") {
          pflau += nmeaa;

          // Generate Taffic-Warning based on configuation
          if (trafficwarnled > 0) {
            if (msg.toInt() >= trafficwarnled) {
              trafficwarning = preferences.getInt("twresetcycles", 0);
            }
          }
        }
        pflau += "\"},";
      }

      nmeaa = strsep(&bufptr, ",");

      inmeaa++;
    }
    // Debug full string to Serial
    if (debug > 0) {
      Serial.print("Result of extracting $PFLAU: ");
      Serial.println(pflau);
    }
    // Debug to TCP
    if (preferences.getInt("debugtcp", 0) > 0) {
      for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
        if (TCPClient[1][cln])
          TCPClient[1][cln].println("Result of extracting $PFLAU: ");
        TCPClient[1][cln].println(pflau);
      }
    }
  }




  // Extract Data, if Message is $pflaa
  if (messagetype == "$PFLAA") {
    String tfcgs = "";
    String tfcalt = "";
    String tfcid = "";
    String tfcidtype = "";
    String tfcnorth = "";
    String tfceast = "";
    String tfchead = "";
    String tfcclimb = "";
    String tfcactype = "";
    int inmeaa = 0;
    pflaa = "";
    String msg = nmeaa;
    while (nmeaa != NULL) {
      msg = nmeaa;

      if (preferences.getInt("sndudptffromnm", 0) == 1) {
        if (inmeaa == 2) {
          tfcnorth = nmeaa;
        }
        if (inmeaa == 3) {
          tfceast = nmeaa;
        }
        if (inmeaa == 4) {
          tfcalt = nmeaa;
        }
        if (inmeaa == 5) {
          tfcidtype = nmeaa;
        }
        if (inmeaa == 6) {
          tfcid = nmeaa;
        }
        if (inmeaa == 7) {
          tfchead = nmeaa;
        }
        if (inmeaa == 9) {
          tfcgs = nmeaa;
        }
        if (inmeaa == 10) {
          tfcclimb = nmeaa;
        }
        if (inmeaa == 11) {
          tfcactype = nmeaa;
        }
      }

      nmeaa = strsep(&bufptr, ",");
      inmeaa++;
    }
    if (preferences.getInt("sndudptffromnm", 0) == 1) {
      int space = planedb(tfcid);
      if (space < 100 && tfcid != "") {
        String logtext = tfcid;
        logtext += ": space is ";
        logtext += space;
        logtext += ". Writing!";
        debuglog(logtext, 1);
        traffic[space][0] = "PFLAA";
        traffic[space][1] = "123456";
        traffic[space][2] = tfcid;
        traffic[space][3] = tfcidtype;
        traffic[space][4] = tfcactype;
        traffic[space][5] = tfcnorth;
        traffic[space][6] = tfceast;
        traffic[space][7] = tfcalt;
        traffic[space][8] = tfchead;
        traffic[space][9] = tfcgs;
        traffic[space][10] = tfcclimb;
      }
      String logtext = tfcid;
      logtext += ": no space left.";
      debuglog(logtext, 1);
    }

    // Debug full string to Serial
    if (debug > 0) {
      Serial.print("Result of extracting $PFLAA: ");
      Serial.println(pflaa);
    }
    // Debug to TCP
    if (preferences.getInt("debugtcp", 0) > 0) {
      for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
        if (TCPClient[1][cln])
          TCPClient[1][cln].println("Result of extracting $PFLAA: ");
        TCPClient[1][cln].println(pflaa);
      }
    }
  }
}


// Restart
void restart() {
  webserver.send(200, "text/html", "...rebooting...");
  delay(500);
  ESP.restart();
}



// Reset to "Default-Settings"
void defaultsettings() {
  esp_efuse_mac_get_default(chipid);
  sprintf(ssid, "GS-Traffic-%02x%02x", chipid[4], chipid[5]);
  sprintf(wpakey, "%02x%02x%02x%02x", chipid[2], chipid[3], chipid[4], chipid[5]);

  String tmpssid = ssid;
  String tmpwpakey = wpakey;
  preferences.begin("GS-Traffic", false);
  preferences.clear();
  preferences.putString("countrycode", "DE");
  preferences.putString("ssid", tmpssid);
  preferences.putString("wpakey", tmpwpakey);
  preferences.putInt("sffmode", 0);
  preferences.putInt("sffpflax", 0);
  preferences.putInt("debug", 0);
  preferences.putInt("defgw", 0);
  preferences.putInt("redpw", 0);
  preferences.putInt("debugtcp", 0);
  preferences.putInt("baudrate", 19200);
  preferences.putInt("tcpport", 2000);
  preferences.putInt("udpport", 4000);
  preferences.putInt("sndudptffromnm", 0);
  preferences.putInt("fakenmea", 0);
  preferences.putInt("fakenmeaserial", 0);
  preferences.putInt("clientmode", 0);
  preferences.putInt("trafficwarnled", 0);
  preferences.putInt("positionwarnled", 0);
  preferences.putInt("twresetcycles", 50);
  preferences.putInt("posresetcycles", 50);
  preferences.putInt("tmbright", 30);
  preferences.putInt("screenbright", 100);

  if (hwtype == "GS-Traffic2WIFIv3") {
    preferences.putInt("extractstream", 1);
    preferences.putInt("trafficmod", 1);
    preferences.putInt("sendudpfromnmea", 1);
    preferences.putInt("sendudphbstx", 1);
  } else {
    preferences.putInt("extractstream", 0);
    preferences.putInt("trafficmod", 0);
    preferences.putInt("sendudpfromnmea", 0);
    preferences.putInt("sendudphbstx", 0);
  }

  preferences.end();
  webserver.send(200, "text/html", "Settings restored to default...rebooting...");
  delay(500);
  ESP.restart();
}


// Apply new settings
void applysettings() {
  preferences.begin("GS-Traffic", false);
  preferences.clear();
  preferences.putString("countrycode", webserver.arg("countrycode"));
  preferences.putString("ssid", webserver.arg("ssid"));
  preferences.putString("wpakey", webserver.arg("wpakey"));
  preferences.putInt("baudrate", webserver.arg("baudrate").toInt());
  preferences.putInt("tcpport", webserver.arg("tcpport").toInt());
  preferences.putInt("debug", webserver.arg("debug").toInt());
  preferences.putInt("debugtcp", webserver.arg("debugtcp").toInt());
  preferences.putInt("udpport", webserver.arg("udpport").toInt());
  preferences.putInt("trafficwarnled", webserver.arg("trafficwarnled").toInt());
  preferences.putInt("twresetcycles", webserver.arg("twresetcycles").toInt());
  preferences.putInt("posresetcycles", webserver.arg("posresetcycles").toInt());
  preferences.putInt("tmbright", webserver.arg("tmbright").toInt());
  preferences.putInt("screenbright", webserver.arg("screenbright").toInt());
  if (webserver.arg("sffmode") == "on") {
    preferences.putInt("sffmode", 1);
  } else {
    preferences.putInt("sffmode", 0);
  }
  if (webserver.arg("sffpflax") == "on") {
    preferences.putInt("sffpflax", 1);
  } else {
    preferences.putInt("sffpflax", 0);
  }
  if (webserver.arg("sendudpfromnmea") == "on") {
    preferences.putInt("sendudpfromnmea", 1);
  } else {
    preferences.putInt("sendudpfromnmea", 0);
  }
  if (webserver.arg("sndudptffromnm") == "on") {
    preferences.putInt("sndudptffromnm", 1);
  } else {
    preferences.putInt("sndudptffromnm", 0);
  }
  if (webserver.arg("sendudphbstx") == "on") {
    preferences.putInt("sendudphbstx", 1);
  } else {
    preferences.putInt("sendudphbstx", 0);
  }
  if (webserver.arg("trafficmod") == "on") {
    preferences.putInt("trafficmod", 1);
  } else {
    preferences.putInt("trafficmod", 0);
  }
  if (webserver.arg("positionwarnled") == "on") {
    preferences.putInt("positionwarnled", 1);
  } else {
    preferences.putInt("positionwarnled", 0);
  }
  if (webserver.arg("extractstream") == "on") {
    preferences.putInt("extractstream", 1);
  } else {
    preferences.putInt("extractstream", 0);
  }
  if (webserver.arg("defgw") == "on") {
    preferences.putInt("defgw", 1);
  } else {
    preferences.putInt("defgw", 0);
  }
  if (webserver.arg("redpw") == "on") {
    preferences.putInt("redpw", 1);
  } else {
    preferences.putInt("redpw", 0);
  }
  if (webserver.arg("fakenmea") == "on") {
    preferences.putInt("fakenmea", 1);
  } else {
    preferences.putInt("fakenmea", 0);
  }
  if (webserver.arg("fakenmeaserial") == "on") {
    preferences.putInt("fakenmeaserial", 1);
  } else {
    preferences.putInt("fakenmeaserial", 0);
  }
  if (webserver.arg("clientmode") == "on") {
    preferences.putInt("clientmode", 1);
  } else {
    preferences.putInt("clientmode", 0);
  }

  preferences.end();
  webserver.send(200, "text/html", "<html><head><meta http-equiv=refresh content=\"2;url=http://192.168.1.1\"/></head><body>Settings saved...rebooting...</body></html>");
  delay(500);
  ESP.restart();
}


// Sending JSON (traffic.json) to Client
void sendtraffic() {
  preferences.begin("GS-Traffic", false);
  String response = "{\"traffic\":[";

  for (int i = 0; i < maxtraffic; ++i) {
    if (traffic[i][0] == "PFLAA") {
      response += "{\"internalid\":\"";
      response += i;
      response += "\",\"identifier\":\"";
      response += traffic[i][2];
      response += "\",\"pos-north\":\"";
      response += traffic[i][5];
      response += "\",\"pos-east\":\"";
      response += traffic[i][6];
      response += "\",\"gs\":\"";
      response += traffic[i][9];
      response += "\",\"altitude\":\"";
      response += traffic[i][7];
      response += "\",\"heading\":\"";
      response += traffic[i][8];
      response += "\",\"climbrate\":\"";
      response += traffic[i][10];
      response += "\"},";
    }
  }

  response += "]}";

  webserver.send(200, "text/json", response);
}



// Sending JSON (gs.json) to Client
void sendjson() {
  preferences.begin("GS-Traffic", false);

  String response = "{";
  response += "\"version\":\"";
  response += versionmajor;
  response += ".";
  response += versionminor;
  response += " ";
  response += versionextend;

  response += "\",\"compiledate\":\"";
  response += compile_date;

  char mac[50] = "";
  esp_efuse_mac_get_default(chipid);

  sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", chipid[0], chipid[1], chipid[2], chipid[3], chipid[4], chipid[5]);

  response += "\",\"mac\":\"";
  response += mac;

  response += "\",\"debug\":";
  response += debug;

  response += ",\"debugtcp\":";
  response += preferences.getInt("debugtcp", 0);

  response += ",\"sffmode\":";
  response += preferences.getInt("sffmode", 0);

  response += ",\"sffpflax\":";
  response += preferences.getInt("sffpflax", 0);

  response += ",\"hwtypeid\":";
  response += hwtypeid;

  response += ",\"hwversion\":\"";
  response += hwtype;

  response += "\",\"efuse-adc-tp\":\"";
  response += esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP);

  response += "\",\"efuse-vref\":\"";
  response += esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF);

  response += "\",\"ssid\":\"";
  response += preferences.getString("ssid", "");

  response += "\",\"wpakey\":\"";
  response += preferences.getString("wpakey", "");

  response += "\",\"countrycode\":\"";
  response += preferences.getString("countrycode", "");

  response += "\",\"extractstream\":";
  response += preferences.getInt("extractstream", 0);

  response += ",\"defgw\":";
  response += preferences.getInt("defgw", 0);

  response += ",\"trafficmod\":";
  response += preferences.getInt("trafficmod", 0);

  response += ",\"tmbright\":";
  response += preferences.getInt("tmbright", 0);

  response += ",\"screenbright\":";
  response += preferences.getInt("screenbright", 0);

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

  response += ",\"fakenmeaserial\":";
  response += preferences.getInt("fakenmeaserial", 0);

  response += ",\"sendudpfromnmea\":";
  response += preferences.getInt("sendudpfromnmea", 0);

  response += ",\"sndudptffromnm\":";
  response += preferences.getInt("sndudptffromnm", 0);

  response += ",\"sendudphbstx\":";
  response += preferences.getInt("sendudphbstx", 0);

  response += ",\"clientmode\":";
  response += preferences.getInt("clientmode", 0);

  response += ",\"trafficwarnled\":";
  response += preferences.getInt("trafficwarnled", 0);

  response += ",\"positionwarnled\":";
  response += preferences.getInt("positionwarnled", 0);

  response += ",\"countertrafficwarning\":";
  response += trafficwarning;

  response += ",\"counterpositionwarning\":";
  response += positionwarning;

  response += ",\"twresetcycles\":";
  response += preferences.getInt("twresetcycles", 0);

  response += ",\"posresetcycles\":";
  response += preferences.getInt("posresetcycles", 0);

  response += ",\"statusled\":";
  response += statusled;

  response += Wifistations();

  response += ",\"cap\":[";

  if (hwtypeid < 100) {
    response += gprmc;
    response += gpgsa;
    response += pgrmz;
    response += gpgga;
    response += pflau;
  }

  if (preferences.getInt("sendudpfromnmea", 0) == 1) {
    response += "{\"t\":\"GDL90 LON\",\"p\":\"generated GDL90\",\"v\":\"";
    response += String(nmea2gdl90lon, 8);
    response += "\",\"h\":\"";
    response += nmea2gdl90lon, 6;
    response += " \"},";

    response += "{\"t\":\"GDL90 LAT\",\"p\":\"generated GDL90\",\"v\":\"";
    response += String(nmea2gdl90lat, 8);
    response += "\",\"h\":\"";
    response += nmea2gdl90lat, 6;
    response += " \"},";
  }

  response += "{\"t\":\"ESP32\",\"p\":\"Free Heap\",\"v\":\"";
  response += ESP.getFreeHeap();
  response += "\",\"h\":\"";
  response += ESP.getFreeHeap();
  response += "\"},";

  response += "{\"t\":\"ESP32\",\"p\":\"Minium Free Heap\",\"v\":\"";
  response += ESP.getMinFreeHeap();
  response += "\",\"h\":\"";
  response += ESP.getMinFreeHeap();
  response += "\"},";

  response += "{\"t\":\"ESP32\",\"p\":\"Chip Model\",\"v\":\"";
  response += ESP.getChipModel();
  response += "\",\"h\":\"";
  response += ESP.getChipModel();
  response += "\"},";

  response += "{\"t\":\"ESP32\",\"p\":\"Chip Cores\",\"v\":\"";
  response += ESP.getChipCores();
  response += "\",\"h\":\"";
  response += ESP.getChipCores();
  response += "\"},";

  response += "{\"t\":\"ESP32\",\"p\":\"Flash Chip Size\",\"v\":\"";
  response += ESP.getFlashChipSize();
  response += "\",\"h\":\"";
  response += ESP.getFlashChipSize();
  response += "\"},";

  response += "{\"t\":\"ESP32\",\"p\":\"Chip Revision\",\"v\":\"";
  response += ESP.getChipRevision();
  response += "\",\"h\":\"";
  response += ESP.getChipRevision();
  response += "\"},";

  response += "{\"t\":\"Firmware\",\"p\":\"Core running webserver\",\"v\":\"";
  response += xPortGetCoreID();
  response += "\",\"h\":\"";
  response += xPortGetCoreID();
  response += "\"}";

  response += "]";

  response += "}";
  webserver.send(200, "text/json", response);
}

void showscreen(int updown, int mode) {

  if (mode == 0) {
    M5Dial.Speaker.tone(8000, 20);
    if (updown == 100) {
      actscreen = 1;
    }
    if ((updown == 0) && (actscreen > 1)) {
      actscreen = actscreen - 1;
    }
    if ((updown == 1) && (actscreen < 13)) {
      actscreen = actscreen + 1;
    }
  }

  if ((actscreen == 1) && (mode == 0)) {
    M5Dial.Display.clear();
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextFont(&fonts::Orbitron_Light_24);
    M5Dial.Display.setTextDatum(bottom_center);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("GS Avionik", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
    M5Dial.Display.setTextDatum(top_center);
    M5Dial.Display.setTextDatum(top_center);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("GS-Traffic2WIFI", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
    delay(100);
  }

  if (actscreen == 3) {
    if (notraffic == 1) {
      actscreen == 2;
    } else {
      M5Dial.Display.clear();
      M5Dial.Display.setTextColor(GREEN);
      M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
      M5Dial.Display.setTextSize(1);
      M5Dial.Display.fillCircle(120, 120, 80, DARKGREY);
      M5Dial.Display.fillCircle(120, 120, 2, WHITE);
      M5Dial.Display.drawString("Traffic", 120, 25);
      M5Dial.Display.setTextFont(&fonts::FreeSansBold9pt7b);
      M5Dial.Display.setTextColor(WHITE);
      M5Dial.Display.drawString("1nm", 200, 120);
      M5Dial.Display.setTextColor(WHITE);
      M5Dial.Display.setTextColor(RED);

      // Dummytraffic-Area
      M5Dial.Display.drawString("+2.1", 100, 140);
      M5Dial.Display.fillTriangle(80, 130, 92, 130, 86, 118, RED);
    }
  }

  if (actscreen == 2) {
    int clear = 0;
    if (mode == 0) {
      M5Dial.Display.clear();
    }
    if ((screensatellites != nmea2gdl90satellites) || (screentime != nmea2gdl90time) || (screensatsignal != nmea2gdl90satsignal)) {
      clear = 1;
      screensatellites = nmea2gdl90satellites;
      screentime = nmea2gdl90time;
      screensatsignal = nmea2gdl90satsignal;
    }
    if (clear == 1) {
      M5Dial.Display.fillRect(0, 70, 240, 240, BLACK);  // Clear Background to update
    }
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("GNSS", 120, 25);
    if (screensatsignal == 1) {
      M5Dial.Display.setTextFont(&fonts::FreeSansBold24pt7b);
      M5Dial.Display.drawString(String(screensatellites), 120, 70);
      M5Dial.Display.drawString("Satellites", 120, 110);
    } else {
      M5Dial.Display.setTextColor(RED);
      M5Dial.Display.setTextFont(&fonts::FreeSansBold18pt7b);
      M5Dial.Display.drawString(String(screensatellites), 120, 70);
      M5Dial.Display.drawString("Satellites", 120, 110);
      M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
      M5Dial.Display.drawString("No position!", 120, 150);
    }
    M5Dial.Display.setTextColor(WHITE);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
    M5Dial.Display.drawString(screentime, 120, 180);
  }

  if (actscreen == 4) {
    int clear = 0;
    if (mode == 0) {
      M5Dial.Display.clear();
    }
    if ((int(nmea2gdl90groundspeed) > screengroundspeed + 1) || (int(nmea2gdl90groundspeed) < screengroundspeed - 1)) {
      clear = 1;
      screengroundspeed = int(nmea2gdl90groundspeed);
    }
    if ((int(nmea2gdl90altitide) > screenaltitide + 1) || (int(nmea2gdl90altitide) < screenaltitide - 1)) {
      clear = 1;
      screenaltitide = int(nmea2gdl90altitide);
    }
    if ((int(nmea2gdl9track) > screentrack + 1) || (int(nmea2gdl9track) < screentrack - 1)) {
      clear = 1;
      screentrack = int(nmea2gdl9track);
    }
    if (clear == 1) {
      M5Dial.Display.fillRect(0, 70, 240, 240, BLACK);  // Clear Background to update
    }

    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("Flightdata", 120, 25);
    M5Dial.Display.setTextColor(WHITE);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold9pt7b);
    M5Dial.Display.drawString("Ground: " + String(screengroundspeed) + " kts", 120, 70);
    M5Dial.Display.drawString("Altitude: " + String(screenaltitide) + " ft", 120, 90);
    M5Dial.Display.drawString("Track: " + String(screentrack) + " deg", 120, 110);
  }

  if ((actscreen == 9) && (mode == 0)) {
    M5Dial.Display.clear();
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("Wifi", 120, 25);
    M5Dial.Display.setTextColor(WHITE);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold9pt7b);
    M5Dial.Display.drawString("SSID: " + preferences.getString("ssid", ""), 120, 70);
    M5Dial.Display.drawString("Key: " + preferences.getString("wpakey", ""), 120, 90);
  }


  if (actscreen == 8) {
    if ((mode == 1) && (refrshbuffs == 0)) {
      // Nothing to update
    } else {
      refrshbuffs = 0;
      if (mode == 0) {
        M5Dial.Display.clear();
      }
      M5Dial.Display.setTextColor(GREEN);
      M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
      M5Dial.Display.setTextSize(1);
      M5Dial.Display.drawString("Brightness", 120, 25);
      M5Dial.Display.setTextFont(&fonts::FreeSansBold9pt7b);
      M5Dial.Display.fillRect(0, 70, 240, 240, BLACK);  // Clear Background to update
      if ((mode == 1) && (actscreenbright == 1)) {
        M5Dial.Display.setTextColor(GREEN);
        M5Dial.Display.drawString("Screen: ", 120, 70);
        M5Dial.Display.drawRoundRect(60, 90, 127, 20, 10, GREEN);
        M5Dial.Display.fillRoundRect(60, 90, round(preferences.getInt("screenbright", 0) / 2), 20, 10, GREEN);
      } else {
        M5Dial.Display.setTextColor(WHITE);
        M5Dial.Display.drawString("Screen: ", 120, 70);
        M5Dial.Display.drawRoundRect(60, 90, 127, 20, 10, WHITE);
        M5Dial.Display.fillRoundRect(60, 90, round(preferences.getInt("screenbright", 0) / 2), 20, 10, WHITE);
      }
      if ((mode == 1) && (actscreenbright == 2)) {
        M5Dial.Display.setTextColor(GREEN);
        M5Dial.Display.drawString("LEDs: ", 120, 120);
        M5Dial.Display.drawRoundRect(60, 140, 127, 20, 10, GREEN);
        M5Dial.Display.fillRoundRect(60, 140, round(preferences.getInt("tmbright", 0) / 2), 20, 10, GREEN);

      } else {
        M5Dial.Display.setTextColor(WHITE);
        M5Dial.Display.drawString("LEDs: ", 120, 120);
        M5Dial.Display.drawRoundRect(60, 140, 127, 20, 10, WHITE);
        M5Dial.Display.fillRoundRect(60, 140, round(preferences.getInt("tmbright", 0) / 2), 20, 10, WHITE);
      }

      M5Dial.Display.setTextColor(WHITE);
      M5Dial.Display.drawString("Press Button to set", 120, 180);
    }
  }


  // Screen Settings
  if (actscreen == 10) {
    if ((mode == 1) && (refrshbuffs == 0)) {
      // Nothing to update
    } else {
      refrshbuffs = 0;
      if (mode == 0) {
        M5Dial.Display.clear();
      }

      M5Dial.Display.setTextColor(GREEN);
      M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
      M5Dial.Display.setTextSize(1);
      M5Dial.Display.drawString("Settings", 120, 25);
      M5Dial.Display.setTextColor(WHITE);
      M5Dial.Display.setTextFont(&fonts::FreeSansBold9pt7b);
      M5Dial.Display.fillRect(0, 70, 240, 240, BLACK);  // Clear Background to update

      int pos = 67;
      if ((mode == 1) && (actscreenset == 1)) {
        M5Dial.Display.setTextColor(GREEN);
        M5Dial.Display.drawString("Default-GW", 90, pos + 2);
        // Setting start
        if (preferences.getInt("defgw", 0) == 0) {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, GREEN);
          M5Dial.Display.fillRoundRect(150, pos, 20, 20, 10, GREEN);
          M5Dial.Display.drawString("OFF", 190, pos + 2);
        } else {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, GREEN);
          M5Dial.Display.fillRoundRect(192, pos, 20, 20, 10, GREEN);
          M5Dial.Display.drawString("ON", 180, pos + 2);
        }
        // Setting end
      } else {
        M5Dial.Display.setTextColor(WHITE);
        M5Dial.Display.drawString("Default-GW", 90, pos + 2);
        // Setting start
        if (preferences.getInt("defgw", 0) == 0) {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, WHITE);
          M5Dial.Display.fillRoundRect(150, pos, 20, 20, 10, WHITE);
          M5Dial.Display.drawString("OFF", 190, pos + 2);
        } else {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, WHITE);
          M5Dial.Display.fillRoundRect(192, pos, 20, 20, 10, WHITE);
          M5Dial.Display.drawString("ON", 180, pos + 2);
        }
        // Setting end
      }

      pos = pos + 22;
      if ((mode == 1) && (actscreenset == 2)) {
        M5Dial.Display.setTextColor(GREEN);
        M5Dial.Display.drawString("Red Wifipwr", 90, pos + 2);
        // Setting start
        if (preferences.getInt("redpw", 0) == 0) {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, GREEN);
          M5Dial.Display.fillRoundRect(150, pos, 20, 20, 10, GREEN);
          M5Dial.Display.drawString("OFF", 190, pos + 2);
        } else {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, GREEN);
          M5Dial.Display.fillRoundRect(192, pos, 20, 20, 10, GREEN);
          M5Dial.Display.drawString("ON", 180, pos + 2);
        }
        // Setting end
      } else {
        M5Dial.Display.setTextColor(WHITE);
        M5Dial.Display.drawString("Red Wifipwr", 90, pos + 2);
        // Setting start
        if (preferences.getInt("redpw", 0) == 0) {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, WHITE);
          M5Dial.Display.fillRoundRect(150, pos, 20, 20, 10, WHITE);
          M5Dial.Display.drawString("OFF", 190, pos + 2);
        } else {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, WHITE);
          M5Dial.Display.fillRoundRect(192, pos, 20, 20, 10, WHITE);
          M5Dial.Display.drawString("ON", 180, pos + 2);
        }
        // Setting end
      }
      pos = pos + 22;
      if ((mode == 1) && (actscreenset == 3)) {
        M5Dial.Display.setTextColor(GREEN);
        M5Dial.Display.drawString("Extract-Data", 90, pos + 2);
        // Setting start
        if (preferences.getInt("extractstream", 0) == 0) {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, GREEN);
          M5Dial.Display.fillRoundRect(150, pos, 20, 20, 10, GREEN);
          M5Dial.Display.drawString("OFF", 190, pos + 2);
        } else {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, GREEN);
          M5Dial.Display.fillRoundRect(192, pos, 20, 20, 10, GREEN);
          M5Dial.Display.drawString("ON", 180, pos + 2);
        }
        // Setting end
      } else {
        M5Dial.Display.setTextColor(WHITE);
        M5Dial.Display.drawString("Extract-Data", 90, pos + 2);
        // Setting start
        if (preferences.getInt("extractstream", 0) == 0) {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, WHITE);
          M5Dial.Display.fillRoundRect(150, pos, 20, 20, 10, WHITE);
          M5Dial.Display.drawString("OFF", 190, pos + 2);
        } else {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, WHITE);
          M5Dial.Display.fillRoundRect(192, pos, 20, 20, 10, WHITE);
          M5Dial.Display.drawString("ON", 180, pos + 2);
        }
        // Setting end
      }
      pos = pos + 22;
      if ((mode == 1) && (actscreenset == 4)) {
        M5Dial.Display.setTextColor(GREEN);
        M5Dial.Display.drawString("LED-Display", 90, pos + 2);
        // Setting start
        if (preferences.getInt("trafficmod", 0) == 0) {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, GREEN);
          M5Dial.Display.fillRoundRect(150, pos, 20, 20, 10, GREEN);
          M5Dial.Display.drawString("OFF", 190, pos + 2);
        } else {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, GREEN);
          M5Dial.Display.fillRoundRect(192, pos, 20, 20, 10, GREEN);
          M5Dial.Display.drawString("ON", 180, pos + 2);
        }
        // Setting end
      } else {
        M5Dial.Display.setTextColor(WHITE);
        M5Dial.Display.drawString("LED-Display", 90, pos + 2);
        // Setting start
        if (preferences.getInt("trafficmod", 0) == 0) {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, WHITE);
          M5Dial.Display.fillRoundRect(150, pos, 20, 20, 10, WHITE);
          M5Dial.Display.drawString("OFF", 190, pos + 2);
        } else {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, WHITE);
          M5Dial.Display.fillRoundRect(192, pos, 20, 20, 10, WHITE);
          M5Dial.Display.drawString("ON", 180, pos + 2);
        }
        // Setting end
      }
      pos = pos + 22;
      if ((mode == 1) && (actscreenset == 5)) {
        M5Dial.Display.setTextColor(GREEN);
        M5Dial.Display.drawString("Baudrate", 75, pos);
        // Setting start
        M5Dial.Display.drawRoundRect(135, pos, 80, 20, 10, GREEN);
        M5Dial.Display.drawString(String(preferences.getInt("baudrate", 0)), 175, pos + 2);
        // Setting end
      } else {
        M5Dial.Display.setTextColor(WHITE);
        M5Dial.Display.drawString("Baudrate", 75, pos);
        // Setting start
        M5Dial.Display.drawRoundRect(135, pos, 80, 20, 10, WHITE);
        M5Dial.Display.drawString(String(preferences.getInt("baudrate", 0)), 175, pos + 2);
        // Setting end
      }
    }
  }
  // Screen GDL90-Settings
  if (actscreen == 11) {
    if ((mode == 1) && (refrshbuffs == 0)) {
      // Nothing to update
    } else {
      refrshbuffs = 0;
      if (mode == 0) {
        M5Dial.Display.clear();
      }
      M5Dial.Display.setTextColor(GREEN);
      M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
      M5Dial.Display.setTextSize(1);
      M5Dial.Display.drawString("GDL90/UDP", 120, 25);
      M5Dial.Display.setTextColor(WHITE);
      M5Dial.Display.setTextFont(&fonts::FreeSansBold9pt7b);
      M5Dial.Display.fillRect(0, 70, 240, 240, BLACK);  // Clear Background to update

      int pos = 67;

      if ((mode == 1) && (actscreengdl == 1)) {
        M5Dial.Display.setTextColor(GREEN);
        M5Dial.Display.drawString("Ownship", 90, pos + 2);
        if (preferences.getInt("sendudpfromnmea", 0) == 0) {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, GREEN);
          M5Dial.Display.fillRoundRect(150, pos, 20, 20, 10, GREEN);
          M5Dial.Display.drawString("OFF", 190, pos + 2);
        } else {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, GREEN);
          M5Dial.Display.fillRoundRect(192, pos, 20, 20, 10, GREEN);
          M5Dial.Display.drawString("ON", 180, pos + 2);
        }
      } else {
        M5Dial.Display.setTextColor(WHITE);
        M5Dial.Display.drawString("Ownship", 90, pos + 2);
        if (preferences.getInt("sendudpfromnmea", 0) == 0) {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, WHITE);
          M5Dial.Display.fillRoundRect(150, pos, 20, 20, 10, WHITE);
          M5Dial.Display.drawString("OFF", 190, pos + 2);
        } else {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, WHITE);
          M5Dial.Display.fillRoundRect(192, pos, 20, 20, 10, WHITE);
          M5Dial.Display.drawString("ON", 180, pos + 2);
        }
      }




      pos = pos + 22;

      if ((mode == 1) && (actscreengdl == 2)) {
        M5Dial.Display.setTextColor(GREEN);
        M5Dial.Display.drawString("Traffic", 90, pos + 2);

        if (preferences.getInt("sndudptffromnm", 0) == 0) {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, GREEN);
          M5Dial.Display.fillRoundRect(150, pos, 20, 20, 10, GREEN);
          M5Dial.Display.drawString("OFF", 190, pos + 2);
        } else {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, GREEN);
          M5Dial.Display.fillRoundRect(192, pos, 20, 20, 10, GREEN);
          M5Dial.Display.drawString("ON", 180, pos + 2);
        }




      } else {
        M5Dial.Display.setTextColor(WHITE);
        M5Dial.Display.drawString("Traffic", 90, pos + 2);

        if (preferences.getInt("sndudptffromnm", 0) == 0) {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, WHITE);
          M5Dial.Display.fillRoundRect(150, pos, 20, 20, 10, WHITE);
          M5Dial.Display.drawString("OFF", 190, pos + 2);
        } else {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, WHITE);
          M5Dial.Display.fillRoundRect(192, pos, 20, 20, 10, WHITE);
          M5Dial.Display.drawString("ON", 180, pos + 2);
        }
      }


      pos = pos + 22;
      if ((mode == 1) && (actscreengdl == 3)) {
        M5Dial.Display.setTextColor(GREEN);
        M5Dial.Display.drawString("Heartbeat", 90, pos + 2);

        if (preferences.getInt("sendudphbstx", 0) == 0) {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, GREEN);
          M5Dial.Display.fillRoundRect(150, pos, 20, 20, 10, GREEN);
          M5Dial.Display.drawString("OFF", 190, pos + 2);
        } else {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, GREEN);
          M5Dial.Display.fillRoundRect(192, pos, 20, 20, 10, GREEN);
          M5Dial.Display.drawString("ON", 180, pos + 2);
        }


      } else {
        M5Dial.Display.setTextColor(WHITE);
        M5Dial.Display.drawString("Heartbeat", 90, pos + 2);

        if (preferences.getInt("sendudphbstx", 0) == 0) {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, WHITE);
          M5Dial.Display.fillRoundRect(150, pos, 20, 20, 10, WHITE);
          M5Dial.Display.drawString("OFF", 190, pos + 2);
        } else {
          M5Dial.Display.drawRoundRect(150, pos, 65, 20, 10, WHITE);
          M5Dial.Display.fillRoundRect(192, pos, 20, 20, 10, WHITE);
          M5Dial.Display.drawString("ON", 180, pos + 2);
        }
      }
    }
  }


  if ((actscreen == 12) && (mode == 0)) {
    M5Dial.Display.clear();
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("Reset", 120, 25);
    M5Dial.Display.setTextColor(WHITE);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold9pt7b);
    M5Dial.Display.drawString("Press Button to reboot", 120, 70);
    M5Dial.Display.drawString("Turn to right (About) and", 120, 110);
    M5Dial.Display.drawString("hold 5s to Factory-Reset", 120, 130);
  }

  if ((actscreen == 13) && (mode == 0)) {
    M5Dial.Display.clear();
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("About", 120, 25);
    M5Dial.Display.setTextColor(WHITE);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold9pt7b);

    M5Dial.Display.drawString("HW:" + hwtype, 120, 70);

    M5Dial.Display.drawString("SW:" + String(versionfull) + String(versionextend), 120, 90);

    M5Dial.Display.drawString("www.gs-avionik.de", 110, 130);
  }

  if (actscreen == 5) {
    int clear = 0;
    if (mode == 0) {
      M5Dial.Display.clear();
    }
    if ((int(nmea2gdl90groundspeed) > screengroundspeed + 1) || (int(nmea2gdl90groundspeed) < screengroundspeed - 1)) {
      clear = 1;
      screengroundspeed = int(nmea2gdl90groundspeed);
    }
    if (clear == 1) {
      M5Dial.Display.fillRect(0, 70, 240, 240, BLACK);  // Clear Background to update
    }
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("Flightdata", 120, 25);
    M5Dial.Display.setTextColor(WHITE);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold24pt7b);
    M5Dial.Display.drawString(String(screengroundspeed) + " kts", 120, 95);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
    M5Dial.Display.drawString(String(int(screengroundspeed * 1.852)) + " km/h", 120, 150);
    M5Dial.Display.drawString("Groundspeed", 120, 180);
  }

  if (actscreen == 6) {
    int clear = 0;
    if (mode == 0) {
      M5Dial.Display.clear();
    }
    if ((int(nmea2gdl90altitide) > screenaltitide + 1) || (int(nmea2gdl90altitide) < screenaltitide - 1)) {
      clear = 1;
      screenaltitide = int(nmea2gdl90altitide);
    }
    if (clear == 1) {
      M5Dial.Display.fillRect(0, 70, 240, 240, BLACK);  // Clear Background to update
    }
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("Flightdata", 120, 25);
    M5Dial.Display.setTextColor(WHITE);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold24pt7b);
    M5Dial.Display.drawString(String(screenaltitide) + " ft", 120, 95);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
    M5Dial.Display.setTextColor(WHITE);
    M5Dial.Display.drawString("Altitude", 120, 180);
  }

  if (actscreen == 7) {
    int clear = 0;
    if (mode == 0) {
      M5Dial.Display.clear();
    }
    if ((int(nmea2gdl9track) > screentrack + 1) || (int(nmea2gdl9track) < screentrack - 1)) {
      clear = 1;
      screentrack = int(nmea2gdl9track);
    }
    if (clear == 1) {
      M5Dial.Display.fillRect(0, 70, 240, 240, BLACK);  // Clear Background to update
    }
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString("Flightdata", 120, 25);
    M5Dial.Display.setTextColor(WHITE);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold24pt7b);
    M5Dial.Display.drawString(String(screentrack) + " deg", 120, 95);
    M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
    M5Dial.Display.setTextColor(WHITE);
    M5Dial.Display.drawString("GNSS-Heading", 120, 180);
  }
}
void setup() {

  preferences.begin("GS-Traffic", false);

  // Checking for ESP32-S3-Model to ident GS-Traffic2WIFIv3-compatible, otherwise check for HWTYPEPINS (v1/Selfmade oder v2)
  if (ESP.getChipModel() == "ESP32-S3") {
    hwtype = "GS-Traffic2WIFIv3";
    hwtypeid = 30;
  } else {
    // Checking HWTYPEPINS by Hardware, only if 1 and 2 are DOWN (GND), HW-Version is 2.0
    pinMode(HWTYPEPIN, INPUT);
    pinMode(HWTYPEPIN, INPUT_PULLUP);
    pinMode(HWTYPEPIN2, INPUT);
    pinMode(HWTYPEPIN2, INPUT_PULLUP);
    if (digitalRead(HWTYPEPIN) == 0) {
      if (digitalRead(HWTYPEPIN2) == 0) {
        hwtype = "GS-Traffic2WIFIv2";
        hwtypeid = 10;
      }
    }

    // Resetting to Defaults, if no SSID is found (ESP32 seems to be blank)
    if (preferences.getString("ssid", "") == "") {
      defaultsettings();
    }

    // Checking Resetpin by Hardware, if DOWN (GND), Reset to defaults
    pinMode(RESETPIN, INPUT);
    pinMode(RESETPIN, INPUT_PULLUP);
    delay(50);
    if (digitalRead(RESETPIN) == 0) {
      Serial.println("Resetting");
      digitalWrite(LEDPINGREEN, HIGH);
      digitalWrite(LEDPINRED, HIGH);
      delay(1700);
      defaultsettings();
    }

    // Boot-Seq shows firmware-version green/red directly to HW without WebUI and variable statusled because of time of execution
    pinMode(LEDPINGREEN, OUTPUT);
    pinMode(LEDPINRED, OUTPUT);
    digitalWrite(LEDPINGREEN, LOW);
    digitalWrite(LEDPINRED, LOW);

    for (int i = 0; i < versionmajor; i++) {
      digitalWrite(LEDPINGREEN, HIGH);
      delay(400);
      digitalWrite(LEDPINGREEN, LOW);
      delay(400);
    }
    for (int i = 0; i < versionminor; i++) {
      digitalWrite(LEDPINRED, HIGH);
      delay(400);
      digitalWrite(LEDPINRED, LOW);
      delay(400);
    }
  }

  // Starting tmleds, if Trafficmod is enabled and doing boot-animation
  if (preferences.getInt("trafficmod", 0) == 1) {
    tmleds.begin();
    tmleds.show();
    tmshowleds("", 2, 0);  // Show Firmware on circle
    tmshowleds("", 1, 0);  // Boot-Animation on circle
  }

  // Checking value of countrycode. If they are 0 due to update from previous versions, set them to DE (Germany)
  if (preferences.getString("countrycode", "") == "") {
    preferences.putString("countrycode", "DE");
  }

  // Checking values of posresetcycles and twresetcycles. If they are 0 due to update from previous versions, set them to 50
  if (preferences.getInt("posresetcycles", 0) == 0) {
    preferences.putInt("posresetcycles", 50);
  }
  if (preferences.getInt("twresetcycles", 0) == 0) {
    preferences.putInt("twresetcycles", 50);
  }

  // Starting serial based on configuration
  Serial.begin(preferences.getInt("baudrate", 0));
  String tmpssid;
  String tmpwpakey;
  tmpssid = preferences.getString("ssid", "");
  tmpwpakey = preferences.getString("wpakey", "");
  tmpssid.toCharArray(ssid, 50);
  tmpwpakey.toCharArray(wpakey, 50);
  delay(500);

  // Hardware-Serial-COM-Port based on HW-Type
  if (hwtype == "GS-Traffic2WIFIv3") {
    Serial2.begin(preferences.getInt("baudrate", 0), SERIAL_PARAM0, SERIAL0_RXPINv3, SERIAL0_TXPINv3);  // Changed to variable Baudrate
  } else {
    if (hwtype == "GS-Traffic2WIFIv2") {
      Serial2.begin(preferences.getInt("baudrate", 0), SERIAL_PARAM0, SERIAL0_RXPINv2, SERIAL0_TXPINv2);  // Changed to variable Baudrate
    } else {
      Serial2.begin(preferences.getInt("baudrate", 0), SERIAL_PARAM0, SERIAL0_RXPIN, SERIAL0_TXPIN);  // Changed to variable Baudrate
    }
  }

  // No Default-GW-Routing, if disabled (default) and Clientmode is off
  if ((preferences.getInt("defgw", 0) == 0) && (preferences.getInt("clientmode", 0) == 0)) {
    uint8_t val = 0;
    tcpip_adapter_dhcps_option(TCPIP_ADAPTER_OP_SET, TCPIP_ADAPTER_ROUTER_SOLICITATION_ADDRESS, &val, sizeof(dhcps_offer_t));
  }


  if (preferences.getInt("extractstream", 0) == 1) {
    extractstream = 1;
  }

  if (preferences.getInt("debug", 0) != 0) {
    debug = preferences.getInt("debug", 0);
  }

  if (preferences.getInt("trafficwarnled", 0) == 1) {
    trafficwarnled = preferences.getInt("trafficwarnled", 0);
  }

  if (preferences.getInt("clientmode", 0) == 0) {
    // Starting Wifi as AP-Mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, wpakey);
    delay(200);
    WiFi.softAPConfig(ip, ip, netmask);
    Serial.println("Wifi started as AP");
    Serial.print(ssid);
    Serial.print("@");
    Serial.print(wpakey);
    server[0]->begin(preferences.getInt("tcpport", 0));  //Added variable port instead of initalizing outside setup()
    server[0]->setNoDelay(true);
    server[1]->begin(666);  // Starting with administrative Port for debugging
    server[1]->setNoDelay(true);
  } else {
    // Starting Wifi as Client
    WiFi.begin(ssid, wpakey);
    Serial.println("Wifi started as Client");
    Serial.print(ssid);
    Serial.print("@");
    Serial.print(wpakey);
    Serial.print(": connecting");
    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
  // Reducing Wifi-Power if enabled (not default)
  if (preferences.getInt("redpw", 0) == 1) {
    esp_err_t esp_wifi_set_max_tx_power(50);
  }

  // Setting default WIFI-Country to Germnay
  esp_err_t esp_wifi_set_country(const wifi_country_t *DE);

  // Starting UDP for GDL90
  udp.begin(preferences.getInt("udpport", 0));

  // Create Core1-Task for Bridging
  xTaskCreatePinnedToCore(core1bridging, "core1taskbridging", 10000, NULL, 1, &core1taskbridging, 1);

  // Create Core1-Task for Faking NMEA
  xTaskCreatePinnedToCore(core1faking, "core1taskfaking", 10000, NULL, 1, &core1taskfaking, 1);

  // Create Core0-Task for Webserver
  xTaskCreatePinnedToCore(core0webserver, "core0taskwebserver", 10000, NULL, 2, &core0taskwebserver, 0);

  // Create Core0-Task for v3-Hardware-Features
  xTaskCreatePinnedToCore(core0v3, "core0taskv3", 10000, NULL, 2, &core0taskv3, 0);

  // Create Core1-Task for GDL90 Sender
  xTaskCreatePinnedToCore(core0gdl90sender, "core0taskgdl90sender", 10000, NULL, 2, &core0taskgdl90sender, 1);

  debuglog("Tasks created", 1);
}



//Core 0 Code for v3-Hardware-Features (Display, Button, Rotary-Encoder etc)
void core0v3(void *pvParameters) {
  if (hwtype == "GS-Traffic2WIFIv3") {
    // Initalize v3-Features
    auto cfg = M5.config();
    M5Dial.begin(cfg, true, false);
    M5.Speaker.setVolume(11);
    showscreen(100, 0);
    M5Dial.Display.setBrightness(preferences.getInt("screenbright", 0));
  }
  for (;;)
  //Loop on Core 0 for v3 Hardware-Features
  {
    // Checking Rotary Encoder on v3 Hardware to change screens
    if (hwtype == "GS-Traffic2WIFIv3") {
      M5Dial.update();
      long newPosition = M5Dial.Encoder.read();
      if (newPosition != rotoldpos) {
        refrshbuffs = 1;
        if (newPosition > rotoldpos) {
          int fallback = 1;
          rotoldpos = newPosition;  // Save new position
          // Changed position to higher

          // Start 8
          if ((actscreen == 8) && (actscreenbright > 0)) {
            fallback = 0;
            if (actscreenbright == 1) {
              if (preferences.getInt("screenbright", 0) < 245) {
                preferences.putInt("screenbright", preferences.getInt("screenbright", 0) + 10);
                M5Dial.Display.setBrightness(preferences.getInt("screenbright", 0));
              } else {
                preferences.putInt("screenbright", 255);
                M5Dial.Display.setBrightness(preferences.getInt("screenbright", 0));
              }
            } else {
              if (preferences.getInt("tmbright", 0) < 245) {
                preferences.putInt("tmbright", preferences.getInt("tmbright", 0) + 10);
                tmshowleds("", 10, 0);
              } else {
                preferences.putInt("tmbright", 255);
                tmshowleds("", 10, 0);
              }
            }
          }
          // End 8

          // Start 11
          if ((actscreen == 11) && (actscreengdl > 0)) {
            fallback = 0;
            if (actscreengdl == 1) {
              if (preferences.getInt("sendudpfromnmea", 0) == 0) {
                preferences.putInt("sendudpfromnmea", 1);
              } else {
                preferences.putInt("sendudpfromnmea", 0);
              }
            }
            if (actscreengdl == 2) {
              if (preferences.getInt("sndudptffromnm", 0) == 0) {
                preferences.putInt("sndudptffromnm", 1);
              } else {
                preferences.putInt("sndudptffromnm", 0);
              }
            }
            if (actscreengdl == 3) {
              if (preferences.getInt("sendudphbstx", 0) == 0) {
                preferences.putInt("sendudphbstx", 1);
              } else {
                preferences.putInt("sendudphbstx", 0);
              }
            }
          }
          // End 11

          // Start 10
          if ((actscreen == 10) && (actscreenset > 0)) {
            fallback = 0;
            if (actscreenset == 1) {
              if (preferences.getInt("defgw", 0) == 0) {
                preferences.putInt("defgw", 1);
              } else {
                preferences.putInt("defgw", 0);
              }
            }
            if (actscreenset == 2) {
              if (preferences.getInt("redpw", 0) == 0) {
                preferences.putInt("redpw", 1);
              } else {
                preferences.putInt("redpw", 0);
              }
            }
            if (actscreenset == 3) {
              if (preferences.getInt("extractstream", 0) == 0) {
                preferences.putInt("extractstream", 1);
              } else {
                preferences.putInt("extractstream", 0);
              }
            }
            if (actscreenset == 4) {
              if (preferences.getInt("trafficmod", 0) == 0) {
                preferences.putInt("trafficmod", 1);
              } else {
                preferences.putInt("trafficmod", 0);
              }
            }
            if (actscreenset == 5) {
              int posbr;
              for (int i = 0; i < 8; i++) {
                if (preferences.getInt("baudrate", 0) == baudrates[i]) {
                  posbr = i;
                  break;
                }
              }
              if (posbr < 7) {
                preferences.putInt("baudrate", baudrates[posbr + 1]);
              }
            }
          }
          // End 10


          if (fallback == 1) {
            showscreen(1, 0);
          }
        } else {
          int fallback = 1;
          rotoldpos = newPosition;  // Save new position
                                    // Changed position to lower

          // Start 8
          if ((actscreen == 8) && (actscreenbright > 0)) {
            fallback = 0;
            if (actscreenbright == 1) {
              if (preferences.getInt("screenbright", 0) > 80) {
                preferences.putInt("screenbright", preferences.getInt("screenbright", 0) - 10);
                M5Dial.Display.setBrightness(preferences.getInt("screenbright", 0));
              } else {
                preferences.putInt("screenbright", 80);
                M5Dial.Display.setBrightness(preferences.getInt("screenbright", 0));
              }
            } else {
              if (preferences.getInt("tmbright", 0) > 40) {
                preferences.putInt("tmbright", preferences.getInt("tmbright", 0) - 10);
                tmshowleds("", 10, 0);
              } else {
                preferences.putInt("tmbright", 30);
                tmshowleds("", 10, 0);
              }
            }
          }
          // End 8


          // Start 11
          if ((actscreen == 11) && (actscreengdl > 0)) {
            fallback = 0;
            if (actscreengdl == 1) {
              if (preferences.getInt("sendudpfromnmea", 0) == 0) {
                preferences.putInt("sendudpfromnmea", 1);
              } else {
                preferences.putInt("sendudpfromnmea", 0);
              }
            }
            if (actscreengdl == 2) {
              if (preferences.getInt("sndudptffromnm", 0) == 0) {
                preferences.putInt("sndudptffromnm", 1);
              } else {
                preferences.putInt("sndudptffromnm", 0);
              }
            }
            if (actscreengdl == 3) {
              if (preferences.getInt("sendudphbstx", 0) == 0) {
                preferences.putInt("sendudphbstx", 1);
              } else {
                preferences.putInt("sendudphbstx", 0);
              }
            }
          }
          // End 11
          // Start 10
          if ((actscreen == 10) && (actscreenset > 0)) {
            fallback = 0;
            if (actscreenset == 1) {
              if (preferences.getInt("defgw", 0) == 0) {
                preferences.putInt("defgw", 1);
              } else {
                preferences.putInt("defgw", 0);
              }
            }
            if (actscreenset == 2) {
              if (preferences.getInt("redpw", 0) == 0) {
                preferences.putInt("redpw", 1);
              } else {
                preferences.putInt("redpw", 0);
              }
            }
            if (actscreenset == 3) {
              if (preferences.getInt("extractstream", 0) == 0) {
                preferences.putInt("extractstream", 1);
              } else {
                preferences.putInt("extractstream", 0);
              }
            }
            if (actscreenset == 4) {
              if (preferences.getInt("trafficmod", 0) == 0) {
                preferences.putInt("trafficmod", 1);
              } else {
                preferences.putInt("trafficmod", 0);
              }
            }
            if (actscreenset == 5) {
              int posbr;
              for (int i = 0; i < 8; i++) {
                if (preferences.getInt("baudrate", 0) == baudrates[i]) {
                  posbr = i;
                  break;
                }
              }
              if (posbr > 0) {
                preferences.putInt("baudrate", baudrates[posbr - 1]);
              }
            }
          }
          // End 10


          if (fallback == 1) {
            showscreen(0, 0);
          }
        }
        // rotoldpos = newPosition; // Save new position
      }
      if (M5Dial.BtnA.pressedFor(4000)) {
        if (actscreen == 13) {
          M5Dial.Display.clear();
          M5Dial.Display.fillScreen(RED);
          M5Dial.Display.setTextColor(GREEN);
          M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
          M5Dial.Display.setTextSize(1);
          M5Dial.Display.drawString("Factory-Reset", 120, 120);
          delay(1000);
          defaultsettings();
        }
      } else {
        if (M5Dial.BtnA.wasPressed()) {
          refrshbuffs = 1;
          if (actscreen == 12) {
            M5Dial.Display.clear();
            M5Dial.Display.fillScreen(GREEN);
            M5Dial.Display.setTextColor(RED);
            M5Dial.Display.setTextFont(&fonts::FreeSansBold12pt7b);
            M5Dial.Display.setTextSize(1);
            M5Dial.Display.drawString("Resetting", 120, 120);
            delay(1000);
            ESP.restart();
          }

          // Check for Brightness-Configuration (Screen 8) and change to color of the settings
          if (actscreen == 8) {
            if (actscreenbright == 2) {
              tmshowleds("", 11, 0);  // Turn off LEDs on TMOD
              actscreenbright = 0;
            } else {
              if (actscreenbright == 0) {
                actscreenbright = 1;
              } else {
                if (actscreenbright == 1) {
                  tmshowleds("", 10, 0);  // Turn on LEDs on TMOD
                  actscreenbright = 2;
                }
              }
            }
            showscreen(8, 1);
          }


          // Check for GDL-Configuration (Screen 11) and change settings
          if (actscreen == 11) {
            if (actscreengdl == 3) {
              actscreengdl = 0;
            } else {
              actscreengdl = actscreengdl + 1;
            }
            showscreen(11, 1);
          }

          // Check for Settings (Screen 10) and change settings
          if (actscreen == 10) {
            if (actscreenset == 5) {
              actscreenset = 0;
            } else {
              actscreenset = actscreenset + 1;
            }
            showscreen(10, 1);
          }
        }
      }
    }
    delay(20);

    // Refresh data
    showscreen(0, 1);
  }
}

//Core 0 Code for Webserver
void core0webserver(void *pvParameters) {
  // Setup on Core0 for Webserver
  // Configuring Webserver
  webserver.on("/", []() {
    webserver.send(200, "text/html", htmlindex);
  });
  webserver.on("/mini-default.min.css", []() {
    webserver.send(200, "text/css", htmlcss);
  });
  webserver.on(F("/gs.json"), HTTP_GET, sendjson);
  webserver.on(F("/restart"), HTTP_POST, restart);
  webserver.on(F("/resetsettings"), HTTP_POST, defaultsettings);
  webserver.on(F("/savesettings"), HTTP_POST, applysettings);
  webserver.on(F("/traffic.json"), HTTP_GET, sendtraffic);
  webserver.on("/jquery.min.js", HTTP_GET, onJavaScript);
  webserver.on(F("/tmdemo"), HTTP_GET, showdemo);

  webserver.on(
    "/update", HTTP_POST, []() {
      Serial.println("Webserver.on(/update)");
      webserver.sendHeader("Connection", "close");
      webserver.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    },
    []() {
      HTTPUpload &upload = webserver.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {  //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
      }
    });

  // Starting Webserver
  webserver.begin(80);
  for (;;)
  //Loop on Core 0
  {
    // Handling Webserver
    webserver.handleClient();
    delay(100);
  }
}


//Core 0 Code for GDL90Sender
void core0gdl90sender(void *pvParameters) {
  for (;;)
  //Loop on Core 0
  {
    // Sending GLD90
    if (preferences.getInt("sendudpfromnmea", 0) == 1) {
      sendgdl90fromnmea(0);
    }
    delay(300);
  }
}




//Core 1 Code for faking NMEA
void core1faking(void *pvParameters) {
  // Initalize CRC-Table for GDL90
  crcInit();
  for (;;)
  //Loop on Core 1
  {
    delay(500);
    if (preferences.getInt("clientmode", 0) == 0) {
      // Checking if Fake-Mode is activated for faking
      if (preferences.getInt("fakenmea", 0) == 1 || preferences.getInt("fakenmeaserial", 0) == 1) {
        // Fakecode for NMEA via Wifi
        if (preferences.getInt("fakenmea", 0) == 1) {
          fakenmea(0);
        }
        // Fakecode for NMEA via Serial
        if (preferences.getInt("fakenmeaserial", 0) == 1) {
          fakenmea(1);
        }
      }
    }
  }
}


//Core 1 Code for bridging
void core1bridging(void *pvParameters) {
  // Initalize CRC-Table for GDL90
  crcInit();
  for (;;)
  //Loop on Core 1
  {

    // Nodata-Delay for whole Loop, will be set to 0 if Data is received
    delay(nodatadelay);

    // Full code of AP-Mode
    if (preferences.getInt("clientmode", 0) == 0) {
      // Handle TCP-Server
      if (server[0]->hasClient()) {
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

      // Handle TCP-Server for debugging
      if (server[1]->hasClient()) {
        for (byte i = 0; i < MAX_NMEA_CLIENTS; i++) {
          if (!TCPClient[1][i] || !TCPClient[1][i].connected()) {
            if (TCPClient[1][i]) TCPClient[1][i].stop();
            TCPClient[1][i] = server[1]->available();
            continue;
          }
        }
        WiFiClient TmpserverClient = server[1]->available();
        TmpserverClient.stop();
      }


      // Checking if Fake-Mode is activated (deactivating serial-wifi-bridging)
      if (preferences.getInt("fakenmea", 0) == 0 && preferences.getInt("fakenmeaserial", 0) == 0) {
        // No simulation, doing real serial-wifi-bridging
        debuglog("No simulation, doing real serial-wifi-bridging", 1);


        if (Serial2 != NULL) {
          debuglog("Serial2 is not null", 1);

          if (Serial2.available()) {
            debuglog("Bridging: COM0 available", 1);
            while (Serial2.available()) {
              buf2[0][i2[0]] = Serial2.read();
              if (i2[0] < bufferSize - 1) i2[0]++;
              nodatadelay = 0;  //Data is received, no delay anymore
            }


            // Debug received Data to serial
            if (debug > 1) {
              Serial.println("Full received data:");
              Serial.write(buf2[0], i2[0]);
            }

            // Debug to TCP
            if (preferences.getInt("debugtcp", 0) > 0) {
              for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
                if (TCPClient[1][cln])
                  TCPClient[1][cln].println("Full received data: ");
                TCPClient[1][cln].write(buf2[0], i2[0]);
              }
            }


            if (extractstream == 1)  // Extract data vom Stream if option is turned on
            {
              // Put received Data to String for interpreting
              int serialcount = 0;
              while (serialcount < i2[0]) {
                // Check for Carriage return, If yes=Break string and push to interpreter
                if (buf2[0][serialcount] == 10) {
                  if (debug > 1) {
                    Serial.print("Separated message:");
                    Serial.println(inserial);
                  }
                  // Debug to TCP
                  if (preferences.getInt("debugtcp", 0) > 0) {
                    for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
                      if (TCPClient[1][cln])
                        TCPClient[1][cln].println("Separated message:");
                      TCPClient[1][cln].println(inserial);
                    }
                  }
                  extractdata(inserial);  // Push data to interpreter
                  inserial = "";          // New empty string after new line
                } else {
                  inserial += (char)buf2[0][serialcount];  // Add data to String, if not CR
                }
                serialcount++;
              }
            }

            for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
              if (TCPClient[0][cln])
                TCPClient[0][cln].write(buf2[0], i2[0]);
            }
            i2[0] = 0;
          }
        }
      }
    } else {

      // Code for Clientmode
      const uint16_t port = preferences.getInt("tcpport", 0);  // Use the configurable TCP-Port in Clientmode, too
      const char *host = "192.168.1.1";
      WiFiClient client;
      if (!client.connect(host, port)) {
        Serial.println("Connection failed.");
        return;
      }
      int maxloops = 0;
      while (!client.available() && maxloops < 1000) {
        maxloops++;
      }
      if (client.available() > 0) {
        if (debug > 1) {
          Serial.println(client.readString());
        }
        if (preferences.getInt("debugtcp", 0) > 0) {
          for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
            if (TCPClient[1][cln])
              TCPClient[1][cln].println(client.readString());
          }
        }
        Serial2.print(client.readString());
      }
      client.stop();
    }
  }
}

void loop()  // Main loop on Core 1
{
  // Warning position and/or traffic based on configuration
  if (trafficwarning > 0) {
    statusled = 2;
  } else {
    if (positionwarning > 0) {
      statusled = 3;
    } else {
      statusled = 0;
    }
  }

  if (statusled == 0) {
    digitalWrite(LEDPINGREEN, LOW);
    digitalWrite(LEDPINRED, LOW);
  }

  if (statusled == 1) {
    digitalWrite(LEDPINGREEN, HIGH);
    digitalWrite(LEDPINRED, LOW);
  }

  if (statusled == 2) {
    digitalWrite(LEDPINGREEN, LOW);
    digitalWrite(LEDPINRED, HIGH);
  }

  if (statusled == 3) {
    digitalWrite(LEDPINGREEN, HIGH);
    digitalWrite(LEDPINRED, HIGH);
  }
  //delay(300);
}
