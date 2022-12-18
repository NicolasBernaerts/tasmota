/*
  xdrv_96_korad.ino - Driver for KORAD power supplies KAxxxxP
  Tested on KA3005P
  Copyright (C) 2020  Nicolas Bernaerts

  History :
    02/05/2021 - v1.0 - Creation
    20/09/2021 - v1.1 - Add LittleFS support
                        Save configuration to korad.cfg
                        Allow realtime data recording to CSV file
    15/12/2022 - v1.2 - Rewrite serial communication management

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

// declare korad driver and sensor
#define XDRV_96                   96

// constants
#define KORAD_VOLTAGE_MAX         30        // maximum voltage
#define KORAD_CURRENT_MAX         3         // maximum current
#define KORAD_PRESET_VOLTAGE_MAX  4         // maximum number of predefined voltage
#define KORAD_PRESET_CURRENT_MAX  4         // maximum number of predefined max current

#define KORAD_INIT_TIMEOUT        5         // timeout before receiving data (sec.)
#define KORAD_SEND_QUEUE_MAX      8         // maximum number of messages to send
#define KORAD_SEND_QUEUE_LIMIT    6         // limit maximum number of messages to send
#define KORAD_SEND_BUFFER_MAX     16        // maximum size of a emission buffer
#define KORAD_RECV_BUFFER_MAX     64        // maximum size of a reception buffer

#define KORAD_GRAPH_SAMPLE        1200       // number of samples in graph (2mn with one sample every 100ms)
#define KORAD_GRAPH_HEIGHT        500        // height of graph in pixels
#define KORAD_GRAPH_WIDTH         1200       // width of graph in pixels
#define KORAD_GRAPH_PERCENT_START 10     
#define KORAD_GRAPH_PERCENT_STOP  90

#define KORAD_UNIT_SIZE           4
#define KORAD_VALUE_SIZE          8

// commands
#define D_CMND_KORAD_STATUS       "status"
#define D_CMND_KORAD_HELP         "help"
#define D_CMND_KORAD_USER         "user"
#define D_CMND_KORAD_POWER        "power"
#define D_CMND_KORAD_IDN          "idn"
#define D_CMND_KORAD_VCONF        "vconf"
#define D_CMND_KORAD_RECORD       "rec"
#define D_CMND_KORAD_VSET         "vset"
#define D_CMND_KORAD_ISET         "iset"
#define D_CMND_KORAD_VOUT         "vout"
#define D_CMND_KORAD_IOUT         "iout"
#define D_CMND_KORAD_OVP          "ovp"
#define D_CMND_KORAD_OCP          "ocp"
#define D_CMND_KORAD_VMAX         "vmax"
#define D_CMND_KORAD_IMAX         "imax"
#define D_CMND_KORAD_HEIGHT       "height"
#define D_CMND_KORAD_VOLT         "volt"
#define D_CMND_KORAD_AMP          "amp"

#define D_KORAD                   "Korad"
#define D_KORAD_MODEL             "Model"
#define D_KORAD_VERSION           "Version"
#define D_KORAD_UNKNOWN           "Unknown"
#define D_KORAD_VOLTAGE           "Voltage"
#define D_KORAD_CURRENT           "Current"
#define D_KORAD_REFERENCE         "Ref"
#define D_KORAD_OUTPUT            "Output"
#define D_KORAD_CONTROL           "Control"
#define D_KORAD_GRAPH             "Graph"
#define D_KORAD_CONFIGURE         "Configure"
#define D_KORAD_POWERSUPPLY       "Power Supply"

// filesystem
#define D_KORAD_CFG               "/korad.cfg"

#define D_CMND_KORAD_DATA         "data"

// form strings
const char KORAD_FIELD_START[]  PROGMEM = "<p><fieldset><legend><b> Predefined %s </b></legend>\n";
const char KORAD_FIELD_STOP[]   PROGMEM = "</fieldset></p><br>\n";
const char KORAD_INPUT_NUMBER[] PROGMEM = "<span class='half'>%s %d<br><input type='number' name='%s%d' min='%d' max='%d' step='%s' value='%s'></span>\n";
const char KORAD_HTML_UNIT[]    PROGMEM = "<text class='%s' x='%d%%' y='%d%%'>%s %s</text>\n";
const char KORAD_HTML_DASH[]    PROGMEM = "<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n";
const char KORAD_VOLT_CLICK2[]  PROGMEM = "Click twice to set output voltage";
const char KORAD_VOLT_CLICK[]   PROGMEM = "Set output voltage";
const char KORAD_AMP_CLICK[]    PROGMEM = "Set max output current";

// web colors
const char D_KORAD_COLOR_BACKGROUND[]  PROGMEM = "#252525";
const char D_KORAD_COLOR_VOLTAGE[]     PROGMEM = "yellow";
const char D_KORAD_COLOR_VOLTAGE_OFF[] PROGMEM = "#606030";
const char D_KORAD_COLOR_CURRENT[]     PROGMEM = "#6bc4ff";
const char D_KORAD_COLOR_CURRENT_OFF[] PROGMEM = "#3b6d8e";
const char D_KORAD_COLOR_WATT[]        PROGMEM = "white";
const char D_KORAD_COLOR_WATT_OFF[]    PROGMEM = "grey";
const char D_KORAD_COLOR_POWER[]       PROGMEM = "limegreen";
const char D_KORAD_COLOR_POWER_OFF[]   PROGMEM = "red";
const char D_KORAD_COLOR_RECORD[]      PROGMEM = "red";
const char D_KORAD_COLOR_RECORD_OFF[]  PROGMEM = "#800";

// web URL
const char D_KORAD_PAGE_CONFIG[]      PROGMEM = "/config";
const char D_KORAD_PAGE_CONTROL[]     PROGMEM = "/ctrl";
const char D_KORAD_PAGE_DATA_UPDATE[] PROGMEM = "/data.upd";
const char D_KORAD_PAGE_GRAPH_FRAME[] PROGMEM = "/frame.svg";
const char D_KORAD_PAGE_GRAPH_DATA[]  PROGMEM = "/curve.svg";

// power supply units
enum KoradUnit { KORAD_UNIT_V, KORAD_UNIT_A, KORAD_UNIT_W };

// power supply modes
enum KoradMode                    { KORAD_CC, KORAD_CV };
const char kKoradMode[] PROGMEM = "Current|Voltage";

// power supply MQTT commands
const char kKoradMqttCommands[] PROGMEM = "ps_" "|" D_CMND_KORAD_HELP "|" D_CMND_KORAD_USER "|" D_CMND_KORAD_POWER "|" D_CMND_KORAD_IDN "|" D_CMND_KORAD_VSET "|" D_CMND_KORAD_ISET "|" D_CMND_KORAD_OVP "|" D_CMND_KORAD_OCP "|" D_CMND_KORAD_RECORD;
void (* const arrKoradMqttCommand[])(void) PROGMEM = { &KoradCommandHelp, &KoradCommandUser, &KoradCommandPower, &KoradCommandIDN, &KoradCommandVSET, &KoradCommandISET, &KoradCommandOVP, &KoradCommandOCP, &KoradCommandRecord };

// power supply commands
enum KoradCommand { KORAD_COMMAND_USER, KORAD_COMMAND_IDN, KORAD_COMMAND_STATUS, KORAD_COMMAND_VREF_GET, KORAD_COMMAND_VREF_SET, KORAD_COMMAND_VOUT_GET, KORAD_COMMAND_IREF_GET, KORAD_COMMAND_IREF_SET, KORAD_COMMAND_IOUT_GET, KORAD_COMMAND_OUT_ON, KORAD_COMMAND_OUT_OFF, KORAD_COMMAND_OVP_ON, KORAD_COMMAND_OVP_OFF, KORAD_COMMAND_OCP_ON, KORAD_COMMAND_OCP_OFF, KORAD_COMMAND_TRACK0, KORAD_COMMAND_RCL, KORAD_COMMAND_SAV, KORAD_COMMAND_NONE };
const char kKoradCommand[] PROGMEM = "  |*IDN?|STATUS?|VSET1?|VSET1:|VOUT1?|ISET1?|ISET1:|IOUT1?|OUT1|OUT0|OVP1|OVP0|OCP1|OCP0|TRACK0|RCL|SAV";

// power supply status
static struct {
  float preset_vset[KORAD_PRESET_VOLTAGE_MAX];      // preset voltage
  float preset_iset[KORAD_PRESET_CURRENT_MAX];      // preset current
} korad_config;

static struct {
  bool     out    = false;                      // output status
  bool     ovp    = false;                      // over voltage protection status
  bool     ocp    = false;                      // over current protection status
  float    vconf  = 0;                          // voltage value needing confirmation stage
  float    vset   = 0;                          // voltage value set
  float    iset   = 0;                          // max current value set
  float    vout   = 0;                          // actual voltage output
  float    iout   = 0;                          // actual current output
  uint8_t  mode   = 0;                          // voltage or current mode
  uint8_t  last_cmnd = KORAD_COMMAND_NONE;      // currently handled command
  uint8_t  last_recv = 0;                       // previously received number of caracters
  uint32_t time_over = 0;                       // timestamp of end of voltage change command
  char  str_brand[16];                          // power supply brand
  char  str_model[16];                          // power supply model
  char  str_version[16];                        // power supply firmware version
  char  str_serial[16];                         // power supply serial number
  char  str_buffer[KORAD_RECV_BUFFER_MAX];      // reception buffer
} korad_status;

static struct {
  uint32_t start_ms   = 0;                      // timestamp when current recording started
  uint32_t start_time = 0;                      // timestamp when current recording started
  File     file;                                // handle of recorded file
  String   str_buffer;                          // recording buffer
} korad_record;

// commands handling
static struct {
  uint8_t type  = KORAD_COMMAND_IDN;
  float   value = NAN; 
  char    str_send[KORAD_SEND_BUFFER_MAX];      // reception buffer
} korad_command;

// graph
static struct {
  int   index;                                   // current array index per refresh period
  int   height;                                  // height of graph in pixels
  float vmax;                                    // maximum voltage scale
  float imax;                                    // maximum current scale
  uint16_t arr_voltage[KORAD_GRAPH_SAMPLE];      // array of instant voltage (x 100)
  uint16_t arr_current[KORAD_GRAPH_SAMPLE];      // array of instant current (x 1000)
} korad_graph; 


/***************************************\
 *             Korad commands
\***************************************/

