/*
  xnrg_15_teleinfo.ino - France Teleinfo energy sensor support for Tasmota devices

  Copyright (C) 2019-2023  Nicolas Bernaerts

  Connexions :
    * On ESP8266, Teleinfo Rx must be connected to GPIO3 (as it must be forced to 7E1)
    * On ESP32, Teleinfo Rx must NOT be connected to GPIO3 (to avoid nasty ESP32 ESP_Restart bug where ESP32 hangs if restarted when Rx is under heavy load)
    * Teleinfo EN may (or may not) be connected to any GPIO starting from GPIO5 ...

  Settings are stored using unused parameters :
    - Settings->rf_code[0..12][0..1] : Today's contract level daily slots
    - Settings->rf_code[0..12][2..3] : Tomorrow's contract level daily slots

  Version history :
    05/05/2019 - v1.0  - Creation
    16/05/2019 - v1.1  - Add Tempo and EJP contracts
    08/06/2019 - v1.2  - Handle active and apparent power
    05/07/2019 - v2.0  - Rework with selection thru web interface
    02/01/2020 - v3.0  - Functions rewrite for Tasmota 8.x compatibility
    05/02/2020 - v3.1  - Add support for 3 phases meters
    14/03/2020 - v3.2  - Add apparent power graph
    05/04/2020 - v3.3  - Add Timezone management
    13/05/2020 - v3.4  - Add overload management per phase
    19/05/2020 - v3.6  - Add configuration for first NTP server
    26/05/2020 - v3.7  - Add Information JSON page
    29/07/2020 - v3.8  - Add Meter section to JSON
    05/08/2020 - v4.0  - Major code rewrite, JSON section is now TIC, numbered like new official Teleinfo module
                         Web sensor display update
    18/09/2020 - v4.1  - Based on Tasmota 8.4
    07/10/2020 - v5.0  - Handle different graph periods and javascript auto update
    18/10/2020 - v5.1  - Expose icon on web server
    25/10/2020 - v5.2  - Real time graph page update
    30/10/2020 - v5.3  - Add TIC message page
    02/11/2020 - v5.4  - Tasmota 9.0 compatibility
    09/11/2020 - v6.0  - Handle ESP32 ethernet devices with board selection
    11/11/2020 - v6.1  - Add data.json page
    20/11/2020 - v6.2  - Checksum bug
    29/12/2020 - v6.3  - Strengthen message error control
    25/02/2021 - v7.0  - Prepare compatibility with TIC standard
                         Add power status bar
    05/03/2021 - v7.1  - Correct bug on hardware energy counter
    08/03/2021 - v7.2  - Handle voltage and checksum for horodatage
    12/03/2021 - v7.3  - Use average / overload for graph
    15/03/2021 - v7.4  - Change graph period parameter
    21/03/2021 - v7.5  - Support for TIC Standard
    29/03/2021 - v7.6  - Add voltage graph
    04/04/2021 - v7.7  - Change in serial port & graph height selection
                         Handle number of indexes according to contract
                         Remove use of String to avoid heap fragmentation 
    14/04/2021 - v7.8  - Calculate Cos phi and Active power (W)
    21/04/2021 - v8.0  - Fixed IP configuration and change in Cos phi calculation
    29/04/2021 - v8.1  - Bug fix in serial port management and realtime energy totals
                         Control initial baud rate to avoid crash (thanks to Seb)
    26/05/2021 - v8.2  - Add active power (W) graph
    22/06/2021 - v8.3  - Change in serial management for ESP32
    04/08/2021 - v9.0  - Tasmota 9.5 compatibility
                         Add LittleFS historic data record
                         Complete change in VA, W and cos phi measurement based on transmission time
                         Add PME/PMI ACE6000 management
                         Add energy update interval configuration
                         Add TIC to TCP bridge (command 'tcpstart 8888' to publish teleinfo stream on port 8888)
    04/09/2021 - v9.1  - Save settings in LittleFS partition if available
                         Log rotate and old files deletion if space low
    10/10/2021 - v9.2  - Add peak VA and V in history files
    02/11/2021 - v9.3  - Add period and totals in history files
                         Add simple FTP server to access history files
    13/03/2022 - v9.4  - Change keys to ISUB and PSUB in METER section
    20/03/2022 - v9.5  - Change serial init and major rework in active power calculation
    01/04/2022 - v9.6  - Add software watchdog feed to avoid lock
    22/04/2022 - v9.7  - Option to minimise LittleFS writes (day:every 1h and week:every 6h)
                         Correction of EAIT bug
    04/08/2022 - v9.8  - Based on Tasmota 12, add ESP32S2 support
                         Remove FTP server auto start
    18/08/2022 - v9.9  - Force GPIO_TELEINFO_RX as digital input
                         Correct bug littlefs config and graph data recording
                         Add Tempo and Production mode (thanks to Sébastien)
                         Correct publication synchronised with teleperiod
    26/10/2022 - v10.0 - Add bar graph monthly (every day) and yearly (every month)
    06/11/2022 - v10.1 - Bug fixes on bar graphs and change in lltoa conversion
    15/11/2022 - v10.2 - Add bar graph daily (every hour)
    04/02/2023 - v10.3 - Add graph swipe (horizontal and vertical)
                         Disable wifi sleep on ESP32 to avoid latency
    25/02/2023 - v11.0 - Split between xnrg and xsns
                         Use Settings->teleinfo to store configuration
                         Update today and yesterday totals
    03/06/2023 - v11.1 - Rewrite of Tasmota energy update
                         Avoid 100ms rules teleperiod update
    15/08/2023 - v11.3 - Change in cosphi calculation
                         Reorder Tempo periods
    25/08/2023 - v11.4 - Change in ESP32S2 partitionning (1.3M littleFS)
                         Add serial reception in display loops to avoid errors
    17/10/2023 - v12.1 - Handle Production & consommation simultaneously
                         Display all periods with total
    07/11/2023 - v12.2 - Handle RGB color according to total power
    19/11/2023 - v13.0 - Tasmota 13 compatibility
    03/01/2024 - v13.3 - Add alert management thru STGE
    13/01/2024 - v13.4 - Activate serial reception when NTP time is ready
    25/02/2024 - v13.5 - Lots of bug fixes (thanks to B. Monot and Sebastien)
    05/03/2024 - v13.6 - Removal of all float and double calculations

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY orENERGY_WATCHDOG FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FIRMWARE_SAFEBOOT

#ifdef USE_ENERGY_SENSOR
#ifdef USE_TELEINFO

/*********************************************************************************************\
 * Teleinfo
 * docs https://www.enedis.fr/sites/default/files/Enedis-NOI-CPT_54E.pdf
 * Teleinfo hardware will be enabled if 
 *     Hardware RX = [TinfoRX]
 *     Rx enable   = [TinfoEN] (optional)
\*********************************************************************************************/

#include <TasmotaSerial.h>
TasmotaSerial *teleinfo_serial = nullptr;

/*************************************************\
 *               Variables
\*************************************************/

// declare teleinfo energy driver and sensor
#define XNRG_15               15

/**************************************************\
 *                  Commands
\**************************************************/

bool TeleinfoHandleCommand ()
{
  bool   serviced = true;
  int    index;
  char   *pstr_next, *pstr_key, *pstr_value;
  char   str_label[32];
  char   str_item[64];
  String str_line;

  if (Energy->command_code == CMND_ENERGYCONFIG) 
  {
    // if no parameter, show configuration
    if (XdrvMailbox.data_len == 0) 
    {
      AddLog (LOG_LEVEL_INFO, PSTR ("EnergyConfig Teleinfo parameters :"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  historique   set historique mode at 1200 bauds (needs restart)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  standard     set Standard mode at 9600 bauds (needs restart)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  stats        display reception statistics"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  display=%u   display error counters on home page"), teleinfo_meter.display);
      AddLog (LOG_LEVEL_INFO, PSTR ("  percent=%u  maximum acceptable contract (%%)"), teleinfo_config.percent);

      // publishing policy
      str_line = "";
      for (index = 0; index < TELEINFO_POLICY_MAX; index++)
      {
        GetTextIndexed (str_label, sizeof (str_label), index, kTeleinfoMessagePolicy);
        sprintf (str_item, "%u=%s", index, str_label);
        if (str_line.length () > 0) str_line += ", ";
        str_line += str_item;
      }
      AddLog (LOG_LEVEL_INFO, PSTR ("  policy=%u     message policy : %s"), teleinfo_config.policy, str_line.c_str ());

      // publishing type
      AddLog (LOG_LEVEL_INFO, PSTR ("  meter=%u      publish METER & PROD data"), teleinfo_config.meter);
      AddLog (LOG_LEVEL_INFO, PSTR ("  tic=%u        publish TIC data"), teleinfo_config.tic);
      AddLog (LOG_LEVEL_INFO, PSTR ("  calendar=%u   publish CALENDAR data"), teleinfo_config.calendar);
      AddLog (LOG_LEVEL_INFO, PSTR ("  relay=%u      publish RELAY data"), teleinfo_config.relay);
      AddLog (LOG_LEVEL_INFO, PSTR ("  counter=%u    publish COUNTER data"), teleinfo_config.contract);

#ifdef USE_TELEINFO_GRAPH
      AddLog (LOG_LEVEL_INFO, PSTR ("  maxv=%u     graph max voltage (V)"), teleinfo_config.max_volt);
      AddLog (LOG_LEVEL_INFO, PSTR ("  maxva=%u  graph max power (VA or W)"), teleinfo_config.max_power);

      AddLog (LOG_LEVEL_INFO, PSTR ("  nbday=%d      number of daily logs"), teleinfo_config.param[TELEINFO_CONFIG_NBDAY]);
      AddLog (LOG_LEVEL_INFO, PSTR ("  nbweek=%d     number of weekly logs"), teleinfo_config.param[TELEINFO_CONFIG_NBWEEK]);

      AddLog (LOG_LEVEL_INFO, PSTR ("  maxhour=%d    graph max total per hour (Wh)"), teleinfo_config.param[TELEINFO_CONFIG_MAX_HOUR]);
      AddLog (LOG_LEVEL_INFO, PSTR ("  maxday=%d    graph max total per day (Wh)"), teleinfo_config.param[TELEINFO_CONFIG_MAX_DAY]);
      AddLog (LOG_LEVEL_INFO, PSTR ("  maxmonth=%d graph max total per month (Wh)"), teleinfo_config.param[TELEINFO_CONFIG_MAX_MONTH]);
#endif    // USE_TELEINFO_GRAPH

      serviced = true;
    }

    // else some configuration params are given
    else
    {
      // loop thru parameters
      pstr_next = XdrvMailbox.data;
      do
      {
        // extract key, value and next param
        pstr_key   = pstr_next;
        pstr_value = strchr (pstr_next, '=');
        pstr_next  = strchr (pstr_key, ' ');

        // split key, value and next param
        if (pstr_value != nullptr) { *pstr_value = 0; pstr_value++; }
        if (pstr_next != nullptr) { *pstr_next = 0; pstr_next++; }

        // if param is defined, handle it
        if (pstr_key != nullptr) serviced |= TeleinfoExecuteCommand (pstr_key, pstr_value);
      } 
      while (pstr_next != nullptr);

      // if needed, save confoguration
      if (serviced) TeleinfoSaveConfig ();

      // if restart will take place, log
      if (teleinfo_config.restart) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Please restart for new config to take effect"));
    }
  }

  return serviced;
}

