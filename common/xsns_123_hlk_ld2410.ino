/*
  xsns_124_hlkld2410.ino - Driver for Presence and Movement sensor HLK-LD2410

  Copyright (C) 2022  Nicolas Bernaerts

  Connexions :
    * GPIO1 (Tx) should be declared as Serial Tx and connected to HLK-LD2410 Rx
    * GPIO3 (Rx) should be declared as Serial Rx and connected to HLK-LD2410 Tx

  Version history :
    28/06/2022 - v1.0   - Creation

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

#ifndef FIRMWARE_SAFEBOOT
#ifdef USE_HLKLD2410

#include <TasmotaSerial.h>

/*************************************************\
 *               Variables
\*************************************************/

// declare teleinfo energy driver and sensor
#define XSNS_123                   123

// constant
#define HLKLD2410_DEFAULT_TIMEOUT      5      // inactivity after 5 sec
#define HLKLD_SENSOR_MAX           2      // sensor for mov & occ
#define HLKLD_RANGE_MAX            3      // maximum of 3 ranges for each sensor
#define HLKLD2410_QUEUE_MAX            9      // maximum of 3 ranges for each sensor

#define HLKLD2410_CMND_QUEUE_MAX   4      // maximum command queue
#define HLKLD2410_MSG_SIZE_MAX     64     // maximum message size
#define HLKLD2410_CMND_SIZE_MAX    32     // maximum message size

// web pages
//#define D_HLKLD_PAGE_CONFIG       "ld-conf"
//#define D_HLKLD_PAGE_CONFIG_UPD   "ld-upd"

// strings
const char D_HLKLD2410_NAME[]        PROGMEM = "LD2410";
//const char D_HLKLD_CONFIGURE[]   PROGMEM = "Configure";


// type of message
enum HLKLD2410DataMode   { HLKLD2410_MODE_STANDARD, HLKLD2410_MODE_ENGINEERING, HLKLD2410_MODE_MAX };               // sensor receive message type
enum HLKLD2410RecvStatus { HLKLD2410_MSG_UNDEFINED, HLKLD2410_MSG_NONE, HLKLD2410_MSG_CMND_START, HLKLD2410_MSG_CMND_BODY, HLKLD2410_MSG_CMND_STOP, HLKLD2410_MSG_DATA_START, HLKLD2410_MSG_DATA_BODY, HLKLD2410_MSG_DATA_STOP, HLKLD2410_MSG_MAX };               // sensor receive message type

enum HLKLD2410DistType   { HLKLD2410_TYPE_OCC, HLKLD2410_TYPE_MOV, HLKLD2410_TYPE_ANY, HLKLD2410_TYPE_BOTH, HLKLD2410_TYPE_NONE };       // type of distance
const char HLKLD2410DataLabel[] PROGMEM = "occ|mov|any|both";                                            // sensor types description
const char HLKLD2410DataIcon[] PROGMEM = "🧍|🏃|👋|👋";                                              // sensor types icon

// type of received lines
//enum HLKLDLineType  { HLKLD_LINE_OCC, HLKLD_LINE_MOV, HLKLD_LINE_CONFIG, HLKLD_LINE_MAX };      // received line types

// mov and occ parameters available
//enum HLKLDParam  { HLKLD_PARAM_TH1, HLKLD_PARAM_TH2, HLKLD_PARAM_TH3, HLKLD_PARAM_MTH1_MOV, HLKLD_PARAM_MTH2_MOV, HLKLD_PARAM_MTH3_MOV, HLKLD_PARAM_MTH1_OCC, HLKLD_PARAM_MTH2_OCC, HLKLD_PARAM_MTH3_OCC, HLKLD_PARAM_MAX };         // device configuration modes
//const char HLKLDParamLabel[] PROGMEM = "th1|th2|th3|mth1_mov|mth2_mov|mth3_mov|mth1_occ|mth2_occ|mth3_occ";                                                  // device mode labels

// known models
//enum HLKLDModel { HLKLD_MODEL_1115H, HLKLD_MODEL_1125H, HLKLD_MODEL_MAX };              // device reference index
//const char HLKLDModelName[] PROGMEM = "LD1115H-24G|LD1125H-24G|Unknown";              // device reference name
//const char HLKLDModelSignature[] PROGMEM = "th1 th2 th3 ind_min ind_max mov_sn occ_sn dtime|test_mode rmax mth1_mov mth2_mov mth3_mov mth1_occ mth2_occ mth3_occ eff_th accu_num|";                                                  // device mode labels
//const char HLKLDModelDefault[]   PROGMEM = "th1=120;th2=250;dtime=5;mov_sn=3;occ_sn=5;get_all|test_mode=0;rmax=6;mth1_mov=60;mth2_mov=30;mth3_mov=20;mth1_occ=60;mth2_occ=30;mth3_occ=20;get_all|";                                                  // device mode labels

// ----------------
// binary commands
// ----------------

enum HLKLD2410ConfigCommand { HLKLD2410_CMND_ENABLE, HLKLD2410_CMND_DISABLE, HLKLD2410_CMND_READ_PARAM, HLKLD2410_CMND_SET_DISTANCE, HLKLD2410_CMND_SET_SENSITIVITY, HLKLD2410_CMND_OPEN_ENGINEER, HLKLD2410_CMND_CLOSE_ENGINEER, HLKLD2410_CMND_FIRMWARE, HLKLD2410_CMND_MAX };       // list of available commands
uint8_t hlkld2410_cmnd_enable[]          PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa, 0x04, 0x00, 0xff, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01, 0x00 };
uint8_t hlkld2410_cmnd_disable[]         PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa, 0x02, 0x00, 0xfe, 0x00, 0x04, 0x03, 0x02, 0x01, 0x00 };

uint8_t hlkld2410_cmnd_read_param[]      PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa, 0x02, 0x00, 0x61, 0x00, 0x04, 0x03, 0x02, 0x01, 0x00 };
uint8_t hlkld2410_cmnd_set_distance[]    PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa, 0x14, 0x00, 0x60, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x02, 0x00, 0x05, 0x00, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01, 0x00 };
uint8_t hlkld2410_cmnd_set_sensitivity[] PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa, 0x14, 0x00, 0x64, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x28, 0x00, 0x00, 0x00, 0x02, 0x00, 0x28, 0x00, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01, 0x00 };

uint8_t hlkld2410_cmnd_open_engineer[]   PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa, 0x02, 0x00, 0x62, 0x00, 0x04, 0x03, 0x02, 0x01, 0x00 };
uint8_t hlkld2410_cmnd_close_engineer[]  PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa, 0x02, 0x00, 0x63, 0x00, 0x04, 0x03, 0x02, 0x01, 0x00 };

uint8_t hlkld2410_cmnd_firmware[]        PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa, 0x02, 0x00, 0xa0, 0x00, 0x04, 0x03, 0x02, 0x01, 0x00 };


// MQTT commands : ld_help and ld_send
const char kHLKLD2410Commands[]         PROGMEM = "ld_|help|hex|firm";
void (* const HLKLD2410Command[])(void) PROGMEM = { &CmndHLKLD2410Help, &CmndHLKLD2410HexCustom, &CmndHLKLD2410Firmware };

/****************************************\
 *                 Data
\****************************************/

