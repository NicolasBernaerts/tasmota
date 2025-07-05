/*
  xdrv_96_power_supply.ino - Driver for KORAD KAxxxxP and KUAIQU power supplies
  Copyright (C) 2020-2025  Nicolas Bernaerts

  History :
    02/05/2021 - v1.0 - Creation
    20/09/2021 - v1.1 - Add LittleFS support
                        Save configuration to korad.cfg
                        Allow realtime data recording to CSV file
    15/12/2022 - v1.2 - Rewrite serial communication management
    03/02/2023 - v1.3 - Based on Tasmota 12
    29/02/2024 - v1.4 - Based on Tasmota 13
    22/06/2025 - v2.0 - Based on Tasmota 15
                        Complete rewrite to add support for Kuaiqu supplies
                        Add CV and CC display
                        Add ps_custom command

  Configuration is stored in Settings :
   - Settings->knx_GA_param[0]    : power supply family
   - Settings->knx_GA_addr[0]     : vset
   - Settings->knx_GA_addr[1]     : graph vmax
   - Settings->knx_GA_addr[2..5]  : voltage presets
   - Settings->knx_CB_addr[0]     : iset
   - Settings->knx_CB_addr[1]     : graph imax
   - Settings->knx_CB_addr[2..5]  : current preset

  Protocol description :
   - Korad  : https://sigrok.org/wiki/Korad_KAxxxxP_series
   - Kuaiqu : https://www.eevblog.com/forum/testgear/kuaiqu-programmable-switchmode-power-supply-(32v-10-2a)/?action=dlattach;attach=2267752

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

#ifdef USE_POWERSUPPLY

/**************************************\
 *               Variables
\**************************************/

// declare korad driver and sensor
#define XDRV_96                96

// constants
#define PS_VOLTAGE_MAX         30           // maximum voltage
#define PS_CURRENT_MAX         3            // maximum current

#define PS_PRESET_VOLTAGE_MAX  4            // maximum number of predefined voltage
#define PS_PRESET_CURRENT_MAX  4            // maximum number of predefined max current

#define PS_INIT_TIMEOUT        5            // timeout before receiving data (sec.)
#define PS_COMMAND_BUFFER      64           // maximum size of a reception buffer

#define PS_KORAD_TIMEOUT       1000         // timeout for korad command handling (ms)
#define PS_KUAIQU_TIMEOUT      1000         // timeout for kuaiqu command handling (ms)

#define PS_COMMAND_SIZE        8
#define PS_GRAPH_SAMPLE        600          // number of samples in graph (1mn with one sample every 100ms)
#define PS_GRAPH_HEIGHT        600          // height of graph in pixels
#define PS_GRAPH_WIDTH         1200         // width of graph in pixels
#define PS_GRAPH_PERCENT_START 6     
#define PS_GRAPH_PERCENT_STOP  94

// commands
#define D_CMND_PS_HELP         "help"
#define D_CMND_PS_MODEL        "model"
#define D_CMND_PS_CUSTOM       "custom"
#define D_CMND_PS_POWER        "power"
#define D_CMND_PS_VCONF        "vconf"
#define D_CMND_PS_RECORD       "rec"
#define D_CMND_PS_VSET         "vset"
#define D_CMND_PS_ISET         "iset"
#define D_CMND_PS_VOUT         "vout"
#define D_CMND_PS_IOUT         "iout"
#define D_CMND_PS_OVP          "ovp"
#define D_CMND_PS_OCP          "ocp"
#define D_CMND_PS_VMAX         "vmax"
#define D_CMND_PS_IMAX         "imax"
#define D_CMND_PS_VOLT         "volt"
#define D_CMND_PS_AMP          "amp"

#define D_KORAD                "Korad"

#define D_PS_VOLTAGE           "Voltage"
#define D_PS_CURRENT           "Current"
#define D_PS_CONTROL           "Control"
#define D_PS_GRAPH             "Graph"
#define D_PS_CONFIGURE         "Configure"

// filesystem
#define PS_RECORD_BUFFER       512       // record buffer

// form strings
const char PS_FIELD_START[]  PROGMEM = "<p><fieldset><legend><b> Predefined %s </b></legend>\n";
const char PS_FIELD_STOP[]   PROGMEM = "</fieldset></p><br>\n";
const char PS_INPUT_NUMBER[] PROGMEM = "<span class='half'>%s %d<br><input type='number' name='%s%d' min='%d' max='%d' step='%s' value='%s'></span>\n";
const char PS_HTML_UNIT[]    PROGMEM = "<text class='%s' x='%d%%' y='%d%%'>%s %s</text>\n";
const char PS_HTML_DASH[]    PROGMEM = "<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n";
const char PS_VOLT_CLICK2[]  PROGMEM = "Click twice to set output voltage";
const char PS_VOLT_CLICK[]   PROGMEM = "Set output voltage";
const char PS_AMP_CLICK[]    PROGMEM = "Set max output current";

// web colors
const char D_PS_COLOR_BACKGROUND[]  PROGMEM = "#252525";
const char D_PS_COLOR_VOLTAGE[]     PROGMEM = "yellow";
const char D_PS_COLOR_VOLTAGE_OFF[] PROGMEM = "#606030";
const char D_PS_COLOR_CURRENT[]     PROGMEM = "#6bc4ff";
const char D_PS_COLOR_CURRENT_OFF[] PROGMEM = "#3b6d8e";
const char D_PS_COLOR_WATT[]        PROGMEM = "white";
const char D_PS_COLOR_WATT_OFF[]    PROGMEM = "grey";
const char D_PS_COLOR_POWER[]       PROGMEM = "limegreen";
const char D_PS_COLOR_POWER_OFF[]   PROGMEM = "red";
const char D_PS_COLOR_RECORD[]      PROGMEM = "red";
const char D_PS_COLOR_RECORD_OFF[]  PROGMEM = "#800";

// web URL
const char D_PS_PAGE_CONFIG[]     PROGMEM = "/config";
const char D_PS_PAGE_CONTROL[]    PROGMEM = "/ctrl";
const char D_PS_PAGE_DATA[]       PROGMEM = "/data";
const char D_PS_PAGE_ICON_WIFI[]  PROGMEM = "/wifi.svg";

// power supply units
enum PSUnit { PS_UNIT_V, PS_UNIT_A, PS_UNIT_W };
const char kPSUnit[] PROGMEM = "V|A|W";

// power supply models
enum PSModel                               { PS_MODEL_NONE, PS_MODEL_KORAD, PS_MODEL_KUAIQU, PS_MODEL_MAX };
const uint8_t arr_PSHasOVP[PS_MODEL_MAX] = {        0     ,        1      ,         0      };
const uint8_t arr_PSHasOCP[PS_MODEL_MAX] = {        0     ,        1      ,         0      };

// power supply command cycle stages
enum PSCommandStage { PS_STAGE_START, PS_STAGE_RECEIVE, PS_STAGE_LAST, PS_STAGE_MAX };

// power supply MQTT commands
const char kPSMqttCommands[]            PROGMEM = "ps_" "|" D_CMND_PS_HELP "|" D_CMND_PS_MODEL "|" D_CMND_PS_POWER "|"   D_CMND_PS_VSET  "|"   D_CMND_PS_ISET  "|" D_CMND_PS_OVP "|" D_CMND_PS_OCP "|" D_CMND_PS_RECORD "|" D_CMND_PS_CUSTOM  ;
void (* const arrPSMqttCommand[])(void) PROGMEM = {         &PSCommandHelp  ,  &PSCommandModel  ,  &PSCommandPower  , &PSCommandVoltageSet, &PSCommandCurrentSet,  &PSCommandOVP  ,  &PSCommandOCP  ,  &PSCommandRecord  ,  &PSCommandCustom };

// wifi icon
const char ps_icon_wifi_svg[] PROGMEM = 
"<?xml version='1.0' encoding='iso-8859-1'?>" 
"<svg fill='#000' height='800px' width='800px' xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' version='1.1' viewBox='0 0 489.3 489.3' xml:space='preserve'><g><g>" 
"<path d='M79.55,229.675c-10.2,10.2-10.2,26.8,0,37.1c10.2,10.2,26.8,10.2,37.1,0c70.6-70.6,185.5-70.6,256.1,0 c5.1,5.1,11.8,7.7,18.5,7.7s13.4-2.6,18.5-7.7c10.2-10.2,10.2-26.8,0-37.1C318.75,138.575,170.55,138.575,79.55,229.675z'/>" 
"<path d='M150.35,300.475c-10.2,10.2-10.2,26.8,0,37.1c10.2,10.2,26.8,10.2,37.1,0c31.5-31.6,82.9-31.6,114.4,0 c5.1,5.1,11.8,7.7,18.5,7.7s13.4-2.6,18.5-7.7c10.2-10.2,10.2-26.8,0-37C286.95,248.475,202.35,248.475,150.35,300.475z'/>" 
"<path d='M481.65,157.675c-130.7-130.6-343.3-130.6-474,0c-10.2,10.2-10.2,26.8,0,37.1c10.2,10.2,26.8,10.2,37.1,0  c110.2-110.3,289.6-110.3,399.9,0c5.1,5.1,11.8,7.7,18.5,7.7s13.4-2.6,18.5-7.7C491.85,184.575,491.85,167.975,481.65,157.675z'/>" 
"<circle cx='244.65' cy='394.675' r='34.9'/>" 
"</g></g></svg>";

// power supply commands
enum PSCommandType                                { PS_COMMAND_INIT, PS_COMMAND_STATUS, PS_COMMAND_VREF_GET, PS_COMMAND_VREF_SET, PS_COMMAND_VOUT_GET, PS_COMMAND_IREF_GET, PS_COMMAND_IREF_SET, PS_COMMAND_IOUT_GET,   PS_COMMAND_OUT, PS_COMMAND_OVP, PS_COMMAND_OCP, PS_COMMAND_CUSTOM, PS_COMMAND_NONE };

const char kKoradCommand[] PROGMEM               =      "*IDN?"   "|"    "STATUS?"   "|"     "VSET1?"     "|"       "VSET1:"   "|"      "VOUT1?"    "|"       "ISET1?"   "|"     "ISET1:"     "|"      "IOUT1?"    "|"      "OUT"    "|"     "OVP"   "|"    "OCP"    "|"                 ;
const uint8_t arr_KoradRecvSize[PS_COMMAND_NONE] = {      32       ,         1        ,         5          ,           0        ,          5         ,           5        ,         0          ,          5         ,         0       ,        0      ,       0       ,        32       };

// power supply status
static struct {
  uint8_t  device      = PS_MODEL_KORAD;
  uint16_t arr_voltage_preset[PS_PRESET_VOLTAGE_MAX];     // preset voltage
  uint16_t arr_current_preset[PS_PRESET_CURRENT_MAX];     // preset current
} ps_config;


static struct {
  bool     output      = false;                              // power output status
  bool     flag_cv     = true;                               // constant voltage (true) or current (false) mode
  bool     flag_ovp    = false;                              // over voltage protection status
  bool     flag_ocp    = false;                              // over current protection status
  bool     voltage_new = false;                               // flag to trace voltage change command
  uint16_t voltage_ref = 0;                               // voltage value set
  uint16_t voltage_ask = 0;                                      // voltage value needing confirmation stage
  uint16_t voltage_act = 0;                               // actual voltage output
  uint16_t current_ref = 0;                               // max current value set
  uint16_t current_act = 0;                               // actual current output
  String str_brand;                                       // power supply brand.
  String str_model;                                       // power supply model
  String str_version;                                     // power supply firmware version
  String str_serial;                                      // power supply serial number
} ps_device;

// commands handling
struct ps_command_t {
  uint8_t  command;
  uint16_t value;
};

static struct {
  uint8_t  size_buffer = 0;
  uint8_t  stage_step  = PS_STAGE_START;
  uint32_t stage_time  = 0;                               // current command answer timeout
  ps_command_t arr_command[PS_COMMAND_SIZE];
  char         str_buffer[PS_COMMAND_BUFFER];             // reception buffer
} ps_command;

