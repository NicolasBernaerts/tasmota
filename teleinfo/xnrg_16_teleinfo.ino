/*
  xnrg_16_teleinfo.ino - France Teleinfo energy sensor support for Sonoff-Tasmota
  
  Copyright (C) 2019  Nicolas Bernaerts

    05/05/2019 - v1.0 - Creation
    16/05/2019 - v1.1 - Add Tempo and EJP contracts
    08/06/2019 - v1.2 - Handle active and apparent power
    05/07/2019 - v2.0 - Rework with selection thru web interface
    02/01/2020 - v3.0 - Functions rewrite for Tasmota 8.x compatibility
    05/02/2020 - v3.1 - Add support for 3 phases meters
    14/03/2020 - v3.2 - Add power graph on /control page
    
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

// declare energy driver and teleinfo sensor
#define XNRG_16   16

#include <TasmotaSerial.h>

// teleinfo constant
#define TELEINFO_READ_TIMEOUT      200
#define TELEINFO_VOLTAGE           220        // default voltage is 220V
#define TELEINFO_CONTRACT          30         // default contract is 30A

// overload states
enum TeleinfoOverload { TELEINFO_OVERLOAD_NONE, TELEINFO_OVERLOAD_DETECTED, TELEINFO_OVERLOAD_READY };

// teleinfo status
bool    teleinfo_configured     = false;
bool    teleinfo_enabled        = false;
bool    teleinfo_message        = false;
long    teleinfo_phase          = 1;
long    teleinfo_framecount     = 0;
uint8_t teleinfo_overload       = TELEINFO_OVERLOAD_NONE;
float   teleinfo_current_total  = 0;
float   teleinfo_current[3]     = { 0, 0, 0 };
long    teleinfo_papp_total     = 0;
bool    teleinfo_papp_set       = false;

// teleinfo counters
long teleinfo_isousc  = TELEINFO_CONTRACT;
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

// other variables
TasmotaSerial *teleinfo_serial = NULL;
String str_teleinfo_json, str_teleinfo_buffer, str_teleinfo_line, str_teleinfo_contract;

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
    // save mode
    Settings.sbaudrate = new_mode;

    // ask for restart
    restart_flag = 2;
  }
}

// get contract power
int TeleinfoGetContractPower ()
{
  int power;

  // calculate power
  power = int (teleinfo_isousc) * TELEINFO_VOLTAGE;

  return power;
}

/*********************************************\
 *               Functions
\*********************************************/

// convert teleinfo value to float if valid, otherwise use default value
float TeleinfoConvertToFloat (String strValue, float defaultValue)
{
  bool  isValue = true;
  int   index, length;
  char  cValue[8];
  float newValue;

  // split string to char array
  strValue.toCharArray (cValue, 8);
  length = min (8, (int)strValue.length ());

  // loop to check if there is no junk
  for (index = 0; index < length; index ++) if (isDigit(cValue[index]) == false) isValue = false;

  // if provided value is numeric, convert it, else use provided default value
  if (isValue == true) newValue = strValue.toFloat (); 
  else newValue = defaultValue;

  return newValue;
}

// convert teleinfo value to uint if valid, otherwise use default value
long TeleinfoConvertToLong (String strValue, long defaultValue)
{
  bool isValue = true;
  int  index, length;
  char cValue[16];
  long newValue;

  // split string to char array
  strValue.toCharArray (cValue, 16);
  length = min (16, int (strValue.length ()));

  // loop to check if there is no junk
  for (index = 0; index < length; index ++) if (isDigit(cValue[index]) == false) isValue = false;

  // if provided value is numeric, convert it, else use provided default value
  if (isValue == true) newValue = strValue.toInt (); 
  else newValue = defaultValue;

  return newValue;
}