// HLK-LD2410 sensor command
struct hlkld2410_cmnd {
  uint8_t type;                   // command type
  uint8_t param[3];               // parameters
};
static struct {
  uint8_t  next   = 0;            // index of next command in queue
  uint8_t  number = 0;            // number of pending commands
  uint32_t time   = 0;            // time of last command
  struct  hlkld2410_cmnd queue[HLKLD2410_CMND_QUEUE_MAX];     // movement and presence sensor data
} hlkld2410_command; 

// HLK-LD sensor general status
struct hlkld2410_sensor {
  uint8_t  power;           // sensor power (0..100)
  uint16_t dist;            // object distance (cm)
  uint32_t time;            // last detection time (s)
};
static struct {
  TasmotaSerial *pserial = nullptr;               // pointer to serial port

  struct  hlkld2410_sensor occ;                   // presence sensor data
  struct  hlkld2410_sensor mov;                   // movement sensor data

  uint8_t  recv_state = HLKLD2410_MSG_UNDEFINED;  // current state of received message
  uint8_t  recv_size = 0;                         // size of currently received message
  uint8_t  recv_delimiter[4];                     // last 4 octets received (to check delimiter)
  uint8_t  recv_buffer[HLKLD2410_MSG_SIZE_MAX];   // body of currently received message
} hlkld2410_status; 

// HLK-LD sensor range data
struct hlkld_range {
  uint8_t  type;                                  // type of range parameter
  uint32_t time;                                  // timestamp of last detection
  long     power_def;                             // default reference power for detection
  long     power_ref;                             // actual reference power for detection
  long     power_min;                             // actual minimum power detected
  long     power_last;                            // last power detected
  float    dist_max;                              // maximum distance detection
  float    dist_last;                             // actual distance detection
};



/**************************************************\
 *                  Commands
\**************************************************/

// hlk-ld sensor help
void CmndHLKLD2410Help ()
{
  // help on command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld_hex  = send hexadecimal command to the sensor"));
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld_firm = print sensor firmware"));

  ResponseCmndDone ();
}

void CmndHLKLD2410HexCustom (void)
{
  int     length;
  uint8_t hex_command[HLKLD2410_CMND_SIZE_MAX];

  // generation of hex command
  length = HLKLD2410String2Hex (XdrvMailbox.data, hex_command, sizeof (hex_command));

  // send command
  if (HLKLD2410SendCommand (hex_command, length)) ResponseCmndDone ();
  else ResponseCmndFailed();
}

void CmndHLKLD2410Firmware (void)
{
  ResponseCmndChar (hlkld2410_device.str_firmware.c_str ());
}

/**************************************************\
 *                  Functions
\**************************************************/

// convert hexadecimal array to string
String HLKLD2410Hex2String (const uint8_t* phex_array, const int size_array)
{
  int    index;
  char   str_hex[4];
  String str_result;

  for (index = 0; index < size_array; index ++)
  {
      sprintf (str_hex, "%02X", phex_array[index]);
      if (index > 0) str_result += " ";
      str_result += str_hex;
  }

  return str_result;
}

// convert string to hexadecimal array
int HLKLD2410String2Hex (const char* pstr_value, uint8_t *phex_array, const int size_array)
{
  int  idx_value, idx_array, value, length;
  char str_byte[4];

  // check parameter
  if ((pstr_value == nullptr) || (phex_array == nullptr)) return 0;

  // init
  idx_value = 0;
  idx_array = 0;
  length = strlen (pstr_value);
  strcpy (str_byte, "00");

  // loop to populate hex array
  while (idx_value < length - 1)
  {
    // populate conversion string
    str_byte[0] = pstr_value[idx_value];
    str_byte[1] = pstr_value[idx_value + 1];

    // if space is left, convert hex string to byte 
    if (idx_array < size_array)
    {
      sscanf (str_byte, "%2x", &value);
      phex_array[idx_array++] = (uint8_t)value;
    }

    // increment input string counter
    idx_value += 2;
  }

  return idx_array;
}

bool HLKLD2410AppendCommand (const uint8_t command, const uint8_t param1, const uint8_t param2, const uint8_t param3 )
{
  uint8_t index;

  // check parameters
  if (command >= HLKLD2410_CMND_MAX) return false;
  if (hlkld2410_command.number == HLKLD2410_CMND_QUEUE_MAX) return false;

  // calculate index
  index = (hlkld2410_command.next + hlkld2410_command.number) % HLKLD2410_CMND_QUEUE_MAX;

  // append command
  hlkld2410_command.queue[index].type = command;
  hlkld2410_command.queue[index].param[0] = param1;
  hlkld2410_command.queue[index].param[1] = param2;
  hlkld2410_command.queue[index].param[2] = param3;

  // increase counter
  hlkld2410_command.number++;
}


enum HLKLD2410ConfigCommand { , HLKLD2410_CMND_SET_DISTANCE, HLKLD2410_CMND_SET_SENSITIVITY, , , HLKLD2410_CMND_FIRMWARE, HLKLD2410_CMND_MAX };       // list of available commands
uint8_t hlkld2410_cmnd_read_param[]      PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa, 0x02, 0x00, 0x61, 0x00, 0x04, 0x03, 0x02, 0x01, 0x00 };
uint8_t hlkld2410_cmnd_set_distance[]    PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa, 0x14, 0x00, 0x60, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x02, 0x00, 0x05, 0x00, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01, 0x00 };
uint8_t hlkld2410_cmnd_set_sensitivity[] PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa, 0x14, 0x00, 0x64, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x28, 0x00, 0x00, 0x00, 0x02, 0x00, 0x28, 0x00, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01, 0x00 };

uint8_t []   PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa, 0x02, 0x00, 0x62, 0x00, 0x04, 0x03, 0x02, 0x01, 0x00 };
uint8_t []  PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa, 0x02, 0x00, 0x63, 0x00, 0x04, 0x03, 0x02, 0x01, 0x00 };

uint8_t hlkld2410_cmnd_firmware[]        PROGMEM = { 0xfd, 0xfc, 0xfb, 0xfa, 0x02, 0x00, 0xa0, 0x00, 0x04, 0x03, 0x02, 0x01, 0x00 };


bool HLKLD2410ExecutePendingCommand ()
{
  uint8_t index;

  // check parameters
  if (hlkld2410_command.number == 0) return false;

  // execute command
  index = hlkld2410_command.next;
  switch (hlkld2410_command.queue[index].type)
  {
    case HLKLD2410_CMND_ENABLE:
      HLKLD2410SendHexCommand (hlkld2410_cmnd_enable, sizeof(hlkld2410_cmnd_enable));
      break;

    case HLKLD2410_CMND_DISABLE:
      HLKLD2410SendHexCommand (hlkld2410_cmnd_disable, sizeof(hlkld2410_cmnd_disable));
      break;

    case HLKLD2410_CMND_READ_PARAM:
      HLKLD2410SendHexCommand (hlkld2410_cmnd_read_param, sizeof(hlkld2410_cmnd_read_param));
      break;

    case HLKLD2410_CMND_OPEN_ENGINEER:
      HLKLD2410SendHexCommand (hlkld2410_cmnd_open_engineer, sizeof(hlkld2410_cmnd_open_engineer));
      break;

    case HLKLD2410_CMND_CLOSE_ENGINEER:
      HLKLD2410SendHexCommand (hlkld2410_cmnd_close_engineer, sizeof(hlkld2410_cmnd_close_engineer));
      break;
  }

  // update counters
  hlkld2410_command.time = millis ();
  hlkld2410_command.first = (index + 1) % HLKLD2410_CMND_QUEUE_MAX;
  hlkld2410_command.number--;
}