bool TeleinfoExecuteCommand (const char* pstr_command, const char* pstr_param)
{
  bool serviced = false;
  int  index;
  long value;
  char str_buffer[32];

  // check parameter
  if (pstr_command == nullptr) return false;

  // check for command and value
  index = GetCommandCode (str_buffer, sizeof(str_buffer), pstr_command, kTeleinfoCommands);
  if (pstr_param == nullptr) value = 0; else value = atol (pstr_param);

  // handle command
  switch (index)
  {
    case TIC_CMND_HISTORIQUE:
      SetSerialBaudrate (1200);                         // 1200 bauds
      serviced = true;
      teleinfo_config.restart = true;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Set mode Historique (%d bauds)"), TasmotaGlobal.baudrate);
      break;

    case TIC_CMND_STANDARD:
      SetSerialBaudrate (9600);                         // 9600 bauds
      serviced = true;
      teleinfo_config.restart = true;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Set mode Standard (%d bauds)"), TasmotaGlobal.baudrate);
      break;

    case TIC_CMND_STATS:
      if (teleinfo_meter.nb_line > 0) value = (long)(teleinfo_meter.nb_error * 10000 / teleinfo_meter.nb_line);
        else value = 0;

      AddLog (LOG_LEVEL_INFO, PSTR (" - Messages   : %d"), teleinfo_meter.nb_message);
      AddLog (LOG_LEVEL_INFO, PSTR (" - Erreurs    : %d (%d.%02d%%)"), (long)teleinfo_meter.nb_error, value / 100, value % 100);
      AddLog (LOG_LEVEL_INFO, PSTR (" - Reset      : %d"), teleinfo_meter.nb_reset);
      AddLog (LOG_LEVEL_INFO, PSTR (" - Cosφ conso : %d"), teleinfo_conso.cosphi.nb_measure);
      AddLog (LOG_LEVEL_INFO, PSTR (" - Cosφ prod  : %d"), teleinfo_prod.cosphi.nb_measure);
      serviced = true;
      break;

    case TIC_CMND_DISPLAY:
      serviced = true;
      if (serviced) teleinfo_meter.display = (uint8_t)value;
      break;

    case TIC_CMND_PERCENT:
      serviced = (value < TELEINFO_PERCENT_MAX);
      if (serviced) teleinfo_config.percent = (uint8_t)value;
      break;

    case TIC_CMND_POLICY:
      serviced = (value < TELEINFO_POLICY_MAX);
      if (serviced) teleinfo_config.policy = (uint8_t)value;
      break;

    case TIC_CMND_METER:
      serviced = (value < 2);
      if (serviced) teleinfo_config.meter = (uint8_t)value;
      break;

    case TIC_CMND_TIC:
      serviced = (value < 2);
      if (serviced) teleinfo_config.tic = (uint8_t)value;
      break;

    case TIC_CMND_CALENDAR:
      serviced = (value < 2);
      if (serviced) teleinfo_config.calendar = (uint8_t)value;
      break;

    case TIC_CMND_RELAY:
      serviced = (value < 2);
      if (serviced) teleinfo_config.relay = (uint8_t)value;
      break;

    case TIC_CMND_CONTRACT:
      serviced = (value < 2);
      if (serviced) teleinfo_config.contract = (uint8_t)value;
      break;

#ifdef USE_TELEINFO_GRAPH
    case TIC_CMND_MAX_V:
      serviced = ((value >= TELEINFO_GRAPH_MIN_VOLTAGE) && (value <= TELEINFO_GRAPH_MAX_VOLTAGE));
      if (serviced) teleinfo_config.max_volt = value;
      break;

    case TIC_CMND_MAX_VA:
      serviced = ((value >= TELEINFO_GRAPH_MIN_POWER) && (value <= TELEINFO_GRAPH_MAX_POWER));
      if (serviced) teleinfo_config.max_power = value;
      break;

    case TIC_CMND_LOG_DAY:
      serviced = ((value > 0) && (value <= 31));
      if (serviced) teleinfo_config.param[TELEINFO_CONFIG_NBDAY] = value;
      break;

    case TIC_CMND_LOG_WEEK:
      serviced = ((value > 0) && (value <= 52));
      if (serviced) teleinfo_config.param[TELEINFO_CONFIG_NBWEEK] = value;
      break;

    case TIC_CMND_MAX_KWH_HOUR:
      serviced = ((value >= TELEINFO_GRAPH_MIN_WH_HOUR) && (value <= TELEINFO_GRAPH_MAX_WH_HOUR));
      if (serviced) teleinfo_config.param[TELEINFO_CONFIG_MAX_HOUR] = value;
      break;

    case TIC_CMND_MAX_KWH_DAY:
      serviced = ((value >= TELEINFO_GRAPH_MIN_WH_DAY) && (value <= TELEINFO_GRAPH_MAX_WH_DAY));
      if (serviced) teleinfo_config.param[TELEINFO_CONFIG_MAX_DAY] = value;
      break;

    case TIC_CMND_MAX_KWH_MONTH:
      serviced = ((value >= TELEINFO_GRAPH_MIN_WH_MONTH) && (value <= TELEINFO_GRAPH_MAX_WH_MONTH));
      if (serviced) teleinfo_config.param[TELEINFO_CONFIG_MAX_MONTH] = value;
      break;
#endif    // USE_TELEINFO_GRAPH
  }

  return serviced;
}

/*********************************************\
 *               Functions
\*********************************************/

#ifndef ESP32

// conversion from long long to string (not available in standard libraries)
char* lltoa (const long long value, char *pstr_result, const int base)
{
  lldiv_t result;
  char    str_value[12];

  // check parameters
  if (pstr_result == nullptr) return nullptr;
  if (base != 10) return nullptr;

  // if needed convert upper digits
  result = lldiv (value, 10000000000000000LL);
  if (result.quot != 0) ltoa ((long)result.quot, pstr_result, 10);
    else strcpy (pstr_result, "");

  // convert middle digits
  result = lldiv (result.rem, 100000000LL);
  if (result.quot != 0)
  {
    if (strlen (pstr_result) == 0) ltoa ((long)result.quot, str_value, 10);
      else sprintf_P (str_value, PSTR ("%08d"), (long)result.quot);
    strcat (pstr_result, str_value);
  }

  // convert lower digits
  if (strlen (pstr_result) == 0) ltoa ((long)result.rem, str_value, 10);
    else sprintf_P (str_value, PSTR ("%08d"), (long)result.rem);
  strcat (pstr_result, str_value);

  return pstr_result;
}

#endif      // ESP32

long long llsqrt (const long long value)
{
  long long result, tempo;

	// if 0 or 1, result done
  if (value < 0) return 0;
	  else if (value <= 1) return value;

  // Initial estimate (must be too high)
	result = value / 2;
  tempo = (result + value / result) / 2;

  // loop to calculate result
	while (tempo < result)	
	{
		result = tempo;
		tempo = (result + value / result) / 2;
	}

	return result;
}

// General update (wifi & serial)
void TeleinfoProcessRealTime ()
{
  // check if serial reception is active
  if (teleinfo_meter.serial != TIC_SERIAL_ACTIVE) return;

  // read serial data
  TeleinfoReceiveData (TELEINFO_RECV_TIMEOUT_DATA);

  // give control back to system to avoid watchdog
  yield ();
}

// Start serial reception
bool TeleinfoSerialStart ()
{
  bool is_ready;

  // if serial port is not already created
  is_ready = (teleinfo_serial != nullptr);
  if (!is_ready)
  { 
    // check if environment is ok
    is_ready = (PinUsed (GPIO_TELEINFO_RX) && (TasmotaGlobal.baudrate > 0));
    if (is_ready)
    {
#ifdef ESP32
      // create and initialise serial port
      teleinfo_serial = new TasmotaSerial (Pin (GPIO_TELEINFO_RX), -1, 1, 0);
      is_ready = teleinfo_serial->begin (TasmotaGlobal.baudrate, SERIAL_7E1);

      // display UART used
      AddLog (LOG_LEVEL_DEBUG, PSTR("TIC: Serial UART %d"), teleinfo_serial->getUart ());

#else       // ESP8266
      // create and initialise serial port
      teleinfo_serial = new TasmotaSerial (Pin (GPIO_TELEINFO_RX), -1, 1, 0);
      is_ready = teleinfo_serial->begin (TasmotaGlobal.baudrate, SERIAL_7E1);

      // force configuration on ESP8266
      if (teleinfo_serial->hardwareSerial ()) ClaimSerial ();
#endif      // ESP32 & ESP8266

      // flush transmit and receive buffer
//      teleinfo_serial->flush ();

      // log error
      if (!is_ready) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Serial port init failed"));
    }

    // set serial port status
    if (is_ready) teleinfo_meter.serial = TIC_SERIAL_ACTIVE;
      else teleinfo_meter.serial = TIC_SERIAL_FAILED;
  }

  return is_ready;
}

// Stop serial reception
void TeleinfoSerialStop ()
{
  // declare serial as stopped
  teleinfo_meter.serial = TIC_SERIAL_STOPPED;
}

// Check if serial reception is active
bool TeleinfoSerialIsActive ()
{
  return (teleinfo_meter.serial == TIC_SERIAL_ACTIVE);
}

// Load configuration from Settings or from LittleFS
void TeleinfoLoadConfig () 
{
  uint8_t index, day, slot, part;

  // load standard settings
  teleinfo_config.percent   = Settings->teleinfo.percent;
  teleinfo_config.policy    = Settings->teleinfo.policy;
  teleinfo_config.meter     = Settings->teleinfo.meter;
  teleinfo_config.tic       = Settings->teleinfo.tic;
  teleinfo_config.calendar  = Settings->teleinfo.calendar;
  teleinfo_config.relay     = Settings->teleinfo.relay;
  teleinfo_config.contract  = Settings->teleinfo.contract;
  teleinfo_config.max_volt  = TELEINFO_GRAPH_MIN_VOLTAGE + Settings->teleinfo.adjust_v * 5;
  teleinfo_config.max_power = TELEINFO_GRAPH_MIN_POWER + Settings->teleinfo.adjust_va * 3000;

  // validate boundaries
  if ((teleinfo_config.policy < 0) || (teleinfo_config.policy >= TELEINFO_POLICY_MAX)) teleinfo_config.policy = TELEINFO_POLICY_TELEMETRY;
  if (teleinfo_config.meter > 1) teleinfo_config.meter = 1;
  if (teleinfo_config.tic > 1) teleinfo_config.tic = 1;
  if (teleinfo_config.calendar > 1) teleinfo_config.calendar = 1;
  if (teleinfo_config.relay > 1) teleinfo_config.relay = 1;
  if (teleinfo_config.contract > 1) teleinfo_config.contract = 1;
  if ((teleinfo_config.percent < TELEINFO_PERCENT_MIN) || (teleinfo_config.percent > TELEINFO_PERCENT_MAX)) teleinfo_config.percent = 100;

  // load hourly status slots
  for (slot = 0; slot < 24; slot++)
  {
    index = slot % 12;
    part  = slot / 12;
    for (day = TIC_DAY_YESTERDAY; day < TIC_DAY_MAX; day++) teleinfo_meter.arr_day[day].arr_period[slot] = Settings->rf_code[index][2 * day + part];
  }

  // load littlefs settings
#ifdef USE_UFILESYS
  int    position;
  char   str_text[16];
  char   str_line[64];
  char   *pstr_key, *pstr_value;
  File   file;

  // if file exists, open and read each line
  if (ffsp->exists (D_TELEINFO_CFG))
  {
    file = ffsp->open (D_TELEINFO_CFG, "r");
    while (file.available ())
    {
      // read current line and extract key and value
      position = file.readBytesUntil ('\n', str_line, sizeof (str_line) - 1);
      if (position >= 0) str_line[position] = 0;
      pstr_key   = strtok (str_line, "=");
      pstr_value = strtok (nullptr,  "=");

      // if key and value are defined, look for config keys
      if ((pstr_key != nullptr) && (pstr_value != nullptr))
      {
        index = GetCommandCode (str_text, sizeof (str_text), pstr_key, kTeleinfoConfigKey);
        if ((index >= 0) && (index < TELEINFO_CONFIG_MAX)) teleinfo_config.param[index] = atol (pstr_value);
      }
    }
  }

# endif     // USE_UFILESYS

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Loading config"));
}

// Save configuration to Settings or to LittleFS
void TeleinfoSaveConfig () 
{
  uint8_t   index, day, slot, part;
  long long today_wh;

  // save standard settings
  Settings->teleinfo.percent   = teleinfo_config.percent;
  Settings->teleinfo.policy    = teleinfo_config.policy;
  Settings->teleinfo.meter     = teleinfo_config.meter;
  Settings->teleinfo.tic       = teleinfo_config.tic;
  Settings->teleinfo.calendar  = teleinfo_config.calendar;
  Settings->teleinfo.relay     = teleinfo_config.relay;
  Settings->teleinfo.contract  = teleinfo_config.contract;
  Settings->teleinfo.adjust_v  = (teleinfo_config.max_volt - TELEINFO_GRAPH_MIN_VOLTAGE) / 5;
  Settings->teleinfo.adjust_va = (teleinfo_config.max_power - TELEINFO_GRAPH_MIN_POWER) / 3000;

  // save hourly status slots
  for (slot = 0; slot < 24; slot++)
  {
    index = slot % 12;
    part  = slot / 12;
    for (day = TIC_DAY_YESTERDAY; day < TIC_DAY_MAX; day++) Settings->rf_code[index][2 * day + part] = teleinfo_meter.arr_day[day].arr_period[slot];
  }

  // save littlefs settings
#ifdef USE_UFILESYS
  char    str_value[16];
  char    str_text[32];
  File    file;

  // update today conso
  if ((teleinfo_conso.global_wh != 0) && (teleinfo_conso.midnight_wh != 0)) today_wh = teleinfo_conso.global_wh - teleinfo_conso.midnight_wh;
    else today_wh = 0;
  teleinfo_config.param[TELEINFO_CONFIG_TODAY_CONSO] = (long)today_wh;

  // update today production
  if ((teleinfo_prod.total_wh != 0) && (teleinfo_prod.midnight_wh != 0)) today_wh = teleinfo_prod.total_wh - teleinfo_prod.midnight_wh;
    else today_wh = 0;
  teleinfo_config.param[TELEINFO_CONFIG_TODAY_PROD] = (long)today_wh;

  // open file and write content
  file = ffsp->open (D_TELEINFO_CFG, "w");
  for (index = 0; index < TELEINFO_CONFIG_MAX; index ++)
  {
    if (GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoConfigKey) != nullptr)
    {
      // generate key=value
      strcat (str_text, "=");
      ltoa (teleinfo_config.param[index], str_value, 10);
      strcat (str_text, str_value);
      strcat (str_text, "\n");

      // write to file
      file.print (str_text);
    }
  }

  file.close ();
# endif     // USE_UFILESYS

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Saving config"));
}