void TeleinfoEvery200ms ()
{
  int      recv_serial, index, index1, index2, index3;
  uint32_t teleinfo_delta, teleinfo_newtotal;
  float    current_total;
  ulong    time_start, time_now;
  String   str_etiquette, str_donnee, str_power;

  // init timers
  time_start = millis ();
  time_now   = time_start;

  // loop as long as serial port buffer is not empty and timeout not reached
  while ((teleinfo_serial->available () > 0) && (time_now >= time_start) && (time_now - time_start < TELEINFO_READ_TIMEOUT))
  {
    // read caracter
    recv_serial = teleinfo_serial->read ();
    switch (recv_serial)
    {
      // 0x02 : Beginning of message 
      case 2:
        // teleinfo message starts
        teleinfo_message = true;

        // reset JSON message buffer
        str_teleinfo_buffer = "";
        break;
          
      // Ox03 : End of message
      case 3:
        if (teleinfo_message == true)
        {
          // set number of phases
          Energy.phase_count = teleinfo_phase;

          // loop thru phases to calculate total current
          current_total = 0;
          for (index = 0; index < Energy.phase_count; index++)
          {
            Energy.current[index] = teleinfo_current[index];
            Energy.active_power[index] = TELEINFO_VOLTAGE * teleinfo_current[index];
            current_total += teleinfo_current[index];
          }

          // if apparent power not received, calculate according to previous value
          if (teleinfo_papp_set == false)
          {
            if (teleinfo_current_total > 0) teleinfo_papp_total = teleinfo_papp_total * current_total / teleinfo_current_total;
            else teleinfo_papp_total = 0;
          }
          teleinfo_papp_set = false;

          // update total current with calculated value
          teleinfo_current_total = current_total;

          // loop thru phases to update apparent power
          for (index = 0; index < Energy.phase_count; index++)
          {
            // update apparent power according to total current
            if (teleinfo_current_total > 0) Energy.apparent_power[index] = teleinfo_papp_total * Energy.current[index] / teleinfo_current_total;
            else Energy.apparent_power[index] = 0;

            // add information to teleinfo message
            str_power = String (Energy.apparent_power[index], 0);
            str_power.trim ();
            str_teleinfo_buffer += ",\"SINSTS" + String (index + 1) + "\":\"" + str_power + "\"";
          }

          // update total energy counter
          teleinfo_newtotal = teleinfo_base + teleinfo_hchc + teleinfo_hchp;
          teleinfo_newtotal += teleinfo_bbrhcjb + teleinfo_bbrhpjb + teleinfo_bbrhcjw + teleinfo_bbrhpjw + teleinfo_bbrhcjr + teleinfo_bbrhpjr;
          teleinfo_newtotal += teleinfo_ejphn + teleinfo_ejphpm;
          teleinfo_delta = teleinfo_newtotal - teleinfo_total;
          teleinfo_total = teleinfo_newtotal;
          Energy.kWhtoday += (unsigned long)(teleinfo_delta * 100);
          EnergyUpdateToday ();

          // generate final JSON and reset temporary JSON
          str_teleinfo_json = PSTR("\"Teleinfo\":{") + str_teleinfo_buffer + PSTR("}");

          // if overload detected, ready to be publushed
          if (teleinfo_overload == TELEINFO_OVERLOAD_DETECTED) teleinfo_overload = TELEINFO_OVERLOAD_READY;
        }

        // teleinfo message is over
        teleinfo_message = false;

        // increment frame counter
        teleinfo_framecount++;
        break;

      // 0x0A : Beginning of line
      case 10:
        // reset reception buffer
        str_teleinfo_line = "";
        break;

      // 0x0D : End of line
      case 13:
        // init
        str_etiquette = "";
        str_donnee    = "";

        // replace TAB by SPACE in current line
        str_teleinfo_line.replace ('\t', ' ');

        // split of message tokens
        index1 = str_teleinfo_line.indexOf(' ');
        index2 = -1;
        index3 = -1;
        if (index1 > 0) index2 = str_teleinfo_line.indexOf(' ', index1 + 1);
        if (index2 > 0) index3 = str_teleinfo_line.indexOf(' ', index2 + 1);

        // extraction of etiquette (1st token)
        if (index1 > 0) str_etiquette = str_teleinfo_line.substring (0, index1);

        // extraction of donnee
        //   - 2nd token if line of 3 tokens (no horodatage)
        //   - 3rd token if line of 4 tokens (with horodatage)
        if (index3 > 0) str_donnee = str_teleinfo_line.substring (index2 + 1, index3);
        else if (index2 > 0) str_donnee = str_teleinfo_line.substring (index1 + 1, index2);

        // if message is valid
        if ((str_etiquette.length () > 0) && (str_donnee.length () > 0))
        {
          // contract number
          if (str_etiquette.compareTo ("ADCO") == 0) str_teleinfo_contract = str_donnee;

          // max contract current
          else if (str_etiquette.compareTo ("ISOUSC") == 0) teleinfo_isousc = TeleinfoConvertToLong (str_donnee, teleinfo_isousc);

          // instant current and active power
          else if ((str_etiquette.compareTo ("IINST") == 0) || (str_etiquette.compareTo ("IINST1") == 0))
          {
            // save data
            teleinfo_current[0] = TeleinfoConvertToFloat (str_donnee, teleinfo_current[0]);
          }

          else if (str_etiquette.compareTo ("IINST2") == 0)
          {
            // save data
            teleinfo_phase = 3;
            teleinfo_current[1] = TeleinfoConvertToFloat (str_donnee, teleinfo_current[1]);
          }

          else if (str_etiquette.compareTo ("IINST3") == 0)
          {
            // save data
            teleinfo_phase = 3;
            teleinfo_current[2] = TeleinfoConvertToFloat (str_donnee, teleinfo_current[2]);
          }

          // apparent power
          else if (str_etiquette.compareTo ("PAPP") == 0)
          {
            teleinfo_papp_total = TeleinfoConvertToFloat (str_donnee, teleinfo_papp_total);
            teleinfo_papp_set = true;
          }
           
          // maximum power outage (ADPS / ADIR1..3)
          else if (str_etiquette.compareTo ("ADPS") == 0)  teleinfo_overload = TELEINFO_OVERLOAD_DETECTED;
          else if (str_etiquette.compareTo ("ADIR1") == 0) teleinfo_overload = TELEINFO_OVERLOAD_DETECTED;
          else if (str_etiquette.compareTo ("ADIR2") == 0) teleinfo_overload = TELEINFO_OVERLOAD_DETECTED;
          else if (str_etiquette.compareTo ("ADIR3") == 0) teleinfo_overload = TELEINFO_OVERLOAD_DETECTED;

          // Contract Base
          else if (str_etiquette.compareTo ("BASE") == 0) teleinfo_base = TeleinfoConvertToLong (str_donnee, teleinfo_base);

          // Contract Heures Creuses
          else if (str_etiquette.compareTo ("HCHC") == 0) teleinfo_hchc = TeleinfoConvertToLong (str_donnee, teleinfo_hchc);
          else if (str_etiquette.compareTo ("HCHP") == 0) teleinfo_hchp = TeleinfoConvertToLong (str_donnee, teleinfo_hchp);

          // Contract Tempo
          else if (str_etiquette.compareTo ("BBRHCJB") == 0) teleinfo_bbrhcjb = TeleinfoConvertToLong (str_donnee, teleinfo_bbrhcjb);
          else if (str_etiquette.compareTo ("BBRHPJB") == 0) teleinfo_bbrhpjb = TeleinfoConvertToLong (str_donnee, teleinfo_bbrhpjb);
          else if (str_etiquette.compareTo ("BBRHCJW") == 0) teleinfo_bbrhcjw = TeleinfoConvertToLong (str_donnee, teleinfo_bbrhcjw);
          else if (str_etiquette.compareTo ("BBRHPJW") == 0) teleinfo_bbrhpjw = TeleinfoConvertToLong (str_donnee, teleinfo_bbrhpjw);
          else if (str_etiquette.compareTo ("BBRHCJR") == 0) teleinfo_bbrhcjr = TeleinfoConvertToLong (str_donnee, teleinfo_bbrhcjr);
          else if (str_etiquette.compareTo ("BBRHPJR") == 0) teleinfo_bbrhpjr = TeleinfoConvertToLong (str_donnee, teleinfo_bbrhpjr);

          // Contract EJP
          else if (str_etiquette.compareTo ("EJPHN")  == 0) teleinfo_ejphn  = TeleinfoConvertToLong (str_donnee, teleinfo_ejphn);
          else if (str_etiquette.compareTo ("EJPHPM") == 0) teleinfo_ejphpm = TeleinfoConvertToLong (str_donnee, teleinfo_ejphpm);

          // add current data to JSON message
          if (str_teleinfo_buffer.length () > 0) str_teleinfo_buffer += ",";
          str_teleinfo_buffer += "\"" + str_etiquette + "\":\"" + str_donnee + "\"";
        }
        break;

      // if caracter is anything else : message body
      default:
        // if caracter printable or tab, add it to message buffer
        if ((isprint (recv_serial)) || (recv_serial == 9)) str_teleinfo_line += (char) recv_serial;
      }

    // update timer
    time_now = millis();
  }
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
      energy_flg = XNRG_16;
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

/***************************************\
 *              Interface
\***************************************/

// energy driver
bool Xnrg16 (uint8_t function)
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
    case FUNC_EVERY_200_MSECOND:
      if (teleinfo_enabled == true) TeleinfoEvery200ms ();
      break;
  }
  return result;
}

#endif   // USE_TELEINFO
#endif   // USE_ENERGY_SENSOR