// recording data
static struct {
  uint32_t start_ms   = 0;                                // timestamp when current recording started
  uint32_t start_time = 0;                                // timestamp when current recording started
  File     file;                                          // handle of recorded file
  String   str_buffer;                                    // recording buffer
} ps_record;

// graph
static struct {
  bool     serving = false;                               // flag when serving page
  uint16_t index;                                         // current array index per refresh period
  uint16_t vmax;                                          // maximum voltage scale
  uint16_t imax;                                          // maximum current scale
  uint16_t arr_voltage[PS_GRAPH_SAMPLE];                  // array of instant voltage (x 100)
  uint16_t arr_current[PS_GRAPH_SAMPLE];                  // array of instant current (x 1000)
} ps_graph; 


/***************************************\
 *         Power supply commands
\***************************************/

uint16_t PSConvertString2Millis (const char* pstr_value)
{
  uint16_t value = UINT16_MAX;
  char*    pstr_digit;
  char     str_value[8]; 
  char     str_digit[8];

  // check parameter
  if (pstr_value == nullptr) return UINT16_MAX;
  if ((strlen (pstr_value) == 0) || (strlen (pstr_value) > 6)) return UINT16_MAX;

  // analyse string
  strlcpy (str_value, pstr_value, sizeof (str_value));
  pstr_digit = strchr (str_value, '.');
  if (pstr_digit != nullptr)
  {
    *pstr_digit = 0;
    strlcpy (str_digit, pstr_digit + 1, sizeof (str_digit));
    while (strlen (str_digit) < 3) strcat (str_digit, "0");
    value = 1000 * atoi (str_value) + atoi (str_digit);
  }
  else value = 1000 * atoi (str_value);

  // return result
  return value;
}

void PSEnableOutput (bool enable)
{
  // switch unit according to model
  switch (ps_config.device)
  {
    case PS_MODEL_KORAD:  KoradSetOutput (enable);  break;
    case PS_MODEL_KUAIQU: KuaiquSetOutput (enable); break;
  }

  // reset new voltage protection step
  ps_device.voltage_new = false;
}

void PSSetOverVoltage (bool enable)
{
  if (enable) PSAppendCommand (PS_COMMAND_OVP, 1);
    else PSAppendCommand (PS_COMMAND_OVP, 0);
}

void PSOverCurrent (bool enable)
{
  if (enable) PSAppendCommand (PS_COMMAND_OCP, 1);
    else PSAppendCommand (PS_COMMAND_OCP, 0);
}

void PSVoltageSet (const char* pstr_voltage) { PSVoltageSet (PSConvertString2Millis (pstr_voltage)); }
void PSVoltageSet (uint16_t voltage_mv)
{
  // check parameter
  if (voltage_mv > 1000 * PS_VOLTAGE_MAX) return;

  // append command
  PSAppendCommand (PS_COMMAND_VREF_SET, voltage_mv);
}

void PSCurrentSet (const char* pstr_current) { PSCurrentSet (PSConvertString2Millis (pstr_current)); }
void PSCurrentSet (uint16_t current_ma)
{
  // check parameter
  if (current_ma > 1000 * PS_CURRENT_MAX) return;

  // append command
  PSAppendCommand (PS_COMMAND_IREF_SET, current_ma);
}

/*********************************************\
 *                MQTT Commands
\*********************************************/

void PSCommandHelp ()
{
  AddLog (LOG_LEVEL_INFO,   PSTR ("HLP: Power Supply commands :"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_model <%u>      = set device model"), ps_config.device);
  AddLog (LOG_LEVEL_INFO,   PSTR ("   1 : Korad"));
  AddLog (LOG_LEVEL_INFO,   PSTR ("   2 : Kuaiqu"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_power <on/off>  = enable power supply"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_vset <voltage>  = set output voltage"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_iset <current>  = set output max current"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_ovp <on/off>    = enable over voltage protection"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_ocp <on/off>    = enable over current protection"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_record <on/off> = start/stop recording"));
  AddLog (LOG_LEVEL_INFO,   PSTR (" - ps_custom <cmnd>   = send a custom command"));

  ResponseCmndDone();
}

void PSCommandCustom ()
{
  bool is_valid;

  is_valid = (strlen (XdrvMailbox.data) > 0);
  if (is_valid)
  {
    // reset command list
    PSCommandReset ();

    // set custom command
    strlcpy (ps_command.str_buffer, XdrvMailbox.data, PS_COMMAND_BUFFER);
    PSAppendCommand (PS_COMMAND_CUSTOM, 0);
  }

  if (is_valid) ResponseCmndDone ();
    else ResponseCmndFailed ();
}

void PSCommandModel ()
{
  if ((XdrvMailbox.payload >= PS_MODEL_NONE) && (XdrvMailbox.payload < PS_MODEL_MAX)) ps_config.device = (uint8_t)XdrvMailbox.payload;
  ResponseCmndNumber (ps_config.device);
}

void PSCommandPower ()
{
  bool enable;

  enable = ((strcasecmp_P (XdrvMailbox.data, PSTR ("on")) == 0) || (XdrvMailbox.payload == 1));
  PSEnableOutput (enable);
  ResponseCmndDone ();
}

void PSCommandVoltageSet ()
{
  uint16_t value;
  char     str_value[8];

  // convert value
  value = PSConvertString2Millis (XdrvMailbox.data);
  if (value != UINT16_MAX) PSVoltageSet (value);

  // return result
  sprintf (str_value, "%u.%03u", ps_device.voltage_ref / 1000, ps_device.voltage_ref % 1000);
  ResponseCmndChar (str_value);
}

void PSCommandCurrentSet ()
{
  uint16_t value;
  char     str_value[8];

  // convert value
  value = PSConvertString2Millis (XdrvMailbox.data);
  if (value != UINT16_MAX) PSCurrentSet (value);

  // return result
  sprintf (str_value, "%u.%03u", ps_device.current_ref / 1000, ps_device.current_ref % 1000);
  ResponseCmndChar (str_value);
}

void PSCommandOVP ()
{
  PSSetOverVoltage ((strcasecmp (XdrvMailbox.data, "on") == 0) || (XdrvMailbox.payload == 1));
  ResponseCmndDone ();
}

void PSCommandOCP ()
{
  PSOverCurrent ((strcasecmp (XdrvMailbox.data, "on") == 0) || (XdrvMailbox.payload == 1));
  ResponseCmndDone ();
}

void PSCommandRecord ()
{
  if ((strcasecmp (XdrvMailbox.data, "on") == 0) || (XdrvMailbox.payload == 1)) PSRecordStart ();
    else PSRecordStop ();
  ResponseCmndDone ();
}

/***************************************\
 *           Generic Functions
\***************************************/

void PSCommandReset ()
{
  uint8_t index;

  // empty command list
  for (index = 0; index < PS_COMMAND_SIZE; index ++)
  {
    ps_command.arr_command[index].command = UINT8_MAX;
    ps_command.arr_command[index].value   = 0;
  }

  // reset current command data
  ps_command.stage_step    = PS_STAGE_START;
  ps_command.size_buffer   = 0;
  ps_command.str_buffer[0] = 0;
}

void PSLoadConfig () 
{
  uint8_t index;

  // load power supply family
  ps_config.device = Settings->knx_GA_param[0];

  // load reference voltage and current
  ps_device.voltage_ref = Settings->knx_GA_addr[0];
  ps_device.current_ref = Settings->knx_CB_addr[0];

  // load graph limits
  ps_graph.vmax = Settings->knx_GA_addr[1];
  ps_graph.imax = Settings->knx_CB_addr[1];

  // load presets
  for (index = 0; index < PS_PRESET_VOLTAGE_MAX; index++) ps_config.arr_voltage_preset[index] = Settings->knx_GA_addr[index + 2];
  for (index = 0; index < PS_PRESET_CURRENT_MAX; index++) ps_config.arr_current_preset[index] = Settings->knx_CB_addr[index + 2];

  // check boundaries
  if (ps_device.voltage_ref == 0) ps_device.voltage_ref = 3300;
  if (ps_device.current_ref == 0) ps_device.current_ref = 500;
  if (ps_graph.vmax == 0) ps_graph.vmax = PS_VOLTAGE_MAX * 1000;
  if (ps_graph.imax == 0) ps_graph.imax = PS_CURRENT_MAX * 1000;
}

void PSSaveConfig () 
{
  uint8_t index;

  // save power supply family
  Settings->knx_GA_param[0] = ps_config.device;

  // save reference voltage and current
  Settings->knx_GA_addr[0] = ps_device.voltage_ref;
  Settings->knx_CB_addr[0] = ps_device.current_ref;

  // save graph limits
  Settings->knx_GA_addr[1] = ps_graph.vmax;
  Settings->knx_CB_addr[1] = ps_graph.imax;

  // save presets
  for (index = 0; index < PS_PRESET_VOLTAGE_MAX; index++) Settings->knx_GA_addr[index + 2] = ps_config.arr_voltage_preset[index];
  for (index = 0; index < PS_PRESET_CURRENT_MAX; index++) Settings->knx_CB_addr[index + 2] = ps_config.arr_current_preset[index];
}

void PSRecordStart ()
{
  TIME_T dst_start;
  char   str_filename[UFS_FILENAME_SIZE];

  if (ps_record.start_time == 0)
  {
    // save recording start time
    ps_record.start_ms   = millis ();
    ps_record.start_time = LocalTime ();

    // calculate current timestamp
    BreakTime (ps_record.start_time, dst_start);
    sprintf (str_filename, "/%04u%02u%02u-%02u%02u%02u.csv", dst_start.year + 1970, dst_start.month, dst_start.day_of_month, dst_start.hour, dst_start.minute, dst_start.second);

    // record header
    ps_record.file = ffsp->open (str_filename, "w");
    ps_record.str_buffer = "Msec;VSet;Vout;ISet;Iout\n";

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("PSU: Record Start (%s)"), str_filename);
  }
}

void PSRecordStop ()
{
  if (ps_record.start_time != 0)
  {
    // if buffer is not empty, record it
    if (ps_record.str_buffer.length () > 0) ps_record.file.print (ps_record.str_buffer.c_str ());

    // reset recording data and close file
    ps_record.str_buffer = "";
    ps_record.start_time = 0;
    ps_record.start_ms   = 0;
    ps_record.file.close ();

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("PSU: Record Stop"));
  }
}

void PSGetRecordingDuration (char *pstr_time, size_t size_time)
{
  uint32_t duration;
  TIME_T   duration_dst;
  char     str_time[16];

  // check parameters
  if ((pstr_time == nullptr) || (size_time < 6)) return;

  // init
  strcpy (pstr_time, "");

  // if recording has started
  if (ps_record.start_time != 0)
  {
    // calculate current timestamp
    duration = LocalTime () - ps_record.start_time;
    BreakTime (duration, duration_dst);

    // generate time string
    if (duration_dst.hour > 0) sprintf (pstr_time, "%u:", duration_dst.hour);
    sprintf (str_time, "%02u:%02u", duration_dst.minute, duration_dst.second);
    strlcat (pstr_time, str_time, size_time);
  }
}

void PSGenerateValueLabel (char* pstr_value, char* pstr_unit, const uint16_t value, const uint8_t num_digit, const uint8_t unit_type)
{
  uint8_t  index, len_unit, len_digit;
  uint16_t unit, digit;
  char     str_text[8];

  // get unit and digit
  unit  = value / 1000;
  digit = value % 1000;

  // check parameters
  if ((pstr_value == nullptr) || (pstr_unit == nullptr)) return;
  if (unit > 99) return;

  // init
  strcpy (pstr_value, "");
  strcpy (pstr_unit, "");

  // if unit is present
  if (unit > 0) 
  {
    // convert unit
    sprintf (pstr_value, "%u", unit);

    // convert digit according to expected length
    if (num_digit == 1) digit = digit / 100;
      else if (num_digit == 2) digit = digit / 10;
    sprintf (str_text, "%u", digit);
    len_digit = strlen (str_text);

    // generate final string
    strcat (pstr_value, ".");
    for (index = 0; index < num_digit - len_digit; index ++) strcat (pstr_value, "0");
    strcat (pstr_value, str_text);
  }

  // else dealing with millis only
  else
  {
    sprintf (pstr_value, "%u", digit);
    strcpy (pstr_unit, "m");
  }

  GetTextIndexed (str_text, sizeof (str_text), unit_type, kPSUnit);
  strcat (pstr_unit, str_text);
}