// calculate line checksum
char TeleinfoCalculateChecksum (const char* pstr_line, char* pstr_etiquette, const size_t size_etiquette, char* pstr_donnee, const size_t size_donnee) 
{
  bool     prev_space;
  uint8_t  line_checksum, given_checksum;
  uint32_t index;
  size_t   size_line, size_checksum;
  char     str_line[TELEINFO_LINE_MAX];
  char     *pstr_token, *pstr_key, *pstr_data;

  // check parameters
  if ((pstr_line == nullptr) || (pstr_etiquette == nullptr) || (pstr_donnee == nullptr)) return 0;

  // init result
  strcpy (str_line, "");
  strcpy (pstr_etiquette, "");
  strcpy (pstr_donnee, "");
  pstr_key = pstr_data = nullptr;

  // if line is less than 5 char, no handling
  size_line = strlen (pstr_line);
  if ((size_line < 5) || (size_line > TELEINFO_LINE_MAX)) return 0;
  
  // get given checksum
  size_checksum  = size_line - 1;
  given_checksum = (uint8_t)pstr_line[size_checksum];

  // adjust checksum calculation according to mode
  if (teleinfo_meter.sep_line == ' ') size_checksum = size_checksum - 1;

  // loop to calculate checksum, keeping 6 lower bits and adding Ox20 and compare to given checksum
  line_checksum = 0;
  for (index = 0; index < size_checksum; index ++) line_checksum += (uint8_t)pstr_line[index];
  line_checksum = (line_checksum & 0x3F) + 0x20;

  // if different than given checksum,
  if (line_checksum != given_checksum)
  {
    // if network is up, account error
    teleinfo_meter.nb_error++;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Error [%s]"), pstr_line);

    // return error
    line_checksum = 0;
  }

  // replace all TABS and SPACE by single SPACE
  prev_space = true;
  for (index = 0; index < size_line; index ++)
  {
    // if current caracter is TAB or SPACE, keep only ONE
    if ((pstr_line[index] == 0x09) || (pstr_line[index] == ' '))
    {
      if (!prev_space) strcat (str_line, " ");
      prev_space = true;
    }

    // else keep current caracter
    else
    {
      strncat (str_line, &pstr_line[index], 1);
      prev_space = false;
    }
  }

  // remove checksum from string
  pstr_token = strrchr (str_line, ' ');
  if (pstr_token != nullptr) *pstr_token = 0;

  // look for data
  pstr_data = strchr (str_line, ' ');
  if (pstr_data == nullptr)
  {
    AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: No data [%s]"), str_line);
    line_checksum = 0;

    // extract key only
    strlcpy (pstr_etiquette, str_line, size_etiquette);
  }
  else
  {
    // extract key and data
    *pstr_data++ = 0;
    strlcpy (pstr_etiquette, str_line, size_etiquette);
    strlcpy (pstr_donnee, pstr_data, size_donnee);
  }

  return line_checksum;
}

// update global consommation counter :
//  - check that there is no decrease
//  - check that there is a 10% increase max
void TeleinfoUpdateConsoGlobalCounter ()
{
  bool      abnormal = false;
  uint8_t   index;
  long long total;

  // calculate new global counter
  total = 0;
  for (index = 0; index < teleinfo_contract.period_qty; index ++) total += teleinfo_conso.index_wh[index];

  // if total not defined, update total
  if (teleinfo_conso.global_wh == 0) teleinfo_conso.global_wh = total;

  // if needed, update midnight value
  if ((teleinfo_conso.midnight_wh == 0) && (teleinfo_conso.global_wh > 0)) teleinfo_conso.midnight_wh = teleinfo_conso.global_wh - (long long)teleinfo_config.param[TELEINFO_CONFIG_TODAY_CONSO];

  // if no increment, return
  if (total == teleinfo_conso.global_wh) return;

  // if total has decreased, total is abnormal
  if (total < teleinfo_conso.global_wh) abnormal = true;

  // else if total is high enougth and has increased more than 1%, total is abnormal
  else if ((total > 1000) && (total > teleinfo_conso.global_wh + (teleinfo_conso.global_wh / 100))) abnormal = true;

  // if value is abnormal
  if (abnormal)
  {
    teleinfo_conso.global_wh  = 0;
    teleinfo_conso.delta_mwh  = 0;
    teleinfo_conso.delta_mvah = 0;
  }

  // else update counters
  else
  {
    teleinfo_conso.delta_mwh += 1000 * (long)(total - teleinfo_conso.global_wh);
    teleinfo_conso.global_wh  = total;
  }
}

void TeleinfoProdGlobalCounterUpdate (const char* pstr_value)
{
  bool abnormal = false;
  long long total;

  // check parameter
  if (pstr_value == nullptr) return;

  // convert string and update counter
  total = atoll (pstr_value);

  // if total not defined, update total
  if (teleinfo_prod.total_wh == 0) teleinfo_prod.total_wh = total;

  // if needed, update midnight value
  if ((teleinfo_prod.midnight_wh == 0) && (teleinfo_prod.total_wh > 0)) teleinfo_prod.midnight_wh = teleinfo_prod.total_wh - (long long)teleinfo_config.param[TELEINFO_CONFIG_TODAY_PROD];

  // if no increment, return
  if (total == teleinfo_prod.total_wh) return;

  // if total has decreased, problem
  if (total < teleinfo_prod.total_wh) abnormal = true;

  // else if total has increased more than 1%, problem
  else if (total > teleinfo_prod.total_wh + (teleinfo_prod.total_wh / 100)) abnormal = true;

  // if value is abnormal
  if (abnormal)
  {
    teleinfo_prod.total_wh   = 0;
    teleinfo_prod.delta_mwh  = 0;
    teleinfo_prod.delta_mvah = 0;
  }

  // else update counters
  else
  {
    teleinfo_prod.delta_mwh += 1000 * (long)(total - teleinfo_prod.total_wh);
    teleinfo_prod.total_wh   = total;
  }
}

// update indexed consommation counter
//  - check that there is no decrease
//  - check that there is a 10% increase max
void TeleinfoConsoIndexCounterUpdate (const char* pstr_value, const uint8_t index)
{
  long long value;
  long long max_increase;
  char     *pstr_kilo;
  char      str_value[16];

  // check parameter
  if (pstr_value == nullptr) return;
  if (index >= TELEINFO_INDEX_MAX) return;

  // check if value is in kWh
  strlcpy (str_value, pstr_value, sizeof (str_value));
  pstr_kilo = strchr (str_value, 'k');
  if (pstr_kilo != nullptr)
  {
    *pstr_kilo = 0;
    value = 1000 * atoll (str_value);
  }
  else value = atoll (str_value);

  // check validity
  if (value == LONG_LONG_MAX) return;

  // calculate max increment
  max_increase = value / 10;

  // previous value was 0
  if (teleinfo_conso.index_wh[index] == 0) teleinfo_conso.index_wh[index] = value;

  // else if less than 10% increase
  else if ((value > teleinfo_conso.index_wh[index]) && (value < teleinfo_conso.index_wh[index] + max_increase)) teleinfo_conso.index_wh[index] = value;
}

// update phase voltage
void TeleinfoVoltageUpdate (const char* pstr_value, const uint8_t phase, const bool is_rms)
{
  long  value;
  char *pstr_unit;
  char  str_value[16];

  // check parameter
  if (pstr_value == nullptr) return;
  if (phase >= ENERGY_MAX_PHASES) return;

  // remove V unit
  strlcpy (str_value, pstr_value, sizeof (str_value));
  pstr_unit = strchr (str_value, 'V');
  if (pstr_unit != nullptr) *pstr_unit = 0;

  // correct emeraude meter bug
  if (strlen (str_value) == 5)
  {
    str_value[1] = str_value[2];
    str_value[2] = str_value[4];
    str_value[3] = 0;
  }

  // calculate and check validity
  value = atol (str_value);
  if (value > TELEINFO_VOLTAGE_MAXIMUM) return;

  // if value is valid
  if (value > 0)
  {
    // rms voltage
    if (is_rms)
    {
      teleinfo_conso.phase[phase].volt_set = true;
      teleinfo_conso.phase[phase].voltage  = value;
    }

    // average voltage
    else if (!teleinfo_conso.phase[phase].volt_set) teleinfo_conso.phase[phase].voltage = value;
  }
}

// update phase current ("67" or "67,243A")
void TeleinfoCurrentUpdate (const char* pstr_value, const uint8_t phase)
{
  long value;
  char *pstr_token;
  char  str_text[16];
  char  str_value[8];

  // check parameter
  if (pstr_value == nullptr) return;
  if (phase >= ENERGY_MAX_PHASES) return;
  
  // remove A unit
  strlcpy (str_text, pstr_value, sizeof (str_text));
  pstr_token = strchr (str_text, 'A');
  if (pstr_token != nullptr) *pstr_token = 0;

  // check for separator
  pstr_token = strchr (str_text, ',');

  // format 67,243
  if (pstr_token != nullptr)
  {
    *pstr_token = 0;
    strlcpy (str_value, str_text, sizeof (str_value));
    strlcat (str_value, pstr_token + 1, sizeof (str_value));
  }

  // format 67
  else
  {
    strlcpy (str_value, str_text, sizeof (str_value));
    strlcat (str_value, "000", sizeof (str_value));
  }
  
  // calculate and update
  value = atol (str_value);  
  if ((value >= 0) && (value < LONG_MAX)) teleinfo_conso.phase[phase].current = value; 
}

void TeleinfoProdApparentPowerUpdate (const char* pstr_value)
{
  long value;

  // check parameter
  if (pstr_value == nullptr) return;

  // calculate and update
  value = atol (pstr_value);
  if ((value >= 0) && (value < LONG_MAX)) teleinfo_prod.papp = value;
}

void TeleinfoConsoApparentPowerUpdate (const char* pstr_value)
{
  long value;

  // check parameter
  if (pstr_value == nullptr) return;

  // calculate and update
  value = atol (pstr_value);
  if ((value >= 0) && (value < LONG_MAX)) teleinfo_conso.papp = value;
}

// update phase apparent power
void TeleinfoConsoApparentPowerUpdate (const char* pstr_value, const uint8_t phase)
{
  long value;

  // check parameter
  if (pstr_value == nullptr) return;
  if (phase >= ENERGY_MAX_PHASES) return;
  
  // calculate and update
  value = atol (pstr_value);
  if ((value >= 0) && (value < LONG_MAX)) teleinfo_conso.phase[phase].sinsts = value; 
}

void TeleinfoSetPreavis (const uint8_t level, const char* pstr_label)
{
  // check parameters
  if (pstr_label == nullptr) return;
  if (!RtcTime.valid) return;

  // if reset is needed
  if (level == TIC_PREAVIS_NONE)
  {
    // send alert & log
    if (TeleinfoDriverMeterReady ()) teleinfo_meter.json.alert = 1;
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Preavis reset [%u->%u]"), teleinfo_meter.preavis.level, TIC_PREAVIS_NONE);

    // reset preavis
    teleinfo_meter.preavis.level   = TIC_PREAVIS_NONE;
    teleinfo_meter.preavis.timeout = UINT32_MAX;
    strcpy (teleinfo_meter.preavis.str_label, "");
  } 
  
  // else preavis is declared
  else
  {
    // update timeout
    teleinfo_meter.preavis.timeout = LocalTime () + TELEINFO_PREAVIS_TIMEOUT;

    // if level has increased, update data
    if (level > teleinfo_meter.preavis.level)
    {
      // set alert & log
      if (TeleinfoDriverMeterReady ()) teleinfo_meter.json.alert = 1;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Preavis updated to %s [%u->%u]"), pstr_label, teleinfo_meter.preavis.level, level);

      // update data
      teleinfo_meter.preavis.level = level;
      strcpy (teleinfo_meter.preavis.str_label, pstr_label);
    }
  }
}

void TeleinfoAnalyseSTGE (const char* pstr_donnee)
{
  uint8_t  signal, status;
  uint32_t calc;
  long     value;

  value = strtol (pstr_donnee, nullptr, 16);

  // get phase over voltage
  calc   = (uint32_t)value >> 6;
  signal = (uint8_t)calc & 0x01;
  if (TeleinfoDriverMeterReady () && (teleinfo_meter.flag.overvolt != signal)) teleinfo_meter.json.alert = 1;
  teleinfo_meter.flag.overvolt = signal;

  // get phase overload
  calc   = (uint32_t)value >> 7;
  signal = (uint8_t)calc & 0x01;
  if (TeleinfoDriverMeterReady () && (teleinfo_meter.flag.overload != signal)) teleinfo_meter.json.alert = 1;
  teleinfo_meter.flag.overload = signal;

  // get preavis pointe mobile signal
  calc   = (uint32_t)value >> 28;
  signal = (uint8_t)calc & 0x03;
  if (signal != 0) TeleinfoSetPreavis (TIC_PREAVIS_WARNING, "PM");

  // get active pointe mobile signal
  calc   = (uint32_t)value >> 30;
  signal = (uint8_t)calc & 0x03;
  if (signal != 0) TeleinfoSetPreavis (TIC_PREAVIS_ALERT, "PM");
}

/*********************************************\
 *           Contract and Period
\*********************************************/

// get delay between 2 timestamps
long TeleinfoTimestampCalculate (const char* pstr_donnee)
{
  bool    convert = true;
  uint8_t arr_index[3];
  int     size;
  long    result = LONG_MAX;
  char    str_text[4];

  // set conversion parameters according to string size
  size = strlen (pstr_donnee);
  if (size == 12)      { arr_index[0] = 6; arr_index[1] = 8;  arr_index[2] = 10; }      // string format is 010123181235 
  else if (size == 13) { arr_index[0] = 7; arr_index[1] = 9;  arr_index[2] = 11; }      // string format is H010123181235
  else if (size == 17) { arr_index[0] = 9; arr_index[1] = 12; arr_index[2] = 15; }      // string format is 01/01/23 18/12/35 
  else return result;

  // hours
  strlcpy (str_text, pstr_donnee + arr_index[0], 3);
  result = 3600 * atol (str_text);

  // minutes
  strlcpy (str_text, pstr_donnee + arr_index[1], 3);
  result += 60 * atol (str_text);

  // seconds
  strlcpy (str_text, pstr_donnee + arr_index[2], 3);
  result += atol (str_text);

  return result;
}

// get delay between 2 timestamps
long TeleinfoTimestampDelay (const long stamp_start, const long stamp_stop)
{
  long delay;

  // check parameters
  if (stamp_start == LONG_MAX) return 0;
  if (stamp_stop  == LONG_MAX) return 0;

  // calculate delay (in sec.)
  if (stamp_stop < stamp_start) delay = 86400 + stamp_stop - stamp_start;
    else delay = stamp_stop - stamp_start;

  return delay;
}

