/*
  xdrv_96_korad.ino - Driver for KORAD power supplies KAxxxxP
  Tested on KA3005P
  
  Copyright (C) 2020  Nicolas Bernaerts

    02/05/2021 - v1.0 - Creation

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

#ifdef USE_KORAD

/**************************************\
 *               Variables
\**************************************/

// declare teleinfo energy driver and sensor
#define XDRV_96      96

// constants
#define KORAD_VOLTAGE_MAX         30        // maximum voltage
#define KORAD_CURRENT_MAX         3         // maximum current

#define KORAD_INIT_TIMEOUT        4         // seconds before init
#define KORAD_SEND_QUEUE_MAX      8         // maximum number of messages to send
#define KORAD_SEND_QUEUE_LIMIT    6         // limit maximum number of messages to send
#define KORAD_RECV_BUFFER_MAX     64        // maximum size of a reception buffer
#define KORAD_RECV_TIMEOUT        2         // maximum number of loop without received caracters

#define KORAD_GRAPH_SAMPLE        900       // number of samples in the voltage/current graph (every second for 15mn)
#define KORAD_GRAPH_HEIGHT        600       // height of graph in pixels
#define KORAD_GRAPH_WIDTH         900       // width of graph in pixels
#define KORAD_GRAPH_PERCENT_START 10     
#define KORAD_GRAPH_PERCENT_STOP  90

#define D_CMND_KORAD_POWER        "power"
#define D_CMND_KORAD_OVP          "ovp"
#define D_CMND_KORAD_OCP          "ocp"
#define D_CMND_KORAD_VSET         "vset"
#define D_CMND_KORAD_ISET         "iset"
#define D_CMND_KORAD_VMAX         "vmax"
#define D_CMND_KORAD_IMAX         "imax"

#define D_KORAD_MODEL             "Model"
#define D_KORAD_VERSION           "Version"
#define D_KORAD_UNKNOWN           "Unknown"
#define D_KORAD_VOLTAGE           "Voltage"
#define D_KORAD_CURRENT           "Current"
#define D_KORAD_REFERENCE         "Ref"
#define D_KORAD_OUTPUT            "Output"
#define D_KORAD_CONTROL           "Control"
#define D_KORAD_GRAPH             "Graph"

#define D_CMND_KORAD_DATA         "data"

// form strings
//const char TELEINFO_INPUT_RADIO[] PROGMEM = "<p><input type='radio' name='%s' value='%d' %s>%s</p>\n";
//const char TELEINFO_FIELD_START[] PROGMEM = "<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n";
//const char TELEINFO_FIELD_STOP[]  PROGMEM = "</fieldset></p><br>\n";
const char KORAD_HTML_UNIT[] PROGMEM = "<text class='%s' x='%d%%' y='%d%%'>%s</text>\n";
const char KORAD_HTML_DASH[] PROGMEM = "<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n";

// web colors
const char D_KORAD_COLOR_VOLTAGE[]    PROGMEM = "yellow";
const char D_KORAD_COLOR_CURRENT[]    PROGMEM = "red";
const char D_KORAD_COLOR_POWER[]      PROGMEM = "cyan";

// web URL
const char D_KORAD_PAGE_CONTROL[]     PROGMEM = "/ctrl";
const char D_KORAD_PAGE_DATA_UPDATE[] PROGMEM = "/data.upd";
const char D_KORAD_PAGE_GRAPH_FRAME[] PROGMEM = "/frame.svg";
const char D_KORAD_PAGE_GRAPH_DATA[]  PROGMEM = "/curve.svg";

// timezone commands
enum KoradMqttCommands { CMND_KORAD_VSET, CMND_KORAD_ISET, CMND_KORAD_OUT, CMND_KORAD_OVP, CMND_KORAD_OCP };
const char kKoradMqttCommands[] PROGMEM = "kps_vset|kps_iset|kps_out|kps_ovp|kps_ocp";

// power supply modes
enum KoradMode                    { KORAD_CC, KORAD_CV };
const char kKoradMode[] PROGMEM = "Current|Voltage";

// power supply commands
enum KoradCommand                    { KORAD_IDN, KORAD_STATUS, KORAD_Q_VSET1, KORAD_S_VSET1, KORAD_Q_VOUT1, KORAD_Q_ISET1, KORAD_S_ISET1, KORAD_Q_IOUT1, KORAD_OUT1, KORAD_OUT0, KORAD_OVP1, KORAD_OVP0, KORAD_OCP1, KORAD_OCP0, KORAD_TRACK0, KORAD_RCL, KORAD_SAV };
const int ARR_KORAD_ANSWER_SIZE[]  = { 64,  1,      5,     0,     5,     5,     0,     5,     0,   0,   0,   0,   0,   0,   0,     0,   0   };
const char kKoradCommand[] PROGMEM = "*IDN?|STATUS?|VSET1?|VSET1:|VOUT1?|ISET1?|ISET1:|IOUT1?|OUT1|OUT0|OVP1|OVP0|OCP1|OCP0|TRACK0|RCL|SAV";

// korad recurrent 10 queries (in parallel to output current queries)
const int ARR_KORAD_CMND_QUERY[] = { KORAD_Q_VOUT1, KORAD_Q_VSET1, KORAD_Q_VOUT1, KORAD_Q_ISET1, KORAD_Q_VOUT1, KORAD_STATUS, KORAD_Q_VOUT1, KORAD_Q_VSET1, KORAD_Q_VOUT1, KORAD_Q_ISET1 };

// power supply status
static struct {
  int   ready = false;
  int   mode;
  bool  out;            // output status
  bool  ovp;            // over voltage protection status
  bool  ocp;            // over current protection status
  float vset;
  float iset;
  float vout;
  float iout;
  char  str_model[32];
  char  str_version[32];
} korad_status;

static struct {
  int   send_queue   = 0;
  int   last_command = -1;
  int   size_answer  = 0;
  int   recv_timeout = 0;
  int   cmnd_index   = 0;
  int   arr_send_queue[KORAD_SEND_QUEUE_MAX];
  float arr_send_value[KORAD_SEND_QUEUE_MAX];
  char  str_recv_buffer[KORAD_RECV_BUFFER_MAX];
} korad_command;

// graph
static struct {
  int index;                                      // current array index per refresh period
  float vmax;                                     // maximum voltage scale
  float imax;                                     // maximum current scale
  uint16_t arr_voltage[KORAD_GRAPH_SAMPLE];       // array of instant voltage (x 100)
  uint16_t arr_current[KORAD_GRAPH_SAMPLE];       // array of instant current (x 1000)
} korad_graph; 