void KoradGetIDN ()
{
  // set command parameters
  korad_command.type = KORAD_COMMAND_IDN;
  korad_command.value = NAN;
  GetTextIndexed (korad_command.str_send, sizeof (korad_command.str_send), korad_command.type, kKoradCommand);
}

void KoradSetPower (bool enable)
{
  // set command parameters
  if (enable) korad_command.type = KORAD_COMMAND_OUT_ON; else korad_command.type = KORAD_COMMAND_OUT_OFF;
  korad_command.value = NAN;
  GetTextIndexed (korad_command.str_send, sizeof (korad_command.str_send), korad_command.type, kKoradCommand);
}

void KoradOverVoltageProtection (bool enable)
{
  // set command parameters
  if (enable) korad_command.type = KORAD_COMMAND_OVP_ON; else korad_command.type = KORAD_COMMAND_OVP_OFF;
  korad_command.value = NAN;
  GetTextIndexed (korad_command.str_send, sizeof (korad_command.str_send), korad_command.type, kKoradCommand);
}

void KoradOverCurrentProtection (bool enable)
{
  // set command parameters
  if (enable) korad_command.type = KORAD_COMMAND_OCP_ON; else korad_command.type = KORAD_COMMAND_OCP_OFF;
  korad_command.value = NAN;
  GetTextIndexed (korad_command.str_send, sizeof (korad_command.str_send), korad_command.type, kKoradCommand);
}

void KoradSetVoltage (float voltage)
{
  char str_value[8];

  // check parameter
  if (isnan (voltage)) return;

  // set command parameters
  korad_command.type  = KORAD_COMMAND_VREF_SET;
  korad_command.value = voltage;

  // calculate voltage string
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%02_f"), &voltage);
  while (strlen (str_value) < 5) strcat (str_value, "0");

  // generate command string
  GetTextIndexed (korad_command.str_send, sizeof (korad_command.str_send), korad_command.type, kKoradCommand);
  strlcat (korad_command.str_send, str_value, sizeof (korad_command.str_send));
}

void KoradSetCurrent (float current)
{
  char str_value[8];

  // check parameter
  if (isnan (current)) return;

  // set command parameters
  korad_command.type  = KORAD_COMMAND_IREF_SET;
  korad_command.value = current;

  // calculate current string
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%03_f"), &current);
  while (strlen (str_value) < 5) strcat (str_value, "0");

  // generate command string
  GetTextIndexed (korad_command.str_send, sizeof (korad_command.str_send), korad_command.type, kKoradCommand);
  strlcat (korad_command.str_send, str_value, sizeof (korad_command.str_send));
}

void KoradSetUserCommand (const char* pstr_command)
{
  // check parameter
  if (pstr_command == nullptr) return;

  // set command parameters
  korad_command.type = KORAD_COMMAND_USER;
  korad_command.value = NAN;
  strlcpy (korad_command.str_send, pstr_command, sizeof (korad_command.str_send));
}

/*********************************************\
 *                MQTT Commands
\*********************************************/

void KoradCommandHelp ()
{
  AddLog (LOG_LEVEL_INFO,   PSTR ("HLP: TIC commands :"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_user <command>  = send a specific command to the power supply"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_power <on/off>  = enable power supply"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_idn             = ask for device model"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_vset <voltage>  = set output voltage"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_iset <current>  = set output max current"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_ovp <on/off>    = enable over voltage protection"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_ocp <on/off>    = enable over current protection"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_record <on/off> = start/stop recording"));

  ResponseCmndDone();
}

void KoradCommandUser ()
{
  if (strlen (XdrvMailbox.data) > 0) KoradSetUserCommand (XdrvMailbox.data);
  ResponseCmndDone ();
}

void KoradCommandPower ()
{
  bool result = ((strcasecmp (XdrvMailbox.data, "on") == 0) || (XdrvMailbox.payload == 1));

  KoradSetPower (result);
  ResponseCmndNumber ((int)result);
}

void KoradCommandIDN ()
{
  KoradGetIDN ();
  ResponseCmndDone ();
}

void KoradCommandVSET ()
{
  float value = atof (XdrvMailbox.data);

  KoradSetVoltage (value);
  ResponseCmndFloat (value, 2);
}

void KoradCommandISET ()
{
  float value = atof (XdrvMailbox.data);

  KoradSetCurrent (value);
  ResponseCmndFloat (value, 3);
}

void KoradCommandOVP ()
{
  bool result = ((strcasecmp (XdrvMailbox.data, "on") == 0) || (XdrvMailbox.payload == 1));

  KoradOverVoltageProtection (result);
  ResponseCmndNumber ((int)result);
}

void KoradCommandOCP ()
{
  bool result = ((strcasecmp (XdrvMailbox.data, "on") == 0) || (XdrvMailbox.payload == 1));

  KoradOverCurrentProtection (result);
  ResponseCmndNumber ((int)result);
}

void KoradCommandRecord ()
{
  bool result = ((strcasecmp (XdrvMailbox.data, "on") == 0) || (XdrvMailbox.payload == 1));

  if (result) KoradRecordStart (); else KoradRecordStop ();
  ResponseCmndDone ();
}

/***************************************\
 *               Functions
\***************************************/

void KoradLoadConfig () 
{
  int  index;
  char str_key[8];

  // retrieve saved settings from flash filesystem
  for (index = 0; index < KORAD_PRESET_VOLTAGE_MAX; index++)
  {
    sprintf (str_key, "%s%d", D_CMND_KORAD_VSET, index + 1);
    korad_config.preset_vset[index] = UfsCfgLoadKeyFloat (D_KORAD_CFG, str_key, 5);
  }
  for (index = 0; index < KORAD_PRESET_CURRENT_MAX; index++)
  {
    sprintf (str_key, "%s%d", D_CMND_KORAD_ISET, index + 1);
    korad_config.preset_iset[index] = UfsCfgLoadKeyFloat (D_KORAD_CFG, str_key, 1);
  }

  // load graph limits
  korad_graph.vmax = UfsCfgLoadKeyFloat (D_KORAD_CFG, D_CMND_KORAD_VMAX, KORAD_VOLTAGE_MAX);
  korad_graph.imax = UfsCfgLoadKeyFloat (D_KORAD_CFG, D_CMND_KORAD_IMAX, KORAD_CURRENT_MAX);
}

void KoradSaveConfig () 
{
  bool create;
  int  index;
  char str_key[8];

  // save settings to flash filesystem
  for (index = 0; index < KORAD_PRESET_VOLTAGE_MAX; index++)
  {
    create = (index == 0);
    sprintf (str_key, "%s%d", D_CMND_KORAD_VSET, index + 1);
    UfsCfgSaveKeyFloat (D_KORAD_CFG, str_key, korad_config.preset_vset[index], create);
  }
  for (index = 0; index < KORAD_PRESET_CURRENT_MAX; index++)
  {
    sprintf (str_key, "%s%d", D_CMND_KORAD_ISET, index + 1);
    UfsCfgSaveKeyFloat (D_KORAD_CFG, str_key, korad_config.preset_iset[index], false);
  }

  // save graph limits
  UfsCfgSaveKeyFloat (D_KORAD_CFG, D_CMND_KORAD_VMAX, korad_graph.vmax, false);
  UfsCfgSaveKeyFloat (D_KORAD_CFG, D_CMND_KORAD_IMAX, korad_graph.imax, false);
}

void KoradRecordStart ()
{
  TIME_T dst_start;
  char   str_filename[UFS_FILENAME_SIZE];

  if (korad_record.start_time == 0)
  {
    // save recording start time
    korad_record.start_ms   = millis ();
    korad_record.start_time = LocalTime ();

    // calculate current timestamp
    BreakTime (korad_record.start_time, dst_start);
    sprintf (str_filename, "/rec-%04u%02u%02u-%02u%02u%02u.csv", dst_start.year + 1970, dst_start.month, dst_start.day_of_month, dst_start.hour, dst_start.minute, dst_start.second);

    // record header
    korad_record.file = ffsp->open (str_filename, "w");
    korad_record.str_buffer = "Msec;VSet;Vout;ISet;Iout\n";

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("KPS: Start recording in %s"), str_filename);
  }
}

void KoradRecordStop ()
{
  if (korad_record.start_time != 0)
  {
    // if buffer is not empty, record it
    if (korad_record.str_buffer.length () > 0) korad_record.file.print (korad_record.str_buffer.c_str ());

    // reset recording data and close file
    korad_record.str_buffer = "";
    korad_record.start_time = 0;
    korad_record.start_ms   = 0;
    korad_record.file.close ();

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("KPS: Stop recording"));
  }
}