// set contract period
void TeleinfoContractUpdate (const char* pstr_contract)
{
  int  index, quantity;
  uint8_t contract = UINT8_MAX;
  char str_contract[32];
  char str_text[32];

  // check parameter
  if (pstr_contract == nullptr) return;

  // if contract already defined, ignore
  if (teleinfo_contract.contract != TIC_C_UNDEFINED) return;

  // handle specificity of Historique Tempo where last char is dynamic (BBRx)
  strlcpy (str_contract, pstr_contract, 4);
  if (strcmp (str_contract, "BBR") != 0) strlcpy (str_contract, pstr_contract, sizeof (str_contract));

  // look for contract
  index = GetCommandCode (str_text, sizeof (str_text), str_contract, kTicContractCode);

  // if contract is detected
  if (index > 0)
  {
    // set new period
    teleinfo_contract.contract = (uint8_t)index;

    // look for period according to contract
    quantity = 10;
    switch (teleinfo_contract.contract)
    {
      case TIC_C_UNDEFINED:   quantity = sizeof (arrPeriodUndefLevel); break;
      case TIC_C_HISTO_BASE:  quantity = sizeof (arrPeriodHistoBaseLevel); break;
      case TIC_C_HISTO_HCHP:  quantity = sizeof (arrPeriodHistoHcHpLevel); break;
      case TIC_C_HISTO_EJP:   quantity = sizeof (arrPeriodHistoEjpLevel); break;
      case TIC_C_HISTO_TEMPO: quantity = sizeof (arrPeriodHistoTempoLevel); break;
      case TIC_C_STD_BASE:    quantity = sizeof (arrPeriodStandardBaseLevel); break;
      case TIC_C_STD_HCHP:    quantity = sizeof (arrPeriodStandardHcHpLevel); break;
      case TIC_C_STD_EJP:     quantity = sizeof (arrPeriodStandardEjpLevel); break;
      case TIC_C_STD_TEMPO:   quantity = sizeof (arrPeriodStandardTempoLevel); break;
      case TIC_C_BT4SUP36:
      case TIC_C_BT5SUP36:
      case TIC_C_TJEJP:
      case TIC_C_HTA5:  
      case TIC_C_HTA8:        quantity = sizeof (arrPeriodPmePmiLevel); break;
      case TIC_C_A5_BASE:
      case TIC_C_A8_BASE:
      case TIC_C_A5_EJP:
      case TIC_C_A8_EJP:
      case TIC_C_A8_MOD:      quantity = sizeof (arrPeriodEmeraude2Level); break;
      
    }
    teleinfo_contract.period_qty = (uint8_t)quantity;

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Contract %s with %d periods [%u]"), pstr_contract, quantity, index);
  }
}

// set contract period
void TeleinfoPeriodUpdate (const char* pstr_period)
{
  int  index = INT_MAX;
  char str_text[32];

  // check parameter
  if (pstr_period == nullptr) return;
  if (teleinfo_contract.contract == TIC_C_UNDEFINED) return;

  // look for period according to contract
  switch (teleinfo_contract.contract)
  {
    case TIC_C_HISTO_BASE:  index = GetCommandCode (str_text, sizeof (str_text), pstr_period, kTicPeriodHistoBaseCode); break;
    case TIC_C_HISTO_HCHP:  index = GetCommandCode (str_text, sizeof (str_text), pstr_period, kTicPeriodHistoHcHpCode); break;
    case TIC_C_HISTO_EJP:   index = GetCommandCode (str_text, sizeof (str_text), pstr_period, kTicPeriodHistoEjpCode); break;
    case TIC_C_HISTO_TEMPO: index = GetCommandCode (str_text, sizeof (str_text), pstr_period, kTicPeriodHistoTempoCode); break;
    case TIC_C_STD_BASE:    index = GetCommandCode (str_text, sizeof (str_text), pstr_period, kTicPeriodStandardBaseCode); break;
    case TIC_C_STD_HCHP:    index = GetCommandCode (str_text, sizeof (str_text), pstr_period, kTicPeriodStandardHcHpCode); break;
    case TIC_C_STD_EJP:     index = GetCommandCode (str_text, sizeof (str_text), pstr_period, kTicPeriodStandardEjpCode); break;
    case TIC_C_STD_TEMPO:   index = GetCommandCode (str_text, sizeof (str_text), pstr_period, kTicPeriodStandardTempoCode); break;
    case TIC_C_BT4SUP36:
    case TIC_C_BT5SUP36:
    case TIC_C_TJEJP:
    case TIC_C_HTA5:  
    case TIC_C_HTA8:        index = GetCommandCode (str_text, sizeof (str_text), pstr_period, kTicPeriodPmePmiCode); break;
    case TIC_C_A5_BASE:
    case TIC_C_A8_BASE:
    case TIC_C_A5_EJP:
    case TIC_C_A8_EJP:
    case TIC_C_A8_MOD:      index = GetCommandCode (str_text, sizeof (str_text), pstr_period, kTicPeriodEmeraude2Code); break;    
  }

  // if index not found, log
  if (index == -1) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Period unknown [%s]"), pstr_period);
  
  // else if new period changed
  else if ((index != INT_MAX) && (teleinfo_contract.period != (uint8_t)index))    
  {
    // if period update, publish calendar
    if (teleinfo_config.calendar && (teleinfo_contract.period != UINT8_MAX)) teleinfo_meter.json.calendar = 1;

    // set new period and log
    teleinfo_contract.period = (uint8_t)index;
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Period updated to %s [%d]"), pstr_period, index);
  }
}

// set contract max power par phase
void TeleinfoContractPowerUpdate (const char *pstr_donnee, const bool is_kilo)
{
  long value;
  char *pstr_match;
  char str_value[32];

  // check parameters
  if (pstr_donnee == nullptr) return;
  if (teleinfo_contract.phase == 0) return;
  
  // convert value
  strlcpy (str_value, pstr_donnee, sizeof (str_value));
  pstr_match = strchr (str_value, 'k');
  if (pstr_match != nullptr) *pstr_match = 0; 
  value = atol (str_value);

  // update contrat max values
  if ((value > 0) && (value < LONG_MAX))
  {
    // adjust value in kilo and per phase
    if (is_kilo) value *= 1000;
    if (teleinfo_contract.phase > 1) value = value / teleinfo_contract.phase;

    // update boundaries
    teleinfo_contract.ssousc = value;                
    teleinfo_contract.isousc = value / TELEINFO_VOLTAGE_REF;
  }
}

// get contract name
void TeleinfoContractGetName (char* pstr_name, size_t size_name)
{
  // check parameter
  if (pstr_name == nullptr) return;
  if (size_name == 0) return;

  // get contract name
  GetTextIndexed (pstr_name, size_name, teleinfo_contract.contract, kTicContractName);
}

// get period code
void TeleinfoPeriodGetCode (char* pstr_code, size_t size_code) { TeleinfoPeriodGetCode (teleinfo_contract.period, pstr_code, size_code); }
void TeleinfoPeriodGetCode (const uint8_t period, char* pstr_code, size_t size_code)
{
  // check parameter
  if (pstr_code == nullptr) return;
  if (size_code == 0) return;

  // look for period according to contract
  strlcpy (pstr_code, "NONE", size_code);
  switch (teleinfo_contract.contract)
  {
    case TIC_C_UNDEFINED:   GetTextIndexed (pstr_code, size_code, period, kTicPeriodUndefCode); break;
    case TIC_C_HISTO_BASE:  GetTextIndexed (pstr_code, size_code, period, kTicPeriodHistoBaseCode); break;
    case TIC_C_HISTO_HCHP:  GetTextIndexed (pstr_code, size_code, period, kTicPeriodHistoHcHpCode); break;
    case TIC_C_HISTO_EJP:   GetTextIndexed (pstr_code, size_code, period, kTicPeriodHistoEjpCode); break;
    case TIC_C_HISTO_TEMPO: GetTextIndexed (pstr_code, size_code, period, kTicPeriodHistoTempoCode); break;
    case TIC_C_STD_BASE:    GetTextIndexed (pstr_code, size_code, period, kTicPeriodStandardBaseCode); break;
    case TIC_C_STD_HCHP:    GetTextIndexed (pstr_code, size_code, period, kTicPeriodStandardHcHpCode); break;
    case TIC_C_STD_EJP:     GetTextIndexed (pstr_code, size_code, period, kTicPeriodStandardEjpCode); break;
    case TIC_C_STD_TEMPO:   GetTextIndexed (pstr_code, size_code, period, kTicPeriodStandardTempoCode); break;
    case TIC_C_BT4SUP36:
    case TIC_C_BT5SUP36:
    case TIC_C_TJEJP:
    case TIC_C_HTA5:  
    case TIC_C_HTA8:        GetTextIndexed (pstr_code, size_code, period, kTicPeriodPmePmiCode); break;
    case TIC_C_A5_BASE:
    case TIC_C_A8_BASE:
    case TIC_C_A5_EJP:
    case TIC_C_A8_EJP:
    case TIC_C_A8_MOD:      GetTextIndexed (pstr_code, size_code, period, kTicPeriodEmeraude2Code); break;    
  } 
}

// get period name
void TeleinfoPeriodGetName (char* pstr_name, size_t size_name) { TeleinfoPeriodGetName (teleinfo_contract.period, pstr_name, size_name); }
void TeleinfoPeriodGetName (const uint8_t period, char* pstr_name, size_t size_name)
{
  // check parameter
  if (pstr_name == nullptr) return;
  if (size_name == 0) return;

  // look for period according to contract
  strlcpy (pstr_name, "Indéterminée", size_name);
  switch (teleinfo_contract.contract)
  {
    case TIC_C_UNDEFINED:   GetTextIndexed (pstr_name, size_name, period, kTicPeriodUndefName); break;
    case TIC_C_HISTO_BASE:  GetTextIndexed (pstr_name, size_name, period, kTicPeriodHistoBaseName); break;
    case TIC_C_HISTO_HCHP:  GetTextIndexed (pstr_name, size_name, period, kTicPeriodHistoHcHpName); break;
    case TIC_C_HISTO_EJP:   GetTextIndexed (pstr_name, size_name, period, kTicPeriodHistoEjpName); break;
    case TIC_C_HISTO_TEMPO: GetTextIndexed (pstr_name, size_name, period, kTicPeriodHistoTempoName); break;
    case TIC_C_STD_BASE:    GetTextIndexed (pstr_name, size_name, period, kTicPeriodStandardBaseName); break;
    case TIC_C_STD_HCHP:    GetTextIndexed (pstr_name, size_name, period, kTicPeriodStandardHcHpName); break;
    case TIC_C_STD_EJP:     GetTextIndexed (pstr_name, size_name, period, kTicPeriodStandardEjpName); break;
    case TIC_C_STD_TEMPO:   GetTextIndexed (pstr_name, size_name, period, kTicPeriodStandardTempoName); break;
    case TIC_C_BT4SUP36:
    case TIC_C_BT5SUP36:
    case TIC_C_TJEJP:
    case TIC_C_HTA5:  
    case TIC_C_HTA8:        GetTextIndexed (pstr_name, size_name, period, kTicPeriodPmePmiName); break;
    case TIC_C_A5_BASE:
    case TIC_C_A8_BASE:
    case TIC_C_A5_EJP:
    case TIC_C_A8_EJP:
    case TIC_C_A8_MOD:      GetTextIndexed (pstr_name, size_name, period, kTicPeriodEmeraude2Name); break;    
  }
}

// get period level
uint8_t TeleinfoPeriodGetLevel () { return TeleinfoPeriodGetLevel (teleinfo_contract.period); }
uint8_t TeleinfoPeriodGetLevel (const uint8_t period)
{
  uint8_t level;

  // look for period according to contract
  level = TIC_LEVEL_NONE;
  switch (teleinfo_contract.contract)
  {
    case TIC_C_UNDEFINED:   if (period < sizeof (arrPeriodUndefLevel)) level = arrPeriodUndefLevel[period]; break;
    case TIC_C_HISTO_BASE:  if (period < sizeof (arrPeriodHistoBaseLevel)) level = arrPeriodHistoBaseLevel[period]; break;
    case TIC_C_HISTO_HCHP:  if (period < sizeof (arrPeriodHistoHcHpLevel)) level = arrPeriodHistoHcHpLevel[period]; break;
    case TIC_C_HISTO_EJP:   if (period < sizeof (arrPeriodHistoEjpLevel)) level = arrPeriodHistoEjpLevel[period]; break;
    case TIC_C_HISTO_TEMPO: if (period < sizeof (arrPeriodHistoTempoLevel)) level = arrPeriodHistoTempoLevel[period]; break;
    case TIC_C_STD_BASE:    if (period < sizeof (arrPeriodStandardBaseLevel)) level = arrPeriodStandardBaseLevel[period]; break;
    case TIC_C_STD_HCHP:    if (period < sizeof (arrPeriodStandardHcHpLevel)) level = arrPeriodStandardHcHpLevel[period]; break;
    case TIC_C_STD_EJP:     if (period < sizeof (arrPeriodStandardEjpLevel)) level = arrPeriodStandardEjpLevel[period]; break;
    case TIC_C_STD_TEMPO:   if (period < sizeof (arrPeriodStandardTempoLevel)) level = arrPeriodStandardTempoLevel[period]; break;
    case TIC_C_BT4SUP36:
    case TIC_C_BT5SUP36:
    case TIC_C_TJEJP:
    case TIC_C_HTA5:  
    case TIC_C_HTA8:        if (period < sizeof (arrPeriodPmePmiLevel)) level = arrPeriodPmePmiLevel[period]; break;
    case TIC_C_A5_BASE:
    case TIC_C_A8_BASE:
    case TIC_C_A5_EJP:
    case TIC_C_A8_EJP:
    case TIC_C_A8_MOD:      if (period < sizeof (arrPeriodEmeraude2Level)) level = arrPeriodEmeraude2Level[period]; break;   
  }

  return level;
}

