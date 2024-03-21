// Defintion of Version
#define versionmajor 4
#define versionminor 0
#define versionextend "Alpha 1"
#define versionfull "4.0"

// Define PINs on ESP32
#define HWTYPEPIN 12
#define HWTYPEPIN2 14
#define RESETPIN 19
#define LEDPINGREEN 18
#define LEDPINRED 5
#define SERIAL0_RXPIN 21
#define SERIAL0_TXPIN 1
#define SERIAL0_RXPINv2 22
#define SERIAL0_TXPINv2 23
#define SERIAL0_RXPINv3 2
#define SERIAL0_TXPINv3 1
#define SDAPIN 13


// Define RGB-Driver-LEDs (Rear v3 + Trafficmod)
// 0=Rear-LED outside Trafficmod,1-24=CIRCLE
#define TMODLEDCOUNTER 25
#define tmcirclestart 1
#define tmcirclecount 24

#define earth 6378.137

//Definition of Cores
TaskHandle_t core0taskv3;
TaskHandle_t core0taskwebserver;
TaskHandle_t core0taskgdl90sender;
TaskHandle_t core1taskbridging;
TaskHandle_t core1taskfaking;

// Define Serial-Parameters
#define SERIAL_PARAM0 SERIAL_8N1
#define bufferSize 1024
#define MAX_NMEA_CLIENTS 10

const char compile_date[] = __DATE__ " " __TIME__;

// GDL90 Definitions
#define LON_LAT_RESOLUTION  float(180.0/8388608.0)
#define TRACK_RESOLUTION    float(360.0/256.0)