// append command to execute
void PSAppendCommand (const uint8_t command, const uint16_t value)
{
  uint8_t index;

  for (index = 0; index < PS_COMMAND_SIZE; index ++)
    if (ps_command.arr_command[index].command == UINT8_MAX)
    {
      ps_command.arr_command[index].command = command;
      ps_command.arr_command[index].value   = value;
      break;
    }
}

// get next command to execute
void PSShiftToNextCommand ()
{
  uint8_t index;

  // set initial stage
  ps_command.stage_step    = PS_STAGE_START;
  ps_command.size_buffer   = 0;
  ps_command.str_buffer[0] = 0;
  
  // shift command from 1 to 9
  for (index = 0; index < PS_COMMAND_SIZE - 1; index ++)
  {
    ps_command.arr_command[index].command = ps_command.arr_command[index + 1].command;
    ps_command.arr_command[index].value   = ps_command.arr_command[index + 1].value;
  }

  // reset last command
  ps_command.arr_command[PS_COMMAND_SIZE - 1].command = UINT8_MAX;
  ps_command.arr_command[PS_COMMAND_SIZE - 1].value   = 0;

  // if no pending command and device ON, append current and voltage reading
  if (ps_device.output && (ps_command.arr_command[0].command == UINT8_MAX))
  {
    PSAppendCommand (PS_COMMAND_IOUT_GET, 0);
    PSAppendCommand (PS_COMMAND_VOUT_GET, 0);
  }
}

// Show JSON status (for MQTT)
void PSShowJSON (bool append)
{
  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // Korad data  -->  "Korad":{"Model":"model","Version":"version","VSET":5.0,"ISET":1.01,"VOUT":4.99,"IOUT"=0.22,"OUT"=1,"OVP":1,"OCP":0}
  ResponseAppend_P (PSTR ("\"PS\":{"));

  // VSET and ISET
  ResponseAppend_P (PSTR ("\"Vref\":%u.%03u,\"Iref\":%u.%03u"), ps_device.voltage_ref / 1000, ps_device.voltage_ref % 1000, ps_device.current_ref / 1000, ps_device.current_ref % 1000);

  // VOUT and IOUT
  ResponseAppend_P (PSTR (",\"Vout\":%u.%03u,\"Iout\":%u.%03u"), ps_device.voltage_act / 1000, ps_device.voltage_act % 1000, ps_device.current_act / 1000, ps_device.current_act % 1000);

  // Power, OVP and OCP
  ResponseAppend_P (PSTR (",\"Pout\":%d,\"OVP\":%d,\"OCP\":%d"), ps_device.output, ps_device.flag_ovp, ps_device.flag_ocp);

  // model and version
  if (append) ResponseAppend_P (PSTR (",\"Brand\":\"%s\",\"Model\":\"%s\",\"Version\":\"%s\",\"Serial\":\"%s\","), ps_device.str_brand.c_str (), ps_device.str_model.c_str (), ps_device.str_version.c_str (), ps_device.str_serial.c_str ());

  // publish it if not in append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishTeleSensor ();
  } 
}

/***************************************\
 *       Korad Specific Functions
\***************************************/

// korad initialisation
void KoradInit ()
{
  // get identification and set reference voltage and current
  PSAppendCommand (PS_COMMAND_INIT,     0);
  PSAppendCommand (PS_COMMAND_STATUS,   0);
  PSAppendCommand (PS_COMMAND_VREF_SET, ps_device.voltage_ref);
  PSAppendCommand (PS_COMMAND_IREF_SET, ps_device.current_ref);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("PSU: Init KORAD")); 
}

// Korad start/stop unit
void KoradSetOutput (const bool enable)
{
  if (enable) PSAppendCommand (PS_COMMAND_OUT, 1);
    else PSAppendCommand (PS_COMMAND_OUT, 0);

    // update voltage and current
    PSAppendCommand (PS_COMMAND_IOUT_GET, 0);
    PSAppendCommand (PS_COMMAND_VOUT_GET, 0);
}

// Korad get PSU status
void KoradGetStatus ()
{
  PSAppendCommand (PS_COMMAND_STATUS, 0);
}

// korad command handling
bool KoradCommandSend (const uint8_t command, const uint16_t value)
{
  bool result;
  char str_value[8];
  char str_command[16];

  // send command
  str_command[0] = 0;
  switch (command)
  {
    case PS_COMMAND_INIT:
    case PS_COMMAND_STATUS:
    case PS_COMMAND_VREF_GET:
    case PS_COMMAND_VOUT_GET:
    case PS_COMMAND_IREF_GET:
    case PS_COMMAND_IOUT_GET:
      GetTextIndexed (str_command, sizeof (str_command), command, kKoradCommand);
      break;

    case PS_COMMAND_VREF_SET:
    case PS_COMMAND_IREF_SET:
      sprintf (str_value, "%u.%03u", value / 1000, value % 1000);
      GetTextIndexed (str_command, sizeof (str_command), command, kKoradCommand);
      strlcat (str_command, str_value, sizeof (str_command));
      break;

    case PS_COMMAND_OUT:
    case PS_COMMAND_OVP:
    case PS_COMMAND_OCP:
      sprintf (str_value, "%u", value);
      GetTextIndexed (str_command, sizeof (str_command), command, kKoradCommand);
      strlcat (str_command, str_value, sizeof (str_command));
      break;

    case PS_COMMAND_CUSTOM:
      strcpy_P (str_command, ps_command.str_buffer);
      break;
    }

  // if command is defined
  result = (strlen (str_command) > 0);
  if (result) 
  {
    // write command
    Serial.write (str_command);

    // set flag
    if (command == PS_COMMAND_VREF_SET) ps_device.voltage_new = true;
  }

  return result;
}

bool KoradCommandReceive ()
{
  bool    is_valid, result;
  uint8_t command, index;
  char    c_read;
  char   *pstr_token;

  // get current command
  command = ps_command.arr_command[0].command;

  // receive all pending data
  while ((ps_command.size_buffer < sizeof (ps_command.str_buffer)) && (Serial.available () > 0))
  {
    c_read = Serial.read ();
    ps_command.size_buffer++;
    if (command == PS_COMMAND_STATUS) is_valid = true;
      else is_valid = (isprint (c_read) != 0);
    if (is_valid) strncat (ps_command.str_buffer, &c_read, 1);
  }

  // check if we have received expected answer size or if timeout is reached
  result = ((ps_command.size_buffer >= arr_KoradRecvSize[command]) || (ps_command.stage_time + PS_KORAD_TIMEOUT < millis ()));

  // if reception is over, handle message
  if (result)
  {
    // log
    if (command == PS_COMMAND_STATUS) AddLog (LOG_LEVEL_DEBUG, PSTR ("PSU: Recv 0x%X"), ps_command.str_buffer[0]);
      else AddLog (LOG_LEVEL_DEBUG, PSTR ("PSU: Recv %s"), ps_command.str_buffer);

    // handle answer according to command
    switch (command)
    {
      case PS_COMMAND_INIT:
        // device identifier is DEVICE MODEL DEVICE VERSION separated by spaces
        pstr_token = strtok (ps_command.str_buffer, " ");

        // loop to populate model and version using SPACE separator
        index = 0;
        while (pstr_token != nullptr) 
        {
          // update data
          if (index == 0) ps_device.str_brand = pstr_token;
          else if (index == 1) ps_device.str_model = pstr_token;
          else if (index == 2) ps_device.str_version = pstr_token;
          else ps_device.str_serial = pstr_token;
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
      case PS_COMMAND_STATUS:
        // set flags accordingly      
        ps_device.flag_cv  = ((ps_command.str_buffer[0] & 0x01) != 0);          // constant voltage (on) / current (off)
        ps_device.flag_ocp = ((ps_command.str_buffer[0] & 0x20) != 0);          // OCP status (on/off)
        ps_device.flag_ovp = ((ps_command.str_buffer[0] & 0x80) != 0);          // OVP status (on/off)
        ps_device.output   = ((ps_command.str_buffer[0] & 0x40) != 0);          // power output status (on/off)

        // if output switched OFF during voltage change, switch it back ON
        if (ps_device.voltage_new && (ps_device.output == 0)) PSEnableOutput (1);
        break;

      // convert reference voltage
      case PS_COMMAND_VREF_GET:
        if (ps_command.str_buffer[2] == '.')
        {
          ps_command.str_buffer[2] = 0;
          ps_command.str_buffer[5] = 0;
          ps_device.voltage_ref    = 1000 * atoi (ps_command.str_buffer) + 10 * atoi (ps_command.str_buffer + 3);
        }
        break;

      // convert output voltage
      case PS_COMMAND_VOUT_GET:
        if (ps_command.str_buffer[2] == '.')
        {
          ps_command.str_buffer[2] = 0;
          ps_command.str_buffer[5] = 0;
          ps_device.voltage_act    = 1000 * atoi (ps_command.str_buffer) + 10 * atoi (ps_command.str_buffer + 3);
        }
        break;

      // convert reference current
      case PS_COMMAND_IREF_GET:
        if (ps_command.str_buffer[1] == '.')
        {
          ps_command.str_buffer[1] = 0;
          ps_command.str_buffer[5] = 0;
          ps_device.current_ref    = 1000 * atoi (ps_command.str_buffer) + atoi (ps_command.str_buffer + 2);
        }
        break;

      // convert output current
      case PS_COMMAND_IOUT_GET:
        if (ps_command.str_buffer[1] == '.')
        {
          ps_command.str_buffer[1] = 0;
          ps_command.str_buffer[5] = 0;       // handle ISET1? 6th caracter bug after *IDN? command
          ps_device.current_act    = 1000 * atoi (ps_command.str_buffer) + atoi (ps_command.str_buffer + 2);
        }
        break;
    }
  }

  return result;
}

bool KoradCommandFinish ()
{
  return true;  
}

/***************************************\
 *       Kuaiqu Specific Functions
\***************************************/

// Kuaiqu initialisation
void KuaiquInit ()
{
  // set brand
  ps_device.str_brand = "Kuaiqu";

  // read voltage to get PSU status
  PSAppendCommand (PS_COMMAND_VOUT_GET, 0);

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("PSU: Init KUAIQU")); 
}

// Kuaiqu start/stop unit
void KuaiquSetOutput (const bool enable)
{
  if (enable)
  {
    PSAppendCommand (PS_COMMAND_INIT, 1);
    PSAppendCommand (PS_COMMAND_VREF_SET, ps_device.voltage_ref);
    PSAppendCommand (PS_COMMAND_IREF_SET, ps_device.current_ref);
    PSAppendCommand (PS_COMMAND_OUT,  1);
  }

  else
  {
    PSAppendCommand (PS_COMMAND_OUT,  0);
    PSAppendCommand (PS_COMMAND_INIT, 0);
  }

  // update voltage and current
  PSAppendCommand (PS_COMMAND_IOUT_GET, 0);
  PSAppendCommand (PS_COMMAND_VOUT_GET, 0);
}

// Kuaiqu get PSU status
void KuaiquGetStatus ()
{
  // no command to read status
}

// Kuaiqu command handling
bool KuaiquCommandSend (const uint8_t command, const uint16_t value)
{
  bool result;
  char str_command[16];

  // send command
  str_command[0] = 0;
  switch (command)
  {
    case PS_COMMAND_INIT:
      if (value) strcpy_P (str_command, PSTR ("<09100000000>"));
        else strcpy_P (str_command, PSTR ("<09200000000>"));
      break;

    case PS_COMMAND_VOUT_GET:
      strcpy_P (str_command, PSTR ("<02000000000>"));
      break;

    case PS_COMMAND_IOUT_GET:
      strcpy_P (str_command, PSTR ("<04000000000>"));
      break;

    case PS_COMMAND_VREF_SET:
      sprintf_P (str_command, PSTR ("<01%03u%03u000>"), value / 1000, value % 1000);
      ps_device.voltage_ref = value;
      break;

    case PS_COMMAND_IREF_SET:
      sprintf_P (str_command, PSTR ("<03%03u%03u000>"), value / 1000, value % 1000);
      ps_device.current_ref = value;
      break;

    case PS_COMMAND_OUT:
      if (value) strcpy_P (str_command, PSTR ("<07000000000>"));
        else strcpy_P (str_command, PSTR ("<08000000000>"));
      ps_device.output = (value != 0);
      break;

    case PS_COMMAND_CUSTOM:
      strcpy_P (str_command, ps_command.str_buffer);
      break;
    }

  // if command is defined
  result = (strlen (str_command) > 0);
  if (result)
  {
    // send command
    Serial.write (str_command);

    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("KUA: Sent %s"), str_command);
  }

  return result;
}