// get period HP status
uint8_t TeleinfoPeriodGetHP () { return TeleinfoPeriodGetHP (teleinfo_contract.period); }
uint8_t TeleinfoPeriodGetHP (const uint8_t period)
{
  uint8_t status;

  // look for period according to contract
  status = 1;
  switch (teleinfo_contract.contract)
  {
    case TIC_C_UNDEFINED:   if (period < sizeof (arrPeriodUndefHP)) status = arrPeriodUndefHP[period]; break;
    case TIC_C_HISTO_BASE:  if (period < sizeof (arrPeriodHistoBaseHP)) status = arrPeriodHistoBaseHP[period]; break;
    case TIC_C_HISTO_HCHP:  if (period < sizeof (arrPeriodHistoHcHpHP)) status = arrPeriodHistoHcHpHP[period]; break;
    case TIC_C_HISTO_EJP:   if (period < sizeof (arrPeriodHistoEjpHP)) status = arrPeriodHistoEjpHP[period]; break;
    case TIC_C_HISTO_TEMPO: if (period < sizeof (arrPeriodHistoTempoHP)) status = arrPeriodHistoTempoHP[period]; break;
    case TIC_C_STD_BASE:    if (period < sizeof (arrPeriodStandardBaseHP)) status = arrPeriodStandardBaseHP[period]; break;
    case TIC_C_STD_HCHP:    if (period < sizeof (arrPeriodStandardHcHpHP)) status = arrPeriodStandardHcHpHP[period]; break;
    case TIC_C_STD_EJP:     if (period < sizeof (arrPeriodStandardEjpHP)) status = arrPeriodStandardEjpHP[period]; break;
    case TIC_C_STD_TEMPO:   if (period < sizeof (arrPeriodStandardTempoHP)) status = arrPeriodStandardTempoHP[period]; break;
    case TIC_C_BT4SUP36:
    case TIC_C_BT5SUP36:
    case TIC_C_TJEJP:
    case TIC_C_HTA5:  
    case TIC_C_HTA8:        if (period < sizeof (arrPeriodPmePmiHP)) status = arrPeriodPmePmiHP[period]; break;
    case TIC_C_A5_BASE:
    case TIC_C_A8_BASE:
    case TIC_C_A5_EJP:
    case TIC_C_A8_EJP:
    case TIC_C_A8_MOD:      if (period < sizeof (arrPeriodEmeraude2HP)) status = arrPeriodEmeraude2HP[period]; break;   
  }

  return status;
}

/*********************************************\
 *         Calendar and Pointe period
\*********************************************/

// calculate CRC on 64 first char
uint16_t TeleinfoCalculateCRC (const char *pstr_donnee)
{
  int      index, length;
  uint16_t result = 0;

  // calculate CRC
  length = min (64, (int)strlen (pstr_donnee));
  for (index = 0; index < length; index ++) result += (uint8_t)pstr_donnee[index];

  return result;
}

void TeleinfoCalendarTomorrowProfile (char *pstr_donnee)
{
  bool    is_valid;
  uint8_t slot, last, period, value;
  char*   pstr_token;
  char    str_value[4];

  // check parameters
  if (pstr_donnee == nullptr) return;
  if (teleinfo_meter.arr_day[TIC_DAY_TOMORROW].valid) return;

  // loop thru periods
  slot = last = 0;
  period = UINT8_MAX;
  pstr_token = strtok (pstr_donnee, " ");
  while ((last < 24) && (pstr_token != nullptr))
  {
    // check profile validity
    is_valid = ((strlen (pstr_token) == 8) && isdigit (pstr_token[0]));
    if (is_valid)
    { 
      strlcpy (str_value, pstr_token, 3);
      last = (uint8_t)min (24L, strtol (str_value, nullptr, 16));
    }
    else last = 24;

    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Default %s, window %u-%u [%u]"), pstr_token, slot, last, period);

    // loop to update hourly slots
    while (slot < last)
    {
      if (teleinfo_meter.arr_day[TIC_DAY_TODAY].arr_period[slot] == TIC_LEVEL_NONE) teleinfo_meter.arr_day[TIC_DAY_TODAY].arr_period[slot] = period;
      if (teleinfo_meter.arr_day[TIC_DAY_TOMORROW].arr_period[slot] == TIC_LEVEL_NONE) teleinfo_meter.arr_day[TIC_DAY_TOMORROW].arr_period[slot] = period;
      slot++;
    }

    // if profile is defined, calculate period for next loop
    if (is_valid)
    {
      strlcpy (str_value, pstr_token + 6, 3);
      value = (uint8_t)strtol (str_value, nullptr, 16);
      if (value > 0) period = value - 1;
        else period = 0;
    }

    // update to next token
    pstr_token = strtok (nullptr, " ");
  }

  // set today and tommorow as valid
  teleinfo_meter.arr_day[TIC_DAY_TODAY].valid = 1;
  teleinfo_meter.arr_day[TIC_DAY_TOMORROW].valid = 1;

  // ask for JSON update
  if (teleinfo_config.calendar && TeleinfoDriverMeterReady ()) teleinfo_meter.json.calendar = 1;
}

void TeleinfoCalendarPointeDeclare (const char *pstr_donnee)
{
  uint16_t crc;
  uint32_t start;
  uint32_t day = UINT32_MAX;
  TIME_T   dst_target;
  char     str_value[4];

  // check parameters
  if (pstr_donnee == nullptr) return;

  // check CRC
  crc = TeleinfoCalculateCRC (pstr_donnee);
  if (teleinfo_meter.pointe.crc_start == crc) return;

  // update CRC
  teleinfo_meter.pointe.crc_start   = crc;
  teleinfo_meter.pointe.crc_profile = 0;

  // calculate year, month, day and hour
  strlcpy (str_value, pstr_donnee, 3);
  dst_target.year = 30 + (uint16_t)atoi (str_value);
  strlcpy (str_value, pstr_donnee + 2, 3);
  dst_target.month = (uint8_t)atoi (str_value);
  strlcpy (str_value, pstr_donnee + 4, 3);
  dst_target.day_of_month = (uint8_t)atoi (str_value);
  strlcpy (str_value, pstr_donnee + 6, 3);
  dst_target.hour = (uint8_t)atoi (str_value);
  strlcpy (str_value, pstr_donnee + 8, 3);
  dst_target.minute = 0;
  dst_target.second = 0;

  // calculate pointe timing
  start = MakeTime (dst_target);
  BreakTime(start, dst_target);

  // check if pointe is within 2 days
  if (dst_target.days >= RtcTime.days) day = dst_target.days - RtcTime.days;
  if (day < 2)
  {
    // update data (today is 1 and tomorrow is 2)
    teleinfo_meter.pointe.day  = (uint8_t)day + 1;
    teleinfo_meter.pointe.hour = dst_target.hour;
    AddLog (LOG_LEVEL_INFO, PSTR("TIC: Pointe updated, day %u, hour %02u [%s]"), teleinfo_meter.pointe.day, teleinfo_meter.pointe.hour, pstr_donnee);
  }
  else
  {
    teleinfo_meter.pointe.day  = UINT8_MAX;
    teleinfo_meter.pointe.hour = UINT8_MAX;
    AddLog (LOG_LEVEL_INFO, PSTR("TIC: Pointe ignored, day %u [%s]"), teleinfo_meter.pointe.day, pstr_donnee);
  }
}

void TeleinfoCalendarPointeProfile (char *pstr_donnee)
{
  bool     is_valid;
  uint8_t  day, slot, last, period, value;
  uint16_t crc;
  char*    pstr_token;
  char     str_value[4];

  // check parameters
  if (pstr_donnee == nullptr) return;
  if (teleinfo_meter.pointe.day == UINT8_MAX) return;

  // check CRC
  crc = TeleinfoCalculateCRC (pstr_donnee);
  if (teleinfo_meter.pointe.crc_profile == crc) return;

  // update CRC
  teleinfo_meter.pointe.crc_profile = crc;

  // loop thru periods
  slot = last = 0;
  period = UINT8_MAX;
  pstr_token = strtok (pstr_donnee, " ");
  while ((last < 24) && (pstr_token != nullptr))
  {
    // check profile validity
    is_valid = ((strlen (pstr_token) == 8) && isdigit (pstr_token[0]));

    // get current hour as last one to display
    if (is_valid)
    { 
      strlcpy (str_value, pstr_token, 3);
      last = (uint8_t)min (24L, strtol (str_value, nullptr, 16));
    }
    else last = 24;

    // log
    AddLog (LOG_LEVEL_DEBUG, PSTR("TIC: Pointe - %s, day %u, window %u-%u [%u]"), pstr_token, teleinfo_meter.pointe.day, slot, last, period);

    // loop to update hourly slots
    while (slot < last)
    {
      // check day where it applies
      day = teleinfo_meter.pointe.day;
      if (slot < teleinfo_meter.pointe.hour) day++;
      if (day < TIC_DAY_MAX) teleinfo_meter.arr_day[day].arr_period[slot] = period;
      slot++;
    }

    // if profile is defined, calculate hour and period
    if (is_valid)
    {
      strlcpy (str_value, pstr_token + 6, 3);
      value = (uint8_t)strtol (str_value, nullptr, 16);
      if (value > 0) period = value - 1;
        else period = 0;
    }

    // update to next token
    pstr_token = strtok (nullptr, " ");
  }

  // ask for JSON update
  if (teleinfo_config.calendar && TeleinfoDriverMeterReady ()) teleinfo_meter.json.calendar = 1;
}

void TeleinfoUpdateCosphi (long cosphi, struct tic_cosphi &struct_cosphi, const long papp)
{
  uint8_t index;
  long    delta, sample, result;
  long    contract5percent, contract2percent;

  // ignore first measure as duration was not good
  struct_cosphi.nb_measure++;
  if (struct_cosphi.nb_measure == 1) return;

  // if contract not defined, ognore
  if (teleinfo_contract.ssousc == 0) return;

  // calculate 2% and 5% limits according to contract max power
  contract2percent = min (500L,  teleinfo_contract.ssousc * teleinfo_contract.phase / 50);
  contract5percent = min (1500L, teleinfo_contract.ssousc * teleinfo_contract.phase / 20);

  // calculate delta since last value
  delta = papp - struct_cosphi.last_papp;
  struct_cosphi.last_papp = papp;

  // if apparent power increased more than 500 VA, average on 2 samples to update cosphi very fast
  if (delta >= contract5percent) for (index = 1; index < TELEINFO_COSPHI_SAMPLE; index++) struct_cosphi.arr_value[index] = LONG_MAX;

  // else if apparent power increased more than 200 VA, average on 4 samples to update cosphi quite fast
  else if (delta >= contract2percent) for (index = 3; index < TELEINFO_COSPHI_SAMPLE; index++) struct_cosphi.arr_value[index] = LONG_MAX;

  // else if apparent power decreased more than 200 VA, average on 2 samples to update cosphi very fast
  else if (delta <= -contract5percent) for (index = 1; index < TELEINFO_COSPHI_SAMPLE; index++) struct_cosphi.arr_value[index] = LONG_MAX;

  // if apparent power increased, cancel cosphi calculation to avoid spikes
  if (delta >= contract5percent) return;
//  if (delta >= 200) cosphi = struct_cosphi.arr_value[0];

  // shift values and update first one
  for (index = TELEINFO_COSPHI_SAMPLE - 1; index > 0; index--) struct_cosphi.arr_value[index] = struct_cosphi.arr_value[index - 1];
  struct_cosphi.arr_value[0] = cosphi;

  // calculate cosphi average
  result = sample = 0;
  for (index = 0; index < TELEINFO_COSPHI_SAMPLE; index++)
    if (struct_cosphi.arr_value[index] != LONG_MAX) { result += struct_cosphi.arr_value[index]; sample++; }

  // if at least one sample, update resulting value, avoiding out of range values
  if (sample > 0) struct_cosphi.value = min (1000L, result / sample);
}

/*********************************************\
 *                   Callback
\*********************************************/

// Teleinfo GPIO initilisation
void TeleinfoPreInit ()
{
  // declare energy driver
  TasmotaGlobal.energy_driver = XNRG_15;

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR("TIC: Teleinfo driver enabled"));
}