void KoradRecordStream ()
{
  char str_line[32];

  // if recording
  if (korad_record.start_time != 0)
  {
    // save data as "millis;vset,vout,iset;iout"
    ext_snprintf_P (str_line, sizeof (str_line), PSTR ("%u;%2_f;%2_f;%3_f;%3_f\n"), millis () - korad_record.start_ms, &korad_status.vset, &korad_status.vout, &korad_status.iset, &korad_status.iout);

    // append to recording buffer
    korad_record.str_buffer += str_line;

    // if buffer is full, record it
    if (korad_record.str_buffer.length () > 4000)
    {
      korad_record.file.print (korad_record.str_buffer.c_str ());
      korad_record.str_buffer = "";
    }
  }
}

void KoradGetRecordingDuration (char *pstr_time, size_t size_time)
{
  uint32_t duration;
  TIME_T   duration_dst;
  char     str_time[16];

  // init
  strcpy (pstr_time, "");

  // if recording has started
  if (korad_record.start_time != 0)
  {
    // calculate current timestamp
    duration = LocalTime () - korad_record.start_time;
    BreakTime (duration, duration_dst);

    // generate time string
    if (duration_dst.hour > 0) sprintf (pstr_time, "%u:", duration_dst.hour);
    sprintf (str_time, "%02u:%02u", duration_dst.minute, duration_dst.second);
    strlcat (pstr_time, str_time, size_time);
  }
}

void KoradGenerateValueLabel (char* pstr_value, char* pstr_unit, float value, int num_digit, int unit)
{
  bool carry_on;
  int  index;
  char str_digit[8];

  // check parameters
  if ((pstr_value == nullptr) || (pstr_unit == nullptr)) return;

  // init
  strcpy (pstr_unit, "");

  // generate label in case of millis
  if (value < 1)
  {
    value = value * 1000;
    ext_snprintf_P (pstr_value, KORAD_VALUE_SIZE, PSTR ("%0_f"), &value);
    strlcpy (pstr_unit, "m", KORAD_UNIT_SIZE);
  }

  // else, if precision is given, generate normal label with given precision
  else if (num_digit > 0)
  {
    sprintf (str_digit, "%%0%d_f", num_digit);
    ext_snprintf_P (pstr_value, KORAD_VALUE_SIZE, str_digit, &value);
  }

  // else, trim value on the right
  else if (value < 10) ext_snprintf_P (pstr_value, KORAD_VALUE_SIZE, PSTR ("%02_f"), &value);

  // else, trim value on the right
  else if (value < 100) ext_snprintf_P (pstr_value, KORAD_VALUE_SIZE, PSTR ("%01_f"), &value);

  // else, trim value on the right
  else ext_snprintf_P (pstr_value, KORAD_VALUE_SIZE, PSTR ("%0_f"), &value);

  // add unit
  switch (unit)
  {
    case KORAD_UNIT_V:
      strlcat (pstr_unit, "V", KORAD_UNIT_SIZE);
      break;
    case KORAD_UNIT_A:
      strlcat (pstr_unit, "A", KORAD_UNIT_SIZE);
      break;
    case KORAD_UNIT_W:
      strlcat (pstr_unit, "W", KORAD_UNIT_SIZE);
      break;
  }
}

// get next command according to current one
uint8_t KoradNextCommand (const uint8_t actual)
{
  uint8_t result;

  // if recording on ON, only check VOUT and IOUT
  if (korad_record.start_time != 0)
  {
    if (actual == KORAD_COMMAND_VOUT_GET) result = KORAD_COMMAND_IOUT_GET;
    else if (actual == KORAD_COMMAND_IOUT_GET) result = KORAD_COMMAND_VOUT_GET;
    else result = KORAD_COMMAND_IOUT_GET;
  }

  // else no recording, switch between different data
  else
  {
    if (actual == KORAD_COMMAND_VOUT_GET) result = KORAD_COMMAND_VREF_GET;
    else if (actual == KORAD_COMMAND_VREF_GET) result = KORAD_COMMAND_IOUT_GET;
    else if (actual == KORAD_COMMAND_IOUT_GET) result = KORAD_COMMAND_IREF_GET;
    else if (actual == KORAD_COMMAND_IREF_GET) result = KORAD_COMMAND_STATUS;
    else if (actual == KORAD_COMMAND_STATUS) result = KORAD_COMMAND_VOUT_GET;
    else result = KORAD_COMMAND_STATUS;
  }

  return result;
}

void KoradExecuteCommand ()
{
  // empty reception buffer
  while (Serial.available() > 0) Serial.read ();

  // if command string is defined, send it
  if (strlen (korad_command.str_send) > 0)
  {
    // update current command data
    korad_status.last_cmnd = korad_command.type;
    korad_status.last_recv = UINT8_MAX;
    strcpy (korad_status.str_buffer, "");

    // send and log command
    Serial.write (korad_command.str_send);
    AddLog (LOG_LEVEL_DEBUG, PSTR ("KPS: Command %s"), korad_command.str_send);

    // reset command data
    korad_command.type  = KORAD_COMMAND_NONE;
    korad_command.value = NAN;
    strcpy (korad_command.str_send, "");
  }
}

void KoradInit ()
{
  int index;

  // init status strings
  strcpy (korad_status.str_brand, D_KORAD_UNKNOWN);
  strcpy (korad_status.str_model, "");
  strcpy (korad_status.str_version, "");
  strcpy (korad_status.str_serial, "");

  // disable fast cycle power recovery (SetOption65)
  Settings->flag3.fast_power_cycle_disable = true;

  // load configuration
  KoradLoadConfig ();

  // init graph data
  korad_graph.index  = 0;
  korad_graph.height = KORAD_GRAPH_HEIGHT;
  if (korad_graph.vmax == 0) korad_graph.vmax = KORAD_VOLTAGE_MAX;
  if (korad_graph.imax == 0) korad_graph.imax = KORAD_CURRENT_MAX;
  for (index = 0; index < KORAD_GRAPH_SAMPLE; index++)
  {
    korad_graph.arr_current[index] = UINT16_MAX;
    korad_graph.arr_voltage[index] = UINT16_MAX;
  } 

  // check if serial ports are selected 
  if (PinUsed (GPIO_TXD) && PinUsed (GPIO_RXD)) 
  {
    // start ESP8266 serial port
    Serial.flush ();
    Serial.begin (9600, SERIAL_8N1);
    ClaimSerial ();

    // next step is initilisation
    AddLog (LOG_LEVEL_INFO, PSTR ("KPS: Serial port initialised (9600 bauds, 8N1)"));

    // first command will be reading IDN
    KoradGetIDN ();
  }

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ps_help to get help on power supply commands"));

}

void KoradSaveBeforeRestart ()
{
  // stop any running recording
  KoradRecordStop ();

  // save configuration
  KoradSaveConfig ();
}

// Show JSON status (for MQTT)
void KoradShowJSON (bool append)
{
  char str_value[8];

  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // Korad data  -->  "Korad":{"Model":"model","Version":"version","VSET":5.0,"ISET":1.01,"VOUT":4.99,"IOUT"=0.22,"OUT"=1,"OVP":1,"OCP":0}
  ResponseAppend_P (PSTR ("\"Korad\":{"));

  // model and version
  if (append) ResponseAppend_P (PSTR ("\"Brand\":\"%s\",\"Model\":\"%s\",\"Version\":\"%s\",\"Serial\":\"%s\","), korad_status.str_brand, korad_status.str_model, korad_status.str_version, korad_status.str_serial);

  // VSET and ISET
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &korad_status.vset);
  ResponseAppend_P (PSTR ("\"%s\":%s"), D_CMND_KORAD_VSET, str_value);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%3_f"), &korad_status.iset);
  ResponseAppend_P (PSTR (",\"%s\":%s"), D_CMND_KORAD_ISET, str_value);

  // VOUT and IOUT
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &korad_status.vout);
  ResponseAppend_P (PSTR (",\"%s\":%s"), D_CMND_KORAD_VOUT, str_value);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%3_f"), &korad_status.iout);
  ResponseAppend_P (PSTR (",\"%s\":%s"), D_CMND_KORAD_IOUT, str_value);

  // OUT
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_CMND_KORAD_POWER, korad_status.out);

  // OVP
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_CMND_KORAD_OVP, korad_status.ovp);

  // OUT
  ResponseAppend_P (PSTR (",\"%s\":%d"), D_CMND_KORAD_OCP, korad_status.ocp);

  ResponseAppend_P (PSTR ("}"));

  // publish it if not in append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishTeleSensor ();
  } 
}