bool HLKLD2410SendHexCommand (uint8_t *phex_array, const int size_array)
{
  String str_log;

  // check environment and parameters
  if (hlkld2410_status.pserial == nullptr) return false;
  if ((phex_array == nullptr) || (size_array < 1)) return false;

  // send command
  hlkld2410_status.pserial->write (phex_array, size_array);

  // command : log
  str_log = HLKLD2410Hex2String (phex_array, size_array);
  AddLog (LOG_LEVEL_INFO, PSTR ("HLK: send = %s"), str_log.c_str ());

  return true;
}

void HLKLD2410ConfifDistance (const uint8_t motion_max_gate, const uint8_t static_max_gate, const uint8_t detection_timeout)
{
  uint8_t arr_command[HLKLD2410_CMND_SIZE_MAX];

  // copy command structure
  memcpy (arr_command, hlkld2410_cmnd_set_distance, sizeof(hlkld2410_cmnd_set_distance));

  // update gate and sensitivities
  arr_command[11] = motion_max_gate;
  arr_command[17] = static_max_gate;
  arr_command[25] = detection_timeout;
  
  // send commands
  HLKLD2410SendCommand (hlkld2410_cmnd_enable, sizeof(hlkld2410_cmnd_enable));
  HLKLD2410SendCommand (arr_command, sizeof(hlkld2410_cmnd_set_distance));
  HLKLD2410SendCommand (hlkld2410_cmnd_disable, sizeof(hlkld2410_cmnd_disable));
}

void HLKLD2410ConfigSensitivity (const uint8_t gate, const uint8_t motion_sensitivity, const uint8_t static_sensitivity)
{
  uint8_t arr_command[HLKLD2410_CMND_SIZE_MAX];

  // copy command structure
  memcpy (arr_command, hlkld2410_cmnd_set_sensitivity, sizeof(hlkld2410_cmnd_set_sensitivity));

  // update gate and sensitivities
  arr_command[11] = gate;
  arr_command[17] = motion_sensitivity;
  arr_command[25] = static_sensitivity;
  
  // send commands
  HLKLD2410SendCommand (hlkld2410_cmnd_enable, sizeof(hlkld2410_cmnd_enable));
  HLKLD2410SendCommand (arr_command, sizeof(hlkld2410_cmnd_set_sensitivity));
  HLKLD2410SendCommand (hlkld2410_cmnd_disable, sizeof(hlkld2410_cmnd_disable));
}

// driver initialisation
bool HLKLD2410InitDevice ()
{
  bool done;

  // check if already initialised
  done = (hlkld2410_status.pserial != nullptr);

  // if ports are selected, init sensor state
  if (!done && PinUsed (GPIO_TXD) && PinUsed (GPIO_RXD))
  {
#ifdef ESP32
    // create serial port
    hlkld2410_status.pserial = new TasmotaSerial (Pin (GPIO_RXD), Pin (GPIO_TXD), 1);

    // initialise serial port
    done = hlkld2410_status.pserial->begin (256000, SERIAL_8N1);

#else       // ESP8266
    // create serial port
    hlkld2410_status.pserial = new TasmotaSerial (Pin (GPIO_RXD), Pin (GPIO_TXD), 1);

    // initialise serial port
    done = hlkld2410_status.pserial->begin (256000, SERIAL_8N1);

    // force hardware configuration on ESP8266
    if (hlkld2410_status.pserial->hardwareSerial ()) ClaimSerial ();
#endif      // ESP32 & ESP8266

    // flush data
    if (done) hlkld2410_status.pserial->flush ();
  }

  // init device
  if (done) AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s sensor init succesfull"), D_HLKLD2410_NAME);
  else AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s sensor init failed"), D_HLKLD2410_NAME);

  return done;
}