// Teleinfo driver initialisation
void TeleinfoInit ()
{
  uint8_t index, slot, phase;

#ifdef USE_UFILESYS
  // log result
  if (ufs_type) AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Partition mounted"));
  else AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Partition could not be mounted"));
#endif  // USE_UFILESYS

  // remove serial logging
  TasmotaGlobal.seriallog_timer = 0;

  // set measure resolution
  Settings->flag2.current_resolution = 1;

  // disable fast cycle power recovery (SetOption65)
  Settings->flag3.fast_power_cycle_disable = true;

  // init hardware energy counter
  Settings->flag3.hardware_energy_total = true;

  // set default energy parameters
  Energy->voltage_available = true;
  Energy->current_available = true;

  // load configuration
  TeleinfoLoadConfig ();

  // initialise meter indexes
  for (index = 0; index < TELEINFO_INDEX_MAX; index ++) teleinfo_conso.index_wh[index] = 0;
  teleinfo_meter.sep_line = ' ';

  // init JSON flags
  teleinfo_meter.json.alert    = 0;
  teleinfo_meter.json.meter    = 0;
  teleinfo_meter.json.contract = 0;
  teleinfo_meter.json.tic      = 0;
  teleinfo_meter.json.calendar = 0;
  teleinfo_meter.json.relay    = 0;

  // init meter stge data
  teleinfo_meter.flag.overload   = 0;
  teleinfo_meter.flag.overvolt   = 0;
  teleinfo_meter.preavis.level   = TIC_PREAVIS_NONE;
  teleinfo_meter.preavis.timeout = UINT32_MAX;
  strcpy (teleinfo_meter.preavis.str_label, "");

  // init pointe period
  teleinfo_meter.pointe.hour        = UINT8_MAX;
  teleinfo_meter.pointe.day         = UINT8_MAX;
  teleinfo_meter.pointe.start       = UINT32_MAX;
  teleinfo_meter.pointe.crc_start   = 0;
  teleinfo_meter.pointe.crc_profile = 0;

  // init calendar days
  for (index = TIC_DAY_YESTERDAY; index < TIC_DAY_MAX; index ++)
  {
    teleinfo_meter.arr_day[index].valid = 0;
    for (slot = 0; slot < 24; slot ++) teleinfo_meter.arr_day[index].arr_period[slot] = TIC_LEVEL_NONE;
  }

  // initialise message data
  teleinfo_message.timestamp = UINT32_MAX;
  strcpy (teleinfo_message.str_line, "");
  for (index = 0; index < TELEINFO_LINE_QTY; index ++) 
  {
    // init current message data
    teleinfo_message.arr_line[index].str_etiquette = "";
    teleinfo_message.arr_line[index].str_donnee    = "";
    teleinfo_message.arr_line[index].checksum      = 0;

    // init last message data
    teleinfo_message.arr_last[index].str_etiquette = "";
    teleinfo_message.arr_last[index].str_donnee    = "";
    teleinfo_message.arr_last[index].checksum      = 0;
  }

  // init cosphi data
  teleinfo_conso.cosphi.value      = 1000;
  teleinfo_prod.cosphi.value       = 1000;
  teleinfo_conso.cosphi.nb_measure = 0;
  teleinfo_prod.cosphi.nb_measure  = 0;
  teleinfo_conso.cosphi.last_papp  = 0;
  teleinfo_prod.cosphi.last_papp   = 0;
  for (index = 0; index < TELEINFO_COSPHI_SAMPLE; index++)
  {
    teleinfo_conso.cosphi.arr_value[index] = LONG_MAX;
    teleinfo_prod.cosphi.arr_value[index]  = LONG_MAX;
  }

  // loop thru phases
  for (phase = 0; phase < ENERGY_MAX_PHASES; phase++)
  {
    // init energy data 
    Energy->voltage[phase]        = TELEINFO_VOLTAGE;
    Energy->current[phase]        = 0;
    Energy->active_power[phase]   = 0;
    Energy->apparent_power[phase] = 0;

    // init conso data
    teleinfo_conso.phase[phase].volt_set  = false;
    teleinfo_conso.phase[phase].voltage   = TELEINFO_VOLTAGE;
    teleinfo_conso.phase[phase].current   = 0;
    teleinfo_conso.phase[phase].papp      = 0;
    teleinfo_conso.phase[phase].sinsts    = 0;
    teleinfo_conso.phase[phase].pact      = 0;
    teleinfo_conso.phase[phase].preact    = 0;
    teleinfo_conso.phase[phase].papp_last = 0;
    teleinfo_conso.phase[phase].cosphi    = 1000;
  }

  // log default method & help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: EnergyConfig to get help on all Teleinfo commands"));
}

// set environment for new message
void TeleinfoReceptionMessageReset ()
{
  // set next stage
  teleinfo_meter.reception = TIC_RECEPTION_NONE;

  // log and increment reset counter
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Message reset"));
  teleinfo_meter.nb_reset++;
}

// set environment for new message
void TeleinfoReceptionMessageStart ()
{
  uint8_t phase;

  // set next stage
  teleinfo_meter.reception = TIC_RECEPTION_MESSAGE;

  // reset line index and current line content
  teleinfo_message.line_index = 0;
  memset (teleinfo_message.str_line, 0, TELEINFO_LINE_MAX);

  // reset voltage flags
  for (phase = 0; phase < teleinfo_contract.phase; phase++) teleinfo_conso.phase[phase].volt_set = false;
}

// set environment for end of message
void TeleinfoReceptionMessageStop ()
{
  bool      percentage, update;
  uint8_t   phase, period, relay;
  uint32_t  time_over, timestamp, duration;
  int       index;
  long      value, total, delay, increment, total_current, sinsts, cosphi;
  long long llpact, llpreact, llpapp;
  char      checksum;
  char*     pstr_match;
  char      str_byte[2] = {0, 0};
  char      str_etiquette[TELEINFO_KEY_MAX];
  char      str_donnee[TELEINFO_DATA_MAX];
  char      str_text[TELEINFO_DATA_MAX];
  
  // set next stage
  teleinfo_meter.reception = TIC_RECEPTION_NONE;

  // get current timestamp
  timestamp = millis ();

  // calculate delay between 2 messages
  if (teleinfo_message.timestamp != UINT32_MAX) teleinfo_message.duration = (long)(timestamp - teleinfo_message.timestamp);
  teleinfo_message.timestamp = timestamp;

  // increment message counter and declare meter ready after 2nd message
  teleinfo_meter.nb_message++;

  // loop thru message data
  for (index = 0; index < TELEINFO_LINE_QTY; index++)
  {
    // save data from last message
    if (index < teleinfo_message.line_index)
    {
      teleinfo_message.arr_last[index].str_etiquette = teleinfo_message.arr_line[index].str_etiquette;
      teleinfo_message.arr_last[index].str_donnee    = teleinfo_message.arr_line[index].str_donnee;
      teleinfo_message.arr_last[index].checksum      = teleinfo_message.arr_line[index].checksum;
    }
    else
    {
      teleinfo_message.arr_last[index].str_etiquette = "";
      teleinfo_message.arr_last[index].str_donnee    = "";
      teleinfo_message.arr_last[index].checksum      = 0;
    }

    // init next message data
    teleinfo_message.arr_line[index].str_etiquette = "";
    teleinfo_message.arr_line[index].str_donnee    = "";
    teleinfo_message.arr_line[index].checksum      = 0;
  }

  // save number of lines and reset index
  teleinfo_message.line_last  = teleinfo_message.line_index;
  teleinfo_message.line_max   = max (teleinfo_message.line_last, teleinfo_message.line_max);
  teleinfo_message.line_index = 0;
  memset (teleinfo_message.str_line, 0, TELEINFO_LINE_MAX);
  
  // if needed, calculate max contract power from max phase current
  if ((teleinfo_contract.ssousc == 0) && (teleinfo_contract.isousc != 0)) teleinfo_contract.ssousc = teleinfo_contract.isousc * TELEINFO_VOLTAGE_REF;

  // ----------------------
  // counter global indexes

  // if needed, adjust number of phases
  if (Energy->phase_count < teleinfo_contract.phase) Energy->phase_count = teleinfo_contract.phase;

  // calculate total current
  total_current = 0;
  for (phase = 0; phase < teleinfo_contract.phase; phase++) total_current += teleinfo_conso.phase[phase].current;

  // update total conso energy counter
  TeleinfoUpdateConsoGlobalCounter ();

  // -------------------------------------
  //     calculate conso active power 
  //     according to detected method
  // -------------------------------------
  switch (teleinfo_contract.mode) 
  {
    // Historique & Standard : use instant apparent power and active power counter, timestamp from frame recpetion
    // ------------------------------------------------------------------------------------------------
    case TIC_MODE_HISTORIC:
    case TIC_MODE_STANDARD:

      // check if sinsts per phase should be used (+/- 5VA difference with PAPP)
      if ((teleinfo_meter.use_sinsts == 0) && (teleinfo_contract.mode == TIC_MODE_STANDARD) && (teleinfo_conso.papp > 5))
      {
        sinsts = 0;
        for (phase = 0; phase < teleinfo_contract.phase; phase++) sinsts += teleinfo_conso.phase[phase].sinsts;
        if ((sinsts > teleinfo_conso.papp - 5) && (sinsts < teleinfo_conso.papp + 5)) teleinfo_meter.use_sinsts = 1;
      }  

      // loop thru phase to update apparent power
      for (phase = 0; phase < teleinfo_contract.phase; phase++)
      {
        // set apparent power
        if (teleinfo_meter.use_sinsts == 1) teleinfo_conso.phase[phase].papp = teleinfo_conso.phase[phase].sinsts;
        else if (total_current > 0) teleinfo_conso.phase[phase].papp = teleinfo_conso.papp * teleinfo_conso.phase[phase].current / total_current;
        else if (teleinfo_contract.phase > 0) teleinfo_conso.phase[phase].papp = teleinfo_conso.papp / teleinfo_contract.phase;

        // update mvah increment
        teleinfo_conso.delta_mvah += teleinfo_conso.phase[phase].papp * teleinfo_message.duration / 36;
      }

      // if active power has increased
      if (teleinfo_conso.delta_mvah > 0)
      {
        // if global counter increased, calculate new cosphi
        if (teleinfo_conso.delta_mwh > 0)
        {
          cosphi = 1000 * 100 * teleinfo_conso.delta_mwh / teleinfo_conso.delta_mvah;
          TeleinfoUpdateCosphi (cosphi, teleinfo_conso.cosphi, teleinfo_conso.papp);
        }
      }

      // loop thru phase to update cosphi and calculate active power
      for (phase = 0; phase < teleinfo_contract.phase; phase++)
      {
        teleinfo_conso.phase[phase].cosphi = teleinfo_conso.cosphi.value;
        teleinfo_conso.phase[phase].pact   = teleinfo_conso.phase[phase].papp * teleinfo_conso.phase[phase].cosphi / 1000;
      }

      // if global counter increased, reset apparent power sum, active power delta and increase cosphi counter
      if (teleinfo_conso.delta_mwh > 0)
      {
        teleinfo_conso.delta_mwh  = 0;
        teleinfo_conso.delta_mvah = 0;
      }
    break;

    // PME/PMI : use apparent power and active power increments, timestamp from published time
    // ---------------------------------------------------------------------------------------
    case TIC_MODE_PMEPMI:

      // if current stamp is valid
      if (teleinfo_conso.last_stamp != LONG_MAX)
      {
        // init timestamp
        if (teleinfo_conso.papp_stamp == LONG_MAX) teleinfo_conso.papp_stamp = teleinfo_conso.last_stamp;
        if (teleinfo_conso.pact_stamp == LONG_MAX) teleinfo_conso.pact_stamp = teleinfo_conso.last_stamp;

        // apparent power increment management
        if (teleinfo_conso.papp_now != LONG_MAX)
        {
          // init apparent power increment reference
          if (teleinfo_conso.papp_prev == LONG_MAX) teleinfo_conso.papp_prev = teleinfo_conso.papp_now;

          // else detect apparent power rollback
          if (teleinfo_conso.papp_now < teleinfo_conso.papp_prev)
          {
            teleinfo_conso.papp_prev  = LONG_MAX;
            teleinfo_conso.papp_stamp = LONG_MAX;
            AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Counter rollback [VAh], start from %d"), teleinfo_conso.papp_now);
          }
        }

        // active power increment management
        if (teleinfo_conso.pact_now != LONG_MAX)
        {
          // init active power increment
          if (teleinfo_conso.pact_prev == LONG_MAX) teleinfo_conso.pact_prev = teleinfo_conso.pact_now;

          // detect active power rollback
          if (teleinfo_conso.pact_now < teleinfo_conso.pact_prev)
          {
            teleinfo_conso.pact_prev  = LONG_MAX;
            teleinfo_conso.pact_stamp = LONG_MAX;
            AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Counter rollback [Wh], start from %d"), teleinfo_conso.pact_now);
          }
        }

        // if more than 2 sec. difference, calculate new apparent power 
        if (teleinfo_conso.papp_stamp != LONG_MAX)
        {
          delay = TeleinfoTimestampDelay (teleinfo_conso.papp_stamp, teleinfo_conso.last_stamp);
          increment = teleinfo_conso.papp_now - teleinfo_conso.papp_prev;
          if ((delay > 2) && (increment > 2))
          {
            // calculate new apparent power
            teleinfo_conso.phase[0].papp = increment * 3600 / delay;

            // reset calculation data
            teleinfo_conso.papp_prev  = teleinfo_conso.papp_now;
            teleinfo_conso.papp_stamp = teleinfo_conso.last_stamp;
          }
        }

        // if more than 2 sec. difference, calculate new active power 
        if (teleinfo_conso.pact_stamp != LONG_MAX)
        {
          delay = TeleinfoTimestampDelay (teleinfo_conso.pact_stamp, teleinfo_conso.last_stamp);
          increment = teleinfo_conso.pact_now - teleinfo_conso.pact_prev;
          if ((delay > 2) && (increment > 2))
          {
            // calculate new active power
            teleinfo_conso.phase[0].pact = increment * 3600 / delay;

            // reset calculation data
            teleinfo_conso.pact_prev  = teleinfo_conso.pact_now;
            teleinfo_conso.pact_stamp = teleinfo_conso.last_stamp;
          }
        }

        // if possible, calculate new cosphi
        if (teleinfo_conso.phase[0].papp > 0)
        {
          // calculate new cosphi
          cosphi = 1000 * teleinfo_conso.phase[0].pact / teleinfo_conso.phase[0].papp;
          if (cosphi > 1000) cosphi = 1000;
          teleinfo_conso.cosphi.value = (teleinfo_conso.cosphi.value + cosphi) / 2;

          // increase counter
          teleinfo_conso.cosphi.nb_measure++;
        }
      }

      // loop thru phase to update cosphi
      for (phase = 0; phase < teleinfo_contract.phase; phase++) teleinfo_conso.phase[phase].cosphi = teleinfo_conso.cosphi.value;


      // reset last time stamp
      teleinfo_conso.last_stamp = LONG_MAX;
      break;

    // Emeraude : use active power and reactive power increments, timestamp from published time
    // ----------------------------------------------------------------------------------------
    case TIC_MODE_EMERAUDE:

      // if current stamp is valid
      if (teleinfo_conso.last_stamp != LONG_MAX)
      {
        // init timestamp
        if (teleinfo_conso.pact_stamp   == LONG_MAX) teleinfo_conso.pact_stamp   = teleinfo_conso.last_stamp;
        if (teleinfo_conso.preact_stamp == LONG_MAX) teleinfo_conso.preact_stamp = teleinfo_conso.last_stamp;

        // active power increment management
        if (teleinfo_conso.pact_now != LONG_MAX)
        {
          // init active power increment
          if (teleinfo_conso.pact_prev == LONG_MAX) teleinfo_conso.pact_prev = teleinfo_conso.pact_now;

          // detect active power rollback
          if (teleinfo_conso.pact_now < teleinfo_conso.pact_prev)
          {
            teleinfo_conso.pact_prev  = LONG_MAX;
            teleinfo_conso.pact_stamp = LONG_MAX;
            AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Counter rollback [Wh], start from %d"), teleinfo_conso.pact_now);
          }
        }

        // reactive power increment management
        if (teleinfo_conso.preact_now != LONG_MAX)
        {
          // init active power increment
          if (teleinfo_conso.preact_prev == LONG_MAX) teleinfo_conso.preact_prev = teleinfo_conso.preact_now;

          // detect active power rollback
          if (teleinfo_conso.preact_now < teleinfo_conso.preact_prev)
          {
            teleinfo_conso.preact_prev  = LONG_MAX;
            teleinfo_conso.preact_stamp = LONG_MAX;
            AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Counter rollback [varh], start from %d"), teleinfo_conso.preact_now);
          }
        }

        // if more than 2 sec. difference, calculate new active power 
        if (teleinfo_conso.pact_stamp != LONG_MAX)
        {
          delay = TeleinfoTimestampDelay (teleinfo_conso.pact_stamp, teleinfo_conso.last_stamp);
          increment = teleinfo_conso.pact_now - teleinfo_conso.pact_prev;
          if ((delay > 5) && (increment > 2))
          {
            // calculate new active power
            teleinfo_conso.phase[0].pact = increment * 3600 / delay;

            // reset calculation data
            teleinfo_conso.pact_prev  = teleinfo_conso.pact_now;
            teleinfo_conso.pact_stamp = teleinfo_conso.last_stamp;
          }
        }

        // if more than 5 sec. and 2varh difference, calculate new reactive power 
        if (teleinfo_conso.preact_stamp != LONG_MAX)
        {
          delay = TeleinfoTimestampDelay (teleinfo_conso.preact_stamp, teleinfo_conso.last_stamp);
          increment = teleinfo_conso.preact_now - teleinfo_conso.preact_prev;
          if ((delay > 5) && (increment > 2))
          {
            // calculate new active power
            teleinfo_conso.phase[0].preact = increment * 3600 / delay;

            // calculate apparent power
            llpact   = (long long)teleinfo_conso.phase[0].pact;
            llpreact = (long long)teleinfo_conso.phase[0].preact;
            llpapp   = llsqrt (llpact * llpact + llpreact * llpreact);
            teleinfo_conso.phase[0].papp = (long)llpapp;

            // reset calculation data
            teleinfo_conso.preact_prev  = teleinfo_conso.preact_now;
            teleinfo_conso.preact_stamp = teleinfo_conso.last_stamp;
          }
        }

        // if possible, calculate new cosphi
        if (teleinfo_conso.phase[0].papp > 0)
        {
          // calculate new cosphi
          cosphi = 1000 * teleinfo_conso.phase[0].pact / teleinfo_conso.phase[0].papp;
          if (cosphi > 1000) cosphi = 1000;
          teleinfo_conso.cosphi.value = (teleinfo_conso.cosphi.value + cosphi) / 2;

          // update counter
          teleinfo_conso.cosphi.nb_measure++;
        }

        // loop thru phase to update cosphi
        for (phase = 0; phase < teleinfo_contract.phase; phase++) teleinfo_conso.phase[phase].cosphi = teleinfo_conso.cosphi.value;
      }

      // reset last time stamp
      teleinfo_conso.last_stamp = LONG_MAX;
      break;
  }

  // ---------------------------------------
  //    calculate production active power 
  // ---------------------------------------

  if (teleinfo_prod.total_wh > 0)
  {
    // add apparent power increment according to message time window
    teleinfo_prod.delta_mvah += teleinfo_prod.papp * teleinfo_message.duration / 36;

    // if active power has increased 
    if (teleinfo_prod.delta_mvah > 0)
    {
      // if global counter increased, calculate new cosphi
      if (teleinfo_prod.delta_mwh > 0)
      {
        cosphi = 1000 * 100 * teleinfo_prod.delta_mwh / teleinfo_prod.delta_mvah;
        TeleinfoUpdateCosphi (cosphi, teleinfo_prod.cosphi, teleinfo_prod.papp);
      }
    }

    // calculate production active power
    teleinfo_prod.pact = teleinfo_prod.papp * teleinfo_prod.cosphi.value / 1000;

    // if global counter increased, reset apparent power sum, active power delta and increase cosphi counter
    if (teleinfo_prod.delta_mwh > 0)
    {
      teleinfo_prod.delta_mwh  = 0;
      teleinfo_prod.delta_mvah = 0;
    }
  }
  
  // -----------------------------
  //    update publication flags
  // -----------------------------

  percentage = false;
  if (TeleinfoDriverMeterReady ())
  {
    // loop thru phases to detect overload
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
      if (teleinfo_conso.phase[phase].papp > teleinfo_contract.ssousc * (long)teleinfo_config.percent / 100) teleinfo_meter.json.alert = 1;

    // loop thru phase to detect % power change (should be > to handle 0 conso)
    for (phase = 0; phase < teleinfo_contract.phase; phase++) 
      if (abs (teleinfo_conso.phase[phase].papp_last - teleinfo_conso.phase[phase].papp) > (teleinfo_contract.ssousc * TELEINFO_PERCENT_CHANGE / 100)) percentage = true;

    // detect % power change on production (should be > to handle 0 prod)
    if (abs (teleinfo_prod.papp_last - teleinfo_prod.papp) > (teleinfo_contract.ssousc * TELEINFO_PERCENT_CHANGE / 100)) percentage = true;

    // if publication every new message
    if (teleinfo_config.policy == TELEINFO_POLICY_MESSAGE)
    {
      // set publication flags
      if (teleinfo_config.meter) teleinfo_meter.json.meter = 1;
      if (teleinfo_config.tic) teleinfo_meter.json.tic = 1;
    }

    // else if % power change detected
    else if (percentage && (teleinfo_config.policy == TELEINFO_POLICY_PERCENT))
    {
      // update phases reference
      for (phase = 0; phase < teleinfo_contract.phase; phase++) teleinfo_conso.phase[phase].papp_last = teleinfo_conso.phase[phase].papp;
      teleinfo_prod.papp_last = teleinfo_prod.papp;

      // if deepsleep not enabled, set publication flags
      if (Settings->deepsleep == 0)
      {
        if (teleinfo_config.meter) teleinfo_meter.json.meter = 1;
        if (teleinfo_config.tic) teleinfo_meter.json.tic = 1;
      } 
    }
  }

  // --------------------------
  //   update energy counters
  // --------------------------

  // loop thru phases
  teleinfo_conso.pact = 0;
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // calculate global active power
    teleinfo_conso.pact += teleinfo_conso.phase[phase].pact;

    // calculate current
    if (teleinfo_conso.phase[phase].voltage > 0) teleinfo_conso.phase[phase].current = 1000 * teleinfo_conso.phase[phase].papp / teleinfo_conso.phase[phase].voltage;
      else teleinfo_conso.phase[phase].current = 0;

    // set voltage, current, apparent, active and reactive power
    Energy->voltage[phase] = (float)teleinfo_conso.phase[phase].voltage;
    Energy->current[phase] = (float)teleinfo_conso.phase[phase].current / 1000;
    Energy->apparent_power[phase] = (float)teleinfo_conso.phase[phase].papp;
    Energy->active_power[phase]   = (float)teleinfo_conso.phase[phase].pact;
    if (teleinfo_conso.phase[phase].preact != 0) Energy->reactive_power[phase] = (float)teleinfo_conso.phase[phase].preact;
  }
}