/***************************************\
 *               Functions
\***************************************/

void KoradSendCommand (int command_type, float command_value = 0)
{
  int  index, length;
  char str_value[8];
  char str_command[16];

  // init
  strcpy (str_value, "");
  strcpy (str_command, "");

  // get command string
  if (command_type != -1) GetTextIndexed (str_command, sizeof (str_command), command_type, kKoradCommand);

  // if needed, add value
  switch (command_type)
  {
    case KORAD_S_VSET1:
      // convert voltage to text and pad with 0 to get 5 digits
      ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%02_f"), &command_value);
      while (strlen (str_value) < 5) strcat (str_value, "0");
      korad_status.vset = command_value;
      break;
    case KORAD_S_ISET1:
      // convert current to text and pad with 0 to get 5 digits
      ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%03_f"), &command_value);
      while (strlen (str_value) < 5) strcat (str_value, "0");
      korad_status.iset = command_value;
      break;
    case KORAD_SAV:
      // convert index
      ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%0_f"), &command_value);
    case KORAD_RCL:
      // convert index
      ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%0_f"), &command_value);
      break;
  }

  // append value to command
  strlcat (str_command, str_value, sizeof (str_command));

  // send command
  Serial.write (str_command);
  korad_command.last_command = command_type;
  korad_command.size_answer  = ARR_KORAD_ANSWER_SIZE[command_type];
  korad_command.recv_timeout = 0;
}

void KoradAddCommand (int command_type, float command_value = 0)
{
  // if queue is not full
  if (korad_command.send_queue < KORAD_SEND_QUEUE_MAX)
  {
    // add messsage to the queue
    korad_command.arr_send_queue[korad_command.send_queue] = command_type;
    korad_command.arr_send_value[korad_command.send_queue] = command_value;

    // increase index
    korad_command.send_queue++;
  }
}

void KoradSetVoltage (float voltage)
{
  // add command to queue
  KoradAddCommand (KORAD_S_VSET1, voltage);
}

void KoradSetCurrent (float current)
{
  // add command to queue
  KoradAddCommand (KORAD_S_ISET1, current);
}

void KoradEnableOutput (bool enable)
{
  // add command to queue
  if (enable) KoradAddCommand (KORAD_OUT1); else KoradAddCommand (KORAD_OUT0);
}

void KoradOverVoltageProtection (bool enable)
{
  // add command to queue
  if (enable) KoradAddCommand (KORAD_OVP1); else KoradAddCommand (KORAD_OVP0);
}

void KoradOverCurrentProtection (bool enable)
{
  // add command to queue
  if (enable) KoradAddCommand (KORAD_OCP1); else KoradAddCommand (KORAD_OCP0);
}

void KoradSaveToMemory (int index)
{
  // if index is valid, save to memory
  if ((index > 0) && (index < 6)) KoradAddCommand (KORAD_SAV, (float)index);
}

void KoradRecallFromMemory (int index)
{
  // if index is valid, recall from memory
  if ((index > 0) && (index < 6)) KoradAddCommand (KORAD_RCL, (float)index);
}

/*
*IDN? 	KORADKA3005PV2.0 	Request identification from device. See also the full list of recognized IDs in libsigrok in the models[] array.
STATUS? 	(byte) 	Request the actual status. The output is a single byte with the actual status encoded in bits. At least the Velleman PS3005D V2.0 is a bit buggy here. The only reliable bits are: 0x40 (Output mode: 1:on, 0:off), 0x20 (OVP and/or OCP mode: 1:on, 0:off) and 0x01 (CV/CC mode: 1:CV, 0:CC).
VSET1? 	12.34 	Request the voltage as set by the user.
VSET1:12.34 	(none) 	Set the maximum output voltage.
VOUT1? 	12.34 	Request the actual voltage output.
ISET1? 	0.125 	Request the current as set by the user. See notes below for a firmware bug related to this command.
ISET1:0.125 	(none) 	Set the maximum output current.
IOUT1? 	0.125 	Request the actual output current.
*/
void KoradInit ()
{
  int index;

  // check if serial ports are selected 
  if (PinUsed (GPIO_TXD) && PinUsed (GPIO_RXD)) 
  {
    // start ESP8266 serial port
    Serial.flush ();
    Serial.begin (9600, SERIAL_8N1);
    ClaimSerial ();

    // empty reception buffer
    while (Serial.available() > 0) Serial.read ();

    // next step is initilisation
    korad_status.ready = true;
    AddLog (LOG_LEVEL_INFO, PSTR ("KPS: Serial port initialised (9600 bauds, 8N1"));

    // init receive buffer
    strcpy (korad_command.str_recv_buffer, "");
    strcpy (korad_status.str_model, D_KORAD_UNKNOWN);
    strcpy (korad_status.str_version, "");

    // read power supply config 
    KoradAddCommand (KORAD_IDN);
  }

  // init graph data
  korad_graph.index = 0;
  korad_graph.vmax = KORAD_VOLTAGE_MAX;
  korad_graph.imax = KORAD_CURRENT_MAX;
  for (index = 0; index < KORAD_GRAPH_SAMPLE; index++)
  {
    korad_graph.arr_current[index] = UINT16_MAX;
    korad_graph.arr_voltage[index] = UINT16_MAX;
  } 
}

// Handle MQTT commands
bool KoradMqttCommand ()
{
  bool  command_handled = true;
  bool  command_state;
  int   command_code;
  float command_value;
  char  command_text[CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command_text, sizeof(command_text), XdrvMailbox.topic, kKoradMqttCommands);

  // handle command
  switch (command_code)
  {
    case CMND_KORAD_VSET:  // set voltage output
      command_value = atof (XdrvMailbox.data);
      KoradSetVoltage (command_value);
      break;
    case CMND_KORAD_ISET:  // set current output
      command_value = atof (XdrvMailbox.data);
      KoradSetCurrent (command_value);
      break;
    case CMND_KORAD_OUT:  // enable output
      command_state = ((strcasecmp (XdrvMailbox.data, "on") == 0) || (XdrvMailbox.payload == 1));
      KoradEnableOutput (command_state);
      break;
    case CMND_KORAD_OVP:  // enable over-voltage protection
      command_state = ((strcasecmp (XdrvMailbox.data, "on") == 0) || (XdrvMailbox.payload == 1));
      KoradOverVoltageProtection (command_state);
      break;
    case CMND_KORAD_OCP:  // enable over-voltage protection
      command_state = ((strcasecmp (XdrvMailbox.data, "on") == 0) || (XdrvMailbox.payload == 1));
      KoradOverCurrentProtection (command_state);
      break;
    default:
      command_handled = false;
      break;
  }

//  return true;
  return command_handled;
}

