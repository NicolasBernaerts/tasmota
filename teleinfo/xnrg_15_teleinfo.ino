/*
  xnrg_16_teleinfo.ino - France Teleinfo energy sensor support for Sonoff-Tasmota
  
  Copyright (C) 2020  Nicolas Bernaerts

    05/05/2019 - v1.0   - Creation
    16/05/2019 - v1.1   - Add Tempo and EJP contracts
    08/06/2019 - v1.2   - Handle active and apparent power
    05/07/2019 - v2.0   - Rework with selection thru web interface
    02/01/2020 - v3.0   - Functions rewrite for Tasmota 8.x compatibility
    05/02/2020 - v3.1   - Add support for 3 phases meters
    14/03/2020 - v3.2   - Add apparent power graph
    05/04/2020 - v3.3   - Add Timezone management
    13/05/2020 - v3.4   - Add overload management per phase
    15/05/2020 - v3.5   - Add tele.info and tele.json pages
    19/05/2020 - v3.6   - Add configuration for first NTP server
    26/05/2020 - v3.7   - Add Information JSON page
    07/07/2020 - v3.7.1 - Enable discovery (mDNS)
    29/07/2020 - v3.8   - Add Meter section to JSON
    05/08/2020 - v4.0   - Major code rewrite, JSON section is now TIC, numbered like new official Teleinfo module
    24/08/2020 - v4.0.1 - Web sensor display update
    18/09/2020 - v4.1   - Based on Tasmota 8.4
    07/10/2020 - v5.0   - Handle different graph periods, javascript auto update
    18/10/2020 - v5.1   - Expose icon on web server
    25/10/2020 - v5.2   - Real time graph page update

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

#ifdef USE_ENERGY_SENSOR
#ifdef USE_TELEINFO

/*********************************************************************************************\
 * Teleinfo historical
 * docs https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf
 * Teleinfo hardware will be enabled if 
 *     Hardware RX = [Teleinfo 1200] or [Teleinfo 9600]
 *     Hardware TX = [Teleinfo TX]
\*********************************************************************************************/

/*************************************************\
 *               Variables
\*************************************************/

// declare teleinfo energy driver and sensor
#define XNRG_15   15
#define XSNS_99   99

#include <TasmotaSerial.h>

// teleinfo constant
#define TELEINFO_VOLTAGE             230        // default contract voltage is 200V

// web configuration page
#define D_PAGE_TELEINFO_CONFIG       "teleinfo"
#define D_PAGE_TELEINFO_GRAPH        "graph"
#define D_PAGE_TELEINFO_BASE_SVG     "base.svg"
#define D_PAGE_TELEINFO_DATA_SVG     "data.svg"
#define D_CMND_TELEINFO_MODE         "mode"

// graph data
#define TELEINFO_GRAPH_SAMPLE        365         // 1 day per year
#define TELEINFO_GRAPH_WIDTH         800      
#define TELEINFO_GRAPH_HEIGHT        500 
#define TELEINFO_GRAPH_PERCENT_START 10     
#define TELEINFO_GRAPH_PERCENT_STOP  90

// JSON message
#define TELEINFO_JSON_TIC            "TIC"
#define TELEINFO_JSON_PHASE          "PHASE"
#define TELEINFO_JSON_ADCO           "ADCO"
#define TELEINFO_JSON_ISOUSC         "ISOUSC"
#define TELEINFO_JSON_SSOUSC         "SSOUSC"
#define TELEINFO_JSON_IINST          "IINST"
#define TELEINFO_JSON_SINSTS         "SINSTS"
#define TELEINFO_JSON_ADIR           "ADIR"

#define D_TELEINFO_MODE              "Teleinfo counter"
#define D_TELEINFO_CONFIG            "Configure Teleinfo"
#define D_TELEINFO_GRAPH             "Graph"
#define D_TELEINFO_REFERENCE         "Contract n°"
#define D_TELEINFO_TIC               "TIC"
#define D_TELEINFO_DISABLED          "Désactivé"
#define D_TELEINFO_SPEED_DEFAULT     "bauds"
#define D_TELEINFO_SPEED_HISTO       "bauds (Historique)"
#define D_TELEINFO_SPEED_STD         "bauds (Standard)"

// others
#define TELEINFO_PHASE_MAX           3      
#define TELEINFO_MESSAGE_BUFFER_SIZE 64

// form strings
const char TELEINFO_INPUT_SELECT[] PROGMEM = "<input type='radio' name='%s' id='%d' value='%d' %s>%d %s";
const char TELEINFO_FORM_START[] PROGMEM   = "<form method='get' action='%s'>";
const char TELEINFO_FORM_STOP[] PROGMEM    = "</form>";
const char TELEINFO_FIELD_START[] PROGMEM  = "<fieldset><legend><b>&nbsp;%s&nbsp;</b></legend><br />";
const char TELEINFO_FIELD_STOP[] PROGMEM   = "</fieldset><br />";
const char TELEINFO_HTML_POWER[] PROGMEM   = "<text class='power' x='%d%%' y='%d%%'>%d</text>\n";
const char TELEINFO_HTML_DASH[] PROGMEM    = "<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n";

// graph colors
const char *const arr_color_phase[] PROGMEM = { "ph1", "ph2", "ph3" };

// week days name
const char *const arr_week_day[] PROGMEM = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

// TIC message parts
enum TeleinfoMessagePart { TELEINFO_NONE, TELEINFO_ETIQUETTE, TELEINFO_DONNEE, TELEINFO_CHECKSUM };

// overload states
enum TeleinfoGraphPeriod { TELEINFO_LIVE, TELEINFO_DAY, TELEINFO_WEEK, TELEINFO_MONTH, TELEINFO_YEAR, TELEINFO_PERIOD_MAX };
const char *const arr_period_cmnd[] PROGMEM = { "live", "day", "week", "month", "year" };
const char *const arr_period_label[] PROGMEM = { "Live", "Day", "Week", "Month", "Year" };
const long arr_period_sample[] = { 5, 236, 1657, 7338, 86400 };       // number of seconds between samples

// teleinfo driver status
bool teleinfo_configured = false;
bool teleinfo_enabled    = false;
bool teleinfo_updated    = false;
long teleinfo_count      = 0;

// teleinfo line handling
uint8_t teleinfo_line_part = TELEINFO_NONE;
String  str_teleinfo_buffer;
String  str_teleinfo_etiquette;
String  str_teleinfo_donnee;
String  str_teleinfo_checksum;
String  str_teleinfo_last;

// teleinfo update trigger
int  teleinfo_papp_last  = 0;       // last published apparent power
int  teleinfo_papp_delta = 0;       // apparent power delta to publish

// teleinfo data
long teleinfo_adco   = 0;
int  teleinfo_isousc = 0;           // contract max current per phase
int  teleinfo_ssousc = 0;           // contract max power per phase
int  teleinfo_papp   = 0;           // total apparent power

