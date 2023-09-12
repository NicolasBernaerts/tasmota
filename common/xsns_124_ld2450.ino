/*
  xsns_124_ld2450.ino - Driver for Presence and Movement sensor HLK-LD2450

  Copyright (C) 2023  Nicolas Bernaerts

  Connexions :
    * GPIO1 (Tx) should be declared as Serial Tx and connected to HLK-LD2450 Rx
    * GPIO3 (Rx) should be declared as Serial Rx and connected to HLK-LD2450 Tx

  Baud rate is forced at 256000.

  Settings are stored using unused parameters :
    - Settings->free_ea6[26] : Presence detection timeout (sec.)
    - Settings->free_ea6[27] : Minimum detection distance (x10cm)
    - Settings->free_ea6[28] : Maximum detection distance (x10cm)

  Version history :
    03/09/2023 - v1.0 - Creation

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_LD2450

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// declare teleinfo energy driver and sensor
#define XSNS_124                        124

// constant
#define LD2450_START_DELAY              10       // sensor startup delay
#define LD2450_DEFAULT_TIMEOUT          5        // timeout to trigger inactivity (sec.)
#define LD2450_DEFAULT_SAMPLE           10       // number of samples to average
#define LD2450_DEFAULT_DIST_MIN         0        // default minimum detection distance (mm)
#define LD2450_DEFAULT_DIST_MAX         6000     // default minimum detection distance (mm)

#define LD2450_TARGET_MAX               3
#define LD2450_MSG_SIZE_MAX             32

#define LD2450_COLOR_ABSENT             "none"
#define LD2450_COLOR_OUTRANGE           "#555"
#define LD2450_COLOR_PRESENT            "#1fa3ec"


// strings
const char D_LD2450_NAME[] PROGMEM =    "HLK-LD2450";

// MQTT commands
const char kHLKLD2450Commands[]         PROGMEM = "ld2450_|help|min|max";
void (* const HLKLD2450Command[])(void) PROGMEM = { &CmndLD2450Help, &CmndLD2450DistMin, &CmndLD2450DistMax };

/****************************************\
 *                 Data
\****************************************/

// HLK-LD2450 configuration
struct {
  uint8_t  timeout = LD2450_DEFAULT_TIMEOUT;          // default detection timeout
  uint16_t dist_min = LD2450_DEFAULT_DIST_MIN;        // minimum detection distance (mm)
  uint16_t dist_max = LD2450_DEFAULT_DIST_MAX;        // maximum detection distance (mm)
} ld2450_config;

// HLK-LD2450 received message
static struct {    
  uint32_t timestamp = UINT32_MAX;                    // timestamp of last received character
  uint8_t  idx_body  = 0;                             // index of received body
  uint8_t  arr_body[LD2450_MSG_SIZE_MAX];             // body of current received message
  uint8_t  arr_last[4] = {0, 0, 0, 0};                // last received characters
} ld2450_received; 

static struct {
  int16_t  x;             // x coordonnate
  int16_t  y;             // y coordonnate
  int16_t  speed;         // speed
  uint16_t dist;          // gate size
} ld2450_target[LD2450_TARGET_MAX];

static struct {
  TasmotaSerial  *pserial   = nullptr;                // pointer to serial port
  bool            enabled   = false;                  // driver is enabled
  uint8_t         counter   = 0;                      // detected targets counter
//  uint16_t        arr_dist[LD2450_TARGET_MAX];        // distance sorted
  uint32_t        timestamp = 0;                      // timestamp of last detection
} ld2450_status; 



/**************************************************\
 *                  Commands
\**************************************************/

// hlk-ld sensor help
void CmndLD2450Help ()
{
  // help on command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2450_timeout <value> = set timeout (sec.)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2450_min <value>     = minimum detection distance (cm)"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2450_max <value>     = maximum detection distance (cm)"));
  ResponseCmndDone ();
}

void CmndLD2450Timeout ()
{
  if (XdrvMailbox.payload > 0)
  {
    ld2450_config.timeout = XdrvMailbox.payload;
    LD2450SaveConfig ();
  }
  ResponseCmndNumber (ld2450_config.timeout);
}

void CmndLD2450DistMin ()
{
  uint16_t distance;

  if (XdrvMailbox.payload >= 0)
  {
    distance = XdrvMailbox.payload;
    if (distance < ld2450_config.dist_max)
    {
      ld2450_config.dist_min = distance;
      LD2450SaveConfig ();
    }
  }
  ResponseCmndNumber (ld2450_config.dist_min);
}

