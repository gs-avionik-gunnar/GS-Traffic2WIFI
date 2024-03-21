float GpsToDecimalDegrees(char* nmeaPos, char* quadrant)
{
  float v= 0;
  if(strlen(nmeaPos)>5)
  {
    char integerPart[3+1];
    int digitCount= (nmeaPos[4]=='.' ? 2 : 3);
    memcpy(integerPart, nmeaPos, digitCount);
    integerPart[digitCount]= 0;
    nmeaPos+= digitCount;
    v= atoi(integerPart) + atof(nmeaPos)/60.;
   // if(quadrant=='W' || quadrant=='S')
    if(strcmp(quadrant,"W")==0 || strcmp(quadrant,"S")==0)
      v= -v;
  }
  return v;
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
  uint16_t crc = 0; //Modified to uint16_c because of default 32bit integers on esp32
  for (i = 0; i < length; i++)
  {
    crc = Crc16Table[crc >> 8] ^ (crc << 8) ^ block[i];
  }
  return crc;
}

// Generating Lat/Long
void makelatlng (float lat, float lon)
{
  int wk;
  lat = lat / LON_LAT_RESOLUTION;
  wk = int(lat);
  gdl90lat[0] = byte((wk & 0xFF0000) >> 16);
  gdl90lat[1] = byte((wk & 0x00FF00) >> 8);
  gdl90lat[2] = byte((wk & 0x0000FF));

  lon = lon / LON_LAT_RESOLUTION;
  wk = int(lon);
  gdl90lon[0] = byte((wk & 0xFF0000) >> 16);
  gdl90lon[1] = byte((wk & 0x00FF00) >> 8);
  gdl90lon[2] = byte((wk & 0x0000FF));

}

void gdl90sndmsgheartbeat(bool gps, int secondssincemidnightutc)
{
  byte(crc1);
  byte(crc2);
  byte mask = 0x7E;
  byte gpsmsg;
  if (gps == 1)
  {
    gpsmsg = 0x81;
  }
  else
  {
    gpsmsg = 0x01;
  }
  unsigned char payload[] = {0x00, gpsmsg, byte(((secondssincemidnightutc >> 16) << 7) | 0x1), byte((secondssincemidnightutc & 0xFF)), byte((secondssincemidnightutc & 0xFFFF) >> 8), 0x08, 0x02}; // Datenpayload
  crc1 = (crcCompute(payload, 7) & 0xFF);
  crc2 = ((crcCompute(payload, 7) >> 8) & 0xFF);

  byte commandheartbeat[11] = {mask, 0x00, gpsmsg, byte(((secondssincemidnightutc >> 16) << 7) | 0x1), byte((secondssincemidnightutc & 0xFF)), byte((secondssincemidnightutc & 0xFFFF) >> 8), 0x08, 0x02, crc1, crc2, mask};
  udp.beginPacket(udpAddress, preferences.getInt("udpport", 0));
  udp.write(commandheartbeat, 11);
  udp.endPacket();

}

void gdl90sndmsgpos(int mode, float lat, float lon, int altitude, byte groundspeed, float track,  char* identifier, int idcode) // Ownship and Traffic-Report message
{
  byte(crc1);
  byte(crc2);
  byte mask = 0x7E;
  byte ownid1 = 0xF0;
  byte ownid2 = 0x00;
  byte ownid3 = 0x00;
  byte ownid4 = 0x00;
  byte ownid5 = 0x00;
  byte ownid6 = 0x00;
  byte ownid7 = 0x00;
  byte ownid8 = 0x00;
  byte ownid9 = 0x00;

  byte msgtype = 0x0A;
  if (mode!=0)
  {
    msgtype=0x14; // Trafficreport (not ownship)
  }

  // Calculating lat/lon
  makelatlng(lat, lon);

  // Calculating Altitude
  uint16_t alt;
  if (altitude < -1000 || altitude > 101350)
  {
    alt = 0x0FFF;
  }
  else
  {
    altitude = (altitude + 1000) / 25;
    alt = uint16_t(altitude) & 0xFFF;
  }

  // Set ID-Type to ICAO, if Code is 1
  byte idtype=0x01;
  if (idcode==1)
  {
    byte idtype=0x00;
  }

   // Generating Tailnumber (ICAO-ID) of the plane

   for (int i = 0; i < strlen(identifier) && i < 8; i++) {
    unsigned char c =identifier[i];
    if (c != 20 && !((c >= 48) && (c <= 57)) && !((c >= 65) && (c <= 90)) && c != 'e' && c != 'u' && c != 'a' && c != 'r' && c != 't')
    {
        c = 20;
    }
            if (i==0)
            {
            ownid4=c;
            }
            if (i==1)
            {
            ownid5=c;
            }
            if (i==2)
            {
            ownid6=c;
            }
            if (i==3)
            {
              ownid7=c;
            }
            if (i==4)
            {
              ownid8=c;
            }
             if (i==5)
            {
              ownid9=c;
            }         
     
   }



  int16_t verticalVelocity = 0x800; // ft/min. 64 ft/min resolution.

  // Calculating track
  uint8_t trackb = track / TRACK_RESOLUTION;
  unsigned char payloadpos[] = {msgtype, idtype, ownid1, ownid2, ownid3, gdl90lat[0], gdl90lat[1], gdl90lat[2], gdl90lon[0], gdl90lon[1], gdl90lon[2],  byte((alt & 0xFF0) >> 4), byte((alt & 0x00F) << 4) | 0x09, 0xA9, byte((groundspeed & 0xFF0) >> 4), byte((groundspeed & 0x00F) << 4) | byte((verticalVelocity & 0x0F00) >> 8), byte(verticalVelocity & 0xFF), trackb, 0x01, ownid4, ownid5, ownid6, ownid7, ownid8, ownid9, 0x00, 0x00, 0x00}; // Datenpayload

  crc1 = (crcCompute(payloadpos, 28) & 0xFF);
  crc2 = ((crcCompute(payloadpos, 28) >> 8) & 0xFF);

  byte commandposition[32] = {mask, msgtype, idtype, ownid1, ownid2, ownid3, gdl90lat[0], gdl90lat[1], gdl90lat[2], gdl90lon[0], gdl90lon[1], gdl90lon[2], byte((alt & 0xFF0) >> 4), byte((alt & 0x00F) << 4) | 0x09, 0xA9, byte((groundspeed & 0xFF0) >> 4), byte((groundspeed & 0x00F) << 4) | byte((verticalVelocity & 0x0F00) >> 8), byte(verticalVelocity & 0xFF), trackb, 0x01, ownid4, ownid5, ownid6, ownid7, ownid8, ownid9, 0x00, 0x00, 0x00, crc1, crc2, mask};
  udp.beginPacket(udpAddress, preferences.getInt("udpport", 0));
  udp.write(commandposition, 32);
  udp.endPacket();
}


