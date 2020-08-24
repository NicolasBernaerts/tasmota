/*
  xnrg_16_teleinfo.ino - France Teleinfo energy sensor support for Sonoff-Tasmota
  
  Copyright (C) 2019  Nicolas Bernaerts

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
#define XSNS_15   15

#include <TasmotaSerial.h>

// teleinfo constant
#define TELEINFO_LENGTH_MESSAGE      800        // max size of a complete message
#define TELEINFO_LENGTH_LINE         50         // max size of a message line
#define TELEINFO_LENGTH_PART         20         // max size of a message line part
#define TELEINFO_NUM_PART            4          // max number of message parts
#define TELEINFO_READ_TIMEOUT        150        // 150ms serial reading timeout
#define TELEINFO_VOLTAGE             230        // default contract voltage is 200V
#define TELEINFO_DEFAULT_CONTRACT    30         // default contract is 30A

// web configuration page
#define D_PAGE_TELEINFO_CONFIG       "teleinfo"
#define D_PAGE_TELEINFO_GRAPH        "graph"
#define D_CMND_TELEINFO_MODE         "mode"
#define D_WEB_TELEINFO_CHECKED       "checked"

// graph data
#define TELEINFO_GRAPH_STEP          5           // collect graph data every 5 mn
#define TELEINFO_GRAPH_SAMPLE        288         // 24 hours if data is collected every 5mn
#define TELEINFO_GRAPH_WIDTH         800      
#define TELEINFO_GRAPH_HEIGHT        400 
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
#define D_TELEINFO_1200              "TIC Historique (1200 bauds)"
#define D_TELEINFO_9600              "TIC Standard (9600 bauds)"
//#define D_TELEINFO_COUNTER           "Messages reçus"

// others
#define TELEINFO_MAX_PHASE           3      
#define TELEINFO_MESSAGE_BUFFER_SIZE 64

// form strings
const char TELEINFO_INPUT_SELECT[] PROGMEM  = "<input type='radio' name='%s' id='%d' value='%d' %s>%s";
const char TELEINFO_FORM_START[] PROGMEM    = "<form method='get' action='%s'>";
const char TELEINFO_FORM_STOP[] PROGMEM     = "</form>";
const char TELEINFO_FIELD_START[] PROGMEM   = "<fieldset><legend><b>&nbsp;%s&nbsp;</b></legend><br />";
const char TELEINFO_FIELD_STOP[] PROGMEM    = "</fieldset><br />";
const char TELEINFO_HTML_POWER[] PROGMEM    = "<text class='power' x='%d%%' y='%d%%'>%d</text>\n";
const char TELEINFO_HTML_DASH[] PROGMEM     = "<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n";
const char TELEINFO_HTML_BUTTON[] PROGMEM   = "<button name='save' type='submit' class='button bgrn'>%s</button>\n";
const char TELEINFO_HTML_OVERLOAD[] PROGMEM = "style='color:#FF0000;font-weight:bold;'";

// graph colors
const char arrColorPhase[TELEINFO_MAX_PHASE][8] PROGMEM = { "phase1", "phase2", "phase3" };

// overload states
enum TeleinfoMessagePart { TELEINFO_NONE, TELEINFO_ETIQUETTE, TELEINFO_DONNEE, TELEINFO_CHECKSUM };

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

// teleinfo data
long teleinfo_adco     = 0;
int  teleinfo_isousc   = 0;             // contract max current per phase
int  teleinfo_ssousc   = 0;             // contract max power per phase
int  teleinfo_papp     = 0;             // total apparent power
int  teleinfo_iinst[3] = { 0, 0, 0 };   // instant current for each phase
int  teleinfo_adir[3]  = { 0, 0, 0 };   // percentage of power for each phase

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

// graph 
int      teleinfo_graph_refresh;
uint32_t teleinfo_graph_index;
uint32_t teleinfo_graph_counter;
int      teleinfo_power_perphasis;
int      teleinfo_graph_papp[TELEINFO_MAX_PHASE];
int      arr_graph_papp[TELEINFO_MAX_PHASE][TELEINFO_GRAPH_SAMPLE];

// serial port
TasmotaSerial *teleinfo_serial = NULL;

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
  if ((actual_mode != 1200) && (actual_mode != 9600)) actual_mode = 0;
  
  return actual_mode;
}

// set teleinfo mode (baud rate)
void TeleinfoSetMode (uint16_t new_mode)
{
  // if within range, set baud rate
  if ((new_mode == 0) || (new_mode == 1200) || (new_mode == 9600))
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
  if ((pstr_etiquette != NULL) && (pstr_donnee != NULL) && (pstr_checksum != NULL))
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
  if (pstr_value != NULL)
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
    if ((pin[GPIO_TXD] < 99) && (pin[GPIO_RXD] < 99))
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
    teleinfo_serial = new TasmotaSerial (pin[GPIO_RXD], pin[GPIO_TXD], 1);
    
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
  int phase, index;

  // init default values
  teleinfo_graph_index     = 0;
  teleinfo_graph_counter   = 0;
  teleinfo_graph_refresh   = 60 * TELEINFO_GRAPH_STEP;
  teleinfo_power_perphasis = 0;

  // initialise graph data
  for (phase = 0; phase < TELEINFO_MAX_PHASE; phase++)
  {
    teleinfo_graph_papp[phase] = 0;
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++) 
      arr_graph_papp[phase][index] = 0;
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
          if (current_total == 0) Energy.apparent_power[index] = teleinfo_papp / 3;
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

        // increment message counter
        teleinfo_count++;
        break;

      // ---------------------------
      // \t or SPACE : new line part
      // ---------------------------
      case 9:
      case ' ':
        // update current line part
        if (teleinfo_line_part == TELEINFO_ETIQUETTE) str_teleinfo_etiquette = str_teleinfo_buffer;
        else if (teleinfo_line_part == TELEINFO_DONNEE) str_teleinfo_donnee = str_teleinfo_buffer;
        else if (teleinfo_line_part == TELEINFO_CHECKSUM) str_teleinfo_checksum = str_teleinfo_buffer;

        // switch to next part of line
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
        if (checksum_ok == true)
        {
          if (is_numeric && (str_teleinfo_etiquette == "ADCO")) teleinfo_adco = str_teleinfo_donnee.toInt ();
          else if (is_numeric && (str_teleinfo_etiquette == "IINST"))
          {
            current_inst = str_teleinfo_donnee.toInt ();
            if (teleinfo_iinst[0] != current_inst) teleinfo_updated = true;
            teleinfo_iinst[0] = current_inst;
          }   
          else if (is_numeric && (str_teleinfo_etiquette == "IINST1"))
          {
            current_inst = str_teleinfo_donnee.toInt ();
            if (teleinfo_iinst[0] != current_inst) teleinfo_updated = true;
            teleinfo_iinst[0] = current_inst;
          }   
          else if (is_numeric && (str_teleinfo_etiquette == "IINST2"))
          {
            current_inst = str_teleinfo_donnee.toInt ();
            if (teleinfo_iinst[1] != current_inst) teleinfo_updated = true;
            teleinfo_iinst[1] = current_inst;
          }   
          else if (is_numeric && (str_teleinfo_etiquette == "IINST3"))
          {
            current_inst = str_teleinfo_donnee.toInt ();
            if (teleinfo_iinst[2] != current_inst) teleinfo_updated = true;
            teleinfo_iinst[2] = current_inst;
            Energy.phase_count = 3;
          }
          else if (is_numeric && (str_teleinfo_etiquette == "ADPS"))
          {
            teleinfo_updated = true;
            teleinfo_adir[0] = str_teleinfo_donnee.toInt ();
          }
          else if (is_numeric && (str_teleinfo_etiquette == "ADIR1"))
          {
            teleinfo_updated = true;
            teleinfo_adir[0] = str_teleinfo_donnee.toInt ();
          }
          else if (is_numeric && (str_teleinfo_etiquette == "ADIR2"))
          {
            teleinfo_updated = true;
            teleinfo_adir[1] = str_teleinfo_donnee.toInt ();
          }
          else if (is_numeric && (str_teleinfo_etiquette == "ADIR3"))
          {
            teleinfo_updated = true;
            teleinfo_adir[2] = str_teleinfo_donnee.toInt ();
          }
          else if (is_numeric && (str_teleinfo_etiquette == "ISOUSC"))
          {
            teleinfo_isousc = str_teleinfo_donnee.toInt ();
            teleinfo_ssousc = teleinfo_isousc * 200;
          }
          else if (is_numeric && (str_teleinfo_etiquette == "PAPP"))    teleinfo_papp    = str_teleinfo_donnee.toInt ();
          else if (is_numeric && (str_teleinfo_etiquette == "BASE"))    teleinfo_base    = str_teleinfo_donnee.toInt ();
          else if (is_numeric && (str_teleinfo_etiquette == "HCHC"))    teleinfo_hchc    = str_teleinfo_donnee.toInt ();
          else if (is_numeric && (str_teleinfo_etiquette == "HCHP"))    teleinfo_hchp    = str_teleinfo_donnee.toInt ();
          else if (is_numeric && (str_teleinfo_etiquette == "BBRHCJB")) teleinfo_bbrhcjb = str_teleinfo_donnee.toInt ();
          else if (is_numeric && (str_teleinfo_etiquette == "BBRHPJB")) teleinfo_bbrhpjb = str_teleinfo_donnee.toInt ();
          else if (is_numeric && (str_teleinfo_etiquette == "BBRHCJW")) teleinfo_bbrhcjw = str_teleinfo_donnee.toInt ();
          else if (is_numeric && (str_teleinfo_etiquette == "BBRHPJW")) teleinfo_bbrhpjw = str_teleinfo_donnee.toInt ();
          else if (is_numeric && (str_teleinfo_etiquette == "BBRHCJR")) teleinfo_bbrhcjr = str_teleinfo_donnee.toInt ();
          else if (is_numeric && (str_teleinfo_etiquette == "BBRHPJR")) teleinfo_bbrhpjr = str_teleinfo_donnee.toInt ();
          else if (is_numeric && (str_teleinfo_etiquette == "EJPHN"))   teleinfo_ejphn   = str_teleinfo_donnee.toInt ();
          else if (is_numeric && (str_teleinfo_etiquette == "EJPHPM"))  teleinfo_ejphpm  = str_teleinfo_donnee.toInt ();
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
  int phase;

  // if overload has been detected, publish teleinfo data
  if (teleinfo_updated == true) TeleinfoShowJSON (false);

  // loop thru the phases, to update apparent power to the max on the period
  for (phase = 0; phase < Energy.phase_count; phase++)
    teleinfo_graph_papp[phase] = max (int (Energy.apparent_power[phase]), teleinfo_graph_papp[phase]);

  // increment delay counter and if delay reached, update history data
  if (teleinfo_graph_counter == 0) TeleinfoUpdateHistory ();
  teleinfo_graph_counter ++;
  teleinfo_graph_counter = teleinfo_graph_counter % teleinfo_graph_refresh;
}

// Show JSON status (for MQTT)
//  "TIC":{
//         "PHASE":3,"ADCO":"1234567890","ISOUSC":30,"SSOUSC":6000
//        ,"SINSTS":2567,"SINSTS1":1243,"IINST1":14.7,"ADIR1":"ON","SINSTS2":290,"IINST2":4.4,"ADIR2":"ON","SINSTS3":856,"IINST3":7.8,"ADIR3":"OFF"
//        }
void TeleinfoShowJSON (bool append)
{
  int    index, power_apparent, power_percent; 
  String str_json, str_mqtt, str_index, str_status;

  // reset update flag
  teleinfo_updated = false;

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
    // calculate data
    power_apparent = (int)Energy.apparent_power[index];
    if (teleinfo_ssousc > 0) power_percent = 100 * power_apparent / teleinfo_ssousc;
    else power_percent = 100;

    // generate strings
    str_index = String (index + 1);
    if (teleinfo_adir[index] > 0) str_status = MQTT_STATUS_ON;
    else str_status = MQTT_STATUS_OFF;

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

// update graph history data
void TeleinfoUpdateHistory ()
{
  int phase;

  // set indexed graph values with current values
  for (phase = 0; phase < Energy.phase_count; phase++)
  {
    arr_graph_papp[phase][teleinfo_graph_index] = teleinfo_graph_papp[phase];
    teleinfo_graph_papp[phase] = 0;
  }

  // increase power data index and reset if max reached
  teleinfo_graph_index ++;
  teleinfo_graph_index = teleinfo_graph_index % TELEINFO_GRAPH_SAMPLE;
}

/*********************************************\
 *                   Web
\*********************************************/