// Kuaiqu serial reception
bool KuaiquCommandReceive ()
{
  bool    result;
  uint8_t command;
  char    c_read;
  char   *pstr_start;
  char   *pstr_end;

  // get current command
  command = ps_command.arr_command[0].command;

  // receive all pending data
  result = false;
  while (!result && (ps_command.size_buffer < sizeof (ps_command.str_buffer)) && (Serial.available () > 0))
  {
    c_read = Serial.read ();
    strncat (ps_command.str_buffer, &c_read, 1);
    ps_command.size_buffer++;
    if (c_read == '>') result = true;
  }

  // check if we have received expected answer size or if timeout is reached
  if (!result) result = (ps_command.stage_time + PS_KUAIQU_TIMEOUT < millis ());

  // if reception is over, handle message
  if (result)
  {
    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("KUA: Recv %s"), ps_command.str_buffer);

    // handle answer according to command
    switch (command)
    {
      case PS_COMMAND_INIT:
        break;

      // read output voltage
      case PS_COMMAND_VOUT_GET:
        pstr_start = ps_command.str_buffer + 3;
        pstr_end = pstr_start + 6;
        *pstr_end = 0;
        ps_device.voltage_act = (uint16_t)atoi (pstr_start);
        if (ps_device.voltage_act > 0) ps_device.output = true;
        break;

      // read output current
      case PS_COMMAND_IOUT_GET:
        // cc/cv
        ps_device.flag_cv = (ps_command.str_buffer[1] != 'C');

        // current
        pstr_start = ps_command.str_buffer + 3;
        pstr_end = pstr_start + 6;
        *pstr_end = 0;
        ps_device.current_act = (uint16_t)atoi (pstr_start);
        break;
    }
  }

  return result;
}

bool KuaiquCommandFinish ()
{
  return true;  
}

/***************************************\
 *          Generic callback
\***************************************/

void PSPreInit ()
{
  int index;

  // disable fast cycle power recovery (SetOption65)
  Settings->flag3.fast_power_cycle_disable = true;

  // check if serial ports are selected 
  if (PinUsed (GPIO_TXD) && PinUsed (GPIO_RXD)) 
  {
    // start ESP8266 serial port
    Serial.flush ();
    Serial.begin (9600, SERIAL_8N1);
    ClaimSerial ();

    // next step is initilisation
    AddLog (LOG_LEVEL_INFO, PSTR ("PSU: Init 9600 bauds, 8N1"));
  }
}

void PSInit ()
{
  int index;

  // init status strings
  ps_device.str_brand   = "Unknown";
  ps_device.str_model   = "";
  ps_device.str_version = "";
  ps_device.str_serial  = "";

  // init command array
  PSCommandReset ();

  // init graph data
  ps_graph.index = 0;
  if (ps_graph.vmax < 1000) ps_graph.vmax = 1000;
  if (ps_graph.imax < 500)  ps_graph.imax = 500;
  for (index = 0; index < PS_GRAPH_SAMPLE; index++)
  {
    ps_graph.arr_current[index] = UINT16_MAX;
    ps_graph.arr_voltage[index] = UINT16_MAX;
  } 

  // load configuration
  PSLoadConfig ();

  // init according to power supply family
  switch (ps_config.device)
  {
    case PS_MODEL_KORAD:  KoradInit ();  break;
    case PS_MODEL_KUAIQU: KuaiquInit (); break;
  }
}

// Receive serial messages and handle them 20 times per second
void PSUpdateStatus ()
{
  bool result;

  // check start delay
  if (TasmotaGlobal.uptime < PS_INIT_TIMEOUT) return;

  // if needed, shift to next command
  if (ps_command.arr_command[0].command == UINT8_MAX) PSShiftToNextCommand ();

  // if no pending command, ignore
  if (ps_command.arr_command[0].command == UINT8_MAX) return;

  // if command not defined
  result = false;
  switch (ps_command.stage_step)
  {
    case PS_STAGE_START:
      // log
      AddLog (LOG_LEVEL_DEBUG, PSTR ("PSU: Cmnd %u=%u"), ps_command.arr_command[0].command, ps_command.arr_command[0].value);

      // empty reception buffer
      while (Serial.available () > 0) Serial.read ();

      // send command
      switch (ps_config.device)
      {
        case PS_MODEL_KORAD:  result = KoradCommandSend  (ps_command.arr_command[0].command, ps_command.arr_command[0].value); break;
        case PS_MODEL_KUAIQU: result = KuaiquCommandSend (ps_command.arr_command[0].command, ps_command.arr_command[0].value); break;
      }

//      if (result)
//      {
        // change state and set timeout
        ps_command.size_buffer   = 0;
        ps_command.str_buffer[0] = 0;
        ps_command.stage_time    = millis ();
        ps_command.stage_step    = PS_STAGE_RECEIVE;
//      }
      break;

    case PS_STAGE_RECEIVE:

      switch (ps_config.device)
      {
        case PS_MODEL_KORAD:  result = KoradCommandReceive ();  break;
        case PS_MODEL_KUAIQU: result = KuaiquCommandReceive (); break;
      }

      // if reception is done, set last stage
      if (result)
      {
        ps_command.stage_step    = PS_STAGE_LAST;
        ps_command.stage_time    = millis ();
        ps_command.str_buffer[0] = 0;
      }
      break;

    case PS_STAGE_LAST:

      switch (ps_config.device)
      {
        case PS_MODEL_KORAD:  result = KoradCommandFinish ();  break;
        case PS_MODEL_KUAIQU: result = KuaiquCommandFinish (); break;
      }

      // if last stage is succesfull, shift to next command
      if (result) PSShiftToNextCommand ();
      break;
    }
}

// Record data to filesystem
void PSHandleRecord ()
{
  char str_line[32];

  // check start delay
  if (TasmotaGlobal.uptime < PS_INIT_TIMEOUT) return;

  // update graph data for current and voltage
  ps_graph.arr_voltage[ps_graph.index] = ps_device.voltage_act;
  ps_graph.arr_current[ps_graph.index] = ps_device.current_act;
  ps_graph.index++;
  ps_graph.index = ps_graph.index % PS_GRAPH_SAMPLE;

  // if recording
  if (ps_record.start_time != 0)
  {
    // save data as "millis;vset;vout;iset;iout"
    sprintf_P (str_line, PSTR ("%u;%u.%02u;%u.%02u;%u.%03u;%u.%03u\n"), millis () - ps_record.start_ms, ps_device.voltage_ref / 1000, ps_device.voltage_ref % 1000, ps_device.voltage_act / 1000, ps_device.voltage_act % 1000, ps_device.current_ref / 1000, ps_device.current_ref % 1000, ps_device.current_act / 1000, ps_device.current_act % 1000);

    // append to recording buffer
    ps_record.str_buffer += str_line;
    if (ps_record.str_buffer.length () > PS_RECORD_BUFFER)
    {
      ps_record.file.print (ps_record.str_buffer.c_str ());
      ps_record.str_buffer = "";
    }
  }
}

// if output is enabled, send JSON status every second
void PSEverySecond ()
{
  // check start delay
  if (TasmotaGlobal.uptime < PS_INIT_TIMEOUT) return;

  // ask for PSU status every 2 seconds
  if (RtcTime.second % 2)
    switch (ps_config.device)
    {
      case PS_MODEL_KORAD:  KoradGetStatus ();  break;
      case PS_MODEL_KUAIQU: KuaiquGetStatus (); break;
    }
}

/**************************************\
 *                   Web
\**************************************/

#ifdef USE_WEBSERVER