// teleinfo power counters
long teleinfo_total   = 0;
long teleinfo_base    = 0;
long teleinfo_hchc    = 0;
long teleinfo_hchp    = 0;
long teleinfo_bbrhcjb = 0;
long teleinfo_bbrhpjb = 0;
long teleinfo_bbrhcjw = 0;
long teleinfo_bbrhpjw = 0;
long teleinfo_bbrhcjr = 0;
long teleinfo_bbrhpjr = 0;
long teleinfo_ejphn   = 0;
long teleinfo_ejphpm  = 0;

// data per phase
int      teleinfo_iinst[TELEINFO_PHASE_MAX];       // instant current for each phase
int      teleinfo_adir[TELEINFO_PHASE_MAX];        // percentage of power for each phase
int      teleinfo_last_papp[TELEINFO_PHASE_MAX];   // last published apparent power
uint32_t teleinfo_last_diff[TELEINFO_PHASE_MAX];   // last time a power difference was published

// graph
bool teleinfo_graph_updated[TELEINFO_PERIOD_MAX];                       // flag to ask for graph update
int  teleinfo_graph_pmax[TELEINFO_PERIOD_MAX];                          // graph maximum power
int  teleinfo_graph_index[TELEINFO_PERIOD_MAX];                         // current array index per refresh period
int  teleinfo_graph_counter[TELEINFO_PERIOD_MAX];                       // counter in seconds per refresh period 
int  teleinfo_graph_papp[TELEINFO_PERIOD_MAX][TELEINFO_PHASE_MAX];      // current apparent power per refresh period and per phase
unsigned short arr_graph_papp[TELEINFO_PERIOD_MAX][TELEINFO_PHASE_MAX][TELEINFO_GRAPH_SAMPLE];

// serial port
TasmotaSerial *teleinfo_serial = nullptr;

/****************************************\
 *               Icons
\****************************************/

// icon : teleinfo
unsigned char teleinfo_icon_tic_png[] PROGMEM = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x04, 0x03, 0x00, 0x00, 0x00, 0x31, 0x10, 0x7c, 0xf8, 0x00, 0x00, 0x00, 0x12, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x6f, 0x6c, 0x00, 0x00, 0x00, 0x4d, 0x82, 0xbd, 0x61, 0x83, 0xb5, 0x67, 0x83, 0xb1, 0xf7, 0xe4, 0xb9, 0xab, 0xda, 0xea, 0xec, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66, 0x00, 0x00, 0x00, 0x01, 0x62, 0x4b, 0x47, 0x44, 0x00, 0x88, 0x05, 0x1d, 0x48, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x2e, 0x23, 0x00, 0x00, 0x2e, 0x23, 0x01, 0x78, 0xa5, 0x3f, 0x76, 0x00, 0x00, 0x00, 0x07, 0x74, 0x49, 0x4d, 0x45, 0x07, 0xe4, 0x0a, 0x12, 0x17, 0x07, 0x09, 0xde, 0x55, 0xef, 0x17, 0x00, 0x00, 0x02, 0x37, 0x49, 0x44, 0x41, 0x54, 0x68, 0xde, 0xed, 0x99, 0x59, 0x6e, 0xc4, 0x20, 0x0c, 0x86, 0x11, 0x9e, 0x83, 0x58, 0x70, 0x01, 0x64, 0xee, 0x7f, 0xb7, 0x66, 0x99, 0x25, 0xac, 0xc6, 0x38, 0xe9, 0xb4, 0x52, 0xfc, 0x42, 0x35, 0xc4, 0x5f, 0x7e, 0x13, 0xc0, 0x86, 0x1a, 0xf3, 0xf7, 0x0d, 0x9c, 0xf3, 0xb3, 0xbe, 0x71, 0x73, 0x9f, 0x06, 0xac, 0xaf, 0xa6, 0xcd, 0x3f, 0xcc, 0xfa, 0xef, 0x36, 0x1b, 0x00, 0xbd, 0x00, 0x68, 0x74, 0x02, 0x9c, 0x56, 0xc0, 0x2c, 0xc0, 0x29, 0x01, 0x1f, 0x01, 0x5e, 0x0b, 0x40, 0xe5, 0x10, 0x4e, 0x02, 0xec, 0xd7, 0x01, 0xa4, 0x05, 0xb8, 0x13, 0x01, 0x4e, 0x0d, 0xc0, 0xff, 0x09, 0xa0, 0x33, 0x01, 0xe6, 0x2b, 0x00, 0xab, 0x05, 0x80, 0x16, 0x60, 0xd4, 0x00, 0x3b, 0xf1, 0x11, 0x62, 0x5d, 0xc2, 0x38, 0xc0, 0x05, 0xdd, 0xae,
  0x6e, 0x53, 0x00, 0x88, 0x01, 0x90, 0x3d, 0x2a, 0x1e, 0xc3, 0x1c, 0x60, 0xa5, 0x00, 0x9b, 0x3f, 0x4b, 0xd2, 0x31, 0x74, 0x55, 0x09, 0x47, 0x8d, 0x71, 0xcb, 0x13, 0xb1, 0x3d, 0xf7, 0xb0, 0x9c, 0x4c, 0x98, 0xb9, 0xb7, 0xd3, 0xbd, 0x65, 0xe2, 0x8d, 0xc7, 0x2d, 0xa2, 0x24, 0xc0, 0x5e, 0x4a, 0xe0, 0xd0, 0xf2, 0xac, 0x11, 0x88, 0x19, 0xf2, 0xcc, 0xbf, 0x20, 0x40, 0x5b, 0x5b, 0xb1, 0xba, 0xab, 0xd3, 0x8b, 0x29, 0x67, 0xc0, 0x95, 0xd6, 0x52, 0x18, 0x86, 0x02, 0x28, 0x24, 0x50, 0x6f, 0x80, 0xab, 0x02, 0x32, 0xad, 0xc7, 0x9e, 0x18, 0x29, 0xeb, 0xaf, 0x02, 0x5c, 0x13, 0x50, 0x2a, 0xb9, 0x08, 0x80, 0x0c, 0x00, 0x45, 0x00, 0xba, 0x16, 0xb0, 0x76, 0xfb, 0xd8, 0x05, 0x00, 0xf7, 0x9d, 0x2b, 0xcf, 0x21, 0x33, 0x55, 0x90, 0xcb, 0x17, 0xc8, 0xcc, 0x15, 0xe4, 0x32, 0x4e, 0x30, 0xfd, 0x0f, 0xc9, 0x66, 0x1c, 0x66, 0xbd, 0x21, 0x93, 0xf4, 0x3c, 0xb7, 0x62, 0x99, 0xac, 0xe9, 0xb9, 0x25, 0x87, 0x4c, 0xe9, 0x80, 0x4c, 0x42, 0xee, 0x57, 0xe8, 0x80, 0x57, 0x9d, 0x31, 0x5e, 0x22, 0x82, 0x51, 0x58, 0x34, 0xb7, 0x35, 0xc6, 0x16, 0x55, 0xfe, 0x56, 0xf7, 0x69, 0xdf, 0x33, 0x4c, 0x5c, 0x6b, 0x7a, 0xcc, 0x56, 0x89, 0x2c, 0x0e, 0x78, 0xbf, 0xd3, 0xce, 0x4d, 0xd0, 0xc3, 0x2b, 0xa7, 0x8a, 0x56, 0x38, 0x00, 0x66, 0xaa, 0xd6, 0x74, 0xd1, 0xce, 0x1c, 0x3e, 0xd2, 0x55, 0x2d, 0x0f, 0x02, 0xd2, 0xc7, 0xe5, 0x41, 0x50, 0xf6, 0x3e, 0x69, 0x10, 0x90, 0x0b, 0xb6, 0xc2, 0x20, 0xca, 0x9a, 0x4b, 0x26, 0x81, 0xdc, 0x14, 0xe0, 0x11, 0x3a, 0x7b, 0xe3, 0x48, 0x0c, 0xe0, 0x8a, 0xfd, 0x5d, 0x78, 0xa1, 0x40, 0x4b, 0x12, 0x8f, 0xd4, 0x48, 0x2f, 0x23, 0x49, 0x87, 0x7a,
  0x09, 0xf2, 0x4c, 0x00, 0x8e, 0x1c, 0x57, 0xcf, 0x01, 0xa0, 0xf0, 0x4e, 0x43, 0x0d, 0x80, 0xd3, 0x01, 0xd2, 0x3b, 0x0d, 0xe8, 0x08, 0xb0, 0x5a, 0xc0, 0xd8, 0xad, 0x4a, 0xaf, 0xce, 0xb2, 0x72, 0x00, 0x0e, 0x96, 0x88, 0x4d, 0x42, 0xbb, 0x77, 0xf6, 0xf6, 0xc7, 0xf2, 0x80, 0xcf, 0xe9, 0xd4, 0x75, 0x66, 0xda, 0xd0, 0xdd, 0x49, 0xa7, 0x77, 0x28, 0x99, 0x77, 0xfa, 0x94, 0xa5, 0x86, 0xae, 0xd2, 0xb8, 0xed, 0xb6, 0xdb, 0x7e, 0xcd, 0xe2, 0x62, 0xe1, 0xdd, 0x94, 0x3f, 0xae, 0xff, 0x7c, 0xdb, 0x9b, 0xce, 0x5e, 0xe2, 0x9f, 0xf9, 0x21, 0xdf, 0x86, 0x9e, 0x07, 0xba, 0xee, 0xb9, 0xee, 0x79, 0xb2, 0xa8, 0x01, 0x5e, 0x9e, 0x0f, 0x06, 0xe0, 0xcd, 0x06, 0x08, 0xcb, 0xfe, 0xfc, 0x01, 0xf8, 0xa5, 0xee, 0x59, 0x00, 0x1e, 0xf6, 0x26, 0xbf, 0x7d, 0x4d, 0x00, 0xb8, 0x7a, 0xe6, 0x80, 0xf5, 0xc7, 0xb7, 0x27, 0x03, 0x08, 0xbb, 0x82, 0xe5, 0x4f, 0x7f, 0x04, 0x58, 0x97, 0x36, 0x78, 0x25, 0x80, 0xfc, 0x06, 0x20, 0x9f, 0x86, 0x20, 0x00, 0x3c, 0xc7, 0xa0, 0x06, 0x20, 0xad, 0x02, 0x16, 0x40, 0x2f, 0x80, 0x58, 0xc1, 0xe1, 0x6e, 0x87, 0xf6, 0x49, 0x94, 0xe6, 0x69, 0x4a, 0x1a, 0x5b, 0xde, 0x04, 0x0d, 0x02, 0x6c, 0x06, 0xb8, 0xf3, 0xdc, 0x6e, 0x3f, 0x61, 0x43, 0x14, 0x0c, 0xfe, 0x63, 0x6f, 0x4d, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