// Receive serial messages and handle them 20 times per second
void KoradUpdateData ()
{
  bool    received = true;
  bool    new_out;
  int     index;
  float   value;
  uint8_t status;
  char*   pstr_token;
  char    str_received[2] = {0, 0};

  // if a command is being received
  if (korad_status.last_cmnd != KORAD_COMMAND_NONE)
  {
    // receive all pending data
    while (Serial.available() > 0)
    {
      // receive current caracter
      str_received[0] = (char)Serial.read ();
    
      // append answer
      strlcat (korad_status.str_buffer, str_received, KORAD_RECV_BUFFER_MAX);
    }

    // if command has just been sent, update received buffer size
    if (korad_status.last_recv == UINT8_MAX) korad_status.last_recv = strlen (korad_status.str_buffer);

    // else if no new character received since last check
    else if (korad_status.last_recv == strlen (korad_status.str_buffer))
    {
      // handle answer according to last command
      switch (korad_status.last_cmnd)
      {
        case KORAD_COMMAND_IDN:
          // device identifier is DEVICE MODEL DEVICE VERSION separated by spaces
          pstr_token = strtok (korad_status.str_buffer, " ");

          // loop to populate model and version using SPACE separator
          index = 0;
          while (pstr_token != nullptr) 
          {
            // update data
            if (index == 0) strcpy (korad_status.str_brand, pstr_token);
            else if (index == 1) strcpy (korad_status.str_model, pstr_token);
            else if (index == 2) strcpy (korad_status.str_version, pstr_token);
            else strcpy (korad_status.str_serial, pstr_token);
            index++;

            // change token
            pstr_token = strtok (nullptr, " ");
          }
        break;

        // update status
        //   bit 1 : mode CC (0) or CV (1)
        //   bit 6 : OCP mode
        //   bit 7 : output status
        //   bit 8 : OVP mode
        case KORAD_COMMAND_STATUS:
          // get status byte
          status = (uint8_t)korad_status.str_buffer[0];
          korad_status.mode = ((status & 0x01) == 0x01);        // mode voltage / current
          korad_status.ocp  = ((status & 0x20) == 0x20);        // OCP status (on/off)
          korad_status.ovp  = ((status & 0x80) == 0x80);        // OVP status (on/off)

          // if output switched OFF during voltage change, switch it back ON
          if (korad_status.time_over != 0)
          {
            // get new output status
            new_out = ((status & 0x40) == 0x40);                // output status (on/off)

            // if new status different than previous one, supply may have switched off during voltage change, set back previous state
            if (new_out != korad_status.out) KoradSetPower (korad_status.out);

            // if time reached, reset timeout
            if (TimeReached (korad_status.time_over)) korad_status.time_over = 0;
          }
          else korad_status.out = ((status & 0x40) == 0x40);        // output status (on/off)
          break;

        case KORAD_COMMAND_VREF_GET:
          korad_status.vset = atof (korad_status.str_buffer);
          break;

        case KORAD_COMMAND_IREF_GET:
          korad_status.iset = atof (korad_status.str_buffer);
          break;

        case KORAD_COMMAND_VOUT_GET:
          value = korad_status.vout;
          korad_status.vout = atof (korad_status.str_buffer);
          break;

        case KORAD_COMMAND_IOUT_GET:
          korad_status.iout = atof (korad_status.str_buffer);
          break;
      }

      // log
      AddLog (LOG_LEVEL_DEBUG, PSTR ("KPS: Received %s"), korad_status.str_buffer);

      // if no command waiting, ask for next command
      if (korad_command.type == KORAD_COMMAND_NONE)
      {
        korad_command.type  = KoradNextCommand (korad_status.last_cmnd);
        korad_command.value = NAN;
        GetTextIndexed (korad_command.str_send, sizeof (korad_command.str_send), korad_command.type, kKoradCommand);
      }

      // last command is over
      korad_status.last_cmnd = KORAD_COMMAND_NONE;
      strcpy (korad_status.str_buffer, "");
    }

    // else update buffer size
    else korad_status.last_recv = strlen (korad_status.str_buffer);
  }

  // if no active command, sedn next
  if (korad_status.last_cmnd == KORAD_COMMAND_NONE) KoradExecuteCommand ();
}

// Every 250 ms
void KoradEvery250ms ()
{
  // update graph data for current and voltage
  korad_graph.arr_voltage[korad_graph.index] = (uint16_t)(korad_status.vout * 100);
  korad_graph.arr_current[korad_graph.index] = (uint16_t)(korad_status.iout * 1000);
  korad_graph.index++;
  korad_graph.index = korad_graph.index % KORAD_GRAPH_SAMPLE;

  // if recording
  if (korad_record.start_time != 0) KoradRecordStream ();
}

// if output is enabled, send JSON status every second
void KoradEverySecond ()
{
  // if output is active and recording is not started, publish data every second
  if (korad_status.out && (korad_record.start_time == 0)) KoradShowJSON (false);
}

/**************************************\
 *                   Web
\**************************************/

#ifdef USE_WEBSERVER

// append relay status to main page
void KoradWebSensor ()
{
  char str_unit[KORAD_UNIT_SIZE];
  char str_value[KORAD_VALUE_SIZE];
  char str_actual[12];
  char str_set[12];
  char str_text[48];

  // display identification
  WSContentSend_PD (PSTR ("{s}%s{m}%s %s{e}"), D_KORAD_MODEL, korad_status.str_brand, korad_status.str_model);
  WSContentSend_PD (PSTR ("{s}%s{m}%s %s{e}"), D_KORAD_VERSION, korad_status.str_version, korad_status.str_serial);

  // display output status
  if (korad_status.out) WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_KORAD_OUTPUT, PSTR ("<span style='color:green;'><b>ON</b></span>"));
    else WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_KORAD_OUTPUT, PSTR ("<span style='color:red;'>OFF</span>"));

  // display voltage
  strcpy (str_text, "");
  if (korad_status.ovp) strcpy_P (str_text, PSTR ("<span style='color:green;'> [ovp]</span>"));

  KoradGenerateValueLabel (str_value, str_unit, korad_status.vout, 0, KORAD_UNIT_V);
  sprintf (str_actual, "%s %s", str_value, str_unit);

  KoradGenerateValueLabel (str_value, str_unit, korad_status.vset, 0, KORAD_UNIT_V);
  sprintf (str_set, "%s %s", str_value, str_unit);

  WSContentSend_PD (PSTR ("{s}%s %s{m}<b>%s</b> <small>/ %s</small>{e}"), D_KORAD_VOLTAGE, str_text, str_actual, str_set);

  // display current
  strcpy (str_text, "");
  if (korad_status.ocp) strcpy_P (str_text, PSTR ("<span style='color:green;'> [ocp]</span>"));

  KoradGenerateValueLabel (str_value, str_unit, korad_status.iout, 0, KORAD_UNIT_A);
  sprintf (str_actual, "%s %s", str_value, str_unit);

  KoradGenerateValueLabel (str_value, str_unit, korad_status.iset, 0, KORAD_UNIT_A);
  sprintf (str_set, "%s %s", str_value, str_unit);

  WSContentSend_PD (PSTR ("{s}%s %s{m}<b>%s</b> <small>/ %s</small>{e}"), D_KORAD_CURRENT, str_text, str_actual, str_set);
}

// Configuration web page
void KoradWebPageConfig ()
{
  int   index;
  float value;
  char  str_key[8];
  char  str_value[8];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg("save"))
  {
    // loop thru predefined voltage
    for (index = 0; index < KORAD_PRESET_VOLTAGE_MAX; index ++)
    {
      sprintf (str_key, "%s%d", D_CMND_KORAD_VOLT, index);
      WebGetArg (str_key, str_value, sizeof (str_value));
      if (strlen (str_value) > 0) korad_config.preset_vset[index] = atof (str_value);
    }

    // loop thru predefined current
    for (index = 0; index < KORAD_PRESET_CURRENT_MAX; index ++)
    {
      sprintf (str_key, "%s%d", D_CMND_KORAD_AMP, index);
      WebGetArg (str_key, str_value, sizeof (str_value));
      if (strlen (str_value) > 0) korad_config.preset_iset[index] = atof (str_value);
    }

    // save config
    KoradSaveConfig ();
  }

  // beginning of form
  WSContentStart_P (D_KORAD_CONFIGURE);
  WSContentSendStyle ();

  // style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("span.half {display:inline-block;width:47%%;margin-right:2%%;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_KORAD_PAGE_CONFIG);

  // loop thru predefined voltage
  WSContentSend_P (KORAD_FIELD_START, D_KORAD_VOLTAGE);
  for (index = 0; index < KORAD_PRESET_VOLTAGE_MAX; index ++)
  {
    // get value
    ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%02_f"), &korad_config.preset_vset[index]);

    // display input field
    if (index % 2 == 0) WSContentSend_P (PSTR ("<p>\n"));
    WSContentSend_P (KORAD_INPUT_NUMBER, D_KORAD_VOLTAGE, index + 1, D_CMND_KORAD_VOLT, index, 0, 30, "0.01", str_value);
    if (index % 2 == 1) WSContentSend_P (PSTR ("</p>\n"));
  }
  WSContentSend_P (KORAD_FIELD_STOP);

  // loop thru predefined voltage
  WSContentSend_P (KORAD_FIELD_START, D_KORAD_CURRENT);
  for (index = 0; index < KORAD_PRESET_CURRENT_MAX; index ++)
  {
    // get value
    ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%03_f"), &korad_config.preset_iset[index]);

    // display input field
    if (index % 2 == 0) WSContentSend_P (PSTR ("<p>\n"));
    WSContentSend_P (KORAD_INPUT_NUMBER, D_KORAD_CURRENT, index + 1, D_CMND_KORAD_AMP, index, 0, 3, "0.001", str_value);
    if (index % 2 == 1) WSContentSend_P (PSTR ("</p>\n"));
  }
  WSContentSend_P (KORAD_FIELD_STOP);

  // save button  
  // -----------
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// realtime data update
//  format : vout(on/off);ovp(on/off);ocp(on/off);vout;vout-unit(V/mV);iout;iout-unit(A/mA);wout;wout-unit(W/mW);record(on/off);record-duration(hh:mm:ss)
void KoradWebDataUpdate ()
{
  float value;
  char str_unit[KORAD_UNIT_SIZE];
  char str_value[KORAD_VALUE_SIZE];
  char  str_text[8];

  // start of data page
  WSContentBegin (200, CT_PLAIN);

  // power switch status 
  if (korad_status.out) strcpy (str_value, "on"); else strcpy (str_value, "off");
  WSContentSend_P ("%s", str_value);

  // over voltage protection status 
  if (korad_status.ovp) strcpy (str_value, "on"); else strcpy (str_value, "off");
  WSContentSend_P (";%s", str_value);

  // over current protection status 
  if (korad_status.ocp) strcpy (str_value, "on"); else strcpy (str_value, "off");
  WSContentSend_P (";%s", str_value);

  // voltage output status
  KoradGenerateValueLabel (str_value, str_unit, korad_status.vout, 2, KORAD_UNIT_V);
  WSContentSend_P (";%s;%s", str_value, str_unit);

  // current output status
  KoradGenerateValueLabel (str_value, str_unit, korad_status.iout, 3, KORAD_UNIT_A);
  WSContentSend_P (";%s;%s", str_value, str_unit);

  // power output status
  value = korad_status.vout * korad_status.iout;
  KoradGenerateValueLabel (str_value, str_unit, value, 2, KORAD_UNIT_W);
  WSContentSend_P (";%s;%s", str_value, str_unit);

  // data recording status and duration
  if (korad_record.start_time != 0) strcpy (str_value, "on"); else strcpy (str_value, "off");
  KoradGetRecordingDuration (str_text, sizeof (str_text));
  WSContentSend_P (";%s;%s", str_value, str_text);

  // end of data page
  WSContentEnd ();
}