// append relay status to main page
void PSWebSensor ()
{
  uint32_t percentage;
  char     str_unit[8];
  char     str_ref[8];

  // style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("div.section {padding:2px 6px 6px 6px;margin-bottom:5px;background:#333333;border-radius:12px;font-size:12px;}\n"));
  WSContentSend_P (PSTR ("div.line{display:flex;padding:2px 0px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.line div{padding:0px;}\n"));
  WSContentSend_P (PSTR ("div.side {width:22%%;font-size:18px;}\n"));
  WSContentSend_P (PSTR ("div.head {width:56%%;font-size:16px;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.half{width:50%%;}\n"));
  WSContentSend_P (PSTR ("div.bar{margin:0px;height:20px;}\n"));
  WSContentSend_P (PSTR ("div.barh{width:2%%;text-align:left;}\n"));
  WSContentSend_P (PSTR ("div.barm{width:76%%;text-align:left;background-color:#252525;border-radius:6px;}\n"));
  WSContentSend_P (PSTR ("div.barv{height:20px;padding:0px;text-align:center;border-radius:6px;opacity:0.65;}\n"));
  WSContentSend_P (PSTR ("div.bars{width:6%%;}\n"));
  WSContentSend_P (PSTR ("div.baru{width:16%%;text-align:left;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // section start
  WSContentSend_P (PSTR ("<div class='section'>\n"));

  // model
  WSContentSend_P (PSTR ("<div class='line'><div class='side'></div><div class='head'>"));
  if (ps_device.str_model.length () > 0) WSContentSend_P (PSTR ("%s %s"), ps_device.str_brand.c_str (), ps_device.str_model.c_str ());
    else WSContentSend_P (PSTR ("%s"), ps_device.str_brand.c_str ());
  if (ps_device.output) strcpy_P (str_unit, PSTR ("🟢")); else strcpy_P (str_unit, PSTR ("🔴"));
  WSContentSend_P (PSTR ("</div><div class='side'>%s</div></div>\n"), str_unit);

  // version
  if (ps_device.str_version.length () > 0) WSContentSend_P (PSTR ("<div class='line'><div class='half'>%s</div><div class='half'>%s</div></div>\n"), ps_device.str_version.c_str (), ps_device.str_serial.c_str ());

  // separator
  WSContentSend_P (PSTR ("<hr>\n"));

  // voltage
  if (ps_device.output) PSGenerateValueLabel (str_ref, str_unit, ps_device.voltage_act, 2, PS_UNIT_V);
    else PSGenerateValueLabel (str_ref, str_unit, ps_device.voltage_ref, 2, PS_UNIT_V);
  percentage = 0;
  if (ps_device.output && (ps_device.voltage_ref > 0)) percentage = min (100UL, 100UL * (uint32_t)ps_device.voltage_act / (uint32_t)ps_device.voltage_ref);
  WSContentSend_P (PSTR ("<div class='line bar'><div class='barh'></div><div class='barm'><div class='barv' style='width:%u%%;background-color:%s;'></div></div><div class='bars'></div><div class='baru'>%s %s</div></div>\n"), percentage, PSTR ("#1db000"), str_ref, str_unit);

  // current
  if (ps_device.output) PSGenerateValueLabel (str_ref, str_unit, ps_device.current_act, 2, PS_UNIT_A);
    else PSGenerateValueLabel (str_ref, str_unit, ps_device.current_ref, 2, PS_UNIT_A);
  percentage = 0;
  if (ps_device.output && (ps_device.current_ref > 0)) percentage = min (100UL, 100UL * (uint32_t)ps_device.current_act / (uint32_t)ps_device.current_ref);
  WSContentSend_P (PSTR ("<div class='line bar'><div class='barh'></div><div class='barm'><div class='barv' style='width:%u%%;background-color:%s;'></div></div><div class='bars'></div><div class='baru'>%s %s</div></div>\n"), percentage, PSTR ("#1fa3ec"), str_ref, str_unit);
  
  // section end
  WSContentSend_P (PSTR ("</div>\n"));
}

// Configuration web page
void PSWebPageConfig ()
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
    for (index = 0; index < PS_PRESET_VOLTAGE_MAX; index ++)
    {
      sprintf (str_key, "%s%d", D_CMND_PS_VOLT, index);
      WebGetArg (str_key, str_value, sizeof (str_value));
      if (strlen (str_value) > 0) ps_config.arr_voltage_preset[index] = PSConvertString2Millis (str_value);
    }

    // loop thru predefined current
    for (index = 0; index < PS_PRESET_CURRENT_MAX; index ++)
    {
      sprintf (str_key, "%s%d", D_CMND_PS_AMP, index);
      WebGetArg (str_key, str_value, sizeof (str_value));
      if (strlen (str_value) > 0) ps_config.arr_current_preset[index] = PSConvertString2Millis (str_value);
    }

    // save config
    PSSaveConfig ();
  }

  // beginning of form
  WSContentStart_P (D_PS_CONFIGURE);
  WSContentSendStyle ();

  // style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("span.half {display:inline-block;width:47%%;margin-right:2%%;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PS_PAGE_CONFIG);

  // loop thru predefined voltage
  WSContentSend_P (PS_FIELD_START, D_PS_VOLTAGE);
  for (index = 0; index < PS_PRESET_VOLTAGE_MAX; index ++)
  {
    // get value
    sprintf_P (str_value, PSTR ("%u.%02u"), ps_config.arr_voltage_preset[index] / 1000, (ps_config.arr_voltage_preset[index] % 1000) / 10);

    // display input field
    if (index % 2 == 0) WSContentSend_P (PSTR ("<p>\n"));
    WSContentSend_P (PS_INPUT_NUMBER, D_PS_VOLTAGE, index + 1, D_CMND_PS_VOLT, index, 0, 30, "0.01", str_value);
    if (index % 2 == 1) WSContentSend_P (PSTR ("</p>\n"));
  }
  WSContentSend_P (PS_FIELD_STOP);

  // loop thru predefined voltage
  WSContentSend_P (PS_FIELD_START, D_PS_CURRENT);
  for (index = 0; index < PS_PRESET_CURRENT_MAX; index ++)
  {
    // get value
    sprintf_P (str_value, PSTR ("%u.%03u"), ps_config.arr_current_preset[index] / 1000, ps_config.arr_current_preset[index] % 1000);

    // display input field
    if (index % 2 == 0) WSContentSend_P (PSTR ("<p>\n"));
    WSContentSend_P (PS_INPUT_NUMBER, D_PS_CURRENT, index + 1, D_CMND_PS_AMP, index, 0, 3, "0.001", str_value);
    if (index % 2 == 1) WSContentSend_P (PSTR ("</p>\n"));
  }
  WSContentSend_P (PS_FIELD_STOP);

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
//  format : vout(on/off);cv(on/off);ovp(on/off);ocp(on/off);vout;vout-unit(V/mV);iout;iout-unit(A/mA);wout;wout-unit(W/mW);
//           record(on/off);record-duration(hh:mm:ss);voltage polyline;current path
void PSWebPageData ()
{
  uint16_t index, index_array;
  uint32_t power;
  long     graph_left, graph_right, graph_width;  
  long     unit_width;  
  long     graph_value, graph_x, graph_y, last_x, last_y; 
  long     graph_vmax, graph_imax; 
  char     str_unit[8];
  char     str_data[32];
  String   str_result;

  // start stream
  WSContentBegin (200, CT_PLAIN);

  if (!ps_graph.serving)
  {
    // set flag
    ps_graph.serving = true;

    // power switch status 
    if (ps_device.output) str_result = "on"; else str_result = "off";

    // constant voltage flag 
    if (ps_device.flag_cv) str_result += ";on"; else str_result += ";off";

    // over voltage protection status 
    if (arr_PSHasOVP[ps_config.device] == 0) str_result += ";none";
      else if (ps_device.flag_ovp) str_result += ";on";
      else str_result += ";off";

    // over current protection status 
    if (arr_PSHasOVP[ps_config.device] == 0) str_result += ";none";
      else if (ps_device.flag_ocp) str_result += ";on";
      else str_result += ";off";

    // voltage reference status
    PSGenerateValueLabel (str_data, str_unit, ps_device.voltage_ref, 2, PS_UNIT_V);
    str_result += ";"; str_result += str_data;
    str_result += ";"; str_result += str_unit;

    // current reference status
    PSGenerateValueLabel (str_data, str_unit, ps_device.current_ref, 3, PS_UNIT_A);
    str_result += ";"; str_result += str_data;
    str_result += ";"; str_result += str_unit;

    // voltage output status
    PSGenerateValueLabel (str_data, str_unit, ps_device.voltage_act, 2, PS_UNIT_V);
    str_result += ";"; str_result += str_data;
    str_result += ";"; str_result += str_unit;

    // current output status
    PSGenerateValueLabel (str_data, str_unit, ps_device.current_act, 3, PS_UNIT_A);
    str_result += ";"; str_result += str_data;
    str_result += ";"; str_result += str_unit;

    // power output status
    power = (uint32_t)ps_device.voltage_act * (uint32_t)ps_device.current_act / 1000;
    PSGenerateValueLabel (str_data, str_unit, (uint16_t)power, 2, PS_UNIT_W);
    str_result += ";"; str_result += str_data;
    str_result += ";"; str_result += str_unit;

    // data recording status
    if (ps_record.start_time != 0) str_result += ";on"; else str_result += ";off";

    // data recording duration
    PSGetRecordingDuration (str_data, sizeof (str_data));
    str_result += ";";
    str_result += str_data;

    // wifi level
    str_result += ";";
    str_result += WifiGetRssiAsQuality (WiFi.RSSI ());
    str_result += "%";

    // stream result
    WSContentSend_P ("%s", str_result.c_str ());

    // if voltage and current are defined
    if ((ps_graph.vmax != 0) && (ps_graph.imax != 0))
    {
      // boundaries of SVG graph
      graph_vmax = (long)ps_graph.vmax;
      graph_imax = (long)ps_graph.imax;
      graph_left   = PS_GRAPH_PERCENT_START * PS_GRAPH_WIDTH / 100;
      graph_right  = PS_GRAPH_PERCENT_STOP * PS_GRAPH_WIDTH / 100;
      graph_width  = graph_right - graph_left;

      // ------------------
      //   voltage curve
      // ------------------

      // loop for the voltage curve
      str_result = ";";
      last_x = last_y = LONG_MAX;
      for (index = 0; index < PS_GRAPH_SAMPLE; index++)
      {
        // get current array index
        index_array = (index + ps_graph.index) % PS_GRAPH_SAMPLE;

        // if voltage is defined, display point
        if (ps_graph.arr_voltage[index_array] != UINT16_MAX)
        {
          // get graph value and calculate point
          graph_value = (long)ps_graph.arr_voltage[index_array];
          graph_x = graph_left + (graph_width * index / PS_GRAPH_SAMPLE);
          graph_y = PS_GRAPH_HEIGHT - (graph_value * PS_GRAPH_HEIGHT / graph_vmax);

          // if first point
          if (last_x == LONG_MAX)
          {
            // display lower point and current point
            sprintf (str_data, "%d,%d ", graph_x, graph_y);
            str_result += str_data;
          }

          // if last point
          else if ((graph_y != last_y) || (index == PS_GRAPH_SAMPLE - 1))
          {
            // display last point and bottom point
            if (last_y != LONG_MAX)
            { 
              sprintf (str_data, "%d,%d %d,%d ", last_x, last_y, graph_x, graph_y);
              str_result += str_data;
            }
          }

          // save previous point
          last_x = graph_x;
          last_y = graph_y;
        }
      }

      // stream result
      WSContentSend_P ("%s", str_result.c_str ());

      // ------------------
      //   current curve
      // ------------------

      // loop for the current curve
      str_result = ";";
      last_x = last_y = LONG_MAX;
      for (index = 0; index < PS_GRAPH_SAMPLE; index++)
      {
        // get current array index and value
        index_array = (index + ps_graph.index) % PS_GRAPH_SAMPLE;

        // if voltage is defined, display point
        if (ps_graph.arr_current[index_array] != UINT16_MAX)
        {
          // calculate value and graph point
          graph_value = (long)ps_graph.arr_current[index_array];
          graph_x = graph_left + (graph_width * index / PS_GRAPH_SAMPLE);
          graph_y = PS_GRAPH_HEIGHT - (graph_value * PS_GRAPH_HEIGHT / graph_imax);

          // if first point
          if (last_x == LONG_MAX)
          {
            // display lower point and current point
            sprintf (str_data, "M%d,%d %d,%d ", graph_x, PS_GRAPH_HEIGHT, graph_x, graph_y);
            str_result += str_data;
          }

          // if last point
          else if ((graph_y != last_y) || (index == PS_GRAPH_SAMPLE - 1))
          {
            // display last point and bottom point
            sprintf (str_data, "%d,%d %d,%d ", last_x, last_y, graph_x, graph_y);
            str_result += str_data;
          }

          // save previous point
          last_x = graph_x;
          last_y = graph_y;
        }
      }

      // end of polyline
      if (graph_x != LONG_MAX) { sprintf (str_data, "L%d,%d ", graph_x, PS_GRAPH_HEIGHT); str_result += str_data; }
      str_result += "Z";

      // stream result
      WSContentSend_P ("%s", str_result.c_str ());

      // reset flag
      ps_graph.serving = false;
    }
  }

  // end of stream
  WSContentEnd ();
}

// Graph frame
void PSWebGraphFrame ()
{
  long     index, index_array;
  long     graph_left, graph_right, graph_width, graph_x;  
  long     unit_width;  
  uint16_t new_value;
  char     str_unit[8];
  char     str_value[8];

  // boundaries of SVG graph
  graph_left   = PS_GRAPH_PERCENT_START * PS_GRAPH_WIDTH / 100;
  graph_right  = PS_GRAPH_PERCENT_STOP * PS_GRAPH_WIDTH / 100;
  graph_width  = graph_right - graph_left;

  // start of SVG graph
  WSContentSend_P (PSTR ("<svg class='graph' viewBox='%d %d %d %d' preserveAspectRatio='none' width='100%%' height='100%%'>\n"), 0, 0, PS_GRAPH_WIDTH, PS_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("text {font-size:0.8rem;}\n"));
  WSContentSend_P (PSTR ("text.voltage {stroke:%s;fill:%s;}\n"), D_PS_COLOR_VOLTAGE, D_PS_COLOR_VOLTAGE);
  WSContentSend_P (PSTR ("text.current {stroke:%s;fill:%s;}\n"), D_PS_COLOR_CURRENT, D_PS_COLOR_CURRENT);
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // graph frame
  WSContentSend_P (PSTR ("<rect x='%d%%' y='%d%%' width='%d%%' height='%d%%' rx='10' />\n"), PS_GRAPH_PERCENT_START, 0, PS_GRAPH_PERCENT_STOP - PS_GRAPH_PERCENT_START, 100);

  // graph separation lines
  WSContentSend_P (PS_HTML_DASH, PS_GRAPH_PERCENT_START, 25, PS_GRAPH_PERCENT_STOP, 25);
  WSContentSend_P (PS_HTML_DASH, PS_GRAPH_PERCENT_START, 50, PS_GRAPH_PERCENT_STOP, 50);
  WSContentSend_P (PS_HTML_DASH, PS_GRAPH_PERCENT_START, 75, PS_GRAPH_PERCENT_STOP, 75);

  // ----------------------
  //   Voltage graduation
  // ----------------------

  new_value = ps_graph.vmax;
  PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_V);
  WSContentSend_P (PS_HTML_UNIT, PSTR ("voltage"), 1, 3, str_value, str_unit);

  new_value = ps_graph.vmax * 3 / 4;
  PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_V);
  WSContentSend_P (PS_HTML_UNIT, PSTR ("voltage"), 1, 26, str_value, str_unit);

  new_value = ps_graph.vmax / 2;
  PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_V);
  WSContentSend_P (PS_HTML_UNIT, PSTR ("voltage"), 1, 51, str_value, str_unit);

  new_value = ps_graph.vmax / 4;
  PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_V);
  WSContentSend_P (PS_HTML_UNIT, PSTR ("voltage"), 1, 76, str_value, str_unit);

  WSContentSend_P (PS_HTML_UNIT, PSTR ("voltage"), 1, 99, "0", "");

  // ---------------------
  //   Current graduation
  // ---------------------

  new_value = ps_graph.imax;
  PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_A);
  WSContentSend_P (PS_HTML_UNIT, PSTR ("current"), PS_GRAPH_PERCENT_STOP + 1, 3, str_value, str_unit);

  new_value = ps_graph.imax * 3 / 4;
  PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_A);
  WSContentSend_P (PS_HTML_UNIT, PSTR ("current"), PS_GRAPH_PERCENT_STOP + 1, 26, str_value, str_unit);

  new_value = ps_graph.imax / 2;
  PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_A);
  WSContentSend_P (PS_HTML_UNIT, PSTR ("current"), PS_GRAPH_PERCENT_STOP + 1, 51, str_value, str_unit);

  new_value = ps_graph.imax / 4;
  PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_A);
  WSContentSend_P (PS_HTML_UNIT, PSTR ("current"), PS_GRAPH_PERCENT_STOP + 1, 76, str_value, str_unit);

  WSContentSend_P (PS_HTML_UNIT, PSTR ("current"), PS_GRAPH_PERCENT_STOP + 1, 99, "0", "");

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
}