unsigned int teleinfo_icon_tic_len = 720;

/*******************************************\
 *               Accessor
\*******************************************/

// get teleinfo mode (baud rate)
uint16_t TeleinfoGetMode ()
{
  uint16_t actual_mode;

  // read actual teleinfo mode
  actual_mode = Settings.sbaudrate;

  // if outvalue, set to disabled
  if (actual_mode > 19200) actual_mode = 0;
  
  return actual_mode;
}

// set teleinfo mode (baud rate)
void TeleinfoSetMode (uint16_t new_mode)
{
  // if within range, set baud rate
  if (new_mode <= 19200)
  {
    // if mode has changed
    if (Settings.sbaudrate != new_mode)
    {
      // save mode
      Settings.sbaudrate = new_mode;

      // ask for restart
      restart_flag = 2;
    }
  }
}

/*********************************************\
 *               Functions
\*********************************************/

bool TeleinfoValidateChecksum (const char *pstr_etiquette, const char *pstr_donnee, const char *pstr_checksum) 
{
  bool    result = false;
  uint8_t checksum = 32;

  // check pointers 
  if ((pstr_etiquette != nullptr) && (pstr_donnee != nullptr) && (pstr_checksum != nullptr))
  {
    // if etiquette and donnee are defined and checksum is exactly one caracter
    if (strlen(pstr_etiquette) && strlen(pstr_donnee) && (strlen(pstr_checksum) == 1))
    {
      // add every char of etiquette and donnee to checksum
      while (*pstr_etiquette) checksum += *pstr_etiquette++ ;
      while(*pstr_donnee) checksum += *pstr_donnee++ ;
      checksum = (checksum & 63) + 32;
      
      // compare given checksum and calculated one
      result = (*pstr_checksum == checksum);
    }
  }

  return result;
}

bool TeleinfoValidateNumeric (const char *pstr_value) 
{
  bool result = false;

  // check pointer 
  if (pstr_value != nullptr)
  {
    // check that all are digits
    result = true;
    while(*pstr_value) if (isDigit(*pstr_value++) == false) result = false;
  }

  return result;
}

bool TeleinfoPreInit ()
{
  // if no energy sensor detected
  if (!energy_flg)
  {
    // if serial RX and TX are configured
    if (PinUsed(GPIO_TXD) && PinUsed(GPIO_RXD))
    {
      // set configuration flag
      teleinfo_configured = true;

      // set energy flag
      energy_flg = XNRG_15;
    }
  }
  
  return teleinfo_configured;
}

void TeleinfoInit ()
{
  uint16_t teleinfo_mode;

  // voltage not available in teleinfo
  Energy.voltage_available = false;

  // get teleinfo speed
  teleinfo_mode = TeleinfoGetMode ();

  // if sensor has been pre initialised
  if ((teleinfo_configured == true) && (teleinfo_mode > 0))
  {
    // set serial port
    teleinfo_serial = new TasmotaSerial (Pin(GPIO_RXD), Pin(GPIO_TXD), 1);
    
    // flush and set speed
    Serial.flush ();
    Serial.begin (teleinfo_mode, SERIAL_7E1);

    // check port allocated
    teleinfo_enabled = teleinfo_serial->hardwareSerial ();
    if ( teleinfo_enabled == true) ClaimSerial ();
  }

  // if teleinfo is not enabled, reset energy flag
  if ( teleinfo_enabled == false) energy_flg = ENERGY_NONE;
}

void TeleinfoGraphInit ()
{
  int period, phase, index;

  // initialise phase data
  for (phase = 0; phase < TELEINFO_PHASE_MAX; phase++)
  {
    teleinfo_iinst[TELEINFO_PHASE_MAX]     = 0;
    teleinfo_adir[TELEINFO_PHASE_MAX]      = 0;
    teleinfo_last_papp[TELEINFO_PHASE_MAX] = 0;
    teleinfo_last_diff[TELEINFO_PHASE_MAX] = UINT32_MAX;
  }

  // initialise graph data
  for (period = 0; period < TELEINFO_PERIOD_MAX; period++)
  {
    // init counter
    teleinfo_graph_index[period] = 0;
    teleinfo_graph_counter[period] = 0;

    // loop thru phase
    for (phase = 0; phase < TELEINFO_PHASE_MAX; phase++)
    {
      // init max power per period
      teleinfo_graph_papp[period][phase]  = 0;

      // loop thru graph values
      for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++) arr_graph_papp[period][phase][index] = 0;
    }
  }
}

void TeleinfoEvery250ms ()
{
  uint8_t  recv_serial, index;
  bool     checksum_ok, is_numeric;
  uint32_t teleinfo_delta, teleinfo_newtotal;
  int      current_inst, current_total;

  // loop as long as serial port buffer is not empty and timeout not reached
  while (teleinfo_serial->available () > 8)
  {
    // read caracter
    recv_serial = teleinfo_serial->read ();
    switch (recv_serial)
    {
      // ---------------------------
      // 0x02 : Beginning of message 
      // ---------------------------
      case 2:
        // reset overload flags
        for (index = 0; index < Energy.phase_count; index++) teleinfo_adir[index] = 0;
        break;
          
      // ---------------------
      // Ox03 : End of message
      // ---------------------
      case 3:
  
        // loop to calculate total current
        current_total = 0;
        for (index = 0; index < Energy.phase_count; index++) current_total += teleinfo_iinst[index];

        // loop to update current and power
        for (index = 0; index < Energy.phase_count; index++)
        {
          // calculate phase apparent power
          if (current_total == 0) Energy.apparent_power[index] = teleinfo_papp / Energy.phase_count;
          else Energy.apparent_power[index] = teleinfo_papp * teleinfo_iinst[index] / current_total;

          // update phase active power and instant current
          Energy.active_power[index] = Energy.apparent_power[index];
          Energy.current[index] = Energy.apparent_power[index] / TELEINFO_VOLTAGE;
        } 

        // update total energy counter
        teleinfo_newtotal = teleinfo_base + teleinfo_hchc + teleinfo_hchp;
        teleinfo_newtotal += teleinfo_bbrhcjb + teleinfo_bbrhpjb + teleinfo_bbrhcjw + teleinfo_bbrhpjw + teleinfo_bbrhcjr + teleinfo_bbrhpjr;
        teleinfo_newtotal += teleinfo_ejphn + teleinfo_ejphpm;
        teleinfo_delta = teleinfo_newtotal - teleinfo_total;
        teleinfo_total = teleinfo_newtotal;
        Energy.kWhtoday += (unsigned long)(teleinfo_delta * 100);
        EnergyUpdateToday ();

        // message update : if papp above ssousc
        if (teleinfo_papp > teleinfo_ssousc) teleinfo_updated = true;

        // message update : if more than 1% power change
        teleinfo_papp_delta = teleinfo_ssousc / 100;
        if (abs (teleinfo_papp_last - teleinfo_papp) > teleinfo_papp_delta) teleinfo_updated = true;

        // message update : if ADIR is above 100%
        for (index = 0; index < Energy.phase_count; index++)
          if (teleinfo_adir[index] >= 100) teleinfo_updated = true;

        // increment message counter
        teleinfo_count++;
        break;

      // ---------------------------
      // \t or SPACE : new line part
      // ---------------------------
      case 9:
      case ' ':
        // update current line part
        switch (teleinfo_line_part)
        {
          case TELEINFO_ETIQUETTE:
            str_teleinfo_etiquette = str_teleinfo_buffer;
            break;
          case TELEINFO_DONNEE:
            str_teleinfo_donnee = str_teleinfo_buffer;
            break;
          case TELEINFO_CHECKSUM:
            str_teleinfo_checksum = str_teleinfo_buffer;
        }

        // prepare next part of line
        teleinfo_line_part ++;
        str_teleinfo_buffer = "";
        break;

      // ------------------------
      // 0x0A : Beginning of line
      // ------------------------
      case 10:
        teleinfo_line_part = TELEINFO_ETIQUETTE;
        str_teleinfo_buffer    = "";
        str_teleinfo_etiquette = "";
        str_teleinfo_donnee    = "";
        str_teleinfo_checksum  = "";
        break;

      // ------------------
      // 0x0D : End of line
      // ------------------
      case 13:
        // retrieve checksum
        if (teleinfo_line_part == TELEINFO_CHECKSUM) str_teleinfo_checksum = str_teleinfo_buffer;

        // reset line part
        teleinfo_line_part = TELEINFO_NONE;

        // validate checksum and numeric format
        checksum_ok = TeleinfoValidateChecksum (str_teleinfo_etiquette.c_str (), str_teleinfo_donnee.c_str (), str_teleinfo_checksum.c_str ());
        is_numeric  = TeleinfoValidateNumeric (str_teleinfo_donnee.c_str ());

        // last line received
        str_teleinfo_last = str_teleinfo_etiquette + " " + str_teleinfo_donnee + " " + str_teleinfo_checksum;

        // if checksum is ok, handle the line
        if (is_numeric == true && checksum_ok == true)
        {
          if (str_teleinfo_etiquette == "ADCO") teleinfo_adco = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "IINST") teleinfo_iinst[0] = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "IINST1") teleinfo_iinst[0] = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "IINST2") teleinfo_iinst[1] = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "IINST3")
          {
            teleinfo_iinst[2] = str_teleinfo_donnee.toInt ();
            Energy.phase_count = 3;
          }
          else if (str_teleinfo_etiquette == "ADPS") teleinfo_adir[0] = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "ADIR1") teleinfo_adir[0] = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "ADIR2") teleinfo_adir[1] = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "ADIR3") teleinfo_adir[2] = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "ISOUSC")
          {
            teleinfo_isousc = str_teleinfo_donnee.toInt ();
            teleinfo_ssousc = teleinfo_isousc * 200;
          }
          else if (str_teleinfo_etiquette == "PAPP")    teleinfo_papp    = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "BASE")    teleinfo_base    = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "HCHC")    teleinfo_hchc    = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "HCHP")    teleinfo_hchp    = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "BBRHCJB") teleinfo_bbrhcjb = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "BBRHPJB") teleinfo_bbrhpjb = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "BBRHCJW") teleinfo_bbrhcjw = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "BBRHPJW") teleinfo_bbrhpjw = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "BBRHCJR") teleinfo_bbrhcjr = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "BBRHPJR") teleinfo_bbrhpjr = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "EJPHN")   teleinfo_ejphn   = str_teleinfo_donnee.toInt ();
          else if (str_teleinfo_etiquette == "EJPHPM")  teleinfo_ejphpm  = str_teleinfo_donnee.toInt ();
        }
        break;

      // if caracter is anything else : message part content
      default:
        // if a line has started and caracter is printable, add it to current message part
        if ((teleinfo_line_part > TELEINFO_NONE) && isprint (recv_serial)) str_teleinfo_buffer += (char) recv_serial;
        break;
      }
  }
}

void TeleinfoEverySecond ()
{
  int period, phase, power; 

  // loop thru the periods and the phases, to update apparent power to the max on the period
  for (period = 0; period < TELEINFO_PERIOD_MAX; period++)
  {
    // loop thru phases to update max value
    for (phase = 0; phase < Energy.phase_count; phase++)
    {
      if (isnan (Energy.apparent_power[phase])) power = 0;
      else power = (int) Energy.apparent_power[phase];
      teleinfo_graph_papp[period][phase] = max (power, teleinfo_graph_papp[period][phase]);
    }

    // increment graph period counter and update graph data if needed
    if (teleinfo_graph_counter[period] == 0) TeleinfoUpdateGraphData (period);
    teleinfo_graph_counter[period] ++;
    teleinfo_graph_counter[period] = teleinfo_graph_counter[period] % arr_period_sample[period];
  }

  // if current or overload has been updated, publish teleinfo data
  if (teleinfo_updated == true) TeleinfoShowJSON (false);
}

// update graph history data
void TeleinfoUpdateGraphData (int graph_period)
{
  int phase, index;

  // get graph index for the period
  index = teleinfo_graph_index[graph_period];

  // set indexed graph values with current values
  for (phase = 0; phase < Energy.phase_count; phase++)
  {
    // save graph data for current phase
    arr_graph_papp[graph_period][phase][index] = (unsigned short) teleinfo_graph_papp[graph_period][phase];
    teleinfo_graph_papp[graph_period][phase] = 0;
  }

  // increase data index in the graph
  index ++;
  teleinfo_graph_index[graph_period] = index % TELEINFO_GRAPH_SAMPLE;

  // set update flag
  teleinfo_graph_updated[graph_period] = true;
}

// Show JSON status (for MQTT)
//  "TIC":{
//         "PHASE":3,"ADCO":"1234567890","ISOUSC":30,"SSOUSC":6000
//        ,"SINSTS":2567,"SINSTS1":1243,"IINST1":14.7,"ADIR1":"ON","SINSTS2":290,"IINST2":4.4,"ADIR2":"ON","SINSTS3":856,"IINST3":7.8,"ADIR3":"OFF"
//        }
void TeleinfoShowJSON (bool append)
{
  int    index, power_apparent, power_percent; 
  String str_json, str_mqtt, str_index;

  // reset update flag and update published apparent power
  teleinfo_updated = false;
  teleinfo_papp_last = teleinfo_papp;

  // save mqtt_data
  str_mqtt = mqtt_data;

  // if not in append mode, add current time
  if (append == false) str_json = "\"" + String (D_JSON_TIME) + "\":\"" + GetDateAndTime(DT_LOCAL) + "\",";

  // start TIC section
  str_json += "\"" + String (TELEINFO_JSON_TIC) + "\":{";

  // if in append mode, add contract data
  if (append == true)
  {
    str_json += "\"" + String (TELEINFO_JSON_PHASE) + "\":" + String (Energy.phase_count);
    str_json += ",\"" + String (TELEINFO_JSON_ADCO) + "\":\"" + String (teleinfo_adco) + "\"";
    str_json += ",\"" + String (TELEINFO_JSON_ISOUSC) + "\":" + String (teleinfo_isousc);
    str_json += ",";
  }
 
  // add instant values
  str_json += "\"" + String (TELEINFO_JSON_SSOUSC) + "\":" + String (teleinfo_ssousc);
  str_json += ",\"" + String (TELEINFO_JSON_SINSTS) + "\":" + String (teleinfo_papp);
  for (index = 0; index < Energy.phase_count; index++)
  {
    // generate strings
    str_index = String (index + 1);

    // calculate data
    power_apparent = (int)Energy.apparent_power[index];
    if (teleinfo_ssousc > 0) power_percent = 100 * power_apparent / teleinfo_ssousc;
    else power_percent = 100;

    // add to JSON
    str_json += ",\"" + String (TELEINFO_JSON_SINSTS) + str_index + "\":" + String (power_apparent);
    str_json += ",\"" + String (TELEINFO_JSON_IINST) + str_index + "\":" + String (Energy.current[index], 1);
    str_json += ",\"" + String (TELEINFO_JSON_ADIR) + str_index + "\":" + String (power_percent);
  }

  // end of TIC section
  str_json += "}";

  // generate MQTT message according to append mode
  if (append == true) str_mqtt += "," + str_json;
  else str_mqtt = "{" + str_json + "}";

  // place JSON back to MQTT data and publish it if not in append mode
  snprintf_P (mqtt_data, sizeof(mqtt_data), str_mqtt.c_str ());
  if (append == false) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// display offload icons
void TeleinfoWebIconTic () { Webserver->send (200, "image/png", teleinfo_icon_tic_png, teleinfo_icon_tic_len); }

// get status update
void TeleinfoWebUpdate ()
{
  int graph_period = TELEINFO_LIVE;
  int index, power_papp, power_diff;
  float power_read;
  uint32_t time_now;
  String str_text;

  // timestamp
  time_now = millis ();

  // check graph period to be displayed
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++) if (Webserver->hasArg(arr_period_cmnd[index])) graph_period = index;

  // chech for graph update
  if (teleinfo_graph_updated[graph_period]) str_text = "1"; else str_text = "0";

  // check power and power difference for each phase
  for (index = 0; index < Energy.phase_count; index++)
  {
    // calculate power difference
    power_papp = (int)Energy.apparent_power[index];
    power_diff = power_papp - teleinfo_last_papp[index];

    // if power has changed
    str_text += ";";
    if (power_papp != teleinfo_last_papp[index])
    {
      teleinfo_last_papp[index] = power_papp;
      str_text += power_papp;
    }

    // if more than 100 VA power difference
    str_text += ";";
    if (abs (power_diff) >= 100)
    {
      if (power_diff < 0) str_text += "- " + String (abs (power_diff));
      else str_text += "+ " + String (power_diff);
      teleinfo_last_diff[index] = millis ();
    }

    // else if diff has been published 10 sec
    else if ((teleinfo_last_diff[index] != UINT32_MAX) && (time_now - teleinfo_last_diff[index] > 10000))
    {
      str_text += "---";
      teleinfo_last_diff[index] = UINT32_MAX;
    }
  }

  // send result
  Webserver->send (200, "text/plain", str_text.c_str (), str_text.length ());

  // reset update flags
  teleinfo_graph_updated[graph_period] = false;
}

// append Teleinfo graph button to main page
void TeleinfoWebMainButton ()
{
    // Teleinfo control page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_TELEINFO_GRAPH, D_TELEINFO_GRAPH);
}

// append Teleinfo configuration button to configuration page
void TeleinfoWebButton ()
{
  // heater and meter configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_TELEINFO_CONFIG, D_TELEINFO_CONFIG);
}

// append Teleinfo state to main page
bool TeleinfoWebSensor ()
{
  // display last TIC data received
  WSContentSend_PD (PSTR("{s}%s <small><i>(%d)</i></small>{m}%s{e}"), D_TELEINFO_TIC, teleinfo_count, str_teleinfo_last.c_str ());

  // display teleinfo icon
  WSContentSend_PD (PSTR("<tr><td colspan=2 style='width:100%;text-align:center;padding:10px;'><img height=64 src='tic.png'></td></tr>\n"));
}

// Teleinfo web page
void TeleinfoWebPageConfig ()
{
  uint16_t actual_mode;
  String   str_text;
  char     argument[TELEINFO_MESSAGE_BUFFER_SIZE];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get teleinfo mode according to MODE parameter
  if (Webserver->hasArg(D_CMND_TELEINFO_MODE))
  {
    WebGetArg (D_CMND_TELEINFO_MODE, argument, TELEINFO_MESSAGE_BUFFER_SIZE);
    TeleinfoSetMode ((uint16_t) atoi (argument)); 
  }

  // beginning of form
  WSContentStart_P (D_TELEINFO_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (TELEINFO_FORM_START, D_PAGE_TELEINFO_CONFIG);

  // mode selection
  actual_mode = TeleinfoGetMode ();
  WSContentSend_P (TELEINFO_FIELD_START, D_TELEINFO_MODE);

  // selection : disabled
  if (actual_mode == 0) str_text = "checked"; else str_text = "";
  WSContentSend_P ("<input type='radio' name='%s' id='%d' value='%d' %s>%s", D_CMND_TELEINFO_MODE, 0, 0, str_text.c_str (), D_TELEINFO_DISABLED);

  // selection : available modes
  if (teleinfo_configured == true)
  {
    if (actual_mode == 1200) str_text = "checked"; else str_text = "";      // 1200 baud
    WSContentSend_P (TELEINFO_INPUT_SELECT, D_CMND_TELEINFO_MODE, 1200, 1200, str_text.c_str (), 1200, D_TELEINFO_SPEED_HISTO);
    if (actual_mode == 2400) str_text = "checked"; else str_text = "";      // 2400 baud
    WSContentSend_P (TELEINFO_INPUT_SELECT, D_CMND_TELEINFO_MODE, 2400, 2400, str_text.c_str (), 2400, D_TELEINFO_SPEED_DEFAULT);
    if (actual_mode == 4800) str_text = "checked"; else str_text = "";      // 4800 baud
    WSContentSend_P (TELEINFO_INPUT_SELECT, D_CMND_TELEINFO_MODE, 4800, 4800, str_text.c_str (), 4800, D_TELEINFO_SPEED_DEFAULT);
    if (actual_mode == 9600) str_text = "checked"; else str_text = "";      // 9600 baud
    WSContentSend_P (TELEINFO_INPUT_SELECT, D_CMND_TELEINFO_MODE, 9600, 9600, str_text.c_str (), 9600, D_TELEINFO_SPEED_STD);
    if (actual_mode == 19200) str_text = "checked"; else str_text = "";     // 19200 baud
    WSContentSend_P (TELEINFO_INPUT_SELECT, D_CMND_TELEINFO_MODE, 19200, 19200, str_text.c_str (), 19200, D_TELEINFO_SPEED_DEFAULT);
  }

  // end of form
  WSContentSend_P (TELEINFO_FIELD_STOP);

  // save button
  WSContentSend_P (PSTR ("<button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (TELEINFO_FORM_STOP);

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Apparent power graph frame
void TeleinfoWebGraphFrame ()
{
  int index, phase, power_papp;
  int graph_left, graph_right, graph_width;  
  int graph_period = TELEINFO_LIVE;  

  // check graph period to be displayed
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++) if (Webserver->hasArg(arr_period_cmnd[index])) graph_period = index;

  // loop thru phasis and power records to calculate max power
  teleinfo_graph_pmax[graph_period] = teleinfo_ssousc;
  for (phase = 0; phase < Energy.phase_count; phase++)
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      // update max power during the period
      power_papp = (int) arr_graph_papp[graph_period][phase][index];
      if ((power_papp != INT_MAX) && (power_papp > teleinfo_graph_pmax[graph_period])) teleinfo_graph_pmax[graph_period] = power_papp;
    }

  // boundaries of SVG graph
  graph_left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100;
  graph_right = TELEINFO_GRAPH_PERCENT_STOP * TELEINFO_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d' preserveAspectRatio='xMinYMinmeet'>\n"), 0, 0, TELEINFO_GRAPH_WIDTH, TELEINFO_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("text.power {font-size:20px;stroke:white;fill:white;}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:16px;fill:white;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // graph frame
  WSContentSend_P (PSTR ("<rect x='%d%%' y='%d%%' width='%d%%' height='%d%%' rx='10' />\n"), TELEINFO_GRAPH_PERCENT_START, 0, TELEINFO_GRAPH_PERCENT_STOP - TELEINFO_GRAPH_PERCENT_START, 100);

  // graph separation lines
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 25, TELEINFO_GRAPH_PERCENT_STOP, 25);
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 50, TELEINFO_GRAPH_PERCENT_STOP, 50);
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 75, TELEINFO_GRAPH_PERCENT_STOP, 75);

  // power units
  WSContentSend_P (PSTR ("<text class='power' x='%d%%' y='%d%%'>(VA)</text>\n"), TELEINFO_GRAPH_PERCENT_STOP + 2, 4);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 4,  teleinfo_graph_pmax[graph_period]);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 26, teleinfo_graph_pmax[graph_period] * 3 / 4);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 51, teleinfo_graph_pmax[graph_period] / 2);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 76, teleinfo_graph_pmax[graph_period] / 4);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 99, 0);

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd();
}

