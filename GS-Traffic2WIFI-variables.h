// Debug-Messages on serial, can be overwriten bei WebUI
int debug = 0; //0=Off, 1=Many, 2=More, 3=All
int notraffic=1; // Dev-Variable is default 1 for not showing Traffic-Screen

// Rotary-Encoder
long rotoldpos = -999;

// Screen Refresh- and Display-Variables
int actscreen=0;
int actscreenbright=0;
int actscreengdl=0;
int actscreenset=0;
int refrshbuffs=1;
int screengroundspeed;
int screenaltitide;
int screentrack;
int screensatellites;
int screensatsignal;
String screentime="";
int baudrates[] = {4800,9600,19200,28800,38400,57600,115200,230400};

// IP-Configuration
IPAddress ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress netmask(255, 255, 255, 0);
const char * udpAddress = "192.168.1.255";
Preferences preferences;
WiFiUDP udp;




int nodatadelay = 300; // Delaying in loop if no data is received at startup
char ssid[50];
char wpakey[50];
uint8_t chipid[6];
String gprmc = "";
String gpgsa = "";
String pgrmz = "";
String pflau = "";
String pflaa = "";
String gpgga = "";
int extractstream = 0;
int trafficwarnled = 0;
int positionwarnled = 0;
String hwtype = "GS-Traffic2WIFIv1 or selfmade"; // Default
int hwtypeid=1;    // Default
int statusled = 0; //0=off, 1=green, 2=red, 3=orange
int trafficwarning = 0; //0=off; >0 means on couting down with extract data
int positionwarning = 0; //0=off; >0 means on couting down with extract data

// Define Web-Server, without TCP-Port
WebServer webserver(0);

// Define TCP-Servers
WiFiServer server_0(0); // Changed SERIAL0_TCP_PORT to 0, for defining later in setup
WiFiServer server_1(0); // Changed TCP_PORT to 0, for defining later in setup
WiFiServer *server[2] = {&server_0,&server_1};
WiFiClient TCPClient[2][MAX_NMEA_CLIENTS];

WiFiServer debugserver(0); // Debug TCP-Server

// Define Buffersm
uint8_t buf1[1][bufferSize];
uint16_t i1[1] = {0};
uint8_t buf2[1][bufferSize];
uint16_t i2[1] = {0};
uint8_t BTbuf[bufferSize];
uint16_t iBT = 0;


// Define Fakedata
float fakestartlat = 5412.998;
float fakestartlon = 936.007;
float fakealt = 150;
int fake01lat = -1234;
int fake01lon = 1234;
int fake02lat = 5000;
int fake02lon = 327;
int fake03lat = -334;
int fake03lon = 7034;

// Defining Serial-indata-String
String inserial;

// Define Trafficmod-TMLEDs
Adafruit_NeoPixel tmleds = Adafruit_NeoPixel(TMODLEDCOUNTER, SDAPIN, NEO_GRB + NEO_KHZ800);

// GDL90 Variables
byte gdl90lat[3];
byte gdl90lon[3];
float nmea2gdl90lon;                // Based on $GPRMC
float nmea2gdl90lat;                // Based on $GPRMC
String nmea2gdl90time;
char *nmea2gdl90lonquad=(char*) "N";// Based on $GPRMC, default N will be overwritten // TEST: Angepsst wegen compiler
char *nmea2gdl90latquad=(char*) "E";// Based on $GPRMC, default E will be overwritten // TEST: Angepsst wegen compiler
float nmea2gdl90groundspeed;        // Based on $GPRMC
float nmea2gdl90altitide;           // Based on $PGRMZ - barometric height
float nmea2gdl90altitidegeodetic;   // Based on $PGRMZ -  $GPGGA
float nmea2gdl9track;               // Based on $GPRMC
int nmea2gdl90ssmutc=0;             // Based on $GPRMC
int nmea2gdl90gps;                  // Based on $GPRMC
int nmea2gdl90satsignal=0;          // Based on $GPRMC
int nmea2gdl90satellites;           // Based on $GPGSA

// Saving Traffic
int maxtraffic=5;
String traffic[6][11];