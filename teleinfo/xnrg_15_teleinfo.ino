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
#define D_WEB_TELEINFO_CHECKED       "checked"

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
#define D_TELEINFO_1200              "1200 bauds (Historique)"
#define D_TELEINFO_9600              "9600 bauds (Standard)"

// others
#define TELEINFO_PHASE_MAX           3      
#define TELEINFO_MESSAGE_BUFFER_SIZE 64

// form strings
const char TELEINFO_INPUT_SELECT[] PROGMEM  = "<input type='radio' name='%s' id='%d' value='%d' %s>%s";
const char TELEINFO_FORM_START[] PROGMEM    = "<form method='get' action='%s'>";
const char TELEINFO_FORM_STOP[] PROGMEM     = "</form>";
const char TELEINFO_FIELD_START[] PROGMEM   = "<fieldset><legend><b>&nbsp;%s&nbsp;</b></legend><br />";
const char TELEINFO_FIELD_STOP[] PROGMEM    = "</fieldset><br />";
const char TELEINFO_HTML_POWER[] PROGMEM    = "<text class='power' x='%d%%' y='%d%%'>%d</text>\n";
const char TELEINFO_HTML_DASH[] PROGMEM     = "<line class='dash' x1='%d%%' y1='%d%%' x2='%d%%' y2='%d%%' />\n";

// graph colors
const char *const arr_color_phase[] PROGMEM = { "phase1", "phase2", "phase3" };

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
int  teleinfo_papp_last  = 0;           // last published apparent power
int  teleinfo_papp_delta = 0;           // apparent power delta to publish

// teleinfo data
long teleinfo_adco   = 0;
int  teleinfo_isousc = 0;               // contract max current per phase
int  teleinfo_ssousc = 0;               // contract max power per phase
int  teleinfo_papp   = 0;               // total apparent power

int  teleinfo_iinst[TELEINFO_PHASE_MAX] = { 0, 0, 0 };   // instant current for each phase
int  teleinfo_adir[TELEINFO_PHASE_MAX]  = { 0, 0, 0 };   // percentage of power for each phase
int  teleinfo_live_papp[TELEINFO_PHASE_MAX]  = { 0, 0, 0 };   // last live apparent power
int  teleinfo_live_diff[TELEINFO_PHASE_MAX]  = { 0, 0, 0 };   // difference between last and previous apparent power

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
int teleinfo_graph_diff[TELEINFO_PHASE_MAX];                            // power difference since last value (in live only)
int teleinfo_graph_index[TELEINFO_PERIOD_MAX];                          // current array index per refresh period
int teleinfo_graph_counter[TELEINFO_PERIOD_MAX];                        // counter in seconds per refresh period 
int teleinfo_graph_papp[TELEINFO_PERIOD_MAX][TELEINFO_PHASE_MAX];       // current apparent power per refresh period and per phase
unsigned short arr_graph_papp[TELEINFO_PERIOD_MAX][TELEINFO_PHASE_MAX][TELEINFO_GRAPH_SAMPLE];

// serial port
TasmotaSerial *teleinfo_serial = NULL;

/****************************************\
 *               Icons
\****************************************/