// Apparent power graph curve
void TeleinfoWebGraphData ()
{
  int      index, phase, arridx;
  int      graph_left, graph_right, graph_width;  
  int      graph_x, graph_y;  
  int      unit_width, shift_unit, shift_width;  
  int      graph_period = TELEINFO_LIVE;  
  int      power_papp;
  TIME_T   current_dst;
  uint32_t current_time;
  String   str_text;

  // check graph period to be displayed
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++) if (Webserver->hasArg(arr_period_cmnd[index])) graph_period = index;

  // boundaries of SVG graph
  graph_left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100;
  graph_right = TELEINFO_GRAPH_PERCENT_STOP * TELEINFO_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d' preserveAspectRatio='xMinYMinmeet'>\n"), 0, 0, TELEINFO_GRAPH_WIDTH, TELEINFO_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("polyline {fill:none;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.ph1 {stroke:yellow;}\n"));
  WSContentSend_P (PSTR ("polyline.ph2 {stroke:orange;}\n"));
  WSContentSend_P (PSTR ("polyline.ph3 {stroke:red;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:16px;fill:grey;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // -----------------
  //   Power curves
  // -----------------

  // loop thru phasis
  for (phase = 0; phase < Energy.phase_count; phase++)
  {
    // loop for the apparent power graph
    WSContentSend_P (PSTR ("<polyline class='%s' points='"), arr_color_phase[phase]);
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      // get target temperature value and set to minimum if not defined
      arridx = (index + teleinfo_graph_index[graph_period]) % TELEINFO_GRAPH_SAMPLE;
      power_papp = arr_graph_papp[graph_period][phase][arridx];

      // if power is defined
      if ((power_papp > 0) && (teleinfo_graph_pmax[graph_period] > 0))
      {
        // calculate current position and add the point to the line
        graph_x = graph_left + (graph_width * index / TELEINFO_GRAPH_SAMPLE);
        graph_y = TELEINFO_GRAPH_HEIGHT - (power_papp * TELEINFO_GRAPH_HEIGHT / teleinfo_graph_pmax[graph_period]);
        WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
      }
    }
    WSContentSend_P (PSTR("'/>\n"));
  }

  // ------------
  //     Time
  // ------------

  // get current time
  current_time = LocalTime();
  BreakTime (current_time, current_dst);

  // handle graph units according to period
  switch (graph_period) 
  {
    case TELEINFO_LIVE:
      // calculate horizontal shift
      unit_width  = graph_width / 6;
      shift_unit  = current_dst.minute % 5;
      shift_width = unit_width - (unit_width * shift_unit / 5) - (unit_width * current_dst.second / 300);

      // calculate first time displayed by substracting (5 * 5mn + shift) to current time
      current_time -= (5 * 300) + (shift_unit * 60); 

      // display 5 mn separation lines with time
      for (index = 0; index < 6; index++)
      {
        // convert back to date and increase time of 5mn
        BreakTime (current_time, current_dst);
        current_time += 300;

        // display separation line and time
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%02dh%02d</text>\n"), graph_x - 25, 55, current_dst.hour, current_dst.minute);
      }
      break;

    case TELEINFO_DAY:
      // calculate horizontal shift
      unit_width  = graph_width / 6;
      shift_unit  = current_dst.hour % 4;
      shift_width = unit_width - (unit_width * shift_unit / 4) - (unit_width * current_dst.minute / 240);

      // calculate first time displayed by substracting (5 * 4h + shift) to current time
      current_time -= (5 * 14400) + (shift_unit * 3600); 

      // display 4 hours separation lines with hour
      for (index = 0; index < 6; index++)
      {
        // convert back to date and increase time of 4h
        BreakTime (current_time, current_dst);
        current_time += 14400;

        // display separation line and time
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%02dh</text>\n"), graph_x - 15, 55, current_dst.hour);
      }
      break;

    case TELEINFO_WEEK:
      // calculate horizontal shift
      unit_width = graph_width / 7;
      shift_width = unit_width - (unit_width * current_dst.hour / 24) - (unit_width * current_dst.minute / 1440);

      // display day lines with day name
      current_dst.day_of_week --;
      for (index = 0; index < 7; index++)
      {
        // calculate next week day
        current_dst.day_of_week ++;
        current_dst.day_of_week = current_dst.day_of_week % 7;

        // display month separation line and week day (first days or current day after 6pm)
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        if ((index < 6) || (current_dst.hour >= 18)) WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%s</text>\n"), graph_x + 30, 53, arr_week_day[current_dst.day_of_week]);
      }
      break;

    case TELEINFO_MONTH:
      // calculate horizontal shift
      unit_width  = graph_width / 6;
      shift_unit  = current_dst.day_of_month % 5;
      shift_width = unit_width - (unit_width * shift_unit / 5) - (unit_width * current_dst.hour / 120);

      // calculate first time displayed by substracting (5 * 5j + shift en j) to current time
      current_time -= (5 * 432000) + (shift_unit * 86400); 

      // display 5 days separation lines with day number
      for (index = 0; index < 6; index++)
      {
        // convert back to date and increase time of 5 days
        BreakTime (current_time, current_dst);
        current_time += 432000;

        // display separation line and day of month
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%02d</text>\n"), graph_x - 10, 55, current_dst.day_of_month);
      }
      break;
      
    case TELEINFO_YEAR:
      // calculate horizontal shift
      unit_width = graph_width / 12;
      shift_width = unit_width - (unit_width * current_dst.day_of_month / 30);

      // display month separation lines with month name
      for (index = 0; index < 12; index++)
      {
        // calculate next month value
        current_dst.month = (current_dst.month % 12);
        current_dst.month++;

        // convert back to date to get month name
        current_time = MakeTime (current_dst);
        BreakTime (current_time, current_dst);

        // display month separation line and month name (if previous month or current month after 20th)
        graph_x = graph_left + shift_width + (index * unit_width);
        WSContentSend_P (PSTR ("<line class='time' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n"), graph_x, 49, graph_x, 51);
        if ((index < 11) || (current_dst.day_of_month >= 24)) WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%s</text>\n"), graph_x + 12, 53, current_dst.name_of_month);
      }
      break;
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd();
}

// Graph public page
void TeleinfoWebPageGraph ()
{
  int    index, idx_param;
  int    graph_period = TELEINFO_LIVE;  
  long   graph_refresh;
  String str_text;

  // check graph period to be displayed
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++) if (Webserver->hasArg(arr_period_cmnd[index])) graph_period = index;

  // calculate graph refresh cycle in ms (max is 60 sec)
  graph_refresh  = (long) arr_period_sample[graph_period];
  if (graph_refresh > 60) graph_refresh = 60;
  graph_refresh *= 1000;
  
  // beginning of form without authentification with 60 seconds auto refresh
  WSContentStart_P (D_TELEINFO_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function update() {\n"));
  WSContentSend_P (PSTR ("httpTemp=new XMLHttpRequest();\n"));
  WSContentSend_P (PSTR ("httpTemp.onreadystatechange=function() {\n"));
  WSContentSend_P (PSTR (" if (httpTemp.responseText.length>0) {\n"));
  WSContentSend_P (PSTR ("  arr_param=httpTemp.responseText.split(';');\n"));
  WSContentSend_P (PSTR ("  str_random=Math.floor(Math.random()*100000);\n"));
  WSContentSend_P (PSTR ("  if (arr_param[0]==1) {document.getElementById('data').data='data.svg?%s&rnd='+str_random;}\n"), arr_period_cmnd[graph_period]);
  for (index = 0; index < Energy.phase_count; index++)
  {
    idx_param = 1 + index *2;
    WSContentSend_P (PSTR ("  if (arr_param[%d]!='') {document.getElementById('ph%d').innerHTML=arr_param[%d]+' VA';}\n"), idx_param, index + 1, idx_param);
    WSContentSend_P (PSTR ("  if (arr_param[%d]!='') {document.getElementById('dph%d').innerHTML=arr_param[%d];}\n"), idx_param + 1, index + 1, idx_param + 1);
  }
  WSContentSend_P (PSTR (" }\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("httpTemp.open('GET','tic.upd?%s',true);\n"), arr_period_cmnd[graph_period]);
  WSContentSend_P (PSTR ("httpTemp.send();\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("setInterval(function() {update();},1000);\n"));
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#303030;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:12px auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR (".title {font-size:5vw;font-weight:bold;}\n"));
  WSContentSend_P (PSTR ("div.papp {display:inline-block;width:auto;padding:0.25rem 0.75rem;margin:0.5rem;border-radius:8px;}\n"));
  WSContentSend_P (PSTR ("div.papp span.power {font-size:4vw;}\n"));
  WSContentSend_P (PSTR ("div.papp span.diff {font-size:2vw;font-style:italic;}\n"));
  WSContentSend_P (PSTR ("div.ph1 {color:yellow;border:1px yellow solid;}\n"));
  WSContentSend_P (PSTR ("div.ph1 span {color:yellow;}\n"));
  WSContentSend_P (PSTR ("div.ph2 {color:orange;border:1px orange solid;}\n"));
  WSContentSend_P (PSTR ("div.ph2 span {color:orange;}\n"));
  WSContentSend_P (PSTR ("div.ph3 {color:red;border:1px red solid;}\n"));
  WSContentSend_P (PSTR ("div.ph3 span {color:red;}\n"));
  WSContentSend_P (PSTR (".button {font-size:2vw;padding:0.5rem 1rem;border:1px #666 solid;background:none;color:#fff;border-radius:8px;}\n"));
  WSContentSend_P (PSTR (".active {background:#666;}\n"));
  WSContentSend_P (PSTR (".svg-container {position:relative;vertical-align:middle;overflow:hidden;width:100%%;max-width:%dpx;padding-bottom:65%%;}\n"), TELEINFO_GRAPH_WIDTH);
  WSContentSend_P (PSTR (".svg-content {display:inline-block;position:absolute;top:0;left:0;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page body
  WSContentSend_P (PSTR ("</head>\n"));
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (TELEINFO_FORM_START, D_PAGE_TELEINFO_GRAPH);

  // room name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));

  // display icon
  WSContentSend_P (PSTR ("<div><img height=64 src='tic.png'></div>\n"));

  // display values
  WSContentSend_P (PSTR ("<div>\n"));
  for (index = 0; index < Energy.phase_count; index++) WSContentSend_P (PSTR ("<div class='papp %s'><span class='power' id='%s'>%s VA</span><br><span class='diff' id='d%s'>---</span></div>\n"), arr_color_phase[index], arr_color_phase[index], String (Energy.apparent_power[index], 0).c_str (), arr_color_phase[index]);
  WSContentSend_P (PSTR ("</div>\n"));

  // display tabs
  WSContentSend_P (PSTR ("<div>\n"));
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++)
  {
    // if tab is the current graph period
    if (graph_period == index) str_text = "active";
    else str_text = "";

    // display button
    WSContentSend_P (PSTR ("<button name='%s' class='button %s'>%s</button>\n"), arr_period_cmnd[index], str_text.c_str (), arr_period_label[index]);
  }
  WSContentSend_P (PSTR ("</div>\n"));

  // display graph base and data
  WSContentSend_P (PSTR ("<div class='svg-container'>\n"));
  WSContentSend_P (PSTR ("<object class='svg-content' id='base' type='image/svg+xml' width='%d%%' height='%d%%' data='%s?%s'></object>\n"), 100, 100, D_PAGE_TELEINFO_BASE_SVG, arr_period_cmnd[graph_period]);
  WSContentSend_P (PSTR ("<object class='svg-content' id='data' type='image/svg+xml' width='%d%%' height='%d%%' data='%s?%s&ts=0'></object>\n"), 100, 100, D_PAGE_TELEINFO_DATA_SVG, arr_period_cmnd[graph_period]);
  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
  WSContentSend_P (TELEINFO_FORM_STOP);
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

// energy driver
bool Xnrg15 (uint8_t function)
{
  bool result = false;
  
  // swtich according to context
  switch (function)
  {
    case FUNC_PRE_INIT:
      result = TeleinfoPreInit ();
      break;
    case FUNC_INIT:
      TeleinfoInit ();
      break;
    case FUNC_EVERY_250_MSECOND:
      if ((teleinfo_enabled == true) && (uptime > 4)) TeleinfoEvery250ms ();
      break;
  }
  return result;
}

// teleinfo sensor
bool Xsns99 (uint8_t function)
{
  bool result = false;
  
  // swtich according to context
  switch (function) 
  {
    case FUNC_INIT:
      TeleinfoGraphInit ();
      break;
    case FUNC_EVERY_SECOND:
      TeleinfoEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      if (teleinfo_enabled == true) TeleinfoShowJSON (true);
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      // pages
      Webserver->on ("/" D_PAGE_TELEINFO_CONFIG, TeleinfoWebPageConfig);
      Webserver->on ("/" D_PAGE_TELEINFO_GRAPH,  TeleinfoWebPageGraph);
      Webserver->on ("/" D_PAGE_TELEINFO_BASE_SVG, TeleinfoWebGraphFrame);
      Webserver->on ("/" D_PAGE_TELEINFO_DATA_SVG, TeleinfoWebGraphData);

      // icons
      Webserver->on ("/tic.png", TeleinfoWebIconTic);
      
      // update status
      Webserver->on ("/tic.upd", TeleinfoWebUpdate);
      break;
    case FUNC_WEB_ADD_BUTTON:
      TeleinfoWebButton ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      TeleinfoWebMainButton ();
      break;
    case FUNC_WEB_SENSOR:
      TeleinfoWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }
  return result;
}

#endif   // USE_TELEINFO
#endif   // USE_ENERGY_SENSOR