void PSWebPageIconWifi ()
{
  Webserver->send_P (200, PSTR ("image/svg+xml"), ps_icon_wifi_svg, strlen (ps_icon_wifi_svg)); 
}

// Control public page
void PSWebPageControl ()
{
  bool  result;
  int   index, digit;
  uint16_t new_value;
  uint32_t power;
  char  str_unit[8];
  char  str_value[8];
  char  str_next[8];
  char  str_time[16];
  char  str_title[32];

  // check parameters and serving flag
  if (!HttpCheckPriviledgedAccess()) return;

  // beginning of page
  WSContentStart_P (D_PS_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // if not already serving page
  if (!ps_graph.serving)
  {
    // set flag
    ps_graph.serving = true;


    // power button
    if (Webserver->hasArg (D_CMND_PS_POWER))
    {
      WebGetArg (D_CMND_PS_POWER, str_value, sizeof (str_value)); 
      result = (strcasecmp (str_value, "on") == 0);
      if (result != ps_device.output) PSEnableOutput (result);
    }

    // record button
    if (Webserver->hasArg (D_CMND_PS_RECORD)) 
    {
      WebGetArg (D_CMND_PS_RECORD, str_value, sizeof (str_value)); 
      result = (strcasecmp (str_value, "on") == 0);
      if (result && (ps_record.start_time == 0)) PSRecordStart (); 
        else if (!result && (ps_record.start_time != 0)) PSRecordStop ();
    }

    // check over voltage protection change
    if (Webserver->hasArg (D_CMND_PS_OVP))
    {
      WebGetArg (D_CMND_PS_OVP, str_value, sizeof (str_value)); 
      ps_device.flag_ovp = (strcasecmp (str_value, "on") == 0);
      PSSetOverVoltage (ps_device.flag_ovp);
    }

    // check over current protection change
    if (Webserver->hasArg (D_CMND_PS_OCP))
    {
      WebGetArg (D_CMND_PS_OCP, str_value, sizeof (str_value)); 
      ps_device.flag_ocp = (strcasecmp (str_value, "on") == 0);
      PSOverCurrent (ps_device.flag_ocp);
    }

    // set output voltage
    if (Webserver->hasArg (D_CMND_PS_VSET))
    {
      WebGetArg (D_CMND_PS_VSET, str_value, sizeof (str_value)); 
      ps_device.voltage_ask = 0;
      ps_device.voltage_ref   = (uint16_t)atoi (str_value);
      PSVoltageSet (ps_device.voltage_ref);
    }

    // set output voltage
    if (Webserver->hasArg (D_CMND_PS_VCONF))
    {
      WebGetArg (D_CMND_PS_VCONF, str_value, sizeof (str_value));
      ps_device.voltage_ask = (uint16_t)atoi (str_value);
    }
    else ps_device.voltage_ask = 0;

    // set output current
    if (Webserver->hasArg (D_CMND_PS_ISET))
    {
      WebGetArg (D_CMND_PS_ISET, str_value, sizeof (str_value));
      ps_device.current_ref   = (uint16_t)atoi (str_value); 
      PSCurrentSet (ps_device.current_ref);
    }

    // get graph maximum voltage scale
    if (Webserver->hasArg (D_CMND_PS_VMAX))
    {
      WebGetArg (D_CMND_PS_VMAX, str_value, sizeof (str_value));
      ps_graph.vmax = (uint16_t)atoi (str_value);
      if (ps_graph.vmax > 1000 * PS_VOLTAGE_MAX) ps_graph.vmax = 1000 * PS_VOLTAGE_MAX;
    }

    // get graph maximum voltage scale
    if (Webserver->hasArg (D_CMND_PS_IMAX))
    {
      WebGetArg (D_CMND_PS_IMAX, str_value, sizeof (str_value));
      ps_graph.imax = (uint16_t)atoi (str_value);
      if (ps_graph.imax > 1000 * PS_CURRENT_MAX) ps_graph.imax = 1000 * PS_CURRENT_MAX;
    }

    // page data refresh script
    //  format : vout(on/off);ovp(on/off);ocp(on/off);vout;vout-unit(V/mV);iout;iout-unit(A/mA);wout;wout-unit(W/mW)
    WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));

    WSContentSend_P (PSTR ("function updateData()\n"));
    WSContentSend_P (PSTR ("{\n"));
    WSContentSend_P (PSTR (" httpData=new XMLHttpRequest();\n"));
    WSContentSend_P (PSTR (" httpData.onreadystatechange=function()\n"));
    WSContentSend_P (PSTR (" {\n"));
    WSContentSend_P (PSTR ("  if (httpData.readyState==4)\n"));
    WSContentSend_P (PSTR ("  {\n"));
    WSContentSend_P (PSTR ("   arr_value=httpData.responseText.split(';');\n"));
    WSContentSend_P (PSTR ("   if (arr_value[0]=='on') {target='off';} else {target='on';} \n"));
    WSContentSend_P (PSTR ("   document.getElementById('target').href='ctrl?power='+target;\n"));             // link to switch on/off
    WSContentSend_P (PSTR ("   document.getElementById('power').className=arr_value[0];\n"));                 // color of switch on/off
    WSContentSend_P (PSTR ("   document.getElementById('vread').className='read '+arr_value[0];\n"));         // voltage display
    WSContentSend_P (PSTR ("   document.getElementById('iread').className='read '+arr_value[0];\n"));         // current display
    WSContentSend_P (PSTR ("   document.getElementById('wread').className='read '+arr_value[0];\n"));         // power display
    WSContentSend_P (PSTR ("   document.getElementById('cv').className=arr_value[1];\n"));                    // CV indicator
    WSContentSend_P (PSTR ("   if (arr_value[1]=='on') {target='off';} else {target='on';} \n"));
    WSContentSend_P (PSTR ("   document.getElementById('cc').className=target;\n"));                          // CC indicator
    WSContentSend_P (PSTR ("   document.getElementById('ovp').className=arr_value[2];\n"));                   // OVP indicator
    WSContentSend_P (PSTR ("   document.getElementById('ocp').className=arr_value[3];\n"));                   // OCP indicator
    WSContentSend_P (PSTR ("   document.getElementById('vref').textContent=arr_value[4];\n"));                // voltage value
    WSContentSend_P (PSTR ("   document.getElementById('vrunit').textContent=arr_value[5];\n"));              // voltage unit
    WSContentSend_P (PSTR ("   document.getElementById('iref').textContent=arr_value[6];\n"));                // current value
    WSContentSend_P (PSTR ("   document.getElementById('irunit').textContent=arr_value[7];\n"));              // current unit
    WSContentSend_P (PSTR ("   document.getElementById('vout').textContent=arr_value[8];\n"));                // voltage value
    WSContentSend_P (PSTR ("   document.getElementById('vunit').textContent=arr_value[9];\n"));               // voltage unit
    WSContentSend_P (PSTR ("   document.getElementById('iout').textContent=arr_value[10];\n"));               // current value
    WSContentSend_P (PSTR ("   document.getElementById('iunit').textContent=arr_value[11];\n"));              // current unit
    WSContentSend_P (PSTR ("   document.getElementById('wout').textContent=arr_value[12];\n"));               // power value
    WSContentSend_P (PSTR ("   document.getElementById('wunit').textContent=arr_value[13];\n"));              // power unit
    WSContentSend_P (PSTR ("   document.getElementById('record').className=arr_value[14];\n"));               // recording status
    WSContentSend_P (PSTR ("   document.getElementById('duration').textContent=arr_value[15];\n"));           // recording duration
    WSContentSend_P (PSTR ("   document.getElementById('wifi').textContent=arr_value[16];\n"));               // wifi level
    WSContentSend_P (PSTR ("   document.getElementById('volt').setAttribute('points',arr_value[17]);\n"));    // recording duration
    WSContentSend_P (PSTR ("   document.getElementById('amp').setAttribute('d',arr_value[18]);\n"));          // recording duration
    WSContentSend_P (PSTR ("   setTimeout(function() {updateData();},1000);\n"));                             // ask for next update in 1 sec
    WSContentSend_P (PSTR ("  }\n"));
    WSContentSend_P (PSTR (" }\n"));
    WSContentSend_P (PSTR (" httpData.open('GET','%s',true);\n"), D_PS_PAGE_DATA);
    WSContentSend_P (PSTR (" httpData.send();\n"));
    WSContentSend_P (PSTR ("}\n"));

    WSContentSend_P (PSTR ("updateData();\n"));                                                               // first data update
    WSContentSend_P (PSTR ("</script>\n"));

    // allow page scaling
    WSContentSend_P (PSTR ("<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=yes'/>\n"));

    // page style
    WSContentSend_P (PSTR ("<style>\n"));
    WSContentSend_P (PSTR ("body {color:white;background-color:%s;font-family:Arial, Helvetica, sans-serif;}\n"), D_PS_COLOR_BACKGROUND);
    WSContentSend_P (PSTR ("div.main {margin:0.5rem auto;padding:0px;text-align:center;vertical-align:middle;}\n"));

    WSContentSend_P (PSTR ("div.control {display:inline-block;text-align:center;width:10vw;}\n"));
    WSContentSend_P (PSTR ("div.action {width:100%%;padding:0px;margin:auto;}\n"));
    WSContentSend_P (PSTR ("div.section {display:inline-block;text-align:center;width:18rem;margin:0.5vw 2rem;}\n"));

    WSContentSend_P (PSTR ("div.watt {width:14rem;display:inline-block;width:14rem;border-radius:8px;border:1px white solid;border-top:30px white solid;margin-bottom:10px;}\n"));
    WSContentSend_P (PSTR ("div.watt a {color:black;}\n"));

    WSContentSend_P (PSTR ("div.watt div.name {width:100%%;margin-top:-26px;font-style:bold;border:none;}\n"));
    WSContentSend_P (PSTR ("div.watt div.name span {color:black;}\n"));
    WSContentSend_P (PSTR ("div.watt div.name span.name {font-size:1.25rem;}\n"));
    WSContentSend_P (PSTR ("div.watt div.name span.icon {float:right;margin-top:0.1rem;margin-left:0.4rem;}\n"));
    WSContentSend_P (PSTR ("div.watt div.name span.wifi {float:right;width:2rem;font-size:0.8rem;margin-top:0.15rem;margin-right:0.4rem;}\n"));
    WSContentSend_P (PSTR ("div.watt div.read {border:none;margin-top:2px;}\n"));
    WSContentSend_P (PSTR ("div.watt div.read span.unit {margin-right:0rem;}\n"));

    WSContentSend_P (PSTR ("div.read {width:100%%;padding:0px;margin:0px auto;border-radius:8px;border:1px white solid;border-top:10px white solid;}\n"));
    WSContentSend_P (PSTR ("div.read span.value {font-size:2.4rem;}\n"));
    WSContentSend_P (PSTR ("div.read span.unit {font-size:1.2rem;margin-left:0.5rem;margin-right:-4rem;}\n"));

    WSContentSend_P (PSTR ("div.power {width:100%%;padding:0px;margin:auto;}\n"));

    WSContentSend_P (PSTR ("div.preset {width:100%%;font-size:0.8rem;padding-bottom:4px;margin:0px;}\n"));
    WSContentSend_P (PSTR ("div.preset div {display:inline-block;padding:0px 4px;margin:0px;border-radius:6px;color:black;}\n"));

    WSContentSend_P (PSTR ("div.set {width:90%%;font-size:1rem;padding:0px;margin:0px auto;border:1px #666 solid;border-top:none;border-bottom-left-radius:6px;border-bottom-right-radius:6px;}\n"));
    WSContentSend_P (PSTR ("div.set div {display:inline-block;width:10%%;padding:0 0.1rem;margin:0 2%%;font-weight:bold;border-radius:8px;}\n"));
    WSContentSend_P (PSTR ("div.set div.ref {width:25%%;background:none;}\n"));

    // graph + and -
    WSContentSend_P (PSTR ("div.adjust {width:100%%;margin-top:-8rem;margin-bottom:9rem;}\n"));
    WSContentSend_P (PSTR ("div.more {transform:rotate(270deg);}\n"));
    WSContentSend_P (PSTR ("div.less {transform:rotate(90deg);}\n"));
    WSContentSend_P (PSTR ("div.item {padding:0.25rem 0.5rem;margin:0.2rem 0.5rem;border-radius:6px;background:#333;}\n"));
    WSContentSend_P (PSTR ("div.item:hover {background:#666;}\n"));
    WSContentSend_P (PSTR ("div.left {float:left;margin-left:1%%;}\n"));
    WSContentSend_P (PSTR ("div.right {float:right;margin-right:1%%;}\n"));

    // watt meter
    WSContentSend_P (PSTR (".watt div {color:%s;border:1px %s solid;}\n"), D_PS_COLOR_WATT, D_PS_COLOR_WATT_OFF);
    WSContentSend_P (PSTR (".watt div.off {border-color:%s;}\n"), D_PS_COLOR_WATT_OFF);
    WSContentSend_P (PSTR (".watt div span {color:%s;}\n"), D_PS_COLOR_WATT);
    WSContentSend_P (PSTR (".watt div.off span {color:%s;}\n"), D_PS_COLOR_WATT_OFF);

    // volt meter
    WSContentSend_P (PSTR (".volt div {color:%s;border-color:%s;}\n"), D_PS_COLOR_VOLTAGE, D_PS_COLOR_VOLTAGE);
    WSContentSend_P (PSTR (".volt div.off {color:%s;}\n"), D_PS_COLOR_VOLTAGE_OFF);
    WSContentSend_P (PSTR (".volt div span {color:%s;}\n"), D_PS_COLOR_VOLTAGE);
    WSContentSend_P (PSTR (".volt div.off span {color:%s;}\n"), D_PS_COLOR_VOLTAGE_OFF);
    WSContentSend_P (PSTR (".volt div.preset div {background:%s;width:15%%;}\n"), D_PS_COLOR_VOLTAGE_OFF);
    WSContentSend_P (PSTR (".volt div.preset div.confirm {color:%s;font-weight:bold;}\n"), D_PS_COLOR_VOLTAGE);
    WSContentSend_P (PSTR (".volt div.preset div:hover {color:%s;background:%s;}\n"), D_PS_COLOR_BACKGROUND, D_PS_COLOR_VOLTAGE);
    WSContentSend_P (PSTR (".volt div.set a div:hover {background:%s;}\n"), D_PS_COLOR_VOLTAGE_OFF);
    WSContentSend_P (PSTR (".volt span#cv {float:left;width:1.2rem;padding:0.25rem 0.15rem;margin:0.35rem;font-size:80%%;color:black;background:%s;border-radius:50%%;}\n"), D_PS_COLOR_VOLTAGE);
    WSContentSend_P (PSTR (".volt span#cv.off {visibility:hidden;}"));
    WSContentSend_P (PSTR (".volt span#ovp {float:right;width:2rem;padding:0.1rem 0.5rem;margin:0.35rem 0.25rem;border:1px %s solid;color:%s;background:none;border-radius:12px;}\n"), D_PS_COLOR_VOLTAGE_OFF, D_PS_COLOR_VOLTAGE_OFF);
    WSContentSend_P (PSTR (".volt span#ovp.on {color:black;background:%s;}\n"), D_PS_COLOR_VOLTAGE);
    WSContentSend_P (PSTR (".volt span#ovp.none {visibility:hidden;}"));

    // amp meter
    WSContentSend_P (PSTR (".amp div {color:%s;border-color:%s;}\n"), D_PS_COLOR_CURRENT, D_PS_COLOR_CURRENT);
    WSContentSend_P (PSTR (".amp div.off {color:%s;}\n"), D_PS_COLOR_CURRENT_OFF);
    WSContentSend_P (PSTR (".amp div span {color:%s;}\n"), D_PS_COLOR_CURRENT);
    WSContentSend_P (PSTR (".amp div.off span {color:%s;}\n"), D_PS_COLOR_CURRENT_OFF);
    WSContentSend_P (PSTR (".amp div.preset div {background:%s;width:18%%;}\n"), D_PS_COLOR_CURRENT_OFF);
    WSContentSend_P (PSTR (".amp div.preset div:hover {background:%s;}\n"), D_PS_COLOR_CURRENT);
    WSContentSend_P (PSTR (".amp div.set a div:hover {background:%s;}\n"), D_PS_COLOR_CURRENT_OFF);
    WSContentSend_P (PSTR (".amp span#cc {float:left;width:1.2rem;padding:0.25rem 0.15rem;margin:0.35rem;font-size:80%%;color:black;background:%s;border-radius:50%%;}\n"), D_PS_COLOR_CURRENT);
    WSContentSend_P (PSTR (".amp span#cc.off {visibility:hidden;}"));
    WSContentSend_P (PSTR (".amp span#ocp {float:right;width:2rem;padding:0.1rem 0.5rem;margin:0.35rem 0.25rem;border:1px %S solid;color:%s;background:none;border-radius:12px;}\n"), D_PS_COLOR_CURRENT_OFF, D_PS_COLOR_CURRENT_OFF);
    WSContentSend_P (PSTR (".amp span#ocp.on {color:black;background:%s;}\n"), D_PS_COLOR_CURRENT);
    WSContentSend_P (PSTR (".amp span#ocp.none {visibility:hidden;}"));

    // power switch
    WSContentSend_P (PSTR ("#power {position:relative;display:inline-block;width:48px;height:48px;border-radius:24px;padding:0px;border:1px solid grey;}\n"));
    WSContentSend_P (PSTR ("#power #ring {position:absolute;width:24px;height:24px;border:6px solid red;top:6px;left:6px;border-radius:18px;}\n"));
    WSContentSend_P (PSTR ("#power #line {width:8px;height:18px;margin:-8px auto;background:%s;border-radius:6px;border-right:4px solid %s;border-left:4px solid %s;}\n"), D_PS_COLOR_POWER_OFF, D_PS_COLOR_BACKGROUND, D_PS_COLOR_BACKGROUND);
    WSContentSend_P (PSTR ("#power.on #ring {border-color:%s;}\n"), D_PS_COLOR_POWER);
    WSContentSend_P (PSTR ("#power.on #line {background:%s;}\n"), D_PS_COLOR_POWER);

    // record button
    WSContentSend_P (PSTR ("#record {position:relative;display:inline-block;width:48px;height:48px;border-radius:24px;padding:0px;border:1px solid %s;}\n"), D_PS_COLOR_RECORD_OFF);
    WSContentSend_P (PSTR ("#record #ring {position:absolute;width:0px;height:0px;border:18px solid %s;top:6px;left:6px;border-radius:18px;}\n"), D_PS_COLOR_RECORD_OFF);
    WSContentSend_P (PSTR ("#record #duration {visibility:hidden;position:absolute;font-size:18px;font-weight:bold;color:%s;background-color:%s;padding:4px 12px;margin-left:20px;margin-top:-16px;border:1px solid %s;border-left:none;border-top-right-radius:8px;border-bottom-right-radius:8px;}\n"), D_PS_COLOR_RECORD, D_PS_COLOR_BACKGROUND, D_PS_COLOR_RECORD_OFF);
    WSContentSend_P (PSTR ("#record.on #ring {border-color:%s;}\n"), D_PS_COLOR_RECORD);
    WSContentSend_P (PSTR ("#record.on #duration {visibility:visible;}\n"));

    // no line under links
    WSContentSend_P (PSTR ("a:link {text-decoration:none;}\n"));

    // main button
    WSContentSend_P (PSTR ("button.menu {background:%s;color:%s;line-height:2rem;font-size:1.2rem;cursor:pointer;border:none;border-radius:0.3rem;width:150px;margin:2vh;}\n"), COLOR_BUTTON, COLOR_BUTTON_TEXT);

    // graph
    WSContentSend_P (PSTR ("div.graph {position:relative;width:100%%;margin:auto;margin-top:3vh;height:60vh;}\n"));
    WSContentSend_P (PSTR ("object.graph {display:inline-block;position:absolute;top:0;left:0;}\n"));
    WSContentSend_P (PSTR ("svg.graph {display:inline-block;position:absolute;top:0;left:0;}\n"));

    // end of header & start of page
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

    // power button
    if (ps_device.output) { strcpy_P (str_value, PSTR ("on")); strcpy_P (str_next, PSTR ("off")); }
      else { strcpy_P (str_value, PSTR ("off")); strcpy_P (str_next, PSTR ("on")); }
    WSContentSend_P (PSTR ("<a id='target' href='/ctrl?%s=%s'><div id='power' class='%s'><div id='ring'><div id='line'></div></div></div></a>\n"), D_CMND_PS_POWER, str_next, str_value);

    WSContentSend_P (PSTR ("</div></div>\n"));    // action & control

    // --- Power Meter --- //

    WSContentSend_P (PSTR ("<div class='section watt'>\n"));

    // device name
    WSContentSend_P (PSTR ("<div class='name'>\n"));
    WSContentSend_P (PSTR ("<span class='wifi' id='wifi' title='Wifi level'>&nbsp;</span>\n"));
    WSContentSend_P (PSTR ("<span class='icon'><img src='/wifi.svg' height=14 ></span>\n"));
    WSContentSend_P (PSTR ("<span class='name'><a href='/'>%s</a></span>\n"), SettingsText(SET_DEVICENAME));
    WSContentSend_P (PSTR ("</div>\n"));

    if (ps_device.output) strcpy_P (str_value, PSTR ("on")); else strcpy_P (str_value, PSTR ("off"));
    WSContentSend_P (PSTR ("<div class='read %s' id='wread'>\n"), str_value);

    power = (uint32_t)ps_device.voltage_act * (uint32_t)ps_device.current_act / 1000;
    PSGenerateValueLabel (str_value, str_unit, (uint16_t)power, 2, PS_UNIT_W);
    WSContentSend_P (PSTR ("<span class='value' id='wout'>%s</span>\n"), str_value);
    WSContentSend_P (PSTR ("<span class='unit watt' id='wunit'>%s</span>\n"), str_unit);
    WSContentSend_P (PSTR ("</div>\n"));    // read
    WSContentSend_P (PSTR ("</div>\n"));    // watt

    // --- Record Button --- //

    WSContentSend_P (PSTR ("<div class='control'><div class='action'>\n"));

    if (ps_record.start_time != 0) { strcpy (str_value, "off"); strcpy (str_title, "Stop recording"); PSGetRecordingDuration (str_time, sizeof (str_time)); }
      else { strcpy (str_value, "on"); strcpy (str_title, "Start recording"); strcpy (str_time, ""); }
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><div id='record' class='%s' title='%s'><div id='ring'><div id='duration'>%s</div></div></div></a>\n"), D_CMND_PS_RECORD, str_value, str_value, str_title, str_time);

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

    if (ps_device.output) strcpy_P (str_value, PSTR ("on")); else strcpy (str_value, PSTR ("off"));
    WSContentSend_P (PSTR ("<div class='read %s' id='vread'>\n"), str_value);

    if (ps_device.flag_cv) strcpy_P (str_value, PSTR ("on")); else strcpy_P (str_value, PSTR ("off"));
    WSContentSend_P (PSTR ("<span class='%s' id='cv'>CV</span>\n"), str_value);

    if (ps_device.flag_ovp) strcpy_P (str_next, PSTR ("off")); else strcpy_P (str_next, PSTR ("on"));
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='protect off' id='ovp'>OVP</span></a>\n"), D_CMND_PS_OVP, str_next);

    PSGenerateValueLabel (str_value, str_unit, ps_device.voltage_act, 2, PS_UNIT_V);
    WSContentSend_P (PSTR ("<span class='value' id='vout'>%s</span>\n"), str_value);
    WSContentSend_P (PSTR ("<span class='unit' id='vunit'>%s</span>\n"), str_unit);

    WSContentSend_P (PSTR ("<br>\n"));    // read

    // preset voltage buttons
    WSContentSend_P (PSTR ("<div class='preset'>\n"));
    for (index = 0; index < PS_PRESET_VOLTAGE_MAX; index ++)
    {
      // get value to display
      new_value = ps_config.arr_voltage_preset[index];
      PSGenerateValueLabel (str_value, str_unit, new_value, 1, PS_UNIT_V);

      // set button according to stage : confirmation or voltage set
      if ((new_value != UINT16_MAX) && (ps_device.voltage_ask == new_value)) WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u'><div class='confirm' title='%s'>%s %s</div></a>\n"), D_CMND_PS_VSET, new_value, PS_VOLT_CLICK, str_value, str_unit);
        else if (new_value != UINT16_MAX) WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u'><div title='%s'>%s %s</div></a>\n"), D_CMND_PS_VCONF, new_value, PS_VOLT_CLICK2, str_value, str_unit);
    }
    WSContentSend_P (PSTR ("</div>\n"));    // preset

    WSContentSend_P (PSTR ("</div>\n"));    // read

    // voltage reference with decrease and increase buttons
    WSContentSend_P (PSTR ("<div class='set'>\n"));

    if (ps_device.voltage_ref >= 1000) new_value = ps_device.voltage_ref - 1000; else new_value = 100;
    PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_V);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u'><div title='%s %s'>%s</div></a>\n"), D_CMND_PS_VSET, new_value, str_value, str_unit, "<<");

    if (ps_device.voltage_ref >= 100) new_value = ps_device.voltage_ref - 100; else new_value = 100;
    PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_V);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u'><div title='%s %s'>%s</div></a>\n"), D_CMND_PS_VSET, new_value, str_value, str_unit, "<");

    PSGenerateValueLabel (str_value, str_unit, ps_device.voltage_ref, 2, PS_UNIT_V);
    WSContentSend_P (PSTR ("<div class='ref'><span id='vref'>%s</span> <span id='vrunit'><small>%s</small></span></div>\n"), str_value, str_unit);

    if (ps_device.voltage_ref <= 29900) new_value = ps_device.voltage_ref + 100; else new_value = PS_VOLTAGE_MAX * 1000;
    PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_V);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u'><div title='%s %s'>%s</div></a>\n"), D_CMND_PS_VSET, new_value, str_value, str_unit, ">");

    if (ps_device.voltage_ref <= 29000) new_value = ps_device.voltage_ref + 1000; else new_value = PS_VOLTAGE_MAX * 1000;
    PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_V);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u'><div title='%s %s'>%s</div></a>\n"), D_CMND_PS_VSET, new_value, str_value, str_unit, ">>");

    WSContentSend_P (PSTR ("</div>\n"));      // set

    WSContentSend_P (PSTR ("</div>\n"));      // section volt

    // --- Current Meter --- //

    WSContentSend_P (PSTR ("<div class='section amp'>\n"));

    if (ps_device.output) strcpy_P (str_value, PSTR ("on")); else strcpy_P (str_value, PSTR ("off"));
    WSContentSend_P (PSTR ("<div class='read %s' id='iread'>\n"), str_value);

    if (ps_device.flag_cv) strcpy_P (str_value, PSTR ("off")); else strcpy_P (str_value, PSTR ("on"));
    WSContentSend_P (PSTR ("<span class='%s' id='cc'>CC</span>\n"), str_value);

    if (ps_device.flag_ocp) strcpy_P (str_next, PSTR ("off")); else strcpy_P (str_next, PSTR ("on"));
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%s'><span class='protect off' id='ocp'>OCP</span></a>\n"), D_CMND_PS_OCP, str_next);

    PSGenerateValueLabel (str_value, str_unit, ps_device.current_act, 4, PS_UNIT_A);
    WSContentSend_P (PSTR ("<span class='value' id='iout'>%s</span>\n"), str_value);
    WSContentSend_P (PSTR ("<span class='unit' id='iunit'>%s</span>\n"), str_unit);

    WSContentSend_P (PSTR ("<br>\n"));

    // preset max current
    WSContentSend_P (PSTR ("<div class='preset'>\n"));
    for (index = 0; index < PS_PRESET_CURRENT_MAX; index ++)
    {
      new_value = ps_config.arr_current_preset[index];
      PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_A);
      if (new_value != UINT16_MAX) WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u'><div title='%s'>%s %s</div></a>\n"), D_CMND_PS_ISET, new_value, PS_AMP_CLICK, str_value, str_unit);
    }
    WSContentSend_P (PSTR ("</div>\n"));    // preset

    WSContentSend_P (PSTR ("</div>\n"));    // read

    // current reference with decrease and increase buttons
    WSContentSend_P (PSTR ("<div class='set'>\n"));

    if (ps_device.current_ref >= 100) new_value = ps_device.current_ref - 100; else new_value = 50;
    PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_A);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u'><div title='%s %s'>%s</div></a>\n"), D_CMND_PS_ISET, new_value, str_value, str_unit, "<<");

    if (ps_device.current_ref >= 10) new_value = ps_device.current_ref - 5; else new_value = 5;
    PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_A);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u'><div title='%s %s'>%s</div></a>\n"), D_CMND_PS_ISET, new_value, str_value, str_unit, "<");

    PSGenerateValueLabel (str_value, str_unit, ps_device.current_ref, 2, PS_UNIT_A);
    WSContentSend_P (PSTR ("<div class='ref'><span id='iref'>%s</span> <span id='irunit'><small>%s</small></span></div>\n"), str_value, str_unit);

    if (ps_device.current_ref <= PS_CURRENT_MAX * 1000 - 5) new_value = ps_device.current_ref + 5; else new_value = PS_CURRENT_MAX * 1000;
    PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_A);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u'><div title='%s %s'>%s</div></a>\n"), D_CMND_PS_ISET, new_value, str_value, str_unit, ">");

    if (ps_device.current_ref <= PS_CURRENT_MAX * 1000 - 100) new_value = ps_device.current_ref + 100; else new_value = PS_CURRENT_MAX * 1000;
    PSGenerateValueLabel (str_value, str_unit, new_value, 2, PS_UNIT_A);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u'><div title='%s %s'>%s</div></a>\n"), D_CMND_PS_ISET, new_value, str_value, str_unit, ">>");

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
    WSContentSend_P (PSTR ("<div class='choice left volt'>\n"));
    new_value = min (1000 * PS_VOLTAGE_MAX, ps_graph.vmax + 5000);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u' title='+ 5V'><div class='item more'>»</div></a>\n"), D_CMND_PS_VMAX, new_value);
    new_value = min (1000 * PS_VOLTAGE_MAX, ps_graph.vmax + 1000);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u' title='+ 1V'><div class='item more'>›</div></a>\n"), D_CMND_PS_VMAX, new_value);
    new_value = max (1000, ps_graph.vmax - 1000);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u' title='-1 V'><div class='item less'>›</div></a>\n"), D_CMND_PS_VMAX, new_value);
    new_value = max (1000, ps_graph.vmax - 5000);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u' title='-5 V'><div class='item less'>»</div></a>\n"), D_CMND_PS_VMAX, new_value);
    WSContentSend_P (PSTR ("</div>\n"));      // choice left volt

    // current increase/decrease buttons
    WSContentSend_P (PSTR ("<div class='choice right amp'>\n"));
    new_value = min (1000 * PS_CURRENT_MAX, ps_graph.imax + 500);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u' title='+500 mA'><div class='item more'>»</div></a>\n"), D_CMND_PS_IMAX, new_value);
    new_value = min (1000 * PS_CURRENT_MAX, ps_graph.imax + 100);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u' title='+100 mA'><div class='item more'>›</div></a>\n"), D_CMND_PS_IMAX, new_value);
    new_value = max (100, ps_graph.imax - 100);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u' title='-100 mA'><div class='item less'>›</div></a>\n"), D_CMND_PS_IMAX, new_value);
    new_value = max (100, ps_graph.imax - 500);
    WSContentSend_P (PSTR ("<a href='/ctrl?%s=%u' title='-500 mA'><div class='item less'>»</div></a>\n"), D_CMND_PS_IMAX, new_value);
    WSContentSend_P (PSTR ("</div>\n"));      // choice right amp

    // adjust row : stop
    WSContentSend_P (PSTR ("</div>\n"));

    // -----------------
    //      graph
    // -----------------

    // start of graph section
    WSContentSend_P (PSTR ("<div class='graph'>\n"));

    // graph frame
    PSWebGraphFrame ();

    // graph curves
    WSContentSend_P (PSTR ("<svg class='graph' viewBox='0 0 1200 600' preserveAspectRatio='none' width='100%%' height='100%%'>\n"));
    WSContentSend_P (PSTR ("<style type='text/css'>\n"));
    WSContentSend_P (PSTR ("polyline.volt {stroke:yellow;fill:none;stroke-width:2;}\n"));
    WSContentSend_P (PSTR ("path.amp {stroke:#6bc4ff;fill:#6bc4ff;fill-opacity:0.6;}\n"));
    WSContentSend_P (PSTR ("</style>\n"));
    WSContentSend_P (PSTR ("<polyline id='volt' class='volt' points=''/>\n"));
    WSContentSend_P (PSTR ("<path id='amp' class='amp' d=''/>\n"));
    WSContentSend_P (PSTR ("</svg>\n"));

    // end of graph section
    WSContentSend_P (PSTR ("</div>\n"));      // graph

    // reset flag
      ps_graph.serving = false;
  }

  // main menu button
  WSContentSend_P (PSTR ("<div><form action='/' method='get'><button class='menu'>%s</button></form></div>\n"), D_MAIN_MENU);

  // end of page
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/************************************\
 *              Interface
\************************************/