/*
void HLKLDSetSensorType (const uint8_t type)
{
  char str_sensor[32];

  // check parameter
  if (type >= HLKLD_MODEL_MAX) return;

  // set default parameters according to sensor type
  switch (type)
  {
    case HLKLD_MODEL_1115H:
      hlkld_device.model = HLKLD_MODEL_1115H;
      hlkld_device.sensor[HLKLD_DATA_MOV][0].type      = HLKLD_PARAM_TH1;
      hlkld_device.sensor[HLKLD_DATA_MOV][0].power_def = 120;
      hlkld_device.sensor[HLKLD_DATA_MOV][0].dist_max  = 16;
      hlkld_device.sensor[HLKLD_DATA_OCC][0].type      = HLKLD_PARAM_TH2;
      hlkld_device.sensor[HLKLD_DATA_OCC][0].power_def = 250;
      hlkld_device.sensor[HLKLD_DATA_OCC][0].dist_max  = 16;
      break;
      
    case HLKLD_MODEL_1125H:
      hlkld_device.model = HLKLD_MODEL_1125H;
      hlkld_device.sensor[HLKLD_DATA_MOV][0].type      = HLKLD_PARAM_MTH1_MOV;
      hlkld_device.sensor[HLKLD_DATA_MOV][0].power_def = 60;
      hlkld_device.sensor[HLKLD_DATA_MOV][0].dist_max  = 2.8;
      hlkld_device.sensor[HLKLD_DATA_OCC][0].type      = HLKLD_PARAM_MTH1_OCC;
      hlkld_device.sensor[HLKLD_DATA_OCC][0].power_def = 60;
      hlkld_device.sensor[HLKLD_DATA_OCC][0].dist_max  = 2.8;
      hlkld_device.sensor[HLKLD_DATA_MOV][1].type      = HLKLD_PARAM_MTH2_MOV;
      hlkld_device.sensor[HLKLD_DATA_MOV][1].power_def = 30;
      hlkld_device.sensor[HLKLD_DATA_MOV][1].dist_max  = 8;
      hlkld_device.sensor[HLKLD_DATA_OCC][1].type      = HLKLD_PARAM_MTH2_OCC;
      hlkld_device.sensor[HLKLD_DATA_OCC][1].power_def = 30;
      hlkld_device.sensor[HLKLD_DATA_OCC][1].dist_max  = 8;
      hlkld_device.sensor[HLKLD_DATA_MOV][2].type      = HLKLD_PARAM_MTH3_MOV;
      hlkld_device.sensor[HLKLD_DATA_MOV][2].power_def = 20;
      hlkld_device.sensor[HLKLD_DATA_MOV][2].dist_max  = 20;
      hlkld_device.sensor[HLKLD_DATA_OCC][2].type      = HLKLD_PARAM_MTH3_OCC;
      hlkld_device.sensor[HLKLD_DATA_OCC][2].power_def = 20;
      hlkld_device.sensor[HLKLD_DATA_OCC][2].dist_max  = 20;
      break;
  }

  // apply full config
  HLKLDApplyConfig ();

  // log detection
  GetTextIndexed (str_sensor, sizeof (str_sensor), (uint32_t)type, HLKLDModelName);
  AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s sensor detected"), str_sensor);
}

void HLKLDSetPowerFromDistance (const uint8_t sensor, const float distance, const long power)
{
  uint8_t index = UINT8_MAX;

  // check parameters
  if (sensor >= HLKLD_DATA_ANY) return;

  // check which range is targeted
  if (isnan (distance)) index = 0;
  else if (isnan (hlkld_device.sensor[sensor][0].dist_max)) index = 0;
  else if (distance <= hlkld_device.sensor[sensor][0].dist_max) index = 0;
  else if (!isnan (hlkld_device.sensor[sensor][1].dist_max) && (distance <= hlkld_device.sensor[sensor][1].dist_max)) index = 1;
  else if (!isnan (hlkld_device.sensor[sensor][2].dist_max) && (distance <= hlkld_device.sensor[sensor][2].dist_max)) index = 2;


  // if sensor has been targeted, update data
  if (index != UINT8_MAX) HLKLDSetPowerAndDistance (sensor, index, power, distance);
}

void HLKLDApplyConfig ()
{
  int idx_start = 0;
  int idx_next  = 0;
  int idx_equal, idx_stop;

  // check parameters
  if (hlkld_status.str_config.length () == 0) return;

  // loop thru config keys
  while (idx_next != -1)
  {
    // get = and , separators
    idx_equal = hlkld_status.str_config.indexOf ('=', idx_start);
    idx_next  = hlkld_status.str_config.indexOf (',', idx_start);

    // if key and value are defined, extract them
    if (idx_next  == -1) idx_stop = hlkld_status.str_config.length (); else idx_stop = idx_next;
    if (idx_equal != -1) HLKLDSetDeviceConfig (hlkld_status.str_config.substring (idx_start, idx_equal).c_str (), hlkld_status.str_config.substring (idx_equal + 1, idx_stop).c_str ());

    // shift to next value
    idx_start = idx_next + 1; 
  }
}

struct hlkld_range* HLKLDGetRangeFromIndex (const uint8_t type)
{
  uint8_t sensor, range;
  struct  hlkld_range *prange = nullptr;

  // if parameter is known, search within existing sensor/range
  if (type < HLKLD_PARAM_MAX)
  {
    for (sensor = 0; sensor < HLKLD_SENSOR_MAX; sensor ++)
      for (range = 0; range < HLKLD_RANGE_MAX; range ++)
        if (hlkld_device.sensor[sensor][range].type == type) prange = &hlkld_device.sensor[sensor][range];
  }

  return prange;
}

struct hlkld_range* HLKLDGetRangeFromKey (const char* pstr_key)
{
  int    type;
  char   str_label[16];
  struct hlkld_range *prange = nullptr;

  // get parameter index and search within existing sensor/range
  type = GetCommandCode (str_label, sizeof (str_label), pstr_key, HLKLDParamLabel);
  if (type != -1) prange = HLKLDGetRangeFromIndex ((uint8_t)type);

  return prange;
}

// Get configuration update
struct hlkld_range* HLKLDGetLastActiveSensor ()
{
  uint8_t  sensor, range;
  uint32_t curr_time = LocalTime ();
  uint32_t last_time = 0;
  struct hlkld_range *psensor = nullptr;

  // loop thru sensors to get last range received
  for (sensor = 0; sensor < HLKLD_DATA_ANY; sensor ++)
    for (range = 0; range < HLKLD_RANGE_MAX; range ++)

      // if sensor has already got detection and is the newest
      if ((hlkld_device.sensor[sensor][range].time != UINT32_MAX) && (hlkld_device.sensor[sensor][range].time > last_time))
        {
          // save last detection time
          last_time = hlkld_device.sensor[sensor][range].time;

          // if timeout has not been reached, set sensor as the latest
          if (curr_time - last_time < hlkld_device.timeout) psensor = &hlkld_device.sensor[sensor][range];
        }

  return psensor;
}
*/

void HLKLD2410RunNextCommand ()
{
  uint8_t index = UINT8_MAX;

  // get command from current queue
  if (hlkld2410_command.queue_index < hlkld2410_command.queue_number)
  {
    // update queue
    index = hlkld2410_command.queue_index;
    hlkld2410_command.queue_index++;

    // if current command index is valid
    if (index < HLKLD2410_CMND_QUEUE_MAX)
    {
      // send command according to command type
      switch (hlkld2410_command.queue[index].type)
      {
        case HLKLD2410_CMND_ENABLE:
          hlkld2410_status.pserial->write (hlkld2410_cmnd_enable, sizeof (hlkld2410_cmnd_enable));
          break;

        case HLKLD2410_CMND_DISABLE:
          hlkld2410_status.pserial->write (hlkld2410_cmnd_disable, sizeof (hlkld2410_cmnd_disable));
          break;

        case HLKLD2410_CMND_FIRMWARE:
          hlkld2410_status.pserial->write (hlkld2410_cmnd_firmware, sizeof (hlkld2410_cmnd_firmware));
          break;
      }
    }
  }
}

void HLKLD2410AddCommand (const uint8_t command, const uint32_t param1, const uint32_t param2, const uint32_t param3)
{
  // check parameters
  if (command >= HLKLD2410_CMND_MAX) return;

  // set enable, command and disable
  hlkld2410_command.queue[0].type = HLKLD2410_CMND_ENABLE;
  hlkld2410_command.queue[1].type = command;
  hlkld2410_command.queue[1].param1 = param1;
  hlkld2410_command.queue[1].param2 = param2;
  hlkld2410_command.queue[1].param3 = param3;
  hlkld2410_command.queue[2].type = HLKLD2410_CMND_DISABLE;

  // reset command queue
  hlkld2410_command.queue_index  = 0;
  hlkld2410_command.queue_number = 3;
}

uint32_t HLKLD2410GetDelay (const uint8_t type)
{
  uint8_t  index;
  uint32_t time  = 0;
  uint32_t delay = UINT32_MAX;

  // check latest reading time
  if ((type == HLKLD2410_TYPE_OCC) || (type == HLKLD2410_TYPE_ANY)) time = max (time, hlkld2410_status.time_occ);
  if ((type == HLKLD2410_TYPE_MOV) || (type == HLKLD2410_TYPE_ANY)) time = max (time, hlkld2410_status.time_mov);

  // if time is valid
  if (time > 0) delay = LocalTime () - time;

  return delay;
}

void HLKLD2410GetDelayText (const uint8_t type, char* pstr_result, size_t size_result)
{
  uint32_t delay = UINT32_MAX;

  // check parameters
  if (pstr_result == nullptr) return;
  if (size_result < 12) return;

  // get delay
  delay = HLKLD2410GetDelay (type);

  // set unit according to value
  if (delay == UINT32_MAX) strcpy (pstr_result, "---");
  else if (delay >= 172800) sprintf (pstr_result, "%u days", delay / 86400);
  else if (delay >= 86400) sprintf (pstr_result, "1 day");
  else if (delay >= 3600) sprintf (pstr_result, "%u hr", delay / 3600);
  else if (delay >= 60) sprintf (pstr_result, "%u mn", delay / 60);
  else if (delay > 0) sprintf (pstr_result, "%u sec", delay);
  else strcpy (pstr_result, "now");
  
  return;
}

/*
bool HLKLDGetDetectionStatus (uint8_t type = HLKLD_DATA_ANY, uint32_t timeout = UINT32_MAX)
{
  uint32_t delay;

  // check parameters
  if (type > HLKLD_DATA_ANY) return false;

  // if no timeout given, use default one
  if (timeout == UINT32_MAX) timeout = hlkld_device.timeout;

  // calculate delays
  delay = HLKLDGetDelay (type);

  if (delay == UINT32_MAX) return false;
  else return (delay <= timeout);
}
*/