// Voltage and current graph curve
void KoradWebGraphData ()
{
  bool     first_point;
  int      index, index_array;
  long     graph_left, graph_right, graph_width;  
  long     unit_width;  
  long     graph_value, graph_x, graph_y, last_x, last_y; 
  long     graph_vmax, graph_imax; 
  float    value;
  char     str_data[16];
  String   str_result;

  // boundaries of SVG graph
  graph_left   = KORAD_GRAPH_PERCENT_START * KORAD_GRAPH_WIDTH / 100;
  graph_right  = KORAD_GRAPH_PERCENT_STOP * KORAD_GRAPH_WIDTH / 100;
  graph_width  = graph_right - graph_left;

  // calculate graph maximum scales
  value = korad_graph.vmax * 100;
  graph_vmax = (long)value;
  value = korad_graph.imax * 1000;
  graph_imax = (long)value;
  
  // start of SVG graph
  WSContentBegin (200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d'>\n"), 0, 0, KORAD_GRAPH_WIDTH, korad_graph.height);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("polyline {fill:none;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("polyline.voltage {stroke:%s;}\n"), D_KORAD_COLOR_VOLTAGE);
  WSContentSend_P (PSTR ("path.current {stroke:%s;fill:%s;fill-opacity:0.6;}\n"), D_KORAD_COLOR_CURRENT, D_KORAD_COLOR_CURRENT);
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:1.5rem;fill:grey;}\n"), str_data);
  WSContentSend_P (PSTR ("</style>\n"));

  // ------------------
  //   voltage curve
  // ------------------

  // start of polyline
  str_result = "<polyline class='voltage' points='";

  // loop for the apparent power graph
  first_point = true;
  last_x = LONG_MAX;
  last_y = LONG_MAX;
  for (index = 0; index < KORAD_GRAPH_SAMPLE; index++)
  {
    // get current array index and value
    index_array = (index + korad_graph.index) % KORAD_GRAPH_SAMPLE;
    graph_value = (long)korad_graph.arr_voltage[index_array];

    // if voltage is defined, display point
    if (graph_value != UINT16_MAX)
    {
      graph_y = korad_graph.height - (graph_value * korad_graph.height / graph_vmax);
      graph_x = graph_left + (graph_width * index / KORAD_GRAPH_SAMPLE);

      // if point has changed or last point
      if ((graph_y != last_y) || (index == KORAD_GRAPH_SAMPLE - 1))
      {
        // display previous point if needed
        if ((last_x != LONG_MAX) && (last_y != LONG_MAX))
        {
          sprintf (str_data, "%d,%d ", last_x, last_y);
          str_result += str_data;
        }

        // display current point
        sprintf (str_data, "%d,%d ", graph_x, graph_y);
        str_result += str_data;

        // reset previous point
        last_x = LONG_MAX;
      }

      // else save previous point
      else last_x = graph_x;
 
      // save last y position
      last_y = graph_y;
    }

    // if result string is long enough, send it
    if (str_result.length () > 256)
    {
      WSContentSend_P (str_result.c_str ());
      str_result = "";
    }
  }

  // end of polyline
  WSContentSend_P (PSTR("%s'/>\n"), str_result.c_str ());

  // ------------------
  //   current curve
  // ------------------

  // start of path
  str_result = "<path class='current' d='";
  sprintf (str_data, "M%d,%d ", graph_left, KORAD_GRAPH_HEIGHT);
  str_result += str_data;

  // loop for the apparent power graph
  first_point = true;
  last_x = LONG_MAX;
  last_y = LONG_MAX;
  for (index = 0; index < KORAD_GRAPH_SAMPLE; index++)
  {
    // get current array index and value
    index_array = (index + korad_graph.index) % KORAD_GRAPH_SAMPLE;
    graph_value = (long)korad_graph.arr_current[index_array];

    // if voltage is defined, display point
    if (graph_value != UINT16_MAX)
    {
      graph_x = graph_left + (graph_width * index / KORAD_GRAPH_SAMPLE);
      graph_y = korad_graph.height - (graph_value * korad_graph.height / graph_imax);

      // if first point
      if (first_point)
      {
        // display lower point
        sprintf (str_data, "%d,%d ", graph_x, KORAD_GRAPH_HEIGHT);
        str_result += str_data;
        first_point = false;

        // reset previous point
        last_x = LONG_MAX;
      }
      
      // if point has changed or last point
      else if ((graph_y != last_y) || (index == KORAD_GRAPH_SAMPLE - 1))
      {
        // display previous point if needed
        if ((last_x != LONG_MAX) && (last_y != LONG_MAX))
        {
          sprintf (str_data, "%d,%d ", last_x, last_y);
          str_result += str_data;
        }

        // display current point
        sprintf (str_data, "%d,%d ", graph_x, graph_y);
        str_result += str_data;

        // reset previous point
        last_x = LONG_MAX;
      }

      // else save previous point
      else last_x = graph_x;
 
      // save last y position
      last_y = graph_y;
    }

    // if result string is long enough, send it
    if (str_result.length () > 256)
    {
      WSContentSend_P (str_result.c_str ());
      str_result = "";
    }
  }

  // end of polyline
  if (graph_x != LONG_MAX)
  {
    sprintf (str_data, "L%d,%d ", graph_x, KORAD_GRAPH_HEIGHT);
    str_result += str_data;
  }
  WSContentSend_P (PSTR("%sZ'/>\n"), str_result.c_str ());

  // --------------
  //   Time units
  // --------------

  // display 10 sec separation lines (12 separation for 2 mn)
  unit_width  = graph_width / 12;
  for (index = 1; index < 12; index++)
  {
    // display separation line and time
    graph_x = graph_left + (index * unit_width);
    if (index == 6) WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 48, graph_x, 52);
      else WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd ();
}