// icon : teleinfo
const char tic_icon_0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAIAAAACABAMAAAAxEHz4AAAHXHpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHja1VdZdiM5DvznKeYIBAFwOQ7X9/oGc/wJkJmSJdtV7Zr+aaVlZlJcQEQggHTzv38t9x98glJ0oinHEqPHR4qUUHGT/fmclrzs/x+69vNLv3v8ENDFaPk8xnmNr+jX54QkV3977XepX+vkayF6LLw/bDvb/bgsuhbicPrpenblmlDjh+Nc39CvZa/F358lwRlDsR4HFyYT+/0/nJ34fCu+Bf8DEwb63cOcTv9n/7mH675w4OPuzX/+toyf7jgL3ceKb366+knf+vmxTXixiMJj5/DRotafaL/7b62R15rndFXAoyLxOtR9lH2HgQ3u5D0t4kr4Ku7Tvgqu7KvvQG3gqM35hodCAR5fJDSo0qK5204dJkqYIaENocPv1pc5hRI6nE4sdtEKyQGZwRnYdCDH6A4PW2jvW2w/bJax8yCMDITFgPHr5d47/vR6WWgtozmRzw9fwa5g/IIZhpz9xygAQuvyqW7/kjuNf/8YsAwEdbs544DVt7NEU3pyizfO7NVhqPgTL5TGtQBchL0VxhADAR+JoRTkUwiJCH7MwKfC8sASGhAgdRoGrAzCHAFODrY35iTaY4OG0w15ARDKEUGSLXQAlohKRLxlUKg6ZRVVjZo0a9EaOUrUGGOKplM1cZKkKaaUciqpZs6SNceccs4l1xIKQ8bUlVhSyaWUWrFplYq1KsZXdLTQuEnTFltquZVWO+jTpWuPPfXcS68jDB6QADfiSCOPMuqkCSpNmTrjTDPPMus";
const char tic_icon_1[] PROGMEM = "C1xYvWbriSiuvsuoDtQvVV9Tekfs1anShFjZQNi49UUN3SvcSZHKihhkQC0JAPBkCIHQwzHwmkWDIGWa+BHbMGmClGjiDDDEgKJOCLnpg90TuW9wcvPtT3MJXyDmD7p9Azhl0H5D7jNsXqI265ZY3QBaF8CkUkhF+MLfib1XICfu6n/zfaItgwV6qdLcow23NV0TXiEgPsBVQm4Ll6gueSvUsnkYqtoB2JJw8m1GJYBXuihAXEzZBwuzHpBh+YM5L674foJ5zSAOoF5kCoW4gVs64nzJIlKT7s72R0tXF1odZ1xImH9bjf9a6b36QDC4uABtnN5r1PMEGKci3A6yspWuYcYSpKURQI7sO6siYDU7rkTKcpL6MLpPjhBzZ2irytklZSfTsAaIbt9Xx6IgwiNuyImGWxDHzBLU6YqoW4glTOFWzbkaIQWhx2kweKjQT9RSrjunMlNGzVB6jIJ3JSoq8JL1GM2xoyceaXn4JnLtuUIEkH2BP8YnHBmhgdZXVuuGmiONs+XBkihNhNIdIGrNQaSHlCmYjvjvipA2YPluZE8vNBJkYERyDU3ntgyG9Mn5o6ttAFhiIxQFqa6OCKcHV3MdtPHQHaWIMyM+wUPFtLuWg8EingTDnGRGGyPpdUmbJdWTuA2HtsRDIBJFJKdRZKul2rm8QrRxiIW2+gfBq/hfqsxfNFwKIaYUoLAgF9nNLoVYQhYqCFkPXxHIY0jtcjxVQF0BJcM4alxzv1py/cLd7dNzuRuFiwTstajtBnPzSaXFw4gFVIQqckm9mXbqBWIPejXQeIp1oeY1csKHMNWEQpP/aDcXz4g65Ry0TAWqsDn";
const char tic_icon_2[] PROGMEM = "U03DR7YsjBDkSfzKFg2oSYDCM3VoRxMC/kbrKzlx4ggPd7jiJHuPdAqx7zeSwwSaE7oKbF/ANWAUGYVrFfYaKOVtiMIOR+H4GjVLMP5kGbosy9Lh8NNHGDYVab3wdXOyJZykfUZQ2peWTa6QUCXe6T3RJzTfOf2lG/8qiFCAqXDgEDILRk2+bntFHnZOz/jlK5lw6yoIBAWwY8XZAdKVuX9wA/Wx/P2ZGzPdo9XrPCgkEJBEV51ROVV7J9ZcY5HCDAMDmHyEhHlds5PfbzU97m3YTzv2mNkFaBh3Bt8rPpz9Z96MA7ALc1FZ5hhD8jv5nsbEbid1TrMSL8/Jf8d3+Qwl7alKglFBoOIudHoCY3E8PsCPQaJsXY5RmmKUPVVtsvktF4B4hXtRpqI+0eVOM/TGhX6345AMqAGnrTqOJdEu97KBdQ39DnBO/+D/e8tL9d6KpL7uCyag2e8ZInoAWcskeW7H7OmO8J+S9aCNQoIEzcxEJ248aGl6LsQGFnwua1dvPc7M7GWO1clTByot4G8oRXzswfubVdCdG3gaFllDtG1rsD492egIp32JRqch0yKlwYouhC0Sw3143tfrBs7CBTEXnuYQK5uE24StHwOV/94ugW0KGtMZG8kxueCxLyEiT9uQ8VO6qO84TaAzz2VzGZ76Jy6wAcNH2BpV7MFIfEPetOEsM/k84tqSjFk739QHphxEKGqP6ajwtlp1qZZlnSlVOVonIhmXG1hUQNR9wJ0UoIU4VQDoHhFhQ/O6shkbVnwnPfJZ2rPTtUKwxTPmkXCoQSk1ANZ9PmuLfojtusUPOC8mvELeOEtyqkT0/f57qQV";
const char tic_icon_3[] PROGMEM = "6vj/IqFLTKd2ZsVxRne0y6LqQ359kXgrjjsjR2vN8X9D38qsxfSENHRAAABhGlDQ1BJQ0MgcHJvZmlsZQAAeJx9kT1Iw0AcxV9TtSqtDmYQcchQnSyIijhqFYpQIdQKrTqYj35Bk4YkxcVRcC04+LFYdXBx1tXBVRAEP0CcHJ0UXaTE/yWFFjEeHPfj3b3H3TuAq5cVzeoYBzTdNlOJuJDJrgqhV/SARx8i6JIUy5gTxSR8x9c9Amy9i7Es/3N/joiasxQgIBDPKoZpE28QT2/aBuN9Yl4pSirxOfGYSRckfmS67PEb44LLHMvkzXRqnpgnFgptLLexUjQ14iniqKrplM9lPFYZbzHWylWleU/2wnBOX1lmOs1hJLCIJYgQIKOKEsqwEaNVJ8VCivbjPv4h1y+SSyZXCQo5FlCBBsn1g/3B726t/OSElxSOA50vjvMxAoR2gUbNcb6PHadxAgSfgSu95a/UgZlP0mstLXoE9G8DF9ctTd4DLneAwSdDMiVXCtLk8nng/Yy+KQsM3AK9a15vzX2cPgBp6ip5AxwcAqMFyl73eXd3e2//nmn29wMFd3J7E1jIhgAAAA9QTFRFAAAATYK9YYO1Z4Ox9+S5L/suVwAAAAF0Uk5TAEDm2GYAAAABYktHRACIBR1IAAAACXBIWXMAAC4jAAAuIwF4pT92AAAAB3RJTUUH5AkUFiUJGu7mDAAAAjZJREFUaN7tmVmSxCAIhpW+AHoC8f6HnCy9xBWRZHqmKrw41YYvP0YFHWP+voFzftY3bu7TgPXVtPmHWf/dZgOgFwCNToDTCpgFOCXgI8BrAagcwkmA/TqAtAB3";
const char tic_icon_4[] PROGMEM = "IsCpAfg/AXQmwHwFYLUA0AKMGmAnPkKsSxgHuKDb1W0KADEAskfFY5gDrBRg82dJOoauKuGoMW55IrbnHpaTCTP3drq3TLzxuEWUBNhLCRxanjUCMUOe+RcEaGsrVnd1ejHlDLjSWgrDUACFBOoNcFVApvXYEyNl/VWAawJKJRcBkAGgCEDXAtZuH7sA4L5z5Tlkpgpy+QKZuYJcxgmm/yHZjMOsN2SSnudWLJM1PbfkkCkdkEnI/Qod8KozxktEMAqL5rbG2KLK3+o+7XuGiWtNj9kqkcUB73fauQl6eOVU0QoHwEzVmi7amcNHuqrlQUD6uDwIyt4nDQJywVYYRFlzySSQmwI8QmdvHIkBXLG/Cy8UaEnikRrpZSTpUC9BngnAkePqOQAU3mmoAXA6QHqnAR0BVgsYu1Xp1VlWDsDBErFJaPfO3v5YHvA5nbrOTBu6O+n0DiXzTp+y1NBVGrfddtuvWVwsvJvyx/Wfb3vT2Uv8Mz/k29DzQNc91z1PFjXAy/PBALzZAGHZnz8Av9Q9C8DD3uS3rwkAV88csP749mQAYVew/OmPAOvSBq8EkN8A5NMQBIDnGNQApFXAAugFECs43O3QPonSPE1JY8uboEGAzQB3ntvtB9E2jrV6Ej9WAAAAAElFTkSuQmCC";
const char* arr_tic_icon[] PROGMEM = {tic_icon_0, tic_icon_1, tic_icon_2, tic_icon_3, tic_icon_4};

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
    // if live period, save difference with previous record
    if (graph_period == TELEINFO_LIVE)
    {
      teleinfo_live_diff[phase] = teleinfo_graph_papp[TELEINFO_LIVE][phase] - teleinfo_live_papp[phase];
      teleinfo_live_papp[phase] = teleinfo_graph_papp[TELEINFO_LIVE][phase];
    }

    // save graph data for current phase
    arr_graph_papp[graph_period][phase][index] = (unsigned short) teleinfo_graph_papp[graph_period][phase];
    teleinfo_graph_papp[graph_period][phase] = 0;
  }

  // increase data index in the graph
  index ++;
  teleinfo_graph_index[graph_period] = index % TELEINFO_GRAPH_SAMPLE;
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

