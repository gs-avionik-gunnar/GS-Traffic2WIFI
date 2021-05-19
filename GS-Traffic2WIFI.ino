// Changelog Softwareversion 1.1: Gateway wird per DHCP nicht übertragen

char ssid[] = "GS-Traffic";  // Wird später übeschrieben, Fallback
char pw[] = "12345678";      // Wird später übeschrieben, Fallback

IPAddress ip(192, 168, 1, 1);
IPAddress gateway(192,168,1,1);
IPAddress netmask(255, 255, 255, 0);

#define NUM_COM 1                 
#define UART_BAUD0 19200          
#define SERIAL_PARAM0 SERIAL_8N1    
#define SERIAL0_RXPIN 21            
#define SERIAL0_TXPIN 1             
#define SERIAL0_TCP_PORT 2000
#define bufferSize 1024
#define MAX_NMEA_CLIENTS 4

#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>

uint8_t chipid[6];

HardwareSerial* COM[NUM_COM] = {&Serial};

WiFiServer server_0(SERIAL0_TCP_PORT);
WiFiServer *server[NUM_COM]={&server_0};
WiFiClient TCPClient[NUM_COM][MAX_NMEA_CLIENTS];


uint8_t buf1[NUM_COM][bufferSize];
uint16_t i1[NUM_COM]={0};

uint8_t buf2[NUM_COM][bufferSize];
uint16_t i2[NUM_COM]={0};

uint8_t BTbuf[bufferSize];
uint16_t iBT =0;


void setup() {
  delay(500);  
  esp_efuse_read_mac(chipid);

  sprintf(ssid, "GS-Traffic-%02x%02x", chipid[4], chipid[5]);
  sprintf(pw, "%02x%02x%02x%02x", chipid[2], chipid[3], chipid[4], chipid[5]);
    
  COM[0]->begin(UART_BAUD0, SERIAL_PARAM0, SERIAL0_RXPIN, SERIAL0_TXPIN);

  uint8_t val = 0;
  tcpip_adapter_dhcps_option(TCPIP_ADAPTER_OP_SET, TCPIP_ADAPTER_ROUTER_SOLICITATION_ADDRESS, &val, sizeof(dhcps_offer_t));
  
  WiFi.mode(WIFI_AP); 
  WiFi.softAP(ssid, pw);
  delay(1000);
  WiFi.softAPConfig(ip, ip, netmask); 
  server[0]->begin(); 
  server[0]->setNoDelay(true);
 // esp_err_t esp_wifi_set_max_tx_power(50);  //lower WiFi Power
}


void loop() 
{  
  for(int num= 0; num < NUM_COM ; num++)
  {
    if (server[num]->hasClient())
    {
      for(byte i = 0; i < MAX_NMEA_CLIENTS; i++){
        if (!TCPClient[num][i] || !TCPClient[num][i].connected()){
          if(TCPClient[num][i]) TCPClient[num][i].stop();
          TCPClient[num][i] = server[num]->available();
          continue;
        }
      }
      WiFiClient TmpserverClient = server[num]->available();
      TmpserverClient.stop();
    }
  }
 
  for(int num= 0; num < NUM_COM ; num++)
  {
    if(COM[num] != NULL)          
    {
      if(COM[num]->available())
      {
        while(COM[num]->available())
        {     
          buf2[num][i2[num]] = COM[num]->read();
          if(i2[num]<bufferSize-1) i2[num]++;
        }
        for(byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++)
        {   
          if(TCPClient[num][cln])                     
            TCPClient[num][cln].write(buf2[num], i2[num]);
        }
        i2[num] = 0;
      }
    }    
  }
}