void TeleinfoReceptionLineStart ()
{
  // set next stage
  teleinfo_meter.reception = TIC_RECEPTION_LINE;

  // reset line content
  memset (teleinfo_message.str_line, 0, TELEINFO_LINE_MAX);
}

void TeleinfoReceptionLineStop ()
{
  uint8_t relay;
  int     index;
  long    value;
  char    checksum;
  char*   pstr_match;
  char    str_etiquette[TELEINFO_KEY_MAX];
  char    str_donnee[TELEINFO_DATA_MAX];
  char    str_text[TELEINFO_DATA_MAX];

  // set next stage
  teleinfo_meter.reception = TIC_RECEPTION_MESSAGE;

  // increment line counter
  teleinfo_meter.nb_line++;

  // if checksum is ok, handle the line
  checksum = TeleinfoCalculateChecksum (teleinfo_message.str_line, str_etiquette, sizeof (str_etiquette), str_donnee, sizeof (str_donnee));
  if (checksum != 0)
  {
    // get etiquette type
    index = GetCommandCode (str_text, sizeof (str_text), str_etiquette, kTeleinfoEtiquetteName);

    // update data according to etiquette
    switch (index)
    {
      //   Reference
      // -------------
      
      case TIC_ADCO:
        if (teleinfo_contract.mode != TIC_MODE_HISTORIC) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Meter Historique"));
        if (teleinfo_contract.unit != TIC_UNIT_KVA) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Contract in kVA"));
        teleinfo_contract.mode = TIC_MODE_HISTORIC;
        teleinfo_contract.unit = TIC_UNIT_KVA;
        if (teleinfo_contract.ident == 0) teleinfo_contract.ident = atoll (str_donnee);
        break;

      case TIC_ADSC:
        if (teleinfo_contract.mode != TIC_MODE_STANDARD) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Meter Standard"));
        if (teleinfo_contract.unit != TIC_UNIT_KVA) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Contract in kVA"));
        teleinfo_contract.mode = TIC_MODE_STANDARD;
        teleinfo_contract.unit = TIC_UNIT_KVA;
        if (teleinfo_contract.ident == 0) teleinfo_contract.ident = atoll (str_donnee);
        break;

      case TIC_ADS:
        if (teleinfo_contract.ident == 0) teleinfo_contract.ident = atoll (str_donnee);
        break;

      case TIC_CONFIG:
        if (teleinfo_contract.mode != TIC_MODE_PMEPMI) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Meter PME/PMI"));
        if (teleinfo_contract.unit != TIC_UNIT_KW) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Contract in kW"));
        teleinfo_contract.mode = TIC_MODE_PMEPMI;
        teleinfo_contract.unit = TIC_UNIT_KW;
        break;

      //   Timestamp
      // -------------
      
      case TIC_DATE:
      case TIC_DATECOUR:
        teleinfo_conso.last_stamp = TeleinfoTimestampCalculate (str_donnee);
        break;

      //   Contract
      // ------------
      
      case TIC_OPTARIF:
      case TIC_NGTF:
      case TIC_MESURES1:
        TeleinfoContractUpdate (str_donnee);
        break;

      case TIC_CONTRAT:
        if (teleinfo_contract.mode != TIC_MODE_EMERAUDE) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Meter Emeraude"));
        if (teleinfo_contract.unit != TIC_UNIT_KW) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Contract in kW"));
        teleinfo_contract.mode = TIC_MODE_EMERAUDE;
        teleinfo_contract.unit = TIC_UNIT_KW;
        TeleinfoContractUpdate (str_donnee);
        break;

      //   Period
      // -----------

      // period name
      case TIC_PTEC:
      case TIC_LTARF:
      case TIC_PTCOUR:
      case TIC_PTCOUR1:
        TeleinfoPeriodUpdate (str_donnee);
        break;

      //   Current
      // -----------

      case TIC_IINST:
      case TIC_IRMS1:
      case TIC_IINST1:
      case TIC_I1:
        TeleinfoCurrentUpdate (str_donnee, 0);
        break;

      case TIC_IRMS2:
      case TIC_IINST2:
      case TIC_I2:
        TeleinfoCurrentUpdate (str_donnee, 1);
        break;

      case TIC_IRMS3:
      case TIC_IINST3:
      case TIC_I3:
        TeleinfoCurrentUpdate (str_donnee, 2);
        teleinfo_contract.phase = 3; 
        break;

      //   Power
      // ---------

      // instant apparent power, 
      case TIC_PAPP:
      case TIC_SINSTS:
        TeleinfoConsoApparentPowerUpdate (str_donnee);
        break;

      case TIC_SINSTS1:
        TeleinfoConsoApparentPowerUpdate (str_donnee, 0);
        break;

      case TIC_SINSTS2:
        TeleinfoConsoApparentPowerUpdate (str_donnee, 1);
        break;

      case TIC_SINSTS3:
        TeleinfoConsoApparentPowerUpdate (str_donnee, 2);
        break;

      // if in prod mode, instant apparent power, 
      case TIC_SINSTI:
        TeleinfoProdApparentPowerUpdate (str_donnee);
        break;

      // apparent power counter since last period
      case TIC_EAPPS:
        strcpy (str_text, str_donnee);
        pstr_match = strchr (str_text, 'V');
        if (pstr_match != nullptr) *pstr_match = 0;
        teleinfo_conso.papp_now = atol (str_text);
        break;

      // active power counter since last period
      case TIC_EAS:
      case TIC_EA:
        strcpy (str_text, str_donnee);
        pstr_match = strchr (str_text, 'W');
        if (pstr_match != nullptr) *pstr_match = 0;
        teleinfo_conso.pact_now = atol (str_text);
        break;

      // reactive power counter since last period
      case TIC_ERP:
        strcpy (str_text, str_donnee);
        pstr_match = strchr (str_text, 'v');
        if (pstr_match != nullptr) *pstr_match = 0;
        teleinfo_conso.preact_now = atol (str_text);
        break;

      //   Voltage
      // -----------

      // RMS voltage
      case TIC_URMS1:
        TeleinfoVoltageUpdate (str_donnee, 0, true);
        break;

      case TIC_URMS2:
        TeleinfoVoltageUpdate (str_donnee, 1, true);
        break;

      case TIC_URMS3:
        TeleinfoVoltageUpdate (str_donnee, 2, true);
        teleinfo_contract.phase = 3; 
        break;

      // average voltage
      case TIC_U10MN:         // for the last 10 mn
      case TIC_UMOY1:
        TeleinfoVoltageUpdate (str_donnee, 0, false);
        break;

      case TIC_UMOY2:
        TeleinfoVoltageUpdate (str_donnee, 1, false);
        break;

      case TIC_UMOY3:
        TeleinfoVoltageUpdate (str_donnee, 2, false);
        teleinfo_contract.phase = 3; 
        break;

      //   Contract max values
      // -----------------------

      // Maximum Current
      case TIC_ISOUSC:
        value = atol (str_donnee);
        if ((value > 0) && (teleinfo_contract.isousc != value))
        {
          teleinfo_contract.isousc = value;
          teleinfo_contract.ssousc = teleinfo_contract.isousc * TELEINFO_VOLTAGE_REF;
        }
        break;

      // Contract maximum Power
      case TIC_PREF:
      case TIC_PCOUP:
      case TIC_PS:
        TeleinfoContractPowerUpdate (str_donnee, true);
        break;

      case TIC_PSP:
        if (teleinfo_contract.period == 0) TeleinfoContractPowerUpdate (str_donnee, true);
        break;

      case TIC_PSPM:
        if (teleinfo_contract.period == 1) TeleinfoContractPowerUpdate (str_donnee, true);
        break;

      case TIC_PSHPH:
        if (teleinfo_contract.period == 2) TeleinfoContractPowerUpdate (str_donnee, true);
        break;

      case TIC_PSHPD:
        if (teleinfo_contract.period == 3) TeleinfoContractPowerUpdate (str_donnee, true);
        break;

      case TIC_PSHCH:
        if (teleinfo_contract.period == 4) TeleinfoContractPowerUpdate (str_donnee, true);
        break;

      case TIC_PSHCD:
        if (teleinfo_contract.period == 5) TeleinfoContractPowerUpdate (str_donnee, true);
        break;

      case TIC_PSHPE:
        if (teleinfo_contract.period == 6) TeleinfoContractPowerUpdate (str_donnee, true);
        break;

      case TIC_PSHCE:
        if (teleinfo_contract.period == 7) TeleinfoContractPowerUpdate (str_donnee, true);
        break;

      case TIC_PSJA:
        if (teleinfo_contract.period == 8) TeleinfoContractPowerUpdate (str_donnee, true);
        break;

      case TIC_PSHH:
        if (teleinfo_contract.period == 9) TeleinfoContractPowerUpdate (str_donnee, true);
        break;

      case TIC_PSHD:
        if (teleinfo_contract.period == 10) TeleinfoContractPowerUpdate (str_donnee, true);
        break;

      case TIC_PSHM:
        if (teleinfo_contract.period == 11) TeleinfoContractPowerUpdate (str_donnee, true);
        break;

      case TIC_PSDSM:
        if (teleinfo_contract.period == 12) TeleinfoContractPowerUpdate (str_donnee, true);
        break;

      case TIC_PSSCM:
        if (teleinfo_contract.period == 13) TeleinfoContractPowerUpdate (str_donnee, true);
        break;

      //   Counters
      // ------------

      // counter according to current period
      case TIC_EAPS:
        TeleinfoConsoIndexCounterUpdate (str_donnee, teleinfo_contract.period);
        break;

      case TIC_EAIT:
        TeleinfoProdGlobalCounterUpdate (str_donnee);
        break;

      case TIC_BASE:
      case TIC_HCHC:
      case TIC_EJPHN:
      case TIC_BBRHCJB:
      case TIC_EAPP:
      case TIC_EASF01:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 0);
        break;

      case TIC_HCHP:
      case TIC_EJPHPM:
      case TIC_BBRHPJB:
      case TIC_EAPPM:
      case TIC_EASF02:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 1);
        break;

      case TIC_BBRHCJW:
      case TIC_EAPHPH:
      case TIC_EASF03:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 2);
        break;

      case TIC_BBRHPJW:
      case TIC_EAPHPD:
      case TIC_EASF04:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 3);
        break;

      case TIC_BBRHCJR:
      case TIC_EAPHCH:
      case TIC_EASF05:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 4);
        break;

      case TIC_BBRHPJR:
      case TIC_EAPHCD:
      case TIC_EASF06:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 5);
        break;

      case TIC_EAPHPE:
      case TIC_EASF07:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 6);
        break;

      case TIC_EAPHCE:
      case TIC_EASF08:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 7);
        break;

      case TIC_EAPJA:
      case TIC_EASF09:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 8);
      break;

      case TIC_EAPHH:
      case TIC_EASF10:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 9);
        break;

      case TIC_EAPHD:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 10);
        break;

      case TIC_EAPHM:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 11);
        break;

      case TIC_EAPDSM:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 12);
        break;

      case TIC_EAPSCM:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 13);
        break;

      //   Flags
      // ---------

      // Overload
      case TIC_ADPS:
        TeleinfoSetPreavis (TIC_PREAVIS_DANGER, "DEP");
        break;

      case TIC_ADIR1:
        TeleinfoSetPreavis (TIC_PREAVIS_DANGER, "DEP1");
      break;

      case TIC_ADIR2:
        TeleinfoSetPreavis (TIC_PREAVIS_DANGER, "DEP2");
        break;

      case TIC_ADIR3:
        TeleinfoSetPreavis (TIC_PREAVIS_DANGER, "DEP3");
        break;

      case TIC_PEJP:
        TeleinfoSetPreavis (TIC_PREAVIS_ALERT, "EJP");
        break;

      case TIC_PREAVIS:
        TeleinfoSetPreavis (TIC_PREAVIS_ALERT, str_donnee);
        break;

      // start of next pointe period
      case TIC_DPM1:
        TeleinfoCalendarPointeDeclare (str_donnee);
        break;
        
      // next day standard profile
      case TIC_PJOURF1:
        TeleinfoCalendarTomorrowProfile (str_donnee);
        break;
        
      // next pointe profile
      case TIC_PPOINTE:
        TeleinfoCalendarPointeProfile (str_donnee);
        break;
        
      // STGE flags
      case TIC_STGE:
        TeleinfoAnalyseSTGE (str_donnee);
        break;

      case TIC_RELAIS:
        relay = (uint8_t)atoi (str_donnee);
        if ((relay != teleinfo_meter.relay) && TeleinfoDriverMeterReady ()) teleinfo_meter.json.relay = 1;
        teleinfo_meter.relay = relay;
        break;
    }
  }

  // if maximum number of lines not reached, save new line
  index = teleinfo_message.line_index;
  teleinfo_message.arr_line[index].str_etiquette = str_etiquette;
  teleinfo_message.arr_line[index].str_donnee    = str_donnee;
  teleinfo_message.arr_line[index].checksum      = checksum;

  // increment line index and stay on last line if reached
  teleinfo_message.line_index++;
  if (teleinfo_message.line_index >= TELEINFO_LINE_QTY) teleinfo_message.line_index = TELEINFO_LINE_QTY - 1;
}