/*********************************************\
 *                   Callback
\*********************************************/

// driver initialisation
void HLKLD2410Init ()
{
  uint8_t index, sensor, range;

AddLog (LOG_LEVEL_INFO, PSTR ("HLK: Init serial port"));

  // init sensor
  HLKLD2410InitDevice ();

  // init command delimiter
  for (index = 0; index < 4; index ++) hlkld2410_status.recv_delimiter[index] = 0xFF;

  // add firmware reading command

  
/*
  // init device range arrays
  for (sensor = 0; sensor < HLKLD_SENSOR_MAX; sensor++)
    for (range = 0; range < HLKLD_RANGE_MAX; range++)
    {
      hlkld_device.sensor[sensor][range].type       = HLKLD_PARAM_MAX;
      hlkld_device.sensor[sensor][range].time       = 0;   
      hlkld_device.sensor[sensor][range].power_def  = LONG_MAX;
      hlkld_device.sensor[sensor][range].power_ref  = LONG_MAX;
      hlkld_device.sensor[sensor][range].power_last = LONG_MAX;
      hlkld_device.sensor[sensor][range].power_min  = LONG_MAX;
      hlkld_device.sensor[sensor][range].dist_max   = NAN;
      hlkld_device.sensor[sensor][range].dist_last  = NAN;
    }

  // first command will ask configuration
  strcpy (hlkld_status.str_buffer, "");
  HLKLDAddCommand (" ;get_all", true);

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: ld_help to get help on %s sensor driver"), D_HLKLD_NAME);
  */
}

// loop every second
void HLKLD2410EverySecond ()
{
  int  index = -1;
  char str_command[32];
/*
  // check sensor presence
  if (hlkld_status.pserial == nullptr) return;

  // if device is initialised and a command is in the pipe
  if ((TasmotaGlobal.uptime > 5) && HLKLDGetCommand (str_command, sizeof (str_command)))
  {
    // log command
    AddLog (LOG_LEVEL_INFO, PSTR ("HLK: Sending command %s"), str_command);

    // add CR + LF and send command
    strlcat (str_command, "\r\n", sizeof (str_command));
    hlkld_status.pserial->write (str_command, strlen (str_command));
  }

  // if needed, update virtual switch status
  if (hlkld_device.index < MAX_SWITCHES) Switch.virtual_state[hlkld_device.index] = HLKLDGetDetectionStatus (HLKLD_DATA_ANY);
*/
}

void HLKLD2410LogMessage (const bool is_command)
{
  uint8_t index;
  char    str_hex[4];
  char    str_type[8];
  String  str_log;

  // loop to generate string
  for (index = 0; index < hlkld2410_status.recv_size; index ++)
  {
    sprintf(str_hex, "%02X", hlkld2410_status.recv_buffer[index]);
    if (index > 0) str_log += " ";
    str_log += str_hex;
  }

  // log message
  if (is_command) strcpy (str_type, "recv"); else strcpy (str_type, "data"); 
  AddLog (LOG_LEVEL_INFO, PSTR ("HLK: %s = %s"), str_type, str_log.c_str ());
}

void HLKLD2410HandleMessageCommand ()
{
  // log reveived command
  HLKLD2410LogMessage (true);

  // init message buffer
  hlkld2410_status.recv_size = 0;
}

void HLKLD2410HandleMessageData ()
{
  uint8_t  dist_type;
  uint8_t  recv_mode;
  uint16_t recv_data;
  uint32_t time_now = LocalTime ();

  // calculate message size
  recv_data = hlkld2410_status.recv_buffer[4] + 256 * hlkld2410_status.recv_buffer[5];

  // detect standard or engineering mode
  if (hlkld2410_status.recv_buffer[6] == 0x01) recv_mode = HLKLD2410_MODE_ENGINEERING;
  else if (hlkld2410_status.recv_buffer[6] == 0x02) recv_mode = HLKLD2410_MODE_STANDARD;
  else recv_mode = HLKLD2410_MODE_MAX;

  // get distance type
  if (hlkld2410_status.recv_buffer[8] == 0x01) dist_type = HLKLD2410_TYPE_MOV;
  else if (hlkld2410_status.recv_buffer[8] == 0x02) dist_type = HLKLD2410_TYPE_OCC;
  else if (hlkld2410_status.recv_buffer[8] == 0x03) dist_type = HLKLD2410_TYPE_BOTH;
  else dist_type = HLKLD2410_TYPE_NONE;

  // get MOV distance and power
  if ((dist_type == HLKLD2410_TYPE_MOV) || (dist_type == HLKLD2410_TYPE_BOTH))
  {
    hlkld2410_status.mov.dist  = hlkld2410_status.recv_buffer[9] + 256 * hlkld2410_status.recv_buffer[10];
    hlkld2410_status.mov.power = hlkld2410_status.recv_buffer[11];
    hlkld2410_status.mov.time  = time_now;
  }

  // get OCC distance and power
    if ((dist_type == HLKLD2410_TYPE_OCC) || (dist_type == HLKLD2410_TYPE_BOTH))
  {
    hlkld2410_status.occ.dist  = hlkld2410_status.recv_buffer[12] + 256 * hlkld2410_status.recv_buffer[13];
    hlkld2410_status.occ.power = hlkld2410_status.recv_buffer[14];
    hlkld2410_status.occ.time  = time_now;
  }
  
  // if web debug ctive, log message
  if (Settings->weblog_level > 2) HLKLD2410LogMessage (false);

  // init message buffer
  hlkld2410_status.recv_size = 0;
}

uint8_t HLKLD2410CheckMessageState ()
{
  uint8_t result;

  // check command start
  if ((hlkld2410_status.recv_delimiter[0] == 0xFD) && (hlkld2410_status.recv_delimiter[1] == 0xFC) && (hlkld2410_status.recv_delimiter[2] == 0xFB) && (hlkld2410_status.recv_delimiter[3] == 0xFA)) result = HLKLD2410_MSG_CMND_START;
  else if ((hlkld2410_status.recv_delimiter[0] == 0x04) && (hlkld2410_status.recv_delimiter[1] == 0x03) && (hlkld2410_status.recv_delimiter[2] == 0x02) && (hlkld2410_status.recv_delimiter[3] == 0x01)) result = HLKLD2410_MSG_CMND_STOP;
  else if ((hlkld2410_status.recv_delimiter[0] == 0xF4) && (hlkld2410_status.recv_delimiter[1] == 0xF3) && (hlkld2410_status.recv_delimiter[2] == 0xF2) && (hlkld2410_status.recv_delimiter[3] == 0xF1)) result = HLKLD2410_MSG_DATA_START;
  else if ((hlkld2410_status.recv_delimiter[0] == 0xF8) && (hlkld2410_status.recv_delimiter[1] == 0xF7) && (hlkld2410_status.recv_delimiter[2] == 0xF6) && (hlkld2410_status.recv_delimiter[3] == 0xF5)) result = HLKLD2410_MSG_DATA_STOP;
  else result = hlkld2410_status.recv_state;

  return result;
}