void gdl90sndgeoa(int altitude) // Geodetic altitude
{
  byte(crc1);
  byte(crc2);
  byte mask = 0x7E;

  // Calculating Altitude
  uint16_t alt;
  if (altitude < -1000 || altitude > 101350)
  {
    alt = 0x0FFF;
  } else {
    alt = altitude / 5;
  }

  unsigned char payload[] = {0x0B, byte(alt >> 8), byte(alt & 0x00FF), 0x00, 0x0A}; // Datenpayload
  
  crc1 = (crcCompute(payload, 5) & 0xFF);
  crc2 = ((crcCompute(payload, 5) >> 8) & 0xFF);

  byte command[9] = {mask, 0x0B, byte(alt >> 8), byte(alt & 0x00FF), 0x00, 0x0A, crc1, crc2, mask};

  udp.beginPacket(udpAddress, preferences.getInt("udpport", 0));
  udp.write(command, 9);
  udp.endPacket();
}

void gdl90stxhb() // Stratux Heartbeat
{
  byte(crc1);
  byte(crc2);
  byte mask = 0x7E;

  unsigned char payload[] = {0xCC, 0x02}; // Datapayload
  
  crc1 = (crcCompute(payload, 2) & 0xFF);
  crc2 = ((crcCompute(payload, 2) >> 8) & 0xFF);

  byte command[6] = {mask, 0xCC, 0x02, crc1, crc2, mask};

  udp.beginPacket(udpAddress, preferences.getInt("udpport", 0));
  udp.write(command, 6);
  udp.endPacket();
}


float trafficlatlon(int mode, float lat, float lon, float distlat, float distlon)
{
    int meters = 2000;
    float new_lat = lat + distlat*0.000006279;
    float new_lon = lon + distlon*0.000006279 / cos(lat * 0.018);
    if (mode==2)
    {
      return new_lon;
    }
    else
    {
      return new_lat;
    }
}


void sendgdl90fromnmea(int sendmode)
{
    // Sending Heartbeat and ownship-data if available
    if((nmea2gdl90lon!=0)&&(nmea2gdl90lat!=0))
    {  
      gdl90sndmsgheartbeat(1, nmea2gdl90ssmutc); // Sending Heartbeat with data
      if (preferences.getInt("sendudphbstx", 0)==1)
      {
        gdl90stxhb(); // Sending Stratux heartbeat if configured
      }
      gdl90sndmsgpos(0,nmea2gdl90lat, nmea2gdl90lon, round(nmea2gdl90altitide), round(nmea2gdl90groundspeed), round(nmea2gdl9track), "GSTRFC", 2);
      gdl90sndgeoa(round(nmea2gdl90altitide+nmea2gdl90altitidegeodetic));
         // Getting all of the maximum amount of trafficwarnings and sending them via GDL90
         for (int i = 0; i < maxtraffic; ++i ) {
          if (traffic[i][0]=="PFLAA")
          {  
                int id_len = traffic[i][2].length() + 1; 
                char tmptfcid[id_len];
                traffic[i][2].toCharArray(tmptfcid, id_len);
                int tmpfcidtype=traffic[i][2].toInt();
                int tmptfcidtype=traffic[i][3].toInt();
                int tmptfcnorth=traffic[i][5].toFloat();
                int tmptfceast=traffic[i][6].toFloat();  
                int tmptfcalt=traffic[i][7].toInt();        
                int tmptfchead=traffic[i][8].toInt();
                int tmptfcgs=traffic[i][9].toInt();
                float tmptfcclimb=traffic[i][10].toFloat();
                gdl90sndmsgpos(1,trafficlatlon(1,nmea2gdl90lat,nmea2gdl90lon,tmptfcnorth,tmptfceast), trafficlatlon(2,nmea2gdl90lat,nmea2gdl90lon,tmptfcnorth,tmptfceast), round(tmptfcalt*3.281), round(1.94*tmptfcalt), round(tmptfcgs), tmptfcid, tmpfcidtype);   
		      delay(40);
          }
        }
    
     }
    else
    {    
        gdl90sndmsgheartbeat(0, nmea2gdl90ssmutc); // Sending Heartbeat without data
        if (preferences.getInt("sendudphbstx", 0)==1)
        {
          gdl90stxhb(); // Sending Stratux heartbeat if configured
        }
    }







}