#ifdef USE_WEBSERVER

// Teleinfo mode select combo
void TeleinfoWebSelectMode ()
{
  uint16_t actual_mode;
  String   str_checked;

  // get mode
  actual_mode = TeleinfoGetMode ();

  // selection : disabled
  str_checked = "";
  if (actual_mode == 0) str_checked = D_WEB_TELEINFO_CHECKED;
  WSContentSend_P (TELEINFO_INPUT_SELECT, D_CMND_TELEINFO_MODE, 0, 0, str_checked.c_str (), D_TELEINFO_DISABLED);
  WSContentSend_P (PSTR ("<br/>"));

  if (teleinfo_configured == true)
  {
    // selection : 1200 baud
    str_checked = "";
    if (actual_mode == 1200) str_checked = D_WEB_TELEINFO_CHECKED;
    WSContentSend_P (TELEINFO_INPUT_SELECT, D_CMND_TELEINFO_MODE, 1200, 1200, str_checked.c_str (), D_TELEINFO_1200);
    WSContentSend_P (PSTR ("<br/>"));

    // selection : 9600 baud
    str_checked = "";
    if (actual_mode == 9600) str_checked = D_WEB_TELEINFO_CHECKED;
    WSContentSend_P (TELEINFO_INPUT_SELECT, D_CMND_TELEINFO_MODE, 9600, 9600, str_checked.c_str (), D_TELEINFO_9600);
    WSContentSend_P (PSTR ("<br/>"));
  }
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
  // display frame counter
  //WSContentSend_PD (PSTR("{s}%s{m}%d{e}"), D_TELEINFO_COUNTER, teleinfo_count);

  // display last TIC data received
  WSContentSend_PD (PSTR("{s}%s <small><i>(%d)</i></small>{m}%s{e}"), D_TELEINFO_TIC, teleinfo_count, str_teleinfo_last.c_str ());
}