void CmndLD2450DistMax ()
{
  uint16_t distance;

  if (XdrvMailbox.payload >= 0)
  {
    distance = XdrvMailbox.payload;
    if (distance > ld2450_config.dist_min)
    {
      ld2450_config.dist_max = distance;
      LD2450SaveConfig ();
    }
  }
  ResponseCmndNumber (ld2450_config.dist_max);
}

/**************************************************\
 *                  Config
\**************************************************/

// Load configuration from flash memory
void LD2450LoadConfig ()
{
  // read parameters
  ld2450_config.timeout  = Settings->free_ea6[26];
  ld2450_config.dist_min = (uint16_t)Settings->free_ea6[27] * 100;
  ld2450_config.dist_max = (uint16_t)Settings->free_ea6[28] * 100;

  // check parameters
  if (ld2450_config.timeout == 0) ld2450_config.timeout = LD2450_DEFAULT_TIMEOUT;
  if (ld2450_config.dist_min >= ld2450_config.dist_max)
  {
    ld2450_config.dist_min = LD2450_DEFAULT_DIST_MIN;
    ld2450_config.dist_max = LD2450_DEFAULT_DIST_MAX;
  }
}

// Save configuration into flash memory
void LD2450SaveConfig ()
{
  Settings->free_ea6[26] = ld2450_config.timeout;
  Settings->free_ea6[27] = (uint8_t)(ld2450_config.dist_min / 100);
  Settings->free_ea6[28] = (uint8_t)(ld2450_config.dist_max / 100);
}

/**************************************************\
 *                  Functions
\**************************************************/

bool LD2450GetDetectionStatus (const uint32_t delay)
{
  uint32_t timeout = delay;

  // if timestamp not defined, no detection
  if (ld2450_status.timestamp == 0) return false;
  
  // if no timeout given, use default one
  if ((timeout == 0) || (timeout == UINT32_MAX)) timeout = ld2450_config.timeout;

  // return timeout status
  return (ld2450_status.timestamp + timeout > LocalTime ());
}

// driver initialisation
bool LD2450InitDevice ()
{
  // if not done, init sensor state
  if (ld2450_status.pserial == nullptr)
  {
    // calculate serial rate
    Settings->baudrate = 853;

#ifdef ESP32
    // create serial port
    ld2450_status.pserial = new TasmotaSerial (Pin (GPIO_RXD), Pin (GPIO_TXD), 2);

    // initialise serial port
    ld2450_status.enabled = ld2450_status.pserial->begin (256000, SERIAL_8N1);

#else       // ESP8266
    // create serial port
    ld2450_status.pserial = new TasmotaSerial (Pin (GPIO_RXD), Pin (GPIO_TXD), 2);

    // initialise serial port
    ld2450_status.enabled = ld2450_status.pserial->begin (256000, SERIAL_8N1);

    // force hardware configuration on ESP8266
    if (ld2450_status.enabled && ld2450_status.pserial->hardwareSerial ()) ClaimSerial ();
#endif      // ESP32 & ESP8266

    // log
    if (ld2450_status.enabled) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s sensor init at %u"), D_LD2450_NAME, 256000);
      else AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s sensor init failed"), D_LD2450_NAME);
  }

  return ld2450_status.enabled;
}

void LD2450LogMessage ()
{
  uint8_t index;
  char    str_text[8];
  String  str_log;

  // log type
  str_log = "recv";

  // loop to generate string
  for (index = 0; index < ld2450_received.idx_body; index ++)
  {
    sprintf(str_text, " %02X", ld2450_received.arr_body[index]);
    str_log += str_text;
  }

  // log message
  AddLog (LOG_LEVEL_DEBUG_MORE, PSTR ("HLK: %s"), str_log.c_str ());
}

/*********************************************\
 *                   Callback
\*********************************************/

// driver initialisation
void LD2450Init ()
{
  // load configuration
  LD2450LoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld2450_help to get help on %s commands"), D_LD2450_NAME);
}