// Show JSON status (for MQTT)
void KoradShowJSON (bool append)
{
  float value;
  char  str_value[8];

  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // Korad data  -->  "Korad":{"Model":"model","Version":"version","VSET":5.0,"ISET":1.01,"VOUT":4.99,"IOUT"=0.22,"OUT"="ON","OVP":"ON","OCP":"OFF"}
  ResponseAppend_P (PSTR ("\"Korad\":{"));

  // model and version
  if (append) ResponseAppend_P (PSTR ("\"Model\":\"%s\",\"Version\":\"%s\","), korad_status.str_model, korad_status.str_version);

  // VSET and ISET
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &korad_status.vset);
  ResponseAppend_P (PSTR ("\"VSET\":%s"), str_value);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &korad_status.iset);
  ResponseAppend_P (PSTR (",\"ISET\":%s"), str_value);

  // VOUT and IOUT
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &korad_status.vout);
  ResponseAppend_P (PSTR (",\"VOUT\":%s"), str_value);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &korad_status.iout);
  ResponseAppend_P (PSTR (",\"IOUT\":%s"), str_value);

  // OUT
  if (korad_status.out) strcpy (str_value, MQTT_STATUS_ON); else strcpy (str_value, MQTT_STATUS_OFF);
  ResponseAppend_P (PSTR (",\"OUT\":\"%s\""), str_value);

  // OVP
  if (korad_status.ovp) strcpy (str_value, MQTT_STATUS_ON); else strcpy (str_value, MQTT_STATUS_OFF);
  ResponseAppend_P (PSTR (",\"OVP\":\"%s\""), str_value);

  // OUT
  if (korad_status.ocp) strcpy (str_value, MQTT_STATUS_ON); else strcpy (str_value, MQTT_STATUS_OFF);
  ResponseAppend_P (PSTR (",\"OCP\":\"%s\""), str_value);

  ResponseAppend_P (PSTR ("}"));

  // publish it if not in append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishPrefixTopic_P (TELE, PSTR (D_RSLT_SENSOR));
  } 
}

// Receive serial messages and handle them
// 20 times per second
void KoradEvery50ms ()
{
  int     index;
  uint8_t status;
  float   value;
  char*   pstr_token;
  char    str_received[2] = {0, 0};

  // if nothing to receive, reset last command
  if ((korad_command.last_command != -1) && (korad_command.size_answer == 0)) korad_command.last_command = -1;

  // if nothing received and answer awaited, increment timeout
  if ((korad_command.last_command != -1) && (Serial.available() == 0)) korad_command.recv_timeout++;

  // receive all pending data
  while (Serial.available() > 0)
  {
    // receive current caracter
    str_received[0] = (char)Serial.read ();
    strlcat (korad_command.str_recv_buffer, str_received, KORAD_RECV_BUFFER_MAX);

    // reset timeout
    korad_command.recv_timeout = 0;
  }

  // if something has been received
  if (strlen (korad_command.str_recv_buffer) > 0)
  {
    // if nothing was awaited, drop it
    if (korad_command.last_command == -1)
    {
      AddLog (LOG_LEVEL_INFO, PSTR ("KPS : dropped %s"), korad_command.str_recv_buffer);
      strcpy (korad_command.str_recv_buffer, "");
    }

    // else if expected data has been received or reception timeout reached
    else if ((strlen (korad_command.str_recv_buffer) >= korad_command.size_answer) || (korad_command.recv_timeout > KORAD_RECV_TIMEOUT))
    {
      // handle answer according to last command
      switch (korad_command.last_command)
      {
        case KORAD_IDN:
          index = 0;
          pstr_token = strtok (korad_command.str_recv_buffer, " ");
          while (pstr_token != nullptr) 
          {
            // update data
            if (index == 0) strcpy (korad_status.str_model, pstr_token);
            else if (index == 1) { strcat (korad_status.str_model, " "); strcat (korad_status.str_model, pstr_token); }
            else if (index == 2) strcpy (korad_status.str_version, pstr_token);
            else { strcat (korad_status.str_version, " "); strcat (korad_status.str_version, pstr_token); }
            index++;

            // change token
            pstr_token = strtok (nullptr, " ");
          }
          break;

        case KORAD_STATUS:
          // update status
          //   bit 1 : mode CC (0) or CV (1)
          //   bit 6 : OCP mode
          //   bit 7 : output status
          //   bit 8 : OVP mode
          status = (uint8_t)korad_command.str_recv_buffer[0];
          korad_status.mode = (status & 1);
          korad_status.ocp = ((status & 32) == 32);
          korad_status.out = ((status & 64) == 64);
          korad_status.ovp = ((status & 128) == 128);
          break;

        case KORAD_Q_VSET1:
          korad_status.vset = atof (korad_command.str_recv_buffer);
          break;

        case KORAD_Q_ISET1:
          korad_status.iset = atof (korad_command.str_recv_buffer);
          break;

        case KORAD_Q_VOUT1:
          value = korad_status.vout;
          korad_status.vout = atof (korad_command.str_recv_buffer);

          // if output off and vout changed, publish JSON
          if (!korad_status.out && (value != korad_status.vout)) KoradShowJSON (false);
          break;

        case KORAD_Q_IOUT1:
          korad_status.iout = atof (korad_command.str_recv_buffer);
          break;
      }

      // reset last command data
      strcpy (korad_command.str_recv_buffer, "");
      korad_command.last_command = -1;
      korad_command.size_answer  = 0;
      korad_command.recv_timeout = 0;
    }
  }

  // if some commands are in the queue and last command answer has been received
  if ((korad_command.last_command == -1) && (korad_command.send_queue > 0))
  {
    // send first command
    KoradSendCommand (korad_command.arr_send_queue[0], korad_command.arr_send_value[0]);
    korad_command.send_queue--;

    // shift command queue
    for (index = 0; index < korad_command.send_queue; index++)
    {
      korad_command.arr_send_queue[index] = korad_command.arr_send_queue[index + 1];
      korad_command.arr_send_value[index] = korad_command.arr_send_value[index + 1];
    }
  }
}