// Handling of received data
void HLKLD2410ReceiveData ()
{
  uint8_t  msg_state;
  uint8_t  index;
  uint8_t  recv_data;
  uint8_t  msg_part;
  long     power;
  float    distance;
  char     *pstr_tag;
  char     str_recv[2];
  char     str_value[8];
  char     str_key[128];

  // check sensor presence
  if (hlkld2410_status.pserial == nullptr) return;

  // run serial receive loop
  while (hlkld2410_status.pserial->available ()) 
  {
    // update received message delimiter
    for (index = 1; index < 4; index ++) hlkld2410_status.recv_delimiter[index - 1] = hlkld2410_status.recv_delimiter[index];
    hlkld2410_status.recv_delimiter[3] = (uint8_t)hlkld2410_status.pserial->read ();

    // append received value
    if (hlkld2410_status.recv_size < HLKLD2410_MSG_SIZE_MAX) hlkld2410_status.recv_buffer[hlkld2410_status.recv_size++] = hlkld2410_status.recv_delimiter[3];

    // check current message state
    msg_state = HLKLD2410CheckMessageState ();
    switch (msg_state)
    {
      // new command starts
      case HLKLD2410_MSG_CMND_START:
        for (index = 0; index < 4; index ++) hlkld2410_status.recv_buffer[index] = hlkld2410_status.recv_delimiter[index];
        hlkld2410_status.recv_size = 4;
        hlkld2410_status.recv_state = HLKLD2410_MSG_CMND_BODY;
        break;

      // new sensor data starts
      case HLKLD2410_MSG_DATA_START:
        for (index = 0; index < 4; index ++) hlkld2410_status.recv_buffer[index] = hlkld2410_status.recv_delimiter[index];
        hlkld2410_status.recv_size = 4;
        hlkld2410_status.recv_state = HLKLD2410_MSG_DATA_BODY;
        break;

      // command stops
      case HLKLD2410_MSG_CMND_STOP:
        HLKLD2410HandleMessageCommand ();
        hlkld2410_status.recv_state = HLKLD2410_MSG_NONE;
        break;

      // sensor data stops
      case HLKLD2410_MSG_DATA_STOP:
        HLKLD2410HandleMessageData ();
        hlkld2410_status.recv_state = HLKLD2410_MSG_NONE;
        break;
    }
  }
}