// Handling of received data
void LD2450ReceiveData ()
{
  uint8_t  recv_data;
  uint32_t  index, start;
  uint32_t *pheader;
  uint16_t *pfooter;
  int16_t  *pint16;
  
  // check sensor presence
  if (ld2450_status.pserial == nullptr) return;

  // run serial receive loop
  while (ld2450_status.pserial->available ()) 
  {
    // receive character
    recv_data = (uint8_t)ld2450_status.pserial->read ();

    if (TasmotaGlobal.uptime > LD2450_START_DELAY)
    {
      // append character to received message body
      if (ld2450_received.idx_body < LD2450_MSG_SIZE_MAX) ld2450_received.arr_body[ld2450_received.idx_body++] = recv_data;

      // update last received characters
      ld2450_received.arr_last[0] = ld2450_received.arr_last[1];
      ld2450_received.arr_last[1] = ld2450_received.arr_last[2];
      ld2450_received.arr_last[2] = ld2450_received.arr_last[3];
      ld2450_received.arr_last[3] = recv_data;

      // update reception timestamp
      ld2450_received.timestamp = millis ();

      // get header and footer
      pheader = (uint32_t*)&ld2450_received.arr_last;
      pfooter = (uint16_t*)(ld2450_received.arr_last + 2);

      // look for header and footer
      if (*pheader == 0x0003ffaa)
      {
        memcpy (ld2450_received.arr_body, ld2450_received.arr_last, 4);
        ld2450_received.idx_body = 4;
      }
      
      // else if data message received
      else if ((ld2450_received.idx_body == 30) && (*pfooter == 0xcc55))
      {
        // init target counter
        ld2450_status.counter = 0;

        // log received message
        LD2450LogMessage ();

        // loop thru targets
        for (index = 0; index < LD2450_TARGET_MAX; index ++)
        {
          // set target coordonnates
          start = 4 + index * 8;

          // x (negative if left side, positive if right side)
          pint16 = (int16_t*)(ld2450_received.arr_body + start);
          ld2450_target[index].x = *pint16;
          if (ld2450_target[index].x < 0) ld2450_target[index].x = 0 - ld2450_target[index].x - 32768;

          // y
          pint16 = (int16_t*)(ld2450_received.arr_body + start + 2);
          ld2450_target[index].y = *pint16;
          if (ld2450_target[index].y < 0) ld2450_target[index].y = 0 - ld2450_target[index].y - 32768;
          ld2450_target[index].y = 0 - ld2450_target[index].y;

          // speed
          pint16 = (int16_t*)(ld2450_received.arr_body + start + 4);
          ld2450_target[index].speed = *pint16;
          if (ld2450_target[index].speed < 0) ld2450_target[index].speed = 0 - ld2450_target[index].speed - 32768;

          // distance
          ld2450_target[index].dist = (uint16_t)sqrt (pow (ld2450_target[index].x, 2) + pow (ld2450_target[index].y, 2));
          if (ld2450_target[index].dist > LD2450_DEFAULT_DIST_MAX) ld2450_target[index].dist = LD2450_DEFAULT_DIST_MAX;

          // set detection status according to distance
          if ((ld2450_target[index].dist > ld2450_config.dist_min) && (ld2450_target[index].dist <= ld2450_config.dist_max)) ld2450_status.counter++;
        }

        // if at least one target detected, update detection timestamp
        if (ld2450_status.counter > 0) ld2450_status.timestamp = LocalTime ();

        // init reception
        ld2450_received.idx_body = 0;
      }

      // give control back to system
      yield ();
    }
  }
}