// 10 times per second
// Send current reading current
// Save graph data
void KoradEvery100ms ()
{
  // update graph data for current and voltage
  korad_graph.arr_voltage[korad_graph.index] = (uint16_t)(korad_status.vout * 100);
  korad_graph.arr_current[korad_graph.index] = (uint16_t)(korad_status.iout * 1000);
  korad_graph.index++;
  korad_graph.index = korad_graph.index % KORAD_GRAPH_SAMPLE;

  // if command queue is not full, read output current
  if (korad_command.send_queue < KORAD_SEND_QUEUE_LIMIT) KoradAddCommand (KORAD_Q_IOUT1);

  // if command queue is not full, read other value according to command counter
  if (korad_command.send_queue < KORAD_SEND_QUEUE_LIMIT) KoradAddCommand (ARR_KORAD_CMND_QUERY[korad_command.cmnd_index]);
  korad_command.cmnd_index ++;
  korad_command.cmnd_index = korad_command.cmnd_index % 10;
}

void KoradEverySecond ()
{
  // if command queue is empty, update VSET and ISET value
  if (korad_command.send_queue < KORAD_SEND_QUEUE_LIMIT)
  {
    KoradAddCommand (KORAD_Q_VSET1);
    KoradAddCommand (KORAD_Q_ISET1);
  }

  // if output ON, publish data
  if (korad_status.out) KoradShowJSON (false);
}

/**************************************\
 *                   Web
\**************************************/

#ifdef USE_WEBSERVER

// append relay status to main page
void KoradWebSensor ()
{
  char str_value[24];
  char str_set[8];
  char str_text[48];

  // display identification
  WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_KORAD_MODEL, korad_status.str_model);
  WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_KORAD_VERSION, korad_status.str_version);

  // display output status
  if (korad_status.out) strcpy_P (str_value, PSTR ("<span style='color:green;'><b>ON</b></span>")); 
    else strcpy_P (str_value, PSTR ("<span style='color:red;'>OFF</span>")); 
  WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_KORAD_OUTPUT, str_value);

  // display voltage
  if (korad_status.ovp) strcpy_P (str_text, PSTR ("<span style='color:green;'> [ovp]</span>"));
    else strcpy (str_text, "");
  if (korad_status.out) ext_snprintf_P (str_value, sizeof(str_value), PSTR ("<b>%02_f V</b>"), &korad_status.vout);
    else ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%02_f V"), &korad_status.vout);
  ext_snprintf_P (str_set, sizeof(str_set), PSTR ("%02_f"), &korad_status.vset);
  WSContentSend_PD (PSTR ("{s}%s %s{m}%s <small>/ %s</small>{e}"), D_KORAD_VOLTAGE, str_text, str_value, str_set);

  // display current
  if (korad_status.ocp) strcpy_P (str_text, PSTR ("<span style='color:green;'> [ocp]</span>"));
    else strcpy (str_text, "");
  if (korad_status.out) ext_snprintf_P (str_value, sizeof(str_value), PSTR ("<b>%03_f A</b>"), &korad_status.iout);
    else ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%03_f A"), &korad_status.iout);
  ext_snprintf_P (str_set, sizeof(str_set), PSTR ("%03_f"), &korad_status.iset);
  WSContentSend_PD (PSTR ("{s}%s %s{m}%s <small>/ %s</small>{e}"), D_KORAD_CURRENT, str_text, str_value, str_set);
}

// append Korad graph button to main page
void KoradWebMainButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_KORAD_PAGE_CONTROL, D_KORAD_CONTROL);
}

// realtime data update
void KoradWebDataUpdate ()
{
  float value;
  char  str_value[8];
  char  str_unit[4];

  // start of data page
  WSContentBegin (200, CT_PLAIN);

  // power switch status 
  if (korad_status.out) strcpy (str_value, "on"); else strcpy (str_value, "off");
  WSContentSend_P ("%s", str_value);

  // voltage output status
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%02_f"), &korad_status.vout);
  WSContentSend_P (";%s;V", str_value);

  // current outpu status
  if (korad_status.iout < 1)
  {
    value = korad_status.iout * 1000;
    ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%00_f"), &value);
    strcpy (str_unit, "mA");
  }  
  else
  {
    value = korad_status.iout;
    ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%03_f"), &value);
    strcpy (str_unit, "A");
  }  
  WSContentSend_P (";%s;%s", str_value, str_unit);

  // power output status
  value = korad_status.vout * korad_status.iout;
  if (value < 1)
  {
    value = value * 1000;
    ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%00_f"), &value);
    strcpy (str_unit, "mW");
  }  
  else
  {
    ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%02_f"), &value);
    strcpy (str_unit, "W");
  }  
  WSContentSend_P (";%s;%s", str_value, str_unit);

  // end of data page
  WSContentEnd ();
}