// display base64 embeded icon
void TeleinfoWebDisplayIcon (uint8_t icon_height)
{
  uint8_t nbrItem, index;

  WSContentSend_P (PSTR ("<img height=%d src='data:image/png;base64,"), icon_height);
  nbrItem = sizeof (arr_tic_icon) / sizeof (char*);
  for (index=0; index<nbrItem; index++)
    if (arr_tic_icon[index] != nullptr) WSContentSend_P (arr_tic_icon[index]);
  WSContentSend_P (PSTR ("' >"));
}

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
  // display last TIC data received
  WSContentSend_PD (PSTR("{s}%s <small><i>(%d)</i></small>{m}%s{e}"), D_TELEINFO_TIC, teleinfo_count, str_teleinfo_last.c_str ());

  // display teleinfo icon
  WSContentSend_PD (PSTR("<tr><td colspan=2 style='width:100%;text-align:center;padding:10px;'>"));
  TeleinfoWebDisplayIcon (64);
  WSContentSend_PD (PSTR("</td></tr>\n"));
}

// Teleinfo web page
void TeleinfoWebPageConfig ()
{
  char argument[TELEINFO_MESSAGE_BUFFER_SIZE];

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

// Apparent power graph frame
void TeleinfoWebGraphFrame ()
{
  int index, phase, power_max, power_papp;
  int graph_left, graph_right, graph_width;  
  int graph_period = TELEINFO_LIVE;  

  // check graph period to be displayed
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++) if (Webserver->hasArg(arr_period_cmnd[index])) graph_period = index;

  // loop thru phasis and power records to calculate max power
  power_max = teleinfo_ssousc;
  for (phase = 0; phase < Energy.phase_count; phase++)
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      // update max power during the period
      power_papp = (int) arr_graph_papp[graph_period][phase][index];
      if ((power_papp != INT_MAX) && (power_papp > power_max)) power_max = power_papp;
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
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 4, power_max);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 26, power_max * 3 / 4);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 51, power_max / 2);
  WSContentSend_P (TELEINFO_HTML_POWER, 1, 76, power_max / 4);
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
  int      power_papp, power_max;
  TIME_T   current_dst;
  uint32_t current_time;
  String   str_text;

  // check graph period to be displayed
  for (index = 0; index < TELEINFO_PERIOD_MAX; index++) if (Webserver->hasArg(arr_period_cmnd[index])) graph_period = index;

  // loop thru phasis and power records
  power_max = teleinfo_ssousc;
  for (phase = 0; phase < Energy.phase_count; phase++)
    for (index = 0; index < TELEINFO_GRAPH_SAMPLE; index++)
    {
      power_papp = (int) arr_graph_papp[graph_period][phase][index];
      if ((power_papp != INT_MAX) && (power_papp > power_max)) power_max = power_papp;
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
  WSContentSend_P (PSTR ("rect {fill:#333;stroke-width:2;opacity:0.5;}\n"));
  WSContentSend_P (PSTR ("rect.phase1 {stroke:yellow;}\n"));
  WSContentSend_P (PSTR ("rect.phase2 {stroke:orange;}\n"));
  WSContentSend_P (PSTR ("rect.phase3 {stroke:red;}\n"));
  WSContentSend_P (PSTR ("polyline {fill:none;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.phase1 {stroke:yellow;}\n"));
  WSContentSend_P (PSTR ("polyline.phase2 {stroke:orange;}\n"));
  WSContentSend_P (PSTR ("polyline.phase3 {stroke:red;}\n"));
  WSContentSend_P (PSTR ("text.phase1 {font-size:28px;stroke:yellow;fill:yellow;}\n"));
  WSContentSend_P (PSTR ("text.phase2 {font-size:28px;stroke:orange;fill:orange;}\n"));
  WSContentSend_P (PSTR ("text.phase3 {font-size:28px;stroke:red;fill:red;}\n"));
  WSContentSend_P (PSTR ("text.diff-phase1 {font-size:16px;font-style:italic;stroke:yellow;fill:yellow;}\n"));
  WSContentSend_P (PSTR ("text.diff-phase2 {font-size:16px;font-style:italic;stroke:orange;fill:orange;}\n"));
  WSContentSend_P (PSTR ("text.diff-phase3 {font-size:16px;font-style:italic;stroke:red;fill:red;}\n"));
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
      if ((power_papp > 0) && (power_max > 0))
      {
        // calculate current position and add the point to the line
        graph_x = graph_left + (graph_width * index / TELEINFO_GRAPH_SAMPLE);
        graph_y = TELEINFO_GRAPH_HEIGHT - (power_papp * TELEINFO_GRAPH_HEIGHT / power_max);
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

  // ---------------
  //     Values
  // ---------------

  // if live display, add instant apparent power and delte per phasis
if (graph_period == TELEINFO_LIVE) for (phase = 0; phase < Energy.phase_count; phase++)
  {
    // get apparent power for current phasis
    str_text = String (teleinfo_live_papp[phase]);

    // calculate data position (centered if only one phasis)
    if (Energy.phase_count == 1) graph_x = TELEINFO_GRAPH_PERCENT_START + 34 - str_text.length ();
    else graph_x = TELEINFO_GRAPH_PERCENT_START + 7 + (27 * phase) - str_text.length ();
    graph_y = 2;
    unit_width = 11 + 2 * str_text.length ();

    // display apparent power
    WSContentSend_P ("<rect class='%s' x='%d%%' y='%d%%' width='%d%%' height='%d%%' rx='%d' ry='%d' />\n", arr_color_phase[phase], graph_x, graph_y, unit_width, 12, 10, 10);
    WSContentSend_P ("<text class='%s' x='%d%%' y='%d%%'>%s VA</text>\n", arr_color_phase[phase], graph_x + 2, graph_y + 6, str_text.c_str ());

    // if defined, display apparent power delta since last mesure
    if (teleinfo_live_diff[phase] == 0) str_text = "---";
    else if (teleinfo_live_diff[phase] < 0) str_text = "- " + String (abs (teleinfo_live_diff[phase])) + " VA";
    else str_text = "+ " + String (teleinfo_live_diff[phase]) + " VA";
    graph_x = ( graph_x * 2 + unit_width - 3 * str_text.length () / 2) / 2;
    WSContentSend_P ("<text class='diff-%s' x='%d%%' y='%d%%'>%s</text>\n", arr_color_phase[phase], graph_x + 1, graph_y + 10, str_text.c_str ());
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd();
}

// Graph public page
void TeleinfoWebPageGraph ()
{
  int    index;
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
  
  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#303030;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:12px auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR (".title {font-size:5vw;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".button {font-size:2vw;padding:0.5rem 1rem;border:1px #666 solid;background:none;color:#fff;border-radius:8px;}\n"));
  WSContentSend_P (PSTR (".active {background:#666;}\n"));
  WSContentSend_P (PSTR (".svg-container {position:relative;vertical-align:middle;overflow:hidden;width:100%%;max-width:%dpx;padding-bottom:65%%;}\n"), TELEINFO_GRAPH_WIDTH);
  WSContentSend_P (PSTR (".svg-content {display:inline-block;position:absolute;top:0;left:0;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function updateData() {\n"));
  WSContentSend_P (PSTR ("dataId='data';\n"));
  WSContentSend_P (PSTR ("now=new Date();\n"));
  WSContentSend_P (PSTR ("svgObject=document.getElementById(dataId);\n"));
  WSContentSend_P (PSTR ("svgObjectURL=svgObject.data;\n"));
  WSContentSend_P (PSTR ("svgObject.data=svgObjectURL.substring(0,svgObjectURL.indexOf('ts=')) + 'ts=' + now.getTime();\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("setInterval(function() {updateData();},%d);\n"), graph_refresh);
  WSContentSend_P (PSTR ("</script>\n"));

  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (TELEINFO_FORM_START, D_PAGE_TELEINFO_GRAPH);

  // room name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));

  // display icon
  WSContentSend_P (PSTR ("<div>"));
  TeleinfoWebDisplayIcon (64);
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
      Webserver->on ("/" D_PAGE_TELEINFO_CONFIG, TeleinfoWebPageConfig);
      Webserver->on ("/" D_PAGE_TELEINFO_GRAPH,  TeleinfoWebPageGraph);
      Webserver->on ("/" D_PAGE_TELEINFO_BASE_SVG, TeleinfoWebGraphFrame);
      Webserver->on ("/" D_PAGE_TELEINFO_DATA_SVG, TeleinfoWebGraphData);
      
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