// Show JSON status (for MQTT)
//   "HLK-LD2450":{"detect"=1,"target1":{"x"=25,"y":-14,"h":-35,"dist":46,"speed":23},"target2":{"x"=45,"y":34,"h":5,"dist":68,"speed":-12}}
void LD2450ShowJSON (bool append)
{
  uint8_t index;

  // check sensor presence
  if (ld2450_status.pserial == nullptr) return;

  if (append)
  {
    // start of ld2450 section
    ResponseAppend_P (PSTR (",\"%s\":{\"count\":%u,"), D_LD2450_NAME, ld2450_status.counter);

    // loop thru targets
    for (index = 0; index < LD2450_TARGET_MAX; index ++)
      if (ld2450_target[index].dist != 0) ResponseAppend_P (PSTR (",\"goal%u\":{\"x\":%d,\"y\":%d,\"dist\":%u,\"speed\":%d}"), index, ld2450_target[index].x, ld2450_target[index].y, ld2450_target[index].dist, ld2450_target[index].speed);

    // end of ld2450 section
    ResponseAppend_P (PSTR ("}"));
  }
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Append HLK-LD2450 sensor data to main page
void LD2450WebSensor ()
{
  uint8_t index, counter, position;
  uint8_t arr_pos[LD2450_TARGET_MAX];
  bool    arr_zone[LD2450_TARGET_MAX];
  char    str_color[8];

  // check if enabled
  if (!ld2450_status.enabled) return;

  // start of display
  WSContentSend_PD (PSTR ("<div style='font-size:10px;text-align:center;margin-top:4px;padding:2px 6px;background:#333333;border-radius:8px;'>\n"));

  // scale
  WSContentSend_PD (PSTR ("<div style='display:flex;padding:0px;'>\n"));
  WSContentSend_PD (PSTR ("<div style='width:28%%;padding:0px;text-align:left;font-size:12px;font-weight:bold;'>LD2450</div>\n"));
  WSContentSend_PD (PSTR ("<div style='width:6%%;padding:0px;text-align:left;'>0m</div>\n"));
  for (index = 1; index < 6; index ++) WSContentSend_PD (PSTR ("<div style='width:12%%;padding:0px;'>%um</div>\n"), index);
  WSContentSend_PD (PSTR ("<div style='width:6%%;padding:0px;text-align:right;'>6m</div>\n"));
  WSContentSend_PD (PSTR ("</div>\n"));

  // targets
  WSContentSend_PD (PSTR ("<div style='display:flex;padding:0px;background:none;'>\n"));
  WSContentSend_PD (PSTR ("<div style='width:28%%;padding:0px;text-align:left;color:white;'>&nbsp;&nbsp;Presence</div>\n"));

  // calculate target position
  for (index = 0; index < LD2450_TARGET_MAX; index ++)
  {
    // check if target should be displayed
    if (ld2450_target[index].dist == 0) arr_pos[index] = UINT8_MAX;
      else arr_pos[index] = 28 + (uint8_t)((uint32_t)ld2450_target[index].dist * 72 / LD2450_DEFAULT_DIST_MAX);

    // check if target is in the detection zone
    arr_zone[index] = ((ld2450_target[index].dist > ld2450_config.dist_min) && (ld2450_target[index].dist <= ld2450_config.dist_max));
  }

  // adjust lower target position with 5% steps
  position = arr_pos[0];
  for (index = 1; index < LD2450_TARGET_MAX; index++)
  {
    if (position == UINT8_MAX) position = arr_pos[index];
      else if ((arr_pos[index] < 100 - 10) && (arr_pos[index] < position + 5)) arr_pos[index] = position + 5;
    if (arr_pos[index] != UINT8_MAX) position = arr_pos[index];
  }
  
  // adjust higher target position with 5% steps
  position = UINT8_MAX;
  for (counter = LD2450_TARGET_MAX; counter > 0; counter--)
  {
    index = counter - 1;
    if (arr_pos[index] != UINT8_MAX)
    {
      if (arr_pos[index] > 95) arr_pos[index] = 95;
      if ((arr_pos[index] > position - 5) && (arr_pos[index] > 28 + 10)) arr_pos[index] = position - 5;
      position = arr_pos[index];
    } 
  }

  // loop to display targets
  position = 28;
  for (index = 0; index < LD2450_TARGET_MAX; index++)
    if (arr_pos[index] != UINT8_MAX)
    {
      // if needed, set separation zone
      if (arr_pos[index] > position) WSContentSend_P (PSTR ("<div style='width:%u%%;padding:0px;background:none;'>&nbsp;</div>\n"), arr_pos[index] - position);

      // check if target is in detection zone
      if (arr_zone[index]) strcpy (str_color, LD2450_COLOR_PRESENT); else strcpy (str_color, LD2450_COLOR_OUTRANGE);
      WSContentSend_P (PSTR ("<div style='width:5%%;padding:0px;border-radius:50%%;background:%s;'>%u</div>\n"), str_color, index + 1);
    
      // update minimum position
      position = arr_pos[index] + 5;
    }
  if (position < 100) WSContentSend_P (PSTR ("<div style='width:%u%%;padding:0px;background:none;'>&nbsp;</div>\n"), 100 - position);

  // end of display
  WSContentSend_PD (PSTR ("</div>\n"));
}


// Radar page
void LD2450GraphRadarUpdate ()
{
  uint8_t index;
  int32_t pos_x, pos_y;
  char    str_class[8];

  // start of update page
  WSContentBegin (200, CT_PLAIN);

  // loop thru targets
  for (index = 0; index < LD2450_TARGET_MAX; index++)
  {
    // calculate target type
    if (ld2450_target[index].dist == 0) strcpy (str_class, "abs");
      else if ((ld2450_target[index].dist > ld2450_config.dist_min) && (ld2450_target[index].dist <= ld2450_config.dist_max)) strcpy (str_class, "act");
      else strcpy (str_class, "ina");

    // calculate coordonates
    pos_x = (int32_t)ld2450_target[index].x * 400 / LD2450_DEFAULT_DIST_MAX + 400;
    pos_y = (int32_t)ld2450_target[index].y * 400 / LD2450_DEFAULT_DIST_MAX + 50;

    // display target
    WSContentSend_P (PSTR ("%s;%d;%d;%d\n"), str_class, pos_x, pos_y, pos_y + 5);
  }

  // end of update page
  WSContentEnd ();
}

// Radar page
void LD2450GraphRadar ()
{
  uint8_t  index;

  // set page label
  WSContentStart_P ("LD2450 Radar", true);
  WSContentSend_P (PSTR ("\n</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n\n"));

  WSContentSend_P (PSTR ("function updateData(){\n"));

  WSContentSend_P (PSTR (" httpData=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpData.open('GET','/ld2450.upd',true);\n"));

  WSContentSend_P (PSTR (" httpData.onreadystatechange=function(){\n"));
  WSContentSend_P (PSTR ("  if (httpData.readyState===XMLHttpRequest.DONE){\n"));
  WSContentSend_P (PSTR ("   if (httpData.status===0 || (httpData.status>=200 && httpData.status<400)){\n"));
  WSContentSend_P (PSTR ("    arr_param=httpData.responseText.split('\\n');\n"));
  WSContentSend_P (PSTR ("    num_param=arr_param.length;\n"));
  WSContentSend_P (PSTR ("    for (i=0;i<num_param;i++){\n"));
  WSContentSend_P (PSTR ("     arr_value=arr_param[i].split(';');\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('c'+i)!=null) document.getElementById('c'+i).classList.remove('abs','act','ina');\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('c'+i)!=null) document.getElementById('c'+i).classList.add(arr_value[0]);\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('c'+i)!=null) document.getElementById('t'+i).classList.remove('abs','act','ina');\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('c'+i)!=null) document.getElementById('t'+i).classList.add(arr_value[0]);\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('c'+i)!=null) document.getElementById('c'+i).setAttributeNS(null,'cx',arr_value[1]);\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('t'+i)!=null) document.getElementById('t'+i).setAttribute('x',arr_value[1]);\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('c'+i)!=null) document.getElementById('c'+i).setAttributeNS(null,'cy',arr_value[2]);\n"));
  WSContentSend_P (PSTR ("     if (document.getElementById('t'+i)!=null) document.getElementById('t'+i).setAttribute('y',arr_value[3]);\n"));
  WSContentSend_P (PSTR ("    }\n"));
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("   setTimeout(updateData,%u);\n"), 1000);               // ask for next update in 1 sec
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));

  WSContentSend_P (PSTR (" httpData.send();\n"));
  WSContentSend_P (PSTR ("}\n"));

  WSContentSend_P (PSTR ("setTimeout(updateData,%u);\n\n"), 100);                   // ask for first update after 100ms

  WSContentSend_P (PSTR ("</script>\n\n"));

  // set page as scalable
  WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));

  WSContentSend_P (PSTR ("a {color:white;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  WSContentSend_P (PSTR ("div {padding:0px;margin:0px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:4vh;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.header {font-size:3vh;margin:1vh auto;}\n"));

  WSContentSend_P (PSTR ("div.graph {width:100%%;margin:1vh auto;}\n"));
  WSContentSend_P (PSTR ("svg.graph {width:100%%;height:50vh;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>\n"));

  // room name
  WSContentSend_P (PSTR ("<div class='title'><a href='/'>%s</a></div>\n"), SettingsText(SET_DEVICENAME));

  // header
  WSContentSend_P (PSTR ("<div class='header'>Live Radar</div>\n"));

  // ------- Graph --------

  // start of radar
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 800 450'>\n"));

  // style
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("text {font-size:20px;fill:white;text-anchor:middle;}\n"));
  WSContentSend_P (PSTR ("text.start {text-anchor:start;}\n"));
  WSContentSend_P (PSTR ("text.end {text-anchor:end;}\n"));
  WSContentSend_P (PSTR ("text.abs {fill:%s;}\n"), LD2450_COLOR_ABSENT);
  WSContentSend_P (PSTR ("path {stroke:green;fill:none;}\n"));
  WSContentSend_P (PSTR ("circle.ina {fill:#555;}\n"), LD2450_COLOR_OUTRANGE);
  WSContentSend_P (PSTR ("circle.act {fill:%s;}\n"), LD2450_COLOR_PRESENT);
  WSContentSend_P (PSTR ("circle.abs {fill:%s;}\n"), LD2450_COLOR_ABSENT);
  WSContentSend_P (PSTR ("</style>\n"));

  // radar distances
  WSContentSend_P (PSTR ("<text x=0 y=30 class='start'>6m</text>\n"));
  WSContentSend_P (PSTR ("<text x=67 y=30>5m</text>\n"));
  WSContentSend_P (PSTR ("<text x=133 y=30>4m</text>\n"));
  WSContentSend_P (PSTR ("<text x=200 y=30>3m</text>\n"));
  WSContentSend_P (PSTR ("<text x=267 y=30>2m</text>\n"));
  WSContentSend_P (PSTR ("<text x=333 y=30>1m</text>\n"));
  WSContentSend_P (PSTR ("<text x=467 y=30>1m</text>\n"));
  WSContentSend_P (PSTR ("<text x=533 y=30>2m</text>\n"));
  WSContentSend_P (PSTR ("<text x=600 y=30>3m</text>\n"));
  WSContentSend_P (PSTR ("<text x=667 y=30>4m</text>\n"));
  WSContentSend_P (PSTR ("<text x=733 y=30>5m</text>\n"));
  WSContentSend_P (PSTR ("<text x=800 y=30 class='end'>6m</text>\n"));

  // radar frame
  WSContentSend_P (PSTR ("<path d='M 0 50 L 800 50' />\n"));

  WSContentSend_P (PSTR ("<path d='M 800 50 A 400 400 0 0 1 0 50' />\n"));
  WSContentSend_P (PSTR ("<path d='M 733 50 A 330 330 0 0 1 67 50' />\n"));
  WSContentSend_P (PSTR ("<path d='M 667 50 A 267 267 0 0 1 133 50' />\n"));
  WSContentSend_P (PSTR ("<path d='M 600 50 A 200 200 0 0 1 200 50' />\n"));
  WSContentSend_P (PSTR ("<path d='M 533 50 A 133 133 0 0 1 267 50' />\n"));
  WSContentSend_P (PSTR ("<path d='M 467 50 A 67 67 0 0 1 333 50' />\n"));

  WSContentSend_P (PSTR ("<path d='M 400 50 L 54 250' />\n"));
  WSContentSend_P (PSTR ("<path d='M 400 50 L 200 396' />\n"));
  WSContentSend_P (PSTR ("<path d='M 400 50 L 400 450' />\n"));
  WSContentSend_P (PSTR ("<path d='M 400 50 L 600 396' />\n"));
  WSContentSend_P (PSTR ("<path d='M 400 50 L 746 250' />\n"));

  // display targets
  for (index = 0; index < LD2450_TARGET_MAX; index++)
  {
    WSContentSend_P (PSTR ("<circle id='c%u' class='abs' cx=400 cy=50 r=20 />\n"), index);
    WSContentSend_P (PSTR ("<text id='t%u' class='abs' x=400 y=50>%u</text>\n"), index, index + 1);
  }

  // end of radar
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// LD2450 sensor
bool Xsns124 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_INIT:
      LD2450Init ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kHLKLD2450Commands, HLKLD2450Command);
      break;
    case FUNC_JSON_APPEND:
      LD2450ShowJSON (true);
      break;
    case FUNC_LOOP:
      if (ld2450_status.enabled) LD2450ReceiveData ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      LD2450WebSensor ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      WSContentSend_P (PSTR ("<p><form action='ld2450' method='get'><button>LD2450 Radar</button></form></p>\n"));
      break;
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on ("/ld2450", LD2450GraphRadar);
      Webserver->on ("/ld2450.upd", LD2450GraphRadarUpdate);
      break;
#endif  // USE_WEBSERVER
  }

  return result;
}

#endif     // USE_LD2450