// Show JSON status (for MQTT)
void HLKLD2410ShowJSON (bool append)
{
  uint32_t delay;
  char     str_text[32];
  String   str_config;

/*
  // check sensor presence
  if (hlkld_status.pserial == nullptr) return;

  // --------------------
  //   HLK-LD11 section
  //
  //   normal : "HLK-LD":{"timeout"=5,"occ":0,"mov":1,"any":1,"last"="2 s"}
  //   append : "HLK-LD":{"timeout"=5,"occ":0,"mov":1,"any":1,"last"="2 s","Model":"HLK-LD1125H","Config":{"test_mode":0,"rmax":6.00,"mth1_mov":80,"mth2_mov":50, ... }}
  // --------------------

  // add , in append mode or { in direct publish mode
  if (append) ResponseAppend_P (PSTR (",")); else Response_P (PSTR ("{"));

  // start of sensor section
  ResponseAppend_P (PSTR ("\"%s\":{"), D_HLKLD_NAME);

  // timeout
  ResponseAppend_P (PSTR ("\"timeout\":%u"), hlkld_device.timeout);

  // presence (occ)
  ResponseAppend_P (PSTR (",\"occ\":%d"), HLKLDGetDetectionStatus (HLKLD_DATA_OCC));

  // movement (mov)
  ResponseAppend_P (PSTR (",\"mov\":%d"), HLKLDGetDetectionStatus (HLKLD_DATA_MOV));

  // presence or movement (any)
  ResponseAppend_P (PSTR (",\"any\":%d"), HLKLDGetDetectionStatus (HLKLD_DATA_ANY));

  // last detection in human reading format
  HLKLDGetDelayText (HLKLD_DATA_ANY, str_text, sizeof (str_text));
  ResponseAppend_P (PSTR (",\"last\":\"%s\""), str_text);

  // in append mode, add model and all config parameters
  if (append)
  {
    // model
    GetTextIndexed (str_text, sizeof (str_text), hlkld_device.model, HLKLDModelName);
    ResponseAppend_P (PSTR (",\"Model\":\"%s\""), str_text);

    // add , if needed
    if (hlkld_status.str_config.length () > 0)
    {
      // trasnform config as JSON string
      str_config = hlkld_status.str_config;
      str_config.replace (",", ",\"");
      str_config.replace ("=", "\":");

      // start of sensor section
      ResponseAppend_P (PSTR (",\"Config\":{"));
      ResponseAppend_P (PSTR ("\"%s"), str_config.c_str ());
      ResponseAppend_P (PSTR ("}"));
    }
  }

  // end of section
  ResponseAppend_P (PSTR ("}"));

  // publish it if not in append mode
  if (!append)
  {
    ResponseAppend_P (PSTR ("}"));
    MqttPublishTeleSensor ();
  } 
*/
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Append HLK-LD11xx sensor data to main page
void HLKLD2410WebSensor ()
{
  char str_delay[32];
  char str_icon[8];

  GetTextIndexed (str_icon, sizeof (str_icon), HLKLD2410_TYPE_OCC, HLKLD2410DataIcon);
  HLKLD2410GetDelayText (HLKLD2410_TYPE_OCC, str_delay, sizeof (str_delay));
  WSContentSend_PD (PSTR ("{s}%s %s <i>(%s)</i>{m}%u <i>(%u cm)</i>{e}\n"), D_HLKLD2410_NAME, str_icon, str_delay, hlkld2410_status.occ.power, hlkld2410_status.occ.dist);

  GetTextIndexed (str_icon, sizeof (str_icon), HLKLD2410_TYPE_MOV, HLKLD2410DataIcon);
  HLKLD2410GetDelayText (HLKLD2410_TYPE_MOV, str_delay, sizeof (str_delay));
  WSContentSend_PD (PSTR ("{s}%s %s <i>(%s)</i>{m}%u <i>(%u cm)</i>{e}\n"), D_HLKLD2410_NAME, str_icon, str_delay, hlkld2410_status.mov.power, hlkld2410_status.mov.dist);

/*
  // check sensor presence
  if (hlkld_status.pserial == nullptr) return;

  // model name
  GetTextIndexed (str_text, sizeof (str_text), hlkld_device.model, HLKLDModelName);
  WSContentSend_P (PSTR ("{s}%s{m}"), str_text);

  // occ
  GetTextIndexed (str_icon, sizeof (str_icon), HLKLD_DATA_OCC, HLKLDDataIcon);
  HLKLDGetDelayText (HLKLD_DATA_OCC, str_text, sizeof (str_text));
  WSContentSend_P (PSTR ("%s %s<br>"), str_icon, str_text);

  // mov
  GetTextIndexed (str_icon, sizeof (str_icon), HLKLD_DATA_MOV, HLKLDDataIcon);
  HLKLDGetDelayText (HLKLD_DATA_MOV, str_text, sizeof (str_text));
  WSContentSend_P (PSTR ("%s %s{e}"), str_icon, str_text);
  */
}

/*
#ifdef USE_HLKLD11_WEB_CONFIG

void HLKLDWebConfigButton ()
{
  char str_model[32];

  // if model is detected, display configuration button
  if (hlkld_device.model != HLKLD_MODEL_MAX)
  {
    GetTextIndexed (str_model, sizeof (str_model), hlkld_device.model, HLKLDModelName);
    WSContentSend_P (PSTR ("<p><form action='/%s' method='get'><button>Configure %s</button></form></p>"), D_HLKLD_PAGE_CONFIG, str_model);
  }
}

void HLKLDWebPageConfigure ()
{
  uint8_t  sensor = UINT8_MAX;
  uint8_t  range  = UINT8_MAX;
  long     value  = LONG_MAX;
  float    dist_min = 0;
  char     str_min[8];
  char     str_max[8];
  char     str_param[16];
  char     str_command[24];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess ()) return;

  // parameter 'sensor'
  WebGetArg ("sensor", str_param, sizeof (str_param));
  if (strlen (str_param) > 0) sensor = (uint8_t)atoi (str_param);

  // parameter 'range'
  WebGetArg ("range", str_param, sizeof (str_param));
  if (strlen (str_param) > 0) range = (uint8_t)atoi (str_param);

  // parameter 'value'
  WebGetArg ("value", str_param, sizeof (str_param));
  if (strlen (str_param) > 0) value = atol (str_param);

  // if sensor type and range are defined, save value
  if ((sensor < HLKLD_DATA_ANY) && (range < HLKLD_RANGE_MAX) && (value != LONG_MAX))
  {
    // if value = 0 set reference as minimum power, else set value
    if (value == 0) hlkld_device.sensor[sensor][range].power_ref = hlkld_device.sensor[sensor][range].power_min;
    else hlkld_device.sensor[sensor][range].power_ref = value;

    // reset minimum power
    hlkld_device.sensor[sensor][range].power_min = LONG_MAX;

    // add command to the queue
    GetTextIndexed (str_param, sizeof (str_param), hlkld_device.sensor[sensor][range].type, HLKLDParamLabel);
    sprintf (str_command, "%s=%d", str_param, hlkld_device.sensor[sensor][range].power_ref);
    HLKLDAddCommand (str_command, true);
  }

  // else first display
  else
  {
    // loop to reset all minimum values
    for (sensor = 0; sensor < HLKLD_DATA_ANY; sensor ++)
      for (range = 0; range < HLKLD_RANGE_MAX; range ++)
        hlkld_device.sensor[sensor][range].power_min = LONG_MAX;
  }

  // beginning of form without authentification
  WSContentStart_P (D_HLKLD_CONFIGURE, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function update()\n"));
  WSContentSend_P (PSTR ("{\n"));
  WSContentSend_P (PSTR (" httpUpd=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR (" httpUpd.onreadystatechange=function()\n"));
  WSContentSend_P (PSTR (" {\n"));
  WSContentSend_P (PSTR ("  if (httpUpd.responseText.length>0)\n"));
  WSContentSend_P (PSTR ("  {\n"));
  WSContentSend_P (PSTR ("   arr_update=httpUpd.responseText.split('\\n');\n"));
  WSContentSend_P (PSTR ("   num_update=arr_update.length;\n"));
  WSContentSend_P (PSTR ("   for (i=0;i<num_update;i++)\n"));
  WSContentSend_P (PSTR ("   {\n"));
  WSContentSend_P (PSTR ("    arr_param=arr_update[i].split(';');\n"));
  WSContentSend_P (PSTR ("    if (arr_param.length>1)\n"));
  WSContentSend_P (PSTR ("    {\n"));
  WSContentSend_P (PSTR ("     if (arr_param[1].length>0) document.getElementById(arr_param[0]).textContent=arr_param[1];\n"));
  WSContentSend_P (PSTR ("     if (arr_param[2].length>0) document.getElementById(arr_param[0]).className=arr_param[2];\n"));
  WSContentSend_P (PSTR ("     if (arr_param[3].length>0) document.getElementById(arr_param[0]).style.visibility=arr_param[3];\n"));
  WSContentSend_P (PSTR ("     if (arr_param[4].length>0) document.getElementById(arr_param[0]).style.top=arr_param[4];\n"));
  WSContentSend_P (PSTR ("    }\n"));
  WSContentSend_P (PSTR ("   }\n"));
  WSContentSend_P (PSTR ("  }\n"));
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR (" httpUpd.open('GET','%s',true);\n"), D_HLKLD_PAGE_CONFIG_UPD);
  WSContentSend_P (PSTR (" httpUpd.send();\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("setInterval(function() {update();},1000);\n"));
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {color:white;background-color:#252525;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div a {color:white;}\n"));

  WSContentSend_P (PSTR ("div.title {font-size:2rem;font-weight:bold;margin-bottom:30px;text-align:center;}\n"));

  WSContentSend_P (PSTR ("div.grid {width:90%%;max-width:800px;margin-bottom:20px;margin:0.25rem auto;padding:0.1rem 0px;text-align:center;}\n"));
  WSContentSend_P (PSTR ("div.pad {width:46%%;min-width:320px;display:inline-block;font-size:40px;padding:0px;margin:20px 1%%;background:transparent;}\n"));
  WSContentSend_P (PSTR ("div.cell {position:relative;padding:0px 4px;margin:0px;border:none;border-radius:12px;}\n"));

  WSContentSend_P (PSTR ("div.occ div.cell {background:#131;border:1px solid #252;}\n"));
  WSContentSend_P (PSTR ("div.mov div.cell {background:#225;border:1px solid #338;}\n"));
  WSContentSend_P (PSTR ("div.occ div span {background:#252;}\n"));
  WSContentSend_P (PSTR ("div.mov div span {background:#338;}\n"));

  WSContentSend_P (PSTR ("div.line {content:'';position:absolute;top:0%%;width:70%%;left:15%%;border-top:1px dashed yellow;visibility:hidden;}\n"));

  WSContentSend_P (PSTR ("div span {margin:2px 2px;padding:1px 5px;float:none;border:1px solid transparent;border-radius:4px;}\n"));
  WSContentSend_P (PSTR ("div span.left {float:left;}\n"));
  WSContentSend_P (PSTR ("div span.right {float:right;}\n"));

  WSContentSend_P (PSTR ("div.top {font-size:20px;padding:0px 2px;margin:2px 0px 15px 0px;}\n"));
  WSContentSend_P (PSTR ("div.top span {font-size:14px;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.top span.left {color:red;}\n"));

  WSContentSend_P (PSTR ("div.old {height:48px;color:grey;font-size:32px;}\n"));
  WSContentSend_P (PSTR ("div.last {height:48px;color:yellow;font-size:40px;font-weight:bold;}\n"));

  WSContentSend_P (PSTR ("div.down {margin:15px 0px 4px 0px;font-size:18px;}\n"));
  WSContentSend_P (PSTR ("div.down span {font-size:12px;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));

  // device name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));
  WSContentSend_P (PSTR ("<div class='grid'>\n"));

  // loop thru sensors
  for (sensor = 0; sensor < HLKLD_DATA_ANY; sensor ++)
  {
    // get sensor label and icon
    GetTextIndexed (str_param,   sizeof (str_param),   sensor, HLKLDDataIcon);
    GetTextIndexed (str_command, sizeof (str_command), sensor, HLKLDDataLabel);
    WSContentSend_P (PSTR ("<div class='pad %s'>%s\n"), str_command, str_param);

    // loop thru sensors
    dist_min = 0;
    for (range = 0; range < HLKLD_RANGE_MAX; range ++)
    {
      if (!isnan (hlkld_device.sensor[sensor][range].dist_max) && (hlkld_device.sensor[sensor][range].power_ref != LONG_MAX))
      {
        WSContentSend_P (PSTR ("<div class='cell'>\n"));

        // distance line (hidden by default)
        WSContentSend_P (PSTR ("<div class='line' id='s%u-r%u-line'></div>\n"), sensor, range);

        // distance
        ext_snprintf_P (str_min, sizeof(str_min), PSTR ("%1_f"), &dist_min);
        ext_snprintf_P (str_max, sizeof(str_max), PSTR ("%1_f"), &hlkld_device.sensor[sensor][range].dist_max);
        dist_min = hlkld_device.sensor[sensor][range].dist_max;
        WSContentSend_P (PSTR ("<div class='top'>%s - %sm\n"), str_min, str_max);

        // lowest value
        WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=low'><span title='Lowest' class='left' id='s%u-r%u-low'>---</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, sensor, range);

        // default value
        value = hlkld_device.sensor[sensor][range].power_def; 
        WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=%d'><span title='Default' class='right'>%d</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, value, value);

        WSContentSend_P (PSTR ("</div>\n"));        // top

        // sensor power
        WSContentSend_P (PSTR ("<div class='old' id='s%u-r%u-last'>---</div>\n"), sensor, range);

        // sensor range adjustments
        WSContentSend_P (PSTR ("<div class='down'>%u\n"), hlkld_device.sensor[sensor][range].power_ref);

        // value - 1000
        value = LONG_MAX;
        if (hlkld_device.sensor[sensor][range].power_ref > 1000) value = hlkld_device.sensor[sensor][range].power_ref - 1000; 
        if (value != LONG_MAX) WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=%d'><span class='left'>-1000</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, value);
        else WSContentSend_P (PSTR ("<span class='left'>---</span>\n"));

        // value - 100
        value = LONG_MAX;
        if (hlkld_device.sensor[sensor][range].power_ref > 100) value = hlkld_device.sensor[sensor][range].power_ref - 100; 
        if (value != LONG_MAX) WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=%d'><span class='left'>-100</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, value);
        else WSContentSend_P (PSTR ("<span class='left'>---</span>\n"));

        // value - 10
        value = LONG_MAX;
        if (hlkld_device.sensor[sensor][range].power_ref > 10) value = hlkld_device.sensor[sensor][range].power_ref - 10; 
        if (value != LONG_MAX) WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=%d'><span class='left'>-10</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, value);
        else WSContentSend_P (PSTR ("<span class='left'>---</span>\n"));

        // value + 1000
        value = hlkld_device.sensor[sensor][range].power_ref + 1000;
        WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=%d'><span class='right'>+1000</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, value);

        // value + 100
        value = hlkld_device.sensor[sensor][range].power_ref + 100;
        WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=%d'><span class='right'>+100</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, value);

        // value + 10
        value = hlkld_device.sensor[sensor][range].power_ref + 10;
        WSContentSend_P (PSTR ("<a href='/%s?sensor=%u&range=%u&value=%d'><span class='right'>+10</span></a>\n"), D_HLKLD_PAGE_CONFIG, sensor, range, value);

        WSContentSend_P (PSTR ("</div>\n"));      // down
        WSContentSend_P (PSTR ("</div>\n"));      // cell
      }
    }
    WSContentSend_P (PSTR ("</div>\n"));          // pad
  }

  // specific LD1125H message
  if (hlkld_device.model == HLKLD_MODEL_1125H) WSContentSend_P (PSTR ("<p><i>To get power level, run following command in console</i> :<br><b>ld_cmnd test_mode=1</b></p>\n"));

  // end of page
  WSContentSend_P (PSTR ("</div>\n"));            // grid
  WSContentStop ();
}

// Send configuration update
//   Each line must be : ObjectID;Content;ClassName;Visibility;TopPositionValue
//   If a value is empty, it won't be updated
void HLKLDWebPageConfigUpdate ()
{
  bool    is_active;
  uint8_t sensor, range;
  float   dist_min, dist_percent;
  struct  hlkld_range *psensor;
  char    str_text[8];

  // get last active sensor
  psensor = HLKLDGetLastActiveSensor ();

  // start of data page
  WSContentBegin (200, CT_PLAIN);

  // loop thru sensors to send latest values
  for (sensor = 0; sensor < HLKLD_DATA_ANY; sensor ++)
    for (range = 0; range < HLKLD_RANGE_MAX; range ++)
    {
      // check if active sensor
      is_active = (psensor == &hlkld_device.sensor[sensor][range]);

      // if last received power defined, send it
      if (hlkld_device.sensor[sensor][range].power_last != LONG_MAX)
      {
        if (is_active) strcpy (str_text, "last"); else strcpy (str_text, "old");
        WSContentSend_P ("s%u-r%u-last;%d;%s;;\n", sensor, range, hlkld_device.sensor[sensor][range].power_last, str_text);
      }

      // minimum received power
      if (hlkld_device.sensor[sensor][range].power_min != LONG_MAX) WSContentSend_P ("s%u-r%u-low;%d;;;\n", sensor, range, hlkld_device.sensor[sensor][range].power_min, str_text);

      // if sensor is active and distance defined, show the line
      if (is_active)
      {
        // calculate minimum distance of current range
        if (range == 0) dist_min = 0;
          else dist_min = hlkld_device.sensor[sensor][range - 1].dist_max;
        
        // if distance not defined, set it to 50% of range, else calculate percentage within the range
        if (isnan (hlkld_device.sensor[sensor][range].dist_last)) dist_percent = 50;
          else dist_percent = 100 * (hlkld_device.sensor[sensor][range].dist_last - dist_min) / (hlkld_device.sensor[sensor][range].dist_max - dist_min);

        // display line
        WSContentSend_P ("s%u-r%u-line;;;visible;%u%%\n", sensor, range, (uint8_t)dist_percent);
      }

      // else hide line
      else WSContentSend_P ("s%u-r%u-line;;;hidden;\n", sensor, range);
    }

  // end of data page
  WSContentEnd ();
}

#endif  // USE_HLKLD11_WEB_CONFIG
*/

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// Teleinfo sensor
bool Xsns123 (uint8_t function)
{
  bool result = false;

  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      HLKLD2410Init ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kHLKLD2410Commands, HLKLD2410Command);
      break;
    case FUNC_EVERY_SECOND:
      HLKLD2410EverySecond ();
      break;
    case FUNC_JSON_APPEND:
      HLKLD2410ShowJSON (true);
      break;
    case FUNC_EVERY_100_MSECOND:
      HLKLD2410ReceiveData ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      HLKLD2410WebSensor ();
      break;

/*
#ifdef USE_HLKLD11_WEB_CONFIG
    case FUNC_WEB_ADD_BUTTON:
      HLKLDWebConfigButton ();
      break;
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on ("/" D_HLKLD_PAGE_CONFIG, HLKLDWebPageConfigure);
      Webserver->on ("/" D_HLKLD_PAGE_CONFIG_UPD, HLKLDWebPageConfigUpdate);
      break;
#endif  // USE_HLKLD24_WEB_CONFIG
*/

#endif  // USE_WEBSERVER

  }

  return result;
}

#endif     // USE_HLKLD24
#endif     // FIRMWARE_SAFEBOOT