// Korad power supply KAxxxxP driver
bool Xdrv96 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_PRE_INIT:
      PSPreInit ();
      break;
    case FUNC_INIT:
      PSInit ();
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      PSRecordStop ();
      PSSaveConfig ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kPSMqttCommands, arrPSMqttCommand);
      break;
    case FUNC_JSON_APPEND:
      PSShowJSON (true);
      break;
    case FUNC_EVERY_50_MSECOND:
      PSUpdateStatus ();
      break;
    case FUNC_EVERY_250_MSECOND:
      PSHandleRecord ();
      break;
    case FUNC_EVERY_SECOND:
      PSEverySecond ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      PSWebSensor ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PS_PAGE_CONTROL, D_PS_CONTROL);
      break;
    case FUNC_WEB_ADD_BUTTON:
      WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PS_PAGE_CONFIG, D_PS_CONFIGURE " " D_KORAD);
      break;
    case FUNC_WEB_ADD_HANDLER:
      // graph
      Webserver->on (FPSTR (D_PS_PAGE_CONFIG),    PSWebPageConfig);
      Webserver->on (FPSTR (D_PS_PAGE_CONTROL),   PSWebPageControl);
      Webserver->on (FPSTR (D_PS_PAGE_ICON_WIFI), PSWebPageIconWifi);
      Webserver->on (FPSTR (D_PS_PAGE_DATA),      PSWebPageData);
      break;
#endif  // USE_WEBSERVER

  }

  return result;
}

#endif   // USE_POWERSUPPLY