// Voltage and current graph curve
void KoradWebGraphData ()
{
  bool     graph_valid, phase_display;
  int      index, phase, index_array;
  int      graph_period, graph_display, graph_height;  
  long     graph_left, graph_right, graph_width;  
  long     unit_width, shift_unit, shift_width;  
  long     graph_value, graph_delta, graph_x, graph_y; 
  long     graph_vmax, graph_imax; 
  float    value;
  TIME_T   current_dst;
  uint32_t current_time;
  char     str_data[16];
  char     str_phase[8];

  // boundaries of SVG graph
  graph_left   = KORAD_GRAPH_PERCENT_START * KORAD_GRAPH_WIDTH / 100;
  graph_right  = KORAD_GRAPH_PERCENT_STOP * KORAD_GRAPH_WIDTH / 100;
  graph_width  = graph_right - graph_left;
  graph_height = KORAD_GRAPH_HEIGHT;

  // calculate graph maximum scales
  value = korad_graph.vmax * 100;
  graph_vmax = (long)value;
  value = korad_graph.imax * 1000;
  graph_imax = (long)value;
  
  // start of SVG graph
  WSContentBegin (200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d'>\n"), 0, 0, KORAD_GRAPH_WIDTH, KORAD_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("polyline {fill:none;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.voltage {stroke:%s;}\n"), D_KORAD_COLOR_VOLTAGE);
  WSContentSend_P (PSTR ("polyline.current {stroke:%s;}\n"), D_KORAD_COLOR_CURRENT);
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:1.5rem;fill:grey;}\n"), str_data);
  WSContentSend_P (PSTR ("</style>\n"));

  // ------------------
  //   voltage curve
  // ------------------

  // start of polyline
  WSContentSend_P (PSTR ("<polyline class='voltage' points='"));

  // loop for the apparent power graph
  for (index = 0; index < KORAD_GRAPH_SAMPLE; index++)
  {
    // get current array index and value
    index_array = (index + korad_graph.index) % KORAD_GRAPH_SAMPLE;
    graph_value = (long)korad_graph.arr_voltage[index_array];

    // if voltage is defined, display point
    if (graph_value != UINT16_MAX)
    {
      graph_y = graph_height - (graph_value * graph_height / graph_vmax);
      graph_x = graph_left + (graph_width * index / KORAD_GRAPH_SAMPLE);
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
    }
  }

  // end of polyline
  WSContentSend_P (PSTR("'/>\n"));

  // ------------------
  //   current curve
  // ------------------

  // start of polyline
  WSContentSend_P (PSTR ("<polyline class='current' points='"));

  // loop for the apparent power graph
  for (index = 0; index < KORAD_GRAPH_SAMPLE; index++)
  {
    // get current array index and value
    index_array = (index + korad_graph.index) % KORAD_GRAPH_SAMPLE;
    graph_value = (long)korad_graph.arr_current[index_array];

    // if voltage is defined, display point
    if (graph_value != UINT16_MAX)
    {
      graph_y = graph_height - (graph_value * graph_height / graph_imax);
      graph_x = graph_left + (graph_width * index / KORAD_GRAPH_SAMPLE);
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
    }
  }

  // end of polyline
  WSContentSend_P (PSTR("'/>\n"));

  // --------------
  //   Time units
  // --------------

  // calculate horizontal shift for 1 mn
  unit_width  = graph_width / 15;

  // display 1 mn separation lines
  for (index = 1; index < 15; index++)
  {
    // display separation line and time
    graph_x = graph_left + (index * unit_width);
    WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd ();
}


// Graph frame
void KoradWebGraphFrame ()
{
  int  graph_left, graph_right, graph_width;
  float value;
  char str_value[12];

  // boundaries of SVG graph
  graph_left   = KORAD_GRAPH_PERCENT_START * KORAD_GRAPH_WIDTH / 100;
  graph_right  = KORAD_GRAPH_PERCENT_STOP * KORAD_GRAPH_WIDTH / 100;
  graph_width  = graph_right - graph_left;

  // start of SVG graph
  WSContentBegin (200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d'>\n"), 0, 0, KORAD_GRAPH_WIDTH, KORAD_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("text {font-size:1rem;}\n"));
  WSContentSend_P (PSTR ("text.voltage {stroke:%s;fill:%s;}\n"), D_KORAD_COLOR_VOLTAGE, D_KORAD_COLOR_VOLTAGE);
  WSContentSend_P (PSTR ("text.current {stroke:%s;fill:%s;}\n"), D_KORAD_COLOR_CURRENT, D_KORAD_COLOR_CURRENT);
  WSContentSend_P (PSTR ("</style>\n"));

  // graph frame
  WSContentSend_P (PSTR ("<rect x='%d%%' y='%d%%' width='%d%%' height='%d%%' rx='10' />\n"), KORAD_GRAPH_PERCENT_START, 0, KORAD_GRAPH_PERCENT_STOP - KORAD_GRAPH_PERCENT_START, 100);

  // graph separation lines
  WSContentSend_P (KORAD_HTML_DASH, KORAD_GRAPH_PERCENT_START, 25, KORAD_GRAPH_PERCENT_STOP, 25);
  WSContentSend_P (KORAD_HTML_DASH, KORAD_GRAPH_PERCENT_START, 50, KORAD_GRAPH_PERCENT_STOP, 50);
  WSContentSend_P (KORAD_HTML_DASH, KORAD_GRAPH_PERCENT_START, 75, KORAD_GRAPH_PERCENT_STOP, 75);

  // Voltage graduation
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f V"), &korad_graph.vmax);
  WSContentSend_P (KORAD_HTML_UNIT, "voltage", 1, 3, str_value);
  value = korad_graph.vmax * 0.75;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (KORAD_HTML_UNIT, "voltage", 1, 26, str_value);
  value = korad_graph.vmax * 0.5;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (KORAD_HTML_UNIT, "voltage", 1, 51, str_value);
  value = korad_graph.vmax * 0.25;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (KORAD_HTML_UNIT, "voltage", 1, 76, str_value);
  WSContentSend_P (KORAD_HTML_UNIT, "voltage", 1, 99, "0");

  // Current graduation
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f A"), &korad_graph.imax);
  WSContentSend_P (KORAD_HTML_UNIT, "current", KORAD_GRAPH_PERCENT_STOP + 2, 3, str_value);
  value = korad_graph.imax * 0.75;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (KORAD_HTML_UNIT, "current", KORAD_GRAPH_PERCENT_STOP + 2, 26, str_value);
  value = korad_graph.imax * 0.5;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (KORAD_HTML_UNIT, "current", KORAD_GRAPH_PERCENT_STOP + 2, 51, str_value);
  value = korad_graph.imax * 0.25;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (KORAD_HTML_UNIT, "current", KORAD_GRAPH_PERCENT_STOP + 2, 76, str_value);
  WSContentSend_P (KORAD_HTML_UNIT, "current", KORAD_GRAPH_PERCENT_STOP + 2, 99, "0");

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd ();
}

// Control public page
void KoradWebPageControl ()
{
  int   index;  
  float value;
  char  str_value[8];
  char  str_next[8];
  const char kKoradStandardVoltage[] = "3.3|5|9|12|15|24";
  const char kKoradStandardCurrent[] = "0.05|0.1|0.5|1|3";

  // check power change
  if (Webserver->hasArg (D_CMND_KORAD_POWER))
  {
    korad_status.out = !korad_status.out;
    KoradEnableOutput (korad_status.out);
  }

  // check over voltage protection change
  if (Webserver->hasArg (D_CMND_KORAD_OVP))
  {
    WebGetArg (D_CMND_KORAD_OVP, str_value, sizeof (str_value)); 
    korad_status.ovp = (strcasecmp (str_value, "on") == 0);
    KoradOverVoltageProtection (korad_status.ovp);
  }

  // check over current protection change
  if (Webserver->hasArg (D_CMND_KORAD_OCP))
  {
    WebGetArg (D_CMND_KORAD_OCP, str_value, sizeof (str_value)); 
    korad_status.ocp = (strcasecmp (str_value, "on") == 0);
    KoradOverCurrentProtection (korad_status.ocp);
  }

  // set output voltage
  if (Webserver->hasArg (D_CMND_KORAD_VSET))
  {
    WebGetArg (D_CMND_KORAD_VSET, str_value, sizeof (str_value)); 
    korad_status.vset = atof (str_value);
    KoradSetVoltage (korad_status.vset);
  }

  // set output current
  if (Webserver->hasArg (D_CMND_KORAD_ISET))
  {
    WebGetArg (D_CMND_KORAD_ISET, str_value, sizeof (str_value)); 
    korad_status.iset = atof (str_value);
    KoradSetCurrent (korad_status.iset);
  }

  // get graph maximum voltage scale
  if (Webserver->hasArg (D_CMND_KORAD_VMAX))
  {
    WebGetArg (D_CMND_KORAD_VMAX, str_value, sizeof (str_value)); 
    korad_graph.vmax = min ((float)KORAD_VOLTAGE_MAX, (float)atof (str_value));
  }

  // get graph maximum voltage scale
  if (Webserver->hasArg (D_CMND_KORAD_IMAX))
  {
    WebGetArg (D_CMND_KORAD_IMAX, str_value, sizeof (str_value)); 
    korad_graph.imax = min ((float)KORAD_CURRENT_MAX, (float)atof (str_value));
  }

  // beginning of form without authentification
  WSContentStart_P (D_KORAD_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));

  WSContentSend_P (PSTR ("function updateData()\n"));
  WSContentSend_P (PSTR ("{\n"));
  WSContentSend_P (PSTR (" httpUpd=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpUpd.onreadystatechange=function()\n"));
  WSContentSend_P (PSTR (" {\n"));
  WSContentSend_P (PSTR ("  if (httpUpd.responseText.length>0)\n"));
  WSContentSend_P (PSTR ("  {\n"));
  WSContentSend_P (PSTR ("   arr_value=httpUpd.responseText.split(';');\n"));
  WSContentSend_P (PSTR ("   document.getElementById('switch').className=arr_value[0];\n"));
  WSContentSend_P (PSTR ("   document.getElementById('vread').className='read '+arr_value[0];\n"));
  WSContentSend_P (PSTR ("   document.getElementById('iread').className='read '+arr_value[0];\n"));
  WSContentSend_P (PSTR ("   document.getElementById('wread').className='read '+arr_value[0];\n"));
  WSContentSend_P (PSTR ("   document.getElementById('vout').textContent=arr_value[1];\n"));
  WSContentSend_P (PSTR ("   document.getElementById('vunit').textContent=arr_value[2];\n"));
  WSContentSend_P (PSTR ("   document.getElementById('iout').textContent=arr_value[3];\n"));
  WSContentSend_P (PSTR ("   document.getElementById('iunit').textContent=arr_value[4];\n"));
  WSContentSend_P (PSTR ("   document.getElementById('wout').textContent=arr_value[5];\n"));
  WSContentSend_P (PSTR ("   document.getElementById('wunit').textContent=arr_value[6];\n"));
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR (" httpUpd.open('GET','%s',true);\n"), D_KORAD_PAGE_DATA_UPDATE);
  WSContentSend_P (PSTR (" httpUpd.send();\n"));
  WSContentSend_P (PSTR ("}\n"));

  WSContentSend_P (PSTR ("function updateGraph()\n"));
  WSContentSend_P (PSTR ("{\n"));
  WSContentSend_P (PSTR (" str_random=Math.floor(Math.random()*100000);\n"));
  WSContentSend_P (PSTR (" document.getElementById('data').data='%s?rnd='+str_random;\n"), D_KORAD_PAGE_GRAPH_DATA);
  WSContentSend_P (PSTR ("}\n"));

  WSContentSend_P (PSTR ("setInterval(function() {updateData();},500);\n"));
  WSContentSend_P (PSTR ("setInterval(function() {updateGraph();},2000);\n"));
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div.main {margin:0.5rem auto;padding:0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("div.title {font-size:3rem;font-weight:bold;margin-bottom:1rem;}\n"));
  WSContentSend_P (PSTR ("div.section {display:inline-block;text-align:center;width:18rem;margin:0.5rem 1rem;}\n"));

  // graph + and -
  WSContentSend_P (PSTR ("div.adjust {display:inline-block;width:38rem;margin:1rem 0 0 0;}\n"));
  WSContentSend_P (PSTR ("@media (max-width:675px) {div.adjust {width:18rem;}}\n"));
  WSContentSend_P (PSTR ("div.adjust div {display:inline-block;}\n"));
  WSContentSend_P (PSTR ("div.left {float:left;}\n"));
  WSContentSend_P (PSTR ("div.right {float:right;}\n"));
  WSContentSend_P (PSTR ("span.adjust {font-weight:bold;padding:0.1rem 0.5rem;margin:0rem 0.25rem;border:1px white solid;border-radius:8px;}\n"));

  WSContentSend_P (PSTR ("div.read {width:100%;padding:0px;margin:0.25rem auto;border-radius:8px;border:1px white solid;border-top:10px white solid;}\n"));
  WSContentSend_P (PSTR ("div.power {width:100%;padding:0px;margin:0.25rem auto;}\n"));
  WSContentSend_P (PSTR ("div.read span.value {font-size:4rem;}\n"));
  WSContentSend_P (PSTR ("div.off span.value {color:#666;}\n"));
  WSContentSend_P (PSTR ("div.read span.unit {font-size:2rem;margin-left:1rem;margin-right:-4rem;}\n"));
  WSContentSend_P (PSTR ("div.read span.watt {margin-right:0rem;}\n"));

  WSContentSend_P (PSTR ("div.off span.unit {color:#666;}\n"));
  WSContentSend_P (PSTR ("div.read span.protect {float:right;color:#666;width:2rem;padding:0.1rem 0.5rem;margin:0.25rem 0.5rem;border-radius:12px;}\n"));
  WSContentSend_P (PSTR ("div.set {width:100%;font-size:1rem;padding:0.1rem;margin:0.5rem auto;}"));
  WSContentSend_P (PSTR ("div.set span {margin:0rem 0.5rem;}\n"));
  WSContentSend_P (PSTR ("div.set span.set {font-size:0.75rem;padding:0.2rem 0.3rem;margin:0.2rem;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.set span.delta {font-size:1rem;margin:0px;padding:0rem 0.5rem;background:none;border:1px #666 dashed;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  WSContentSend_P (PSTR (".volt div {color:%s;border-color:%s;}\n"), D_KORAD_COLOR_VOLTAGE, D_KORAD_COLOR_VOLTAGE);
  WSContentSend_P (PSTR (".volt span {color:%s;border-color:%s;}\n"), D_KORAD_COLOR_VOLTAGE, D_KORAD_COLOR_VOLTAGE);
  WSContentSend_P (PSTR (".volt span.on {background:%s;}\n"), D_KORAD_COLOR_VOLTAGE);
  WSContentSend_P (PSTR (".volt span.off {color:%s;background:none;}\n"), D_KORAD_COLOR_VOLTAGE);
  WSContentSend_P (PSTR (".volt span.protect {border:1px %s solid;}\n"), D_KORAD_COLOR_VOLTAGE);
  WSContentSend_P (PSTR (".volt span.set {border:1px %s solid;}\n"), D_KORAD_COLOR_VOLTAGE);
  WSContentSend_P (PSTR (".volt span.delta {color:%s;}\n"), D_KORAD_COLOR_VOLTAGE);
  WSContentSend_P (PSTR (".volt read.on span.value {color:%s;}\n"), D_KORAD_COLOR_VOLTAGE);

  WSContentSend_P (PSTR (".amp div {color:%s;border-color:%s;}\n"), D_KORAD_COLOR_CURRENT, D_KORAD_COLOR_CURRENT);
  WSContentSend_P (PSTR (".amp span {color:%s;border-color:%s;}\n"), D_KORAD_COLOR_CURRENT, D_KORAD_COLOR_CURRENT);
  WSContentSend_P (PSTR (".amp span.on {background:%s;}\n"), D_KORAD_COLOR_CURRENT);
  WSContentSend_P (PSTR (".amp span.off {color:%s;background:none;}\n"), D_KORAD_COLOR_CURRENT);
  WSContentSend_P (PSTR (".amp span.protect {border:1px %s solid;}\n"), D_KORAD_COLOR_CURRENT);
  WSContentSend_P (PSTR (".amp span.set {border:1px %s solid;}\n"), D_KORAD_COLOR_CURRENT);
  WSContentSend_P (PSTR (".amp span.delta {color:%s;}\n"), D_KORAD_COLOR_CURRENT);
  WSContentSend_P (PSTR (".amp read.on span.value {color:%s;}\n"), D_KORAD_COLOR_CURRENT);

  WSContentSend_P (PSTR (".watt div {color:%s;border-color:%s;}\n"), D_KORAD_COLOR_POWER, D_KORAD_COLOR_POWER);
  WSContentSend_P (PSTR (".watt span {color:%s;border-color:%s;}\n"), D_KORAD_COLOR_POWER, D_KORAD_COLOR_POWER);
  WSContentSend_P (PSTR (".watt read.on span.value {color:%s;}\n"), D_KORAD_COLOR_POWER);

  WSContentSend_P (PSTR (".svg-container {position:relative;max-width:900px;padding-bottom:%dpx;margin:0rem auto;}\n"), KORAD_GRAPH_HEIGHT + 25);
  WSContentSend_P (PSTR (".svg-content {display:inline-block;position:absolute;top:0;left:0;}\n"));

  WSContentSend_P (PSTR ("#switch {width:80px;height:80px;border-radius:40px;padding:0;margin:0rem auto -1.5rem auto;border:1px solid #aa0000;position:relative;}\n"));
  WSContentSend_P (PSTR ("#switch.on {border:1px solid #2faa00;}\n"));
  WSContentSend_P (PSTR ("#switch-ring {position:absolute;width:40px;height:40px;border:10px solid red;top:10px;left:10px;border-radius:31px;}\n"));
  WSContentSend_P (PSTR (".on #switch-ring {border-color:#46ff00;}\n"));
  WSContentSend_P (PSTR ("#switch-line {width:10px;height:30px;margin:-14px auto;background:red;border-radius:7px;border-right:3px solid black;border-left:3px solid black;}\n"));
  WSContentSend_P (PSTR (".on #switch-line {background:#46ff00;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>"));

  // device name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));

  // -----------------
  //   voltage meter
  // -----------------
  WSContentSend_P (PSTR ("<div class='section volt'>\n"));

  if (korad_status.out) strcpy (str_value, "on"); else strcpy (str_value, "off");
  WSContentSend_P (PSTR ("<div class='read %s' id='vread'>\n"), str_value);

  if (korad_status.ovp) { strcpy (str_value, "on"); strcpy (str_next, "off"); }
    else { strcpy (str_value, "off"); strcpy (str_next, "on"); }
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='protect %s' id='ovp'>OVP</span></a>\n"), D_CMND_KORAD_OVP, str_next, str_value);

  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%02_f"), &korad_status.vout);
  WSContentSend_P (PSTR ("<span class='value' id='vout'>%s</span>\n"), str_value);

  WSContentSend_P (PSTR ("<span class='unit' id='vunit'>V</span>\n"));
  WSContentSend_P (PSTR ("</div>\n"));    // read

  WSContentSend_P (PSTR ("<div class='set'>\n"));
  for (index = 0; index < 6; index ++)
  {
    GetTextIndexed (str_value, sizeof (str_value), index, kKoradStandardVoltage);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='set'>%s V</span></a>\n"), D_CMND_KORAD_VSET, str_value, str_value);
  }
  WSContentSend_P (PSTR ("</div>\n"));    // set

  WSContentSend_P (PSTR ("<div class='set'>\n"));
  if (korad_status.vset >= 0.1) value = korad_status.vset - 0.1; else value = 0;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='delta' title='%s V (-100 mV)'><</span></a>\n"), D_CMND_KORAD_VSET, str_value, str_value);
  if (korad_status.vset >= 1) value = korad_status.vset - 1; else value = 0;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='delta' title='%s V (-1 V)'><<</span></a>\n"), D_CMND_KORAD_VSET, str_value, str_value);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &korad_status.vset);
  WSContentSend_P (PSTR ("<span class='ref'>%s <small>V</small></span>\n"), str_value);
  if (korad_status.vset <= 29) value = korad_status.vset + 1; else value = 30;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='delta' title='%s V (+1 V)'>>></span></a>\n"), D_CMND_KORAD_VSET, str_value, str_value);
  if (korad_status.vset <= 29.9) value = korad_status.vset + 0.1; else value = 30;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='delta' title='%s V (+100 mV)'>></span></a>\n"), D_CMND_KORAD_VSET, str_value, str_value);
  WSContentSend_P (PSTR ("</div>\n"));      // set

  WSContentSend_P (PSTR ("</div>\n"));      // section volt

  // -----------------
  //   current meter
  // -----------------
  WSContentSend_P (PSTR ("<div class='section amp'>\n"));

  if (korad_status.out) strcpy (str_value, "on"); else strcpy (str_value, "off");
  WSContentSend_P (PSTR ("<div class='read %s' id='iread'>\n"), str_value);

  if (korad_status.ocp) { strcpy (str_value, "on"); strcpy (str_next, "off"); }
    else { strcpy (str_value, "off"); strcpy (str_next, "on"); }
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='protect %s' id='ocp'>OCP</span></a>\n"), D_CMND_KORAD_OCP, str_next, str_value);

  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%03_f"), &korad_status.iout);
  WSContentSend_P (PSTR ("<span class='value' id='iout'>%s</span>\n"), str_value);

  WSContentSend_P (PSTR ("<span class='unit' id='iunit'>A</span>\n"));
  WSContentSend_P (PSTR ("</div>\n"));    // read

  WSContentSend_P (PSTR ("<div class='set'>\n"));
  for (index = 0; index < 6; index ++)
  {
    GetTextIndexed (str_value, sizeof (str_value), index, kKoradStandardCurrent);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='set'>%s A</span></a>\n"), D_CMND_KORAD_ISET, str_value, str_value);
  }
  WSContentSend_P (PSTR ("</div>\n"));    // set

  WSContentSend_P (PSTR ("<div class='set'>\n"));
  if (korad_status.iset >= 0.1) value = korad_status.iset - 0.1; else value = 0;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='delta' title='%s A (-100 mA)'><</span></a>\n"), D_CMND_KORAD_ISET, str_value, str_value);
  if (korad_status.iset >= 1) value = korad_status.iset - 1; else value = 0;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='delta' title='%s A (-1 A)'><<</span></a>\n"), D_CMND_KORAD_ISET, str_value, str_value);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &korad_status.iset);
  WSContentSend_P (PSTR ("<span class='ref'>%s <small>A</small></span>\n"), str_value);
  if (korad_status.iset <= 2) value = korad_status.iset + 1; else value = 3;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='delta' title='%s A (+1 A)'>>></span></a>\n"), D_CMND_KORAD_ISET, str_value, str_value);
  if (korad_status.iset <= 2.9) value = korad_status.iset + 0.1; else value = 3;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='delta' title='%s A (+100 mA)'>></span></a>\n"), D_CMND_KORAD_ISET, str_value, str_value);
  WSContentSend_P (PSTR ("</div>\n"));    // set

  WSContentSend_P (PSTR ("</div>\n"));    // section amp

  // line break
  WSContentSend_P (PSTR ("<br>\n"));

  // ---------------
  //   power meter
  // ---------------
  WSContentSend_P (PSTR ("<div class='section watt'>\n"));

  if (korad_status.out) strcpy (str_value, "on"); else strcpy (str_value, "off");
  WSContentSend_P (PSTR ("<div class='read %s' id='wread'>\n"), str_value);

  value = korad_status.vout * korad_status.iout;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%02_f"), &value);
  WSContentSend_P (PSTR ("<span class='value' id='wout'>%s</span>\n"), str_value);

  WSContentSend_P (PSTR ("<span class='unit watt' id='wunit'>W</span>\n"));
  WSContentSend_P (PSTR ("</div>\n"));    // read
  WSContentSend_P (PSTR ("</div>\n"));    // section

  // -----------------
  //   switch button
  // -----------------
  WSContentSend_P (PSTR ("<div class='section'>\n"));
  WSContentSend_P (PSTR ("<div class='power'>\n"));

  if (korad_status.out) strcpy (str_value, "on"); else strcpy (str_value, "off");
  WSContentSend_P (PSTR ("<a href='/ctrl?%s'><div id='switch' class='%s'><div id='switch-ring'><div id='switch-line'></div></div></div></a>\n"), D_CMND_KORAD_POWER, str_value);

  WSContentSend_P (PSTR ("</div>\n"));    // power
  WSContentSend_P (PSTR ("</div>\n"));    // section

  // line break
  WSContentSend_P (PSTR ("<br>\n"));


  // -----------------
  //      graph
  // -----------------

  // + / - buttons
  WSContentSend_P (PSTR ("<div class='adjust'>\n"));

  WSContentSend_P (PSTR ("<div class='volt left'>"));
  value = korad_graph.vmax / 2;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%02_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='adjust'>-</span></a>"), D_CMND_KORAD_VMAX, str_value);
  value = korad_graph.vmax * 2;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%02_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='adjust'>+</span></a>"), D_CMND_KORAD_VMAX, str_value);
  WSContentSend_P (PSTR ("</div>\n"));      // volt left

  WSContentSend_P (PSTR ("<div class='amp right'>"));
  value = korad_graph.imax / 2;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%02_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='adjust'>-</span></a>"), D_CMND_KORAD_IMAX, str_value);
  value = min ((float)KORAD_VOLTAGE_MAX, korad_graph.imax * 2);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%02_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='adjust'>+</span></a>"), D_CMND_KORAD_IMAX, str_value);
  WSContentSend_P (PSTR ("</div>\n"));      // amp right
 
  WSContentSend_P (PSTR ("</div>\n"));      // adjust
  WSContentSend_P (PSTR ("</div>\n"));

  // graph frame and data
  WSContentSend_P (PSTR ("<div class='svg-container'>\n"));
  WSContentSend_P (PSTR ("<object class='svg-content' id='base' type='image/svg+xml' width='100%%' height='%dpx' data='%s'></object>\n"), KORAD_GRAPH_HEIGHT, D_KORAD_PAGE_GRAPH_FRAME);
  WSContentSend_P (PSTR ("<object class='svg-content' id='data' type='image/svg+xml' width='100%%' height='%dpx' data='%s?ts=0'></object>\n"), KORAD_GRAPH_HEIGHT, D_KORAD_PAGE_GRAPH_DATA);
  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/************************************\
 *              Interface
\************************************/

// Korad power supply KAxxxxP driver
bool Xdrv96 (uint8_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_PRE_INIT:
      KoradInit ();
      break;
    case FUNC_COMMAND:
      result = KoradMqttCommand ();
      break;
    case FUNC_JSON_APPEND:
      KoradShowJSON (true);
      break;
    case FUNC_EVERY_50_MSECOND:
      if (TasmotaGlobal.uptime > KORAD_INIT_TIMEOUT) KoradEvery50ms ();
      break;
    case FUNC_EVERY_100_MSECOND:
      if (TasmotaGlobal.uptime > KORAD_INIT_TIMEOUT) KoradEvery100ms ();
      break;
    case FUNC_EVERY_SECOND:
      if (TasmotaGlobal.uptime > KORAD_INIT_TIMEOUT) KoradEverySecond ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      KoradWebSensor ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      KoradWebMainButton ();
      break;
    case FUNC_WEB_ADD_HANDLER:
      // graph
      Webserver->on (FPSTR (D_KORAD_PAGE_CONTROL),      KoradWebPageControl);
      Webserver->on (FPSTR (D_KORAD_PAGE_GRAPH_FRAME),  KoradWebGraphFrame);
      Webserver->on (FPSTR (D_KORAD_PAGE_GRAPH_DATA),   KoradWebGraphData);
      Webserver->on (FPSTR (D_KORAD_PAGE_DATA_UPDATE), KoradWebDataUpdate);
      break;
#endif  // USE_WEBSERVER

  }

  return result;
}

#endif   // USE_KORAD