// Teleinfo web page
void TeleinfoWebPageConfig ()
{
  char argument[TELEINFO_MESSAGE_BUFFER_SIZE];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get teleinfo mode according to MODE parameter
  if (WebServer->hasArg(D_CMND_TELEINFO_MODE))
  {
    WebGetArg (D_CMND_TELEINFO_MODE, argument, TELEINFO_MESSAGE_BUFFER_SIZE);
    TeleinfoSetMode ((uint16_t) atoi (argument)); 
  }

  // beginning of form
  WSContentStart_P (D_TELEINFO_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (TELEINFO_FORM_START, D_PAGE_TELEINFO_CONFIG);

  // mode selection
  WSContentSend_P (TELEINFO_FIELD_START, D_TELEINFO_MODE);
  TeleinfoWebSelectMode ();

  // end of form
  WSContentSend_P (TELEINFO_FIELD_STOP);
  WSContentSend_P (PSTR ("<button name='save' type='submit' class='button bgrn'>%s</button>"), D_SAVE);
  WSContentSend_P (TELEINFO_FORM_STOP);

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Apparent power graph
void TeleinfoWebDisplayGraph ()
{
  TIME_T   current_dst;
  uint32_t current_time;
  int      index, arridx, phase, hour, power, power_min, power_max;
  int      graph_x, graph_y, graph_left, graph_right, graph_width, graph_hour;  
  char     str_hour[4];
  String   str_color;

  // max power adjustment
  power_min = 0;
  power_max = teleinfo_ssousc;

  // loop thru phasis and power records
  for (phase = 0; phase < TELEINFO_MAX_PHASE; phase++)
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      power = arr_graph_papp[phase][index];
      if ((power != INT_MAX) && (power > power_max)) power_max = power;
    }

  // boundaries of SVG graph
  graph_left  = TELEINFO_GRAPH_PERCENT_START * TELEINFO_GRAPH_WIDTH / 100;
  graph_right = TELEINFO_GRAPH_PERCENT_STOP * TELEINFO_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentSend_P (PSTR ("<svg viewBox='0 0 %d %d'>\n"), TELEINFO_GRAPH_WIDTH, TELEINFO_GRAPH_HEIGHT);

  // graph curve zone
  WSContentSend_P (PSTR ("<rect x='%d%%' y='0%%' width='%d%%' height='100%%' rx='10' />\n"), TELEINFO_GRAPH_PERCENT_START, TELEINFO_GRAPH_PERCENT_STOP - TELEINFO_GRAPH_PERCENT_START);

  // graph separation lines
  WSContentSend_P (TELEINFO_HTML_DASH, TELEINFO_GRAPH_PERCENT_START, 25, TELEINFO_GRAPH_PERCENT_STOP, 25);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), TELEINFO_GRAPH_PERCENT_START, 50, TELEINFO_GRAPH_PERCENT_STOP, 50);
  WSContentSend_P (PSTR ("<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n"), TELEINFO_GRAPH_PERCENT_START, 75, TELEINFO_GRAPH_PERCENT_STOP, 75);

  // power units
  WSContentSend_P (PSTR ("<text class='unit' x='%d%%' y='%d%%'>VA</text>\n"), TELEINFO_GRAPH_PERCENT_STOP + 2, 5, 100);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 5, power_max);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 27, power_max * 3 / 4);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 52, power_max / 2);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 77, power_max / 4);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 99, 0);

  // --------------------
  //   Apparent power
  // --------------------

  // loop thru phasis
  for (phase = 0; phase < Energy.phase_count; phase++)
  {
    // loop for the target humidity graph
    WSContentSend_P (PSTR ("<polyline class='%s' points='"), arrColorPhase[phase]);
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      // get target temperature value and set to minimum if not defined
      arridx = (index + teleinfo_graph_index) % TELEINFO_GRAPH_SAMPLE;
      power  = arr_graph_papp[phase][arridx];

      // if power is defined
      if (power > 0)
      {
        // calculate current position
        graph_x = graph_left + (graph_width * index / TELEINFO_GRAPH_SAMPLE);
        graph_y = TELEINFO_GRAPH_HEIGHT - (power * TELEINFO_GRAPH_HEIGHT / power_max);

        // add the point to the line
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

  // calculate width of remaining (minutes) till next hour
  current_dst.hour = (current_dst.hour + 1) % 24;
  graph_hour = ((60 - current_dst.minute) * graph_width / 1440) - 15; 

  // if shift is too small, shift to next hour
  if (graph_hour < 0)
  {
    current_dst.hour = (current_dst.hour + 1) % 24;
    graph_hour += graph_width / 24; 
  }

  // dislay first time mark
  graph_x = graph_left + graph_hour;
  sprintf(str_hour, "%02d", current_dst.hour);
  WSContentSend_P (PSTR ("<text class='time' x='%d' y='52%%'>%sh</text>\n"), graph_x, str_hour);

  // dislay next 5 time marks (every 4 hours)
  for (index = 0; index < 5; index++)
  {
    current_dst.hour = (current_dst.hour + 4) % 24;
    graph_x += graph_width / 6;
    sprintf(str_hour, "%02d", current_dst.hour);
    WSContentSend_P (PSTR ("<text class='time' x='%d' y='52%%'>%sh</text>\n"), graph_x, str_hour);
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
}

// Graph public page
void TeleinfoWebPageGraph ()
{
  int     phase;
  float   value, target;
  String  str_power;

  // beginning of form without authentification with 60 seconds auto refresh
  WSContentStart_P (D_TELEINFO_GRAPH, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%d;URL=/%s' />\n"), 60, D_PAGE_TELEINFO_GRAPH);
  
  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {color:white;background-color:#303030;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));

  WSContentSend_P (PSTR (".title {font-size:5vh;}\n"));
  WSContentSend_P (PSTR (".contract {font-size:3vh;}\n"));
  WSContentSend_P (PSTR (".phase {font-size:4vh;}\n"));

  WSContentSend_P (PSTR (".power {font-size:24px;}\n"));
  WSContentSend_P (PSTR (".time {font-size:20px;}\n"));
  WSContentSend_P (PSTR (".bold {font-weight:bold;}\n"));

  WSContentSend_P (PSTR (".graph {max-width:%dpx;}\n"), TELEINFO_GRAPH_WIDTH);
  WSContentSend_P (PSTR (".phase1 {color:#FFFF33;}\n"));
  WSContentSend_P (PSTR (".phase2 {color:#FF8C00;}\n"));
  WSContentSend_P (PSTR (".phase3 {color:#FF0000;}\n"));

  WSContentSend_P (PSTR ("rect.graph {stroke:grey;stroke-dasharray:1;fill:#101010;}\n"));
  WSContentSend_P (PSTR ("polyline {fill:none;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.phase1 {stroke:yellow;}\n"));
  WSContentSend_P (PSTR ("polyline.phase2 {stroke:orange;}\n"));
  WSContentSend_P (PSTR ("polyline.phase3 {stroke:red;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("text.power {stroke:yellow;fill:yellow;}\n"));
  WSContentSend_P (PSTR ("text.unit {stroke:white;fill:white;}\n"));
  WSContentSend_P (PSTR ("text.time {stroke:white;fill:white;}\n"));

  WSContentSend_P (PSTR ("</style>\n</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));

  // room name
  WSContentSend_P (PSTR ("<div class='title bold'>%s</div>\n"), SettingsText(SET_FRIENDLYNAME1));

  // contract
  WSContentSend_P (PSTR ("<div class='contract'>%s %d</div>\n"), D_TELEINFO_REFERENCE, teleinfo_adco);

  // display apparent power
  for (phase = 0; phase < Energy.phase_count; phase++)
  {
    if (str_power.length () > 0) str_power += " / ";
    str_power += PSTR ("<span class='") + String (arrColorPhase[phase]) + PSTR ("'>");
    str_power += String (Energy.apparent_power[phase], 0);
    str_power += PSTR ("</span>");
  }
  WSContentSend_P (PSTR ("<div class='phase'>%s VA</div>\n"), str_power.c_str());

  // display power graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  TeleinfoWebDisplayGraph ();
  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
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
bool Xsns15 (uint8_t function)
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
      WebServer->on ("/" D_PAGE_TELEINFO_CONFIG, TeleinfoWebPageConfig);
      WebServer->on ("/" D_PAGE_TELEINFO_GRAPH, TeleinfoWebPageGraph);
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