// Handling of received teleinfo data
//   0x04 : Message reset 
//   0x02 : Message start
//   Ox03 : Message stop
//   0x0A : Line start
//   0x0D : Line stop
//   0x09 : Line separator
void TeleinfoReceiveData (const uint32_t timeout)
{
  uint32_t time_over;
  char     str_byte[2] = {0, 0};
  
  // check serial port
  if (teleinfo_serial == nullptr) return;

  // if reception is not active, drop all data
  if (teleinfo_meter.serial != TIC_SERIAL_ACTIVE)
  {
    teleinfo_serial->flush ();
    return;
  }

  // serial receive loop
  time_over = millis () + timeout;
  while (!TimeReached (time_over) && teleinfo_serial->available ()) 
  {
    // read character
    str_byte[0] = (char)teleinfo_serial->read ();

#ifdef USE_TCPSERVER
    // send character thru TCP stream
    TCPSend (str_byte[0]); 
#endif

    // handle reset
    if (str_byte[0] == 0x04) TeleinfoReceptionMessageReset ();

    // else handle according to reception stage
    else switch (teleinfo_meter.reception)
    {
      case TIC_RECEPTION_NONE:
        // message start
        if (str_byte[0] == 0x02) TeleinfoReceptionMessageStart ();
        break;

      case TIC_RECEPTION_MESSAGE:
        // line start
        if (str_byte[0] == 0x0A) TeleinfoReceptionLineStart ();

        // message stop
        else if (str_byte[0] == 0x03) TeleinfoReceptionMessageStop ();
        break;

      case TIC_RECEPTION_LINE:
        // line stop
        if (str_byte[0] == 0x0D) TeleinfoReceptionLineStop ();

        // message stop
        else if (str_byte[0] == 0x03) TeleinfoReceptionMessageStop ();

        // append character to line
        else
        {
          // set line separator
          if ((str_byte[0] == 0x09) && (teleinfo_meter.nb_message == 0)) teleinfo_meter.sep_line = 0x09;

          // append separator to line
          strlcat (teleinfo_message.str_line, str_byte, sizeof (teleinfo_message.str_line));
        }
        break;

      default:
        break;
    }
  }
}

uint8_t TeleinfoRelayStatus (const uint8_t index)
{
  uint8_t result;

  result = teleinfo_meter.relay >> index;
  result = result & 0x01;

  return result;
}

// Treatments called every second
void TeleinfoEnergyEverySecond ()
{
  bool     to_publish = false;
  bool     to_reset   = false;
  uint8_t  index, slot, phase;
  uint32_t time_now;

  // check time validity
  if (!RtcTime.valid) return;

  // get current time
  time_now = LocalTime ();

  //   Counters and Totals
  // -----------------------

  // force meter grand total on conso total
  Energy->total[0] = (float)(teleinfo_conso.global_wh / 1000);
  for (phase = 1; phase < teleinfo_contract.phase; phase++) Energy->total[phase] = 0;
  RtcSettings.energy_kWhtotal_ph[0] = (int32_t)(teleinfo_conso.global_wh / 100000);
  for (phase = 1; phase < teleinfo_contract.phase; phase++) RtcSettings.energy_kWhtotal_ph[phase] = 0;

  // update today delta
  for (phase = 0; phase < teleinfo_contract.phase; phase++) Energy->kWhtoday_delta[phase] += (int32_t)(Energy->active_power[phase] * 1000 / 36);

#ifndef USE_WINKY
  // update totals every 30 seconds
  if (RtcTime.second % 30 == 0) EnergyUpdateToday ();
#endif    // USE_WINKY 

  //   Midnight day change
  // -----------------------

  if (teleinfo_meter.days == 0) teleinfo_meter.days = RtcTime.days;
  if (teleinfo_meter.days != RtcTime.days)
  {
    // update daily counters 
    teleinfo_conso.midnight_wh = teleinfo_conso.global_wh;
    teleinfo_prod.midnight_wh  = teleinfo_prod.total_wh;

    // rotate profile of yesterday and today
    for (index = TIC_DAY_YESTERDAY; index < TIC_DAY_TOMORROW; index ++)
    {
      teleinfo_meter.arr_day[index].valid = teleinfo_meter.arr_day[index + 1].valid;
      for (slot = 0; slot < 24; slot++) teleinfo_meter.arr_day[index].arr_period[slot] = teleinfo_meter.arr_day[index + 1].arr_period[slot];
    }

    // init profile of tomorrow
    teleinfo_meter.arr_day[TIC_DAY_TOMORROW].valid = 0;
    for (slot = 0; slot < 24; slot++) teleinfo_meter.arr_day[TIC_DAY_TOMORROW].arr_period[slot] = TIC_LEVEL_NONE;

    // shift pointe day
    if (teleinfo_meter.pointe.day == TIC_DAY_TOMORROW) teleinfo_meter.pointe.day = TIC_DAY_TODAY;
      else to_reset = true;

    // set new day
    teleinfo_meter.days = RtcTime.days;
  }

  //   Reset Preavis & Pointe data
  // -------------------------------

  // check if preavis is over
  if (teleinfo_meter.preavis.timeout < time_now) TeleinfoSetPreavis (TIC_PREAVIS_NONE, "");

  // check if pointe period started and needs to be reset
  if (teleinfo_meter.pointe.start < time_now) to_reset = true;

  // reset pointe period
  if (to_reset)
  {
    teleinfo_meter.pointe.hour  = UINT8_MAX;
    teleinfo_meter.pointe.day   = UINT8_MAX;
    teleinfo_meter.pointe.start = UINT32_MAX;
    teleinfo_meter.pointe.crc_start   = 0;
    teleinfo_meter.pointe.crc_profile = 0;
  }
}

/***************************************\
 *              Interface
\***************************************/

// Teleinfo energy driver
bool Xnrg15 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_PRE_INIT:
      TeleinfoPreInit ();
      TeleinfoSerialStart ();
      break;
    case FUNC_INIT:
      TeleinfoInit ();
      break;
    case FUNC_COMMAND:
      result = TeleinfoHandleCommand ();
      break;
    case FUNC_LOOP:
      TeleinfoReceiveData (TELEINFO_RECV_TIMEOUT_LOOP);
      break;
    case FUNC_ENERGY_EVERY_SECOND:
      TeleinfoEnergyEverySecond ();
      break;
  }

  return result;
}

#endif      // USE_TELEINFO
#endif      // USE_ENERGY_SENSOR

#endif      // FIRMWARE_SAFEBOOT