// Graph frame
void KoradWebGraphFrame ()
{
  int   graph_left, graph_right, graph_width;
  float value;
  char  str_unit[KORAD_UNIT_SIZE];
  char  str_value[KORAD_VALUE_SIZE];

  // boundaries of SVG graph
  graph_left   = KORAD_GRAPH_PERCENT_START * KORAD_GRAPH_WIDTH / 100;
  graph_right  = KORAD_GRAPH_PERCENT_STOP * KORAD_GRAPH_WIDTH / 100;
  graph_width  = graph_right - graph_left;

  // start of SVG graph
  WSContentBegin (200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d'>\n"), 0, 0, KORAD_GRAPH_WIDTH, korad_graph.height);

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
  KoradGenerateValueLabel (str_value, str_unit, korad_graph.vmax, 2, KORAD_UNIT_V);
  WSContentSend_P (KORAD_HTML_UNIT, "voltage", 1, 3, str_value, str_unit);

  value = korad_graph.vmax * 0.75;
  KoradGenerateValueLabel (str_value, str_unit, value, 2, KORAD_UNIT_V);
  WSContentSend_P (KORAD_HTML_UNIT, "voltage", 1, 26, str_value, str_unit);

  value = korad_graph.vmax * 0.5;
  KoradGenerateValueLabel (str_value, str_unit, value, 2, KORAD_UNIT_V);
  WSContentSend_P (KORAD_HTML_UNIT, "voltage", 1, 51, str_value, str_unit);

  value = korad_graph.vmax * 0.25;
  KoradGenerateValueLabel (str_value, str_unit, value, 2, KORAD_UNIT_V);
  WSContentSend_P (KORAD_HTML_UNIT, "voltage", 1, 76, str_value, str_unit);

  WSContentSend_P (KORAD_HTML_UNIT, "voltage", 1, 99, "0", "");

  // Current graduation
  KoradGenerateValueLabel (str_value, str_unit, korad_graph.imax, 2, KORAD_UNIT_A);
  WSContentSend_P (KORAD_HTML_UNIT, "current", KORAD_GRAPH_PERCENT_STOP + 2, 3, str_value, str_unit);

  value = korad_graph.imax * 0.75;
  KoradGenerateValueLabel (str_value, str_unit, value, 2, KORAD_UNIT_A);
  WSContentSend_P (KORAD_HTML_UNIT, "current", KORAD_GRAPH_PERCENT_STOP + 2, 26, str_value, str_unit);

  value = korad_graph.imax * 0.5;
  KoradGenerateValueLabel (str_value, str_unit, value, 2, KORAD_UNIT_A);
  WSContentSend_P (KORAD_HTML_UNIT, "current", KORAD_GRAPH_PERCENT_STOP + 2, 51, str_value, str_unit);

  value = korad_graph.imax * 0.25;
  KoradGenerateValueLabel (str_value, str_unit, value, 2, KORAD_UNIT_A);
  WSContentSend_P (KORAD_HTML_UNIT, "current", KORAD_GRAPH_PERCENT_STOP + 2, 76, str_value, str_unit);

  WSContentSend_P (KORAD_HTML_UNIT, "current", KORAD_GRAPH_PERCENT_STOP + 2, 99, "0", "");

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd ();
}

// Control public page
void KoradWebPageControl ()
{
  bool  result;
  int   index, digit; 
  long  percentage; 
  float value;
  char* pstr_digit;
  char  str_unit[KORAD_UNIT_SIZE];
  char  str_value[KORAD_VALUE_SIZE];
  char  str_next[8];
  char  str_time[12];
  char  str_title[32];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // check power change
  if (Webserver->hasArg (D_CMND_KORAD_POWER))
  {
    // invert power state
    korad_status.out = !korad_status.out;
    KoradSetPower (korad_status.out);
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
    korad_status.vconf = 0;
    korad_status.vset  = atof (str_value);
    KoradSetVoltage (korad_status.vset);
  }

  // set output voltage
  if (Webserver->hasArg (D_CMND_KORAD_VCONF))
  {
    WebGetArg (D_CMND_KORAD_VCONF, str_value, sizeof (str_value)); 
    korad_status.vconf = atof (str_value);
  }
  else korad_status.vconf = 0;

  // set output current
  if (Webserver->hasArg (D_CMND_KORAD_ISET))
  {
    WebGetArg (D_CMND_KORAD_ISET, str_value, sizeof (str_value)); 
    korad_status.iset = atof (str_value);
    KoradSetCurrent (korad_status.iset);
  }

  // activate or stop recording
  if (Webserver->hasArg (D_CMND_KORAD_RECORD)) 
  {
    if (korad_record.start_time == 0) KoradRecordStart (); else KoradRecordStop ();
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

  // get graph height
  if (Webserver->hasArg (D_CMND_KORAD_HEIGHT))
  {
    WebGetArg (D_CMND_KORAD_HEIGHT, str_value, sizeof (str_value)); 
    korad_graph.height = atoi (str_value);
  }

  // beginning of form without authentification
  WSContentStart_P (D_KORAD_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  //  format : vout(on/off);ovp(on/off);ocp(on/off);vout;vout-unit(V/mV);iout;iout-unit(A/mA);wout;wout-unit(W/mW)
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));

  WSContentSend_P (PSTR ("function updateData()\n"));
  WSContentSend_P (PSTR ("{\n"));
  WSContentSend_P (PSTR (" httpUpd=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpUpd.onreadystatechange=function()\n"));
  WSContentSend_P (PSTR (" {\n"));
  WSContentSend_P (PSTR ("  if (httpUpd.responseText.length>0)\n"));
  WSContentSend_P (PSTR ("  {\n"));
  WSContentSend_P (PSTR ("   arr_value=httpUpd.responseText.split(';');\n"));
  WSContentSend_P (PSTR ("   document.getElementById('power').className=arr_value[0];\n"));             // switch on/off
  WSContentSend_P (PSTR ("   document.getElementById('vread').className='read '+arr_value[0];\n"));     // voltage display
  WSContentSend_P (PSTR ("   document.getElementById('iread').className='read '+arr_value[0];\n"));     // current display
  WSContentSend_P (PSTR ("   document.getElementById('wread').className='read '+arr_value[0];\n"));     // power display
  WSContentSend_P (PSTR ("   document.getElementById('ovp').className=arr_value[1];\n"));               // OVP indicator
  WSContentSend_P (PSTR ("   document.getElementById('ocp').className=arr_value[2];\n"));               // OCP indicator
  WSContentSend_P (PSTR ("   document.getElementById('vout').textContent=arr_value[3];\n"));            // voltage value
  WSContentSend_P (PSTR ("   document.getElementById('vunit').textContent=arr_value[4];\n"));           // voltage unit
  WSContentSend_P (PSTR ("   document.getElementById('iout').textContent=arr_value[5];\n"));            // current value
  WSContentSend_P (PSTR ("   document.getElementById('iunit').textContent=arr_value[6];\n"));           // current unit
  WSContentSend_P (PSTR ("   document.getElementById('wout').textContent=arr_value[7];\n"));            // power value
  WSContentSend_P (PSTR ("   document.getElementById('wunit').textContent=arr_value[8];\n"));           // power unit
  WSContentSend_P (PSTR ("   document.getElementById('record').className=arr_value[9];\n"));            // recording status
  WSContentSend_P (PSTR ("   document.getElementById('duration').textContent=arr_value[10];\n"));       // recording duration
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

  WSContentSend_P (PSTR ("setInterval(function() {updateData();},500);\n"));      // update live data 2 times per second
  WSContentSend_P (PSTR ("setInterval(function() {updateGraph();},5000);\n"));    // update graph every 2 seconds
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:%s;font-family:Arial, Helvetica, sans-serif;}\n"), D_KORAD_COLOR_BACKGROUND);
  WSContentSend_P (PSTR ("div.main {margin:0.5rem auto;padding:0px;text-align:center;vertical-align:middle;}\n"));

  WSContentSend_P (PSTR ("div.watt {display:inline-block;width:14rem;border-radius:8px;border:1px white solid;border-top:30px white solid;margin-bottom:10px;}\n"));
  WSContentSend_P (PSTR ("div.watt div.name {width:100%%;margin-top:-30px;font-size:1.6rem;font-style:bold;border:none;}\n"));
  WSContentSend_P (PSTR ("div.watt a {color:black;}\n"));

  WSContentSend_P (PSTR ("div.control {display:inline-block;text-align:center;width:20vw;}\n"));
  WSContentSend_P (PSTR ("div.action {width:100%%;padding:0px;margin:auto;}\n"));
  WSContentSend_P (PSTR ("div.section {display:inline-block;text-align:center;width:18rem;margin:4px 1rem;}\n"));
  WSContentSend_P (PSTR ("div.watt {width:14rem;}\n"));

  WSContentSend_P (PSTR ("div.read {width:100%%;padding:0px;margin:0px auto;border-radius:8px;border:1px white solid;border-top:10px white solid;}\n"));
  WSContentSend_P (PSTR ("div.read span.value {font-size:2.4rem;}\n"));
  WSContentSend_P (PSTR ("div.read span.unit {font-size:1.2rem;margin-left:0.5rem;margin-right:-4rem;}\n"));

  WSContentSend_P (PSTR ("div.watt div.read {border:none;}\n"));
  WSContentSend_P (PSTR ("div.watt div.read span.unit {margin-right:0rem;}\n"));

  WSContentSend_P (PSTR ("div.power {width:100%%;padding:0px;margin:auto;}\n"));

  WSContentSend_P (PSTR ("div.preset {width:100%%;font-size:0.8rem;padding-bottom:4px;margin:0px;}\n"));
  WSContentSend_P (PSTR ("div.preset div {display:inline-block;padding:0px 4px;margin:0px;border-radius:6px;color:black;}\n"));

  WSContentSend_P (PSTR ("div.set {width:90%%;font-size:1rem;padding:0px;margin:0px auto;border:1px #666 solid;border-top:none;border-bottom-left-radius:6px;border-bottom-right-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.set div {display:inline-block;width:10%%;padding:0 0.1rem;margin:0 2%%;font-weight:bold;border-radius:8px;}\n"));
  WSContentSend_P (PSTR ("div.set div.ref {width:25%%;background:none;}\n"));

  // graph + and -
  WSContentSend_P (PSTR ("div.adjust {width:100%%;max-width:1000px;margin:1rem auto 0.25rem auto;}\n"));
  WSContentSend_P (PSTR ("div.choice {display:inline-block;font-size:0.8rem;padding:0.1rem;margin:auto;border:1px #666 solid;background:none;color:#fff;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.choice a {color:white;}\n"));
  WSContentSend_P (PSTR ("div.choice div {font-weight:bold;padding:0.1rem;width:30px;height:15px;}\n"));
  WSContentSend_P (PSTR ("div.choice a div {background:none;}\n"));
  WSContentSend_P (PSTR ("div.item {display:inline-block;padding:0.2rem auto;margin:1px;border:none;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("div.choice a div.item:hover {background:#666;}\n"));
  WSContentSend_P (PSTR ("div.left {float:left;margin-left:1%%;}\n"));
  WSContentSend_P (PSTR ("div.right {float:right;margin-right:2%%;}\n"));
  WSContentSend_P (PSTR ("div.center {float:middle;}\n"));

  // watt meter
  WSContentSend_P (PSTR (".watt div {color:%s;border:1px %s solid;}\n"), D_KORAD_COLOR_WATT, D_KORAD_COLOR_WATT_OFF);
  WSContentSend_P (PSTR (".watt div.off {border-color:%s;}\n"), D_KORAD_COLOR_WATT_OFF);
  WSContentSend_P (PSTR (".watt div span {color:%s;}\n"), D_KORAD_COLOR_WATT);
  WSContentSend_P (PSTR (".watt div.off span {visibility:hidden;}\n"), D_KORAD_COLOR_WATT_OFF);

  // volt meter
  WSContentSend_P (PSTR (".volt div {color:%s;border-color:%s;}\n"), D_KORAD_COLOR_VOLTAGE, D_KORAD_COLOR_VOLTAGE);
  WSContentSend_P (PSTR (".volt div.off {color:%s;}\n"), D_KORAD_COLOR_VOLTAGE_OFF);
  WSContentSend_P (PSTR (".volt div span {color:%s;}\n"), D_KORAD_COLOR_VOLTAGE);
  WSContentSend_P (PSTR (".volt div.off span {color:%s;}\n"), D_KORAD_COLOR_VOLTAGE_OFF);
  WSContentSend_P (PSTR (".volt div.preset div {background:%s;width:15%%;}\n"), D_KORAD_COLOR_VOLTAGE_OFF);
  WSContentSend_P (PSTR (".volt div.preset div.confirm {color:%s;font-weight:bold;}\n"), D_KORAD_COLOR_VOLTAGE);
  WSContentSend_P (PSTR (".volt div.preset div:hover {color:%s;background:%s;}\n"), D_KORAD_COLOR_BACKGROUND, D_KORAD_COLOR_VOLTAGE);
  WSContentSend_P (PSTR (".volt div.set a div:hover {background:%s;}\n"), D_KORAD_COLOR_VOLTAGE_OFF);
  WSContentSend_P (PSTR (".volt span#ovp {float:right;width:2rem;padding:0.1rem 0.5rem;margin:0.25rem 0.25rem;border:1px %s solid;color:%s;background:none;border-radius:12px;}\n"), D_KORAD_COLOR_VOLTAGE_OFF, D_KORAD_COLOR_VOLTAGE_OFF);
  WSContentSend_P (PSTR (".volt span#ovp.on {color:black;background:%s;}\n"), D_KORAD_COLOR_VOLTAGE);

  // amp meter
  WSContentSend_P (PSTR (".amp div {color:%s;border-color:%s;}\n"), D_KORAD_COLOR_CURRENT, D_KORAD_COLOR_CURRENT);
  WSContentSend_P (PSTR (".amp div.off {color:%s;}\n"), D_KORAD_COLOR_CURRENT_OFF);
  WSContentSend_P (PSTR (".amp div span {color:%s;}\n"), D_KORAD_COLOR_CURRENT);
  WSContentSend_P (PSTR (".amp div.off span {color:%s;}\n"), D_KORAD_COLOR_CURRENT_OFF);
  WSContentSend_P (PSTR (".amp div.preset div {background:%s;width:18%%;}\n"), D_KORAD_COLOR_CURRENT_OFF);
  WSContentSend_P (PSTR (".amp div.preset div:hover {background:%s;}\n"), D_KORAD_COLOR_CURRENT);
  WSContentSend_P (PSTR (".amp div.set a div:hover {background:%s;}\n"), D_KORAD_COLOR_CURRENT_OFF);
  WSContentSend_P (PSTR (".amp span#ocp {float:right;width:2rem;padding:0.1rem 0.5rem;margin:0.25rem 0.25rem;border:1px %S solid;color:%s;background:none;border-radius:12px;}\n"), D_KORAD_COLOR_CURRENT_OFF, D_KORAD_COLOR_CURRENT_OFF);
  WSContentSend_P (PSTR (".amp span#ocp.on {color:black;background:%s;}\n"), D_KORAD_COLOR_CURRENT);

  // power switch
  WSContentSend_P (PSTR ("#power {position:relative;display:inline-block;width:48px;height:48px;border-radius:24px;padding:0px;border:1px solid grey;}\n"));
  WSContentSend_P (PSTR ("#power #ring {position:absolute;width:24px;height:24px;border:6px solid red;top:6px;left:6px;border-radius:18px;}\n"), D_KORAD_COLOR_POWER_OFF);
  WSContentSend_P (PSTR ("#power #line {width:8px;height:18px;margin:-8px auto;background:%s;border-radius:6px;border-right:4px solid %s;border-left:4px solid %s;}\n"), D_KORAD_COLOR_POWER_OFF, D_KORAD_COLOR_BACKGROUND, D_KORAD_COLOR_BACKGROUND);
  WSContentSend_P (PSTR ("#power.on #ring {border-color:%s;}\n"), D_KORAD_COLOR_POWER);
  WSContentSend_P (PSTR ("#power.on #line {background:%s;}\n"), D_KORAD_COLOR_POWER);

  // record button
  WSContentSend_P (PSTR ("#record {position:relative;display:inline-block;width:48px;height:48px;border-radius:24px;padding:0px;border:1px solid %s;}\n"), D_KORAD_COLOR_RECORD_OFF);
  WSContentSend_P (PSTR ("#record #ring {position:absolute;width:0px;height:0px;border:18px solid %s;top:6px;left:6px;border-radius:18px;}\n"), D_KORAD_COLOR_RECORD_OFF);
  WSContentSend_P (PSTR ("#record #duration {position:absolute;font-size:18px;font-weight:bold;color:%s;background-color:%s;padding:4px 12px;margin-left:20px;margin-top:-16px;border:1px solid %s;border-left:none;border-top-right-radius:8px;border-bottom-right-radius:8px;}\n"), D_KORAD_COLOR_RECORD, D_KORAD_COLOR_BACKGROUND, D_KORAD_COLOR_RECORD_OFF);
  WSContentSend_P (PSTR ("#record.on #ring {border-color:%s;}\n"), D_KORAD_COLOR_RECORD);
  WSContentSend_P (PSTR ("#record.off #duration {visibility:hidden;}\n"));

  // no line under links
  WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

  // graph
  percentage = (100 * korad_graph.height / KORAD_GRAPH_WIDTH) + 2; 
  WSContentSend_P (PSTR (".svg-container {position:relative;width:100%%;max-width:%dpx;padding-top:%d%%;margin:auto;}\n"), KORAD_GRAPH_WIDTH, percentage);
  WSContentSend_P (PSTR (".svg-content {display:inline-block;position:absolute;top:0;left:0;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<div class='main'>"));

  // -----------------
  //    First Row
  // -----------------

  // upper row : start
  WSContentSend_P (PSTR ("<div>\n"));

  // --- Power Switch --- //

  WSContentSend_P (PSTR ("<div class='control'><div class='action'>\n"));

  if (korad_status.out) { strcpy (str_value, "on"); strcpy (str_title, "Switch OFF"); }
    else { strcpy (str_value, "off"); strcpy (str_title, "Switch ON"); }
  WSContentSend_P (PSTR ("<a href='/ctrl?%s'><div id='power' class='%s' title='%s'><div id='ring'><div id='line'></div></div></div></a>\n"), D_CMND_KORAD_POWER, str_value, str_title);

  WSContentSend_P (PSTR ("</div></div>\n"));    // action & control

  // --- Power Meter --- //

  WSContentSend_P (PSTR ("<div class='watt'>\n"));

  // device name
  WSContentSend_P (PSTR ("<div class='name'><a href='/'>%s</a></div>\n"), SettingsText(SET_DEVICENAME));

  if (korad_status.out) strcpy (str_value, "on"); else strcpy (str_value, "off");
  WSContentSend_P (PSTR ("<div class='read %s' id='wread'>\n"), str_value);

  value = korad_status.vout * korad_status.iout;
  KoradGenerateValueLabel (str_value, str_unit, value, 2, KORAD_UNIT_W);
  WSContentSend_P (PSTR ("<span class='value' id='wout'>%s</span>\n"), str_value);
  WSContentSend_P (PSTR ("<span class='unit watt' id='wunit'>%s</span>\n"), str_unit);
  WSContentSend_P (PSTR ("</div>\n"));    // read
  WSContentSend_P (PSTR ("</div>\n"));    // watt

  // --- Record Button --- //

  WSContentSend_P (PSTR ("<div class='control'><div class='action'>\n"));

  strcpy (str_next,  D_CMND_KORAD_RECORD);
  if (korad_record.start_time != 0) { strcpy (str_value, "off"); strcpy (str_title, "Stop recording"); KoradGetRecordingDuration (str_time, sizeof (str_time)); }
    else { strcpy (str_value, "on"); strcpy (str_title, "Start recording"); strcpy (str_time, ""); }
  WSContentSend_P (PSTR ("<a href='/ctrl?%s'><div id='record' class='%s' title='%s'><div id='ring'><div id='duration'>%s</div></div></div></a>\n"), str_next, str_value, str_title, str_time);

  WSContentSend_P (PSTR ("</div></div>\n"));    // action & control

  // upper row : stop
  WSContentSend_P (PSTR ("</div>\n"));

  // -----------------
  //    Second Row
  // -----------------

  // middle row : start
  WSContentSend_P (PSTR ("<div>\n"));

  // --- Voltage Meter --- //

  // live display
  WSContentSend_P (PSTR ("<div class='section volt'>\n"));

  if (korad_status.out) strcpy (str_value, "on"); else strcpy (str_value, "off");
  WSContentSend_P (PSTR ("<div class='read %s' id='vread'>\n"), str_value);

  if (korad_status.ovp) { strcpy (str_value, "on"); strcpy (str_next, "off"); }
    else { strcpy (str_value, "off"); strcpy (str_next, "on"); }
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='protect %s' id='ovp'>OVP</span></a>\n"), D_CMND_KORAD_OVP, str_next, str_value);

  KoradGenerateValueLabel (str_value, str_unit, korad_status.vout, 2, KORAD_UNIT_V);
  WSContentSend_P (PSTR ("<span class='value' id='vout'>%s</span>\n"), str_value);
  WSContentSend_P (PSTR ("<span class='unit' id='vunit'>%s</span>\n"), str_unit);

  WSContentSend_P (PSTR ("<br>\n"));    // read

  // preset voltage buttons
  WSContentSend_P (PSTR ("<div class='preset'>\n"));
  for (index = 0; index < KORAD_PRESET_VOLTAGE_MAX; index ++)
  {
    // get value to display
    value = korad_config.preset_vset[index];
    ext_snprintf_P (str_next, sizeof(str_next), PSTR ("%02_f"), &value);

    // get value and unit for the button
    KoradGenerateValueLabel (str_value, str_unit, value, 0, KORAD_UNIT_V);

    // set button according to stage : confirmation or voltage set
    if ((value > 0) && (value == korad_status.vconf)) WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><div class='confirm' title='%s'>%s %s</div></a>\n"), D_CMND_KORAD_VSET, str_next, KORAD_VOLT_CLICK, str_value, str_unit);
    else if (value > 0) WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><div title='%s'>%s %s</div></a>\n"), D_CMND_KORAD_VCONF, str_next, KORAD_VOLT_CLICK2, str_value, str_unit);
  }
  WSContentSend_P (PSTR ("</div>\n"));    // preset

  WSContentSend_P (PSTR ("</div>\n"));    // read

  // voltage reference with decrease and increase buttons
  WSContentSend_P (PSTR ("<div class='set'>\n"));

  if (korad_status.vset >= 1) value = korad_status.vset - 1; else value = 0;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><div title='%s V (-1 V)'><<</div></a>\n"), D_CMND_KORAD_VSET, str_value, str_value);

  if (korad_status.vset >= 0.1) value = korad_status.vset - 0.1; else value = 0;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><div title='%s V (-100 mV)'><</div></a>\n"), D_CMND_KORAD_VSET, str_value, str_value);

  KoradGenerateValueLabel (str_value, str_unit, korad_status.vset, 2, KORAD_UNIT_V);
  WSContentSend_P (PSTR ("<div class='ref'>%s <small>%s</small></div>\n"), str_value, str_unit);

  if (korad_status.vset <= 29.9) value = korad_status.vset + 0.1; else value = 30;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><div title='%s V (+100 mV)'>></div></a>\n"), D_CMND_KORAD_VSET, str_value, str_value);

  if (korad_status.vset <= 29) value = korad_status.vset + 1; else value = 30;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><div title='%s V (+1 V)'>>></div></a>\n"), D_CMND_KORAD_VSET, str_value, str_value);

  WSContentSend_P (PSTR ("</div>\n"));      // set

  WSContentSend_P (PSTR ("</div>\n"));      // section volt

  // --- Current Meter --- //

  WSContentSend_P (PSTR ("<div class='section amp'>\n"));

  if (korad_status.out) strcpy (str_value, "on"); else strcpy (str_value, "off");
  WSContentSend_P (PSTR ("<div class='read %s' id='iread'>\n"), str_value);

  if (korad_status.ocp) { strcpy (str_value, "on"); strcpy (str_next, "off"); }
    else { strcpy (str_value, "off"); strcpy (str_next, "on"); }
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='protect %s' id='ocp'>OCP</span></a>\n"), D_CMND_KORAD_OCP, str_next, str_value);

  KoradGenerateValueLabel (str_value, str_unit, korad_status.iout, 3, KORAD_UNIT_A);
  WSContentSend_P (PSTR ("<span class='value' id='iout'>%s</span>\n"), str_value);
  WSContentSend_P (PSTR ("<span class='unit' id='iunit'>%s</span>\n"), str_unit);

  WSContentSend_P (PSTR ("<br>\n"));

  // preset max current
  WSContentSend_P (PSTR ("<div class='preset'>\n"));
  for (index = 0; index < KORAD_PRESET_CURRENT_MAX; index ++)
  {
    // get value to display
    value = korad_config.preset_iset[index];
    ext_snprintf_P (str_next, sizeof(str_next), PSTR ("%03_f"), &value);

    // display button
    KoradGenerateValueLabel (str_value, str_unit, value, 0, KORAD_UNIT_A);
    if (value > 0) WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><div title='%s'>%s %s</div></a>\n"), D_CMND_KORAD_ISET, str_next, KORAD_AMP_CLICK, str_value, str_unit);
  }
  WSContentSend_P (PSTR ("</div>\n"));    // preset

  WSContentSend_P (PSTR ("</div>\n"));    // read

  // voltage reference with decrease and increase buttons
  WSContentSend_P (PSTR ("<div class='set'>\n"));

  if (korad_status.iset >= 1) value = korad_status.iset - 1; else value = 0;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><div title='%s A (-1 A)'><<</div></a>\n"), D_CMND_KORAD_ISET, str_value, str_value);

  if (korad_status.iset >= 0.1) value = korad_status.iset - 0.1; else value = 0;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><div title='%s A (-100 mA)'><</div></a>\n"), D_CMND_KORAD_ISET, str_value, str_value);

  KoradGenerateValueLabel (str_value, str_unit, korad_status.iset, 2, KORAD_UNIT_A);
  WSContentSend_P (PSTR ("<div class='ref'>%s <small>%s</small></div>\n"), str_value, str_unit);

  if (korad_status.iset <= 2.9) value = korad_status.iset + 0.1; else value = 3;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><div title='%s A (+100 mA)'>></div></a>\n"), D_CMND_KORAD_ISET, str_value, str_value);

  if (korad_status.iset <= 2) value = korad_status.iset + 1; else value = 3;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%2_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><div title='%s A (+1 A)'>>></div></a>\n"), D_CMND_KORAD_ISET, str_value, str_value);

  WSContentSend_P (PSTR ("</div>\n"));    // set

  WSContentSend_P (PSTR ("</div>\n"));    // section amp

  // middle row : stop
  WSContentSend_P (PSTR ("</div>\n"));

  // -----------------
  //    Adjust Row
  // -----------------

  // adjust row : start
  WSContentSend_P (PSTR ("<div class='adjust'>\n"));

  // voltage increase/decrease buttons
  WSContentSend_P (PSTR ("<div class='choice left volt'>"));
  value = korad_graph.vmax / 2;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%02_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s' title='Reduce max voltage to %s V'><div class='item size'>∨</div></a>"), D_CMND_KORAD_VMAX, str_value, str_value);
  value = min ((float)KORAD_VOLTAGE_MAX, korad_graph.vmax * 2);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%02_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s' title='Increase max voltage to %s V'><div class='item size'>∧</div></a>"), D_CMND_KORAD_VMAX, str_value, str_value);
  WSContentSend_P (PSTR ("</div>\n"));      // choice left volt

  // graph height
  WSContentSend_P (PSTR ("<div class='choice center'>"));
  index = korad_graph.height - 50;
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%d' title='Reduce graph height to %d pixels'><div class='item size'>-</div></a>"), D_CMND_KORAD_HEIGHT, index, index);
  index = korad_graph.height + 50;
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%d' title='Increase graph height to %d pixels'><div class='item size'>+</div></a>"), D_CMND_KORAD_HEIGHT, index, index);
  WSContentSend_P (PSTR ("</div>\n"));      // choice center

  // current increase/decrease buttons
  WSContentSend_P (PSTR ("<div class='choice right amp'>"));
  value = korad_graph.imax / 2;
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%02_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s' title='Reduce max current to %s A'><div class='item size'>∨</div></a>"), D_CMND_KORAD_IMAX, str_value, str_value);
  value = min ((float)KORAD_CURRENT_MAX, korad_graph.imax * 2);
  ext_snprintf_P (str_value, sizeof(str_value), PSTR ("%02_f"), &value);
  WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s' title='Increase max current to %s A'><div class='item size'>∧</div></a>"), D_CMND_KORAD_IMAX, str_value, str_value);
  WSContentSend_P (PSTR ("</div>\n"));      // choice right amp

  // adjust row : stop
  WSContentSend_P (PSTR ("</div>\n"));

  // -----------------
  //      graph
  // -----------------

  // graph frame and data
  WSContentSend_P (PSTR ("<div class='svg-container'>\n"));
  WSContentSend_P (PSTR ("<object class='svg-content' id='base' type='image/svg+xml' width='100%%' height='100%%' data='%s'></object>\n"), D_KORAD_PAGE_GRAPH_FRAME);
  WSContentSend_P (PSTR ("<object class='svg-content' id='data' type='image/svg+xml' width='100%%' height='100%%' data='%s?ts=0'></object>\n"), D_KORAD_PAGE_GRAPH_DATA); 
  WSContentSend_P (PSTR ("</div>\n"));      // svg-container

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
    case FUNC_SAVE_BEFORE_RESTART:
      KoradSaveBeforeRestart ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kKoradMqttCommands, arrKoradMqttCommand);
      break;
    case FUNC_JSON_APPEND:
      KoradShowJSON (true);
      break;
    case FUNC_EVERY_100_MSECOND:
      if (TasmotaGlobal.uptime > KORAD_INIT_TIMEOUT) KoradUpdateData ();
      break;
    case FUNC_EVERY_250_MSECOND:
       KoradEvery250ms ();
      break;
    case FUNC_EVERY_SECOND:
      if (TasmotaGlobal.uptime > KORAD_INIT_TIMEOUT) KoradEverySecond ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      KoradWebSensor ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_KORAD_PAGE_CONTROL, D_KORAD_CONTROL);
      break;
    case FUNC_WEB_ADD_BUTTON:
      WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_KORAD_PAGE_CONFIG, D_KORAD_CONFIGURE " " D_KORAD_POWERSUPPLY);
      break;
    case FUNC_WEB_ADD_HANDLER:
      // graph
      Webserver->on (FPSTR (D_KORAD_PAGE_CONFIG),      KoradWebPageConfig);
      Webserver->on (FPSTR (D_KORAD_PAGE_CONTROL),     KoradWebPageControl);
      Webserver->on (FPSTR (D_KORAD_PAGE_GRAPH_FRAME), KoradWebGraphFrame);
      Webserver->on (FPSTR (D_KORAD_PAGE_GRAPH_DATA),  KoradWebGraphData);
      Webserver->on (FPSTR (D_KORAD_PAGE_DATA_UPDATE), KoradWebDataUpdate);
      break;
#endif  // USE_WEBSERVER

  }

  return result;
}

#endif   // USE_KORAD
