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
    08/03/2021 - v7.2  - Handle voltage and checksum for srodatage
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
    13/04/2024 - v13.7 - Adjust cosphi calculation for auto-prod router usage
    21/05/2024 - v14.4 - Based on Tasmota 14
                         Add serial reception buffer to minimize reception errors
    01/06/2024 - v14.5 - Add contract auto-discovery for unknown Standard contracts
    28/06/2024 - v14.6 - Remove all String for ESP8266 stability
    28/06/2024 - v14.7 - Add commands full and noraw (compatibility with official version)
    28/08/2024 - v14.8 - Correct bug in Tempo Historique contract management
  
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
  bool  serviced = true;
  int   index;
  char *pstr_next, *pstr_key, *pstr_value;
  char  str_label[32];
  char  str_item[64];
  char  str_line[96];

  if (Energy->command_code == CMND_ENERGYCONFIG) 
  {
    // if no parameter, show configuration
    if (XdrvMailbox.data_len == 0) 
    {
      AddLog (LOG_LEVEL_INFO, PSTR ("EnergyConfig Teleinfo parameters :"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  historique     mode historique (1200 bauds)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  standard       mode standard (9600 bauds)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  noraw          pas d'emission trame TIC"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  full           emission trame TIC"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  stats          statistiques de reception"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  reset          changement de contrat"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  error=%u        affiche les compteurs d'erreurs"), teleinfo_config.error);
      AddLog (LOG_LEVEL_INFO, PSTR ("  percent=%u    puissance maximale acceptable (%% du contrat)"), teleinfo_config.percent);

      // publishing policy
      str_line[0] = 0;
      for (index = 0; index < TIC_POLICY_MAX; index++)
      {
        GetTextIndexed (str_label, sizeof (str_label), index, kTeleinfoMessagePolicy);
        sprintf_P (str_item, PSTR ("%u=%s"), index, str_label);
        if (strlen (str_line) > 0) strcat_P (str_line, PSTR (", "));
        strlcat (str_line, str_item, sizeof (str_line));
      }
      AddLog (LOG_LEVEL_INFO, PSTR ("  policy=%u       politique de publication : %s"), teleinfo_config.policy, str_line);

      // publishing type
      AddLog (LOG_LEVEL_INFO, PSTR ("  meter=%u        publication sections METER, PROD & CONTRACT"), teleinfo_config.meter);
      AddLog (LOG_LEVEL_INFO, PSTR ("  calendar=%u     publication section CAL"), teleinfo_config.calendar);
      AddLog (LOG_LEVEL_INFO, PSTR ("  relay=%u        publication section RELAY"), teleinfo_config.relay);
      AddLog (LOG_LEVEL_INFO, PSTR ("  led=%u          signal compteur couleur période en cours"), teleinfo_config.led_level);

#ifdef USE_TELEINFO_CURVE
      AddLog (LOG_LEVEL_INFO, PSTR ("  nbday=%d        nbre log quotidiens"), teleinfo_config.param[TIC_CONFIG_NBDAY]);
      AddLog (LOG_LEVEL_INFO, PSTR ("  nbweek=%d       nbre log hebdomadaires"), teleinfo_config.param[TIC_CONFIG_NBWEEK]);

      AddLog (LOG_LEVEL_INFO, PSTR ("  maxv=%u       graph : tension max (V)"), teleinfo_config.max_volt);
      AddLog (LOG_LEVEL_INFO, PSTR ("  maxva=%u     graph : puissance max (VA or W)"), teleinfo_config.max_power);
      AddLog (LOG_LEVEL_INFO, PSTR ("  maxhour=%d      graph : total horaire max (Wh)"), teleinfo_config.param[TIC_CONFIG_MAX_HOUR]);
      AddLog (LOG_LEVEL_INFO, PSTR ("  maxday=%d      graph : total quotidien max (Wh)"), teleinfo_config.param[TIC_CONFIG_MAX_DAY]);
      AddLog (LOG_LEVEL_INFO, PSTR ("  maxmonth=%d   graph : total mensuel max (Wh)"), teleinfo_config.param[TIC_CONFIG_MAX_MONTH]);
#endif    // USE_TELEINFO_CURVE

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

      // if needed, trigger to save configuration
      if (serviced) TeleinfoConfigSave ();
    }
  }

  return serviced;
}

bool TeleinfoExecuteCommand (const char* pstr_command, const char* pstr_param)
{
  bool serviced = false;
  bool restart  = false;
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

    case TIC_CMND_HISTORIQUE:
      SetSerialBaudrate (1200);                         // 1200 bauds
      serviced = true;
      TasmotaGlobal.restart_flag = 2;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Mode Historique (%d bauds)"), TasmotaGlobal.baudrate);
      break;

    case TIC_CMND_STANDARD:
      SetSerialBaudrate (9600);                         // 9600 bauds
      serviced = true;
      TasmotaGlobal.restart_flag = 2;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Mode Standard (%d bauds)"), TasmotaGlobal.baudrate);
      break;

    case TIC_CMND_NORAW:
      teleinfo_config.tic = 0;
      serviced = true;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Non emission section TIC"));
      break;

    case TIC_CMND_FULL:
      teleinfo_config.tic = 1;
      serviced = true;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Emission section TIC"));
      break;

    case TIC_CMND_PERCENT:
      serviced = (value < TIC_PERCENT_MAX);
      if (serviced) teleinfo_config.percent = (uint8_t)value;
      break;

    case TIC_CMND_ERROR:
      serviced = true;
      teleinfo_config.error = (uint8_t)value;
      break;

    case TIC_CMND_RESET:
      serviced = true;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Contrat remis a zéro"));
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Redémarrage en cours"));
      TeleinfoContractReset ();
      restart = true;
      break;

    case TIC_CMND_LED:
      serviced = (value < 2);
      if (serviced) teleinfo_config.led_level = (uint8_t)value;
      break;

    case TIC_CMND_POLICY:
      serviced = (value < TIC_POLICY_MAX);
      if (serviced) teleinfo_config.policy = (uint8_t)value;
      break;

    case TIC_CMND_METER:
      serviced = (value < 2);
      if (serviced) teleinfo_config.meter = (uint8_t)value;
      break;

    case TIC_CMND_CALENDAR:
      serviced = (value < 2);
      if (serviced) teleinfo_config.calendar = (uint8_t)value;
      break;

    case TIC_CMND_RELAY:
      serviced = (value < 2);
      if (serviced) teleinfo_config.relay = (uint8_t)value;
      break;

#ifdef USE_TELEINFO_CURVE
    case TIC_CMND_MAX_V:
      serviced = ((value >= TIC_GRAPH_MIN_VOLTAGE) && (value <= TIC_GRAPH_MAX_VOLTAGE));
      if (serviced) teleinfo_config.max_volt = value;
      break;

    case TIC_CMND_MAX_VA:
      serviced = ((value >= TIC_GRAPH_MIN_POWER) && (value <= TIC_GRAPH_MAX_POWER));
      if (serviced) teleinfo_config.max_power = value;
      break;

    case TIC_CMND_LOG_DAY:
      serviced = ((value > 0) && (value <= 31));
      if (serviced) teleinfo_config.param[TIC_CONFIG_NBDAY] = value;
      break;

    case TIC_CMND_LOG_WEEK:
      serviced = ((value > 0) && (value <= 52));
      if (serviced) teleinfo_config.param[TIC_CONFIG_NBWEEK] = value;
      break;

    case TIC_CMND_MAX_KWH_HOUR:
      serviced = ((value >= TIC_GRAPH_MIN_WH_HOUR) && (value <= TIC_GRAPH_MAX_WH_HOUR));
      if (serviced) teleinfo_config.param[TIC_CONFIG_MAX_HOUR] = value;
      break;

    case TIC_CMND_MAX_KWH_DAY:
      serviced = ((value >= TIC_GRAPH_MIN_WH_DAY) && (value <= TIC_GRAPH_MAX_WH_DAY));
      if (serviced) teleinfo_config.param[TIC_CONFIG_MAX_DAY] = value;
      break;

    case TIC_CMND_MAX_KWH_MONTH:
      serviced = ((value >= TIC_GRAPH_MIN_WH_MONTH) && (value <= TIC_GRAPH_MAX_WH_MONTH));
      if (serviced) teleinfo_config.param[TIC_CONFIG_MAX_MONTH] = value;
      break;
#endif    // USE_TELEINFO_CURVE
  }

  // if needed, trigger to save config
  if (serviced && (index != TIC_CMND_STATS)) TeleinfoConfigSave ();

  // if needed, restart
  if (restart) WebRestart (1);

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
    else pstr_result[0] = 0;

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

// Start serial reception
bool TeleinfoSerialStart ()
{
  bool   is_ready;
  size_t buffer_size;

  // if serial port is not already created
  is_ready = (teleinfo_serial != nullptr);
  if (!is_ready)
  { 
    // GPIO selection step
    teleinfo_meter.serial = TIC_SERIAL_GPIO;
    if (PinUsed (GPIO_TELEINFO_RX))
    {
      // speed selection step
      teleinfo_meter.serial = TIC_SERIAL_SPEED;
      if (TasmotaGlobal.baudrate <= 9600)
      {
        // create and initialise serial port
        teleinfo_serial = new TasmotaSerial (Pin (GPIO_TELEINFO_RX), -1, 1, 0);

        // start reception
        is_ready = teleinfo_serial->begin (TasmotaGlobal.baudrate, SERIAL_7E1);
        if (is_ready)
        {
          // set serial reception buffer
          teleinfo_serial->setRxBufferSize (TIC_BUFFER_MAX);
          buffer_size = teleinfo_serial->getRxBufferSize ();

#ifdef ESP32

          // display UART used and speed
          AddLog (LOG_LEVEL_INFO, PSTR("TIC: Serial UART %d started at %ubps (buffer %u)"), teleinfo_serial->getUart (), TasmotaGlobal.baudrate, buffer_size);

#else       // ESP8266

          // force configuration on ESP8266
          if (teleinfo_serial->hardwareSerial ()) ClaimSerial ();

          // display serial speed
          AddLog (LOG_LEVEL_INFO, PSTR("TIC: Serial started at %ubps (buffer %d)"), TasmotaGlobal.baudrate, buffer_size);

#endif      // ESP32 & ESP8266

          // flush serail data
          teleinfo_serial->flush ();

          // serial init succeeded
          teleinfo_meter.serial = TIC_SERIAL_ACTIVE;
        }

        // serial init failed
        else teleinfo_meter.serial = TIC_SERIAL_FAILED;
      }
    }

    // log serial port init failure
    if (!is_ready) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Serial port init failed"));
  }

  return is_ready;
}

// Stop serial reception
void TeleinfoSerialStop ()
{
  // declare serial as stopped
  teleinfo_meter.serial = TIC_SERIAL_STOPPED;
}

// Load configuration from Settings or from LittleFS
void TeleinfoConfigLoad () 
{
  uint8_t index, day, slot, part;

  // load standard settings
  teleinfo_config.percent    = Settings->teleinfo.percent;
  teleinfo_config.policy     = Settings->teleinfo.policy;
  teleinfo_config.meter      = Settings->teleinfo.meter;
  teleinfo_config.energy     = Settings->teleinfo.energy;
  teleinfo_config.calendar   = Settings->teleinfo.calendar;
  teleinfo_config.relay      = Settings->teleinfo.relay;
  teleinfo_config.tic        = Settings->teleinfo.tic;
  teleinfo_config.led_level  = Settings->teleinfo.led_level;
  teleinfo_config.sensor     = Settings->teleinfo.sensor;
  teleinfo_config.max_volt   = TIC_GRAPH_MIN_VOLTAGE + (long)Settings->teleinfo.adjust_v * 5;
  teleinfo_config.max_power  = TIC_GRAPH_MIN_POWER + (long)Settings->teleinfo.adjust_va * 3000;

  // validate boundaries
  if ((teleinfo_config.policy < 0) || (teleinfo_config.policy >= TIC_POLICY_MAX)) teleinfo_config.policy = TIC_POLICY_TELEMETRY;
  if (teleinfo_config.meter > 1) teleinfo_config.meter = 1;
  if (teleinfo_config.calendar > 1) teleinfo_config.calendar = 1;
  if (teleinfo_config.relay > 1) teleinfo_config.relay = 1;
  if (teleinfo_config.tic > 1) teleinfo_config.tic = 1;
  if ((teleinfo_config.percent < TIC_PERCENT_MIN) || (teleinfo_config.percent > TIC_PERCENT_MAX)) teleinfo_config.percent = 100;

  // init parameters
  for (index = 0; index < TIC_CONFIG_MAX; index ++) teleinfo_config.param[index] = arrTeleinfoConfigDefault[index];

  // init contract data
  teleinfo_contract.period_idx    = UINT8_MAX;
  teleinfo_contract.period_qty    = 0;
  teleinfo_contract.str_code[0]   = 0;
  teleinfo_contract.str_period[0] = 0;
  for (index = 0; index < TIC_PERIOD_MAX; index ++)
  {
    teleinfo_contract.arr_period[index].valid = 0;
    teleinfo_contract.arr_period[index].level = 0;
    teleinfo_contract.arr_period[index].hchp  = 1;
    teleinfo_contract.arr_period[index].str_code  = "";
    teleinfo_contract.arr_period[index].str_label = "";
  }

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
  char   str_text[32];
  char   str_line[256];
  char   *pstr_key, *pstr_value;
  File   file;

  // if file exists, open and read each line
  if (ffsp->exists (F (TIC_FILE_CFG)))
  {
    file = ffsp->open (F (TIC_FILE_CFG), "r");
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
        // look for configuration key
        index = GetCommandCode (str_text, sizeof (str_text), pstr_key, kTeleinfoConfigKey);
        if ((index >= 0) && (index < TIC_CONFIG_MAX)) teleinfo_config.param[index] = atol (pstr_value);

        else
        {
          index = GetCommandCode (str_text, sizeof (str_text), pstr_key, kTeleinfoContractKey);
          switch (index)
          {
            case TIC_CONTRACT_INDEX:
              teleinfo_contract.index = atoi (pstr_value);
              break;

            case TIC_CONTRACT_NAME:
              strcpy (teleinfo_contract.str_code, pstr_value);
              break;

            case TIC_CONTRACT_PERIOD:
              slot = TeleinfoContractString2Period (pstr_value);
              teleinfo_contract.period_qty = max (teleinfo_contract.period_qty, slot);
              break;
          }
        }
      }
    }

    // update yesterday totals
    teleinfo_conso.yesterday_wh = teleinfo_config.param[TIC_CONFIG_YESTERDAY_CONSO];
    teleinfo_prod.yesterday_wh  = teleinfo_config.param[TIC_CONFIG_YESTERDAY_PROD];
  }

# endif     // USE_UFILESYS

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Loading config"));
}

// Save configuration to Settings or to LittleFS
void TeleinfoConfigSave () 
{
  uint8_t   index, day, slot, part;
  long long today_wh;

  // save standard settings
  Settings->teleinfo.percent   = teleinfo_config.percent;
  Settings->teleinfo.policy    = teleinfo_config.policy;
  Settings->teleinfo.meter     = teleinfo_config.meter;
  Settings->teleinfo.energy    = teleinfo_config.energy;
  Settings->teleinfo.calendar  = teleinfo_config.calendar;
  Settings->teleinfo.relay     = teleinfo_config.relay;
  Settings->teleinfo.tic       = teleinfo_config.tic;
  Settings->teleinfo.led_level = teleinfo_config.led_level;
  Settings->teleinfo.sensor    = teleinfo_config.sensor;
  Settings->teleinfo.adjust_v  = (teleinfo_config.max_volt - TIC_GRAPH_MIN_VOLTAGE) / 5;
  Settings->teleinfo.adjust_va = (teleinfo_config.max_power - TIC_GRAPH_MIN_POWER) / 3000;

  // update today and yesterday conso/prod
  teleinfo_config.param[TIC_CONFIG_TODAY_CONSO]     = teleinfo_conso.today_wh;
  teleinfo_config.param[TIC_CONFIG_YESTERDAY_CONSO] = teleinfo_conso.yesterday_wh;
  teleinfo_config.param[TIC_CONFIG_TODAY_PROD]      = teleinfo_prod.today_wh;;
  teleinfo_config.param[TIC_CONFIG_YESTERDAY_PROD]  = teleinfo_prod.yesterday_wh;

  // save hourly status slots
  for (slot = 0; slot < 24; slot++)
  {
    index = slot % 12;
    part  = slot / 12;
    for (day = TIC_DAY_YESTERDAY; day < TIC_DAY_MAX; day++) Settings->rf_code[index][2 * day + part] = teleinfo_meter.arr_day[day].arr_period[slot];
  }

  // save littlefs settings
#ifdef USE_UFILESYS
  char    str_key[64];
  char    str_text[64];
  File    file;

  // open file and write content
  file = ffsp->open (F (TIC_FILE_CFG), "w");

  // save configuration parameters
  file.print (F ("[config]\n"));
  for (index = 0; index < TIC_CONFIG_MAX; index ++)
  {
    GetTextIndexed (str_key, sizeof (str_key), index, kTeleinfoConfigKey);
    sprintf_P (str_text, PSTR ("%s=%d\n"), str_key, teleinfo_config.param[index]);
    file.print (str_text);
  }

  // save contract parameters
  file.print (F ("\n[contract]\n"));

  sprintf_P (str_text, PSTR ("%s=%u\n"), PSTR (CMND_TIC_CONTRACT_INDEX), teleinfo_contract.index);
  file.print (str_text);

  sprintf_P (str_text, PSTR ("%s=%s\n"), PSTR (CMND_TIC_CONTRACT_NAME), teleinfo_contract.str_code);
  file.print (str_text);

  // loop thru periods
  for (index = 0; index < TIC_PERIOD_MAX; index ++)
    if (TeleinfoContractPeriod2String (str_key, sizeof (str_key), index))
    {
      sprintf_P (str_text, PSTR ("%s=%s\n"), PSTR (CMND_TIC_CONTRACT_PERIOD), str_key);
      file.print (str_text);
    }

  file.close ();
# endif     // USE_UFILESYS

  // log
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Configuration saved"));
}

void TeleinfoContractReset ()
{
  uint8_t index;

  // init contract data
  teleinfo_contract.index       = 0;
  teleinfo_contract.period_qty  = 0;
  teleinfo_contract.period_idx  = UINT8_MAX;
  teleinfo_contract.str_code[0] = 0;

  // init periods
  for (index = 0; index < TIC_PERIOD_MAX; index ++) teleinfo_contract.arr_period[index].valid = 0;

  // init conso data
  teleinfo_conso.total_wh = 0;
  for (index = 0; index < TIC_PERIOD_MAX; index ++) teleinfo_conso.index_wh[index] = 0;

  // init prod data
  teleinfo_prod.total_wh = 0;
}

// calculate line checksum
char TeleinfoCalculateChecksum (const char* pstr_line, char* pstr_etiquette, const size_t size_etiquette, char* pstr_donnee, const size_t size_donnee) 
{
  bool     prev_space;
  uint8_t  line_checksum, given_checksum;
  uint32_t index;
  size_t   size_line, size_checksum;
  char     str_line[TIC_LINE_SIZE];
  char     *pstr_token, *pstr_key, *pstr_data;

  // check parameters
  if ((pstr_line == nullptr) || (pstr_etiquette == nullptr) || (pstr_donnee == nullptr)) return 0;

  // init result
  str_line[0]       = 0;
  pstr_etiquette[0] = 0;
  pstr_donnee[0]    = 0;
  pstr_key = pstr_data = nullptr;

  // if line is less than 5 char, no handling
  size_line = strlen (pstr_line);
  if ((size_line < 5) || (size_line > TIC_LINE_SIZE)) return 0;
  
  // get given checksum
  size_checksum  = size_line - 1;
  given_checksum = (uint8_t)pstr_line[size_checksum];

  // adjust checksum calculation according to mode
  if (teleinfo_meter.sep_line == ' ') size_checksum = size_checksum - 1;

  // loop to calculate checksum, keeping 6 lower bits and adding Ox20 and compare to given checksum
  line_checksum = 0;
  for (index = 0; index < size_checksum; index ++) line_checksum += (uint8_t)pstr_line[index];
  line_checksum = (line_checksum & 0x3F) + 0x20;

  // if checksum difference
  if (line_checksum != given_checksum)
  {
    // declare error
    line_checksum = 0;
    teleinfo_message.error = 1;

    // if network is fully up, account error
    if (MqttIsConnected ())
    {
      teleinfo_meter.nb_error++;
      AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: Error [%s]"), pstr_line);
    }
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
    teleinfo_message.error = 1;

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
  if (teleinfo_conso.total_wh == 0) teleinfo_conso.total_wh = total;

  // if needed, update midnight value
  if ((teleinfo_conso.midnight_wh == 0) && (teleinfo_conso.total_wh > 0)) teleinfo_conso.midnight_wh = teleinfo_conso.total_wh - (long long)teleinfo_config.param[TIC_CONFIG_TODAY_CONSO];

  // if no increment, return
  if (total == teleinfo_conso.total_wh) return;

  // if total has decreased, total is abnormal
  if (total < teleinfo_conso.total_wh) abnormal = true;

  // else if total is high enougth and has increased more than 1%, total is abnormal
  else if ((total > 1000) && (total > teleinfo_conso.total_wh + (teleinfo_conso.total_wh / 100))) abnormal = true;

  // if value is abnormal
  if (abnormal)
  {
    teleinfo_conso.total_wh   = 0;
    teleinfo_conso.delta_mwh  = 0;
    teleinfo_conso.delta_mvah = 0;
  }

  // else update counters
  else
  {
    teleinfo_conso.delta_mwh += 1000 * (long)(total - teleinfo_conso.total_wh);
    teleinfo_conso.total_wh   = total;
    if (teleinfo_conso.midnight_wh > 0) teleinfo_conso.today_wh = (long)(teleinfo_conso.total_wh - teleinfo_conso.midnight_wh);
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
  if ((teleinfo_prod.midnight_wh == 0) && (teleinfo_prod.total_wh > 0)) teleinfo_prod.midnight_wh = teleinfo_prod.total_wh - (long long)teleinfo_config.param[TIC_CONFIG_TODAY_PROD];

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
    if (teleinfo_prod.midnight_wh > 0) teleinfo_prod.today_wh = (long)(teleinfo_prod.total_wh - teleinfo_prod.midnight_wh);
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
  if (index >= TIC_PERIOD_MAX) return;

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

  // if needed, update number of periods
  if ((value > 0) && (index >= teleinfo_contract.period_qty)) teleinfo_contract.period_qty = index + 1;
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
  if (value > TIC_VOLTAGE_MAXIMUM) return;

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
    if (TeleinfoDriverMeterReady ()) teleinfo_meter.json.data = 1;
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Preavis reset [%u->%u]"), teleinfo_meter.preavis.level, TIC_PREAVIS_NONE);

    // reset preavis
    teleinfo_meter.preavis.level = TIC_PREAVIS_NONE;
    teleinfo_meter.preavis.timeout = UINT32_MAX;
    teleinfo_meter.preavis.str_label[0] = 0;
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
      if (TeleinfoDriverMeterReady ()) teleinfo_meter.json.data = 1;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Preavis updated to %s [%u->%u]"), pstr_label, teleinfo_meter.preavis.level, level);

      // update data
      teleinfo_meter.preavis.level = level;
      strlcpy (teleinfo_meter.preavis.str_label, pstr_label, sizeof (teleinfo_meter.preavis.str_label));
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
  if (TeleinfoDriverMeterReady () && (teleinfo_meter.flag.overvolt != signal)) teleinfo_meter.json.data = 1;
  teleinfo_meter.flag.overvolt = signal;

  // get phase overload
  calc   = (uint32_t)value >> 7;
  signal = (uint8_t)calc & 0x01;
  if (TeleinfoDriverMeterReady () && (teleinfo_meter.flag.overload != signal)) teleinfo_meter.json.data = 1;
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
bool TeleinfoContractUpdate ()
{
  bool    is_kw;
  bool    result = true;
  uint8_t period;
  int     index;
//  char    str_contract[32];
  char    str_text[64];

  // if time not started, ignore
  if (!RtcTime.valid) result = false;

  // if message contract code not defined, ignore
  else if (strlen (teleinfo_message.str_contract) == 0) result = false;

  // else if contract code and message contract identical, nothing to do 
  else if (strcmp (teleinfo_contract.str_code, teleinfo_message.str_contract) == 0) teleinfo_contract.changed = 0;

  // if contract code defined and different than message contract, contract has changed
  else if (strlen (teleinfo_contract.str_code) > 0)
  {
    teleinfo_contract.changed = 1;
    result = false;
  }

  // else look for new contract
  else
  {
    // handle specificity of Historique Tempo where last char is dynamic (BBRx)
//    strlcpy (str_contract, teleinfo_message.str_contract, 4);
//    if (strcmp_P (str_contract, PSTR ("BBR")) != 0) strlcpy (str_contract, teleinfo_message.str_contract, sizeof (str_contract));

    // handle specificity of historic BASE contract
//    if ((strcmp_P (str_contract, PSTR ("BASE")) == 0) && (teleinfo_contract.mode == TIC_MODE_HISTORIC)) strcpy_P (str_contract, PSTR ("TH.."));

    // look for contract
    index = GetCommandCode (str_text, sizeof (str_text), teleinfo_message.str_contract, kTicContractCode);
    if ((index == -1) || (index >= TIC_C_MAX)) index = TIC_C_UNKNOWN;

    // set contract index and code
    teleinfo_contract.index = (uint8_t)index;
    strcpy (teleinfo_contract.str_code, teleinfo_message.str_contract);

    // init current period
    teleinfo_contract.period_idx = UINT8_MAX;
    teleinfo_contract.str_period[0] = 0;

    // load contract periods
    teleinfo_contract.period_qty = TeleinfoContractString2Period (arr_kTicPeriod[index]);

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Nouveau contrat %s, %d périodes connues"), teleinfo_message.str_contract, teleinfo_contract.period_qty);

    // save current contract
    TeleinfoConfigSave ();
  }

  // if needed, detect contract unit
  if ((teleinfo_contract.unit == TIC_UNIT_NONE) && (teleinfo_contract.mode != TIC_MODE_UNKNOWN))
  {
    // detect if contract unit is in kw
    if (teleinfo_contract.mode == TIC_MODE_EMERAUDE) is_kw = true;
      else if (teleinfo_contract.index == TIC_C_PME_TVA5_BASE) is_kw = true;
      else if (teleinfo_contract.index == TIC_C_PME_TVA8_BASE) is_kw = true;
      else is_kw = false;

    // set contract unit
    if (is_kw) teleinfo_contract.unit = TIC_UNIT_KW;
      else teleinfo_contract.unit = TIC_UNIT_KVA;
    if (teleinfo_contract.unit == TIC_UNIT_KW) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Contrat en kW"));
      else AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Contrat en kVA"));
  }

  return result;
}

// update contract period
void TeleinfoPeriodUpdate ()
{
  bool    new_period = false;
  int     index, period;
  uint8_t count, hchp, level;

  // if time not started, ignore
  if (!RtcTime.valid) return;

  // if message period is undefined, ignore
  if (strlen (teleinfo_message.str_period) == 0) return;

  // if contract period and message period are identical, ignore
  if (strcmp (teleinfo_contract.str_period, teleinfo_message.str_period) == 0) return;
  
  // if message period index is defined, check for an unknown period index
  if (teleinfo_message.period < TIC_PERIOD_MAX) new_period = (teleinfo_contract.arr_period[teleinfo_message.period].valid == 0) ;

  // in historic contract, index is not given, look for period index in period code list
  if (teleinfo_message.period == UINT8_MAX)
  {
    index = -1;
    for (period = 0; period < TIC_PERIOD_MAX; period ++) if (teleinfo_contract.arr_period[period].str_code == teleinfo_message.str_period) index = period;
    if (index != -1) teleinfo_message.period = (uint8_t)index;
  }

  // if message period index still not defined, ignore
  if (teleinfo_message.period == UINT8_MAX) return;

  // update contract current period
  teleinfo_contract.period_idx = teleinfo_message.period;
  strcpy (teleinfo_contract.str_period, teleinfo_message.str_period);

  // if a new period is detected, update contract period
  if (new_period)
  {
    // update contract period validity, code and label
    teleinfo_contract.arr_period[teleinfo_message.period].valid     = 1;
    teleinfo_contract.arr_period[teleinfo_message.period].str_code  = teleinfo_message.str_period;
    teleinfo_contract.arr_period[teleinfo_message.period].str_label = teleinfo_message.str_period;

    // detect and update contract period HC/HP
    hchp  = 1;
    if (strstr_P (teleinfo_message.str_period, PSTR ("HC")) != nullptr) hchp = 0;
      else if (strstr_P (teleinfo_message.str_period, PSTR ("CREUSE")) != nullptr) hchp = 0;
    teleinfo_contract.arr_period[teleinfo_message.period].hchp = hchp;

    // detect and update contract period level
    level = 1;
    if (strstr_P (teleinfo_message.str_period, PSTR ("JW")) != nullptr) level = 2;
      else if (strstr_P (teleinfo_message.str_period, PSTR ("BLANC")) != nullptr) level = 2;
      else if (strstr_P (teleinfo_message.str_period, PSTR ("JR")) != nullptr) level = 3;
      else if (strstr_P (teleinfo_message.str_period, PSTR ("ROUGE")) != nullptr) level = 3;
      else if (strstr_P (teleinfo_message.str_period, PSTR ("PM")) != nullptr) level = 3;
      else if (strstr_P (teleinfo_message.str_period, PSTR ("POINTE")) != nullptr) level = 3;
      else if (strcmp_P (teleinfo_message.str_period, PSTR ("P")) == 0) level = 3;
    teleinfo_contract.arr_period[teleinfo_message.period].level = level;

    // update contract period quantity
    if (teleinfo_contract.period_qty < teleinfo_message.period + 1) teleinfo_contract.period_qty = teleinfo_message.period + 1;

    // publish updated calendar
    if (teleinfo_config.calendar) teleinfo_meter.json.data = 1;

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Nouvelle période %s (index %u, level %u, hchp %u), %u périodes connues"), teleinfo_message.str_period, teleinfo_message.period + 1, level, hchp, teleinfo_contract.period_qty);

    // save new period
    TeleinfoConfigSave ();
  }

  // else log period change
  else 
  {
    level = teleinfo_contract.arr_period[teleinfo_contract.period_idx].level;
    hchp  = teleinfo_contract.arr_period[teleinfo_contract.period_idx].hchp;
    count = teleinfo_contract.period_idx + 1;
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Changement de période %s (index %u, level %u, hchp %u)"), teleinfo_contract.str_period, count, level, hchp);
  }
}

// update periods from a description string (return quantity of periods)
uint8_t TeleinfoContractString2Period (const char *pstr_description)
{
  bool    extract = true;
  uint8_t result  = 0;
  uint8_t period;
  int     index = 0;
  char    str_period[4];
  char    str_level[4];
  char    str_hchp[4];
  char    str_code[16];
  char    str_label[32];
  
  // check parameter
  if (pstr_description == nullptr) extract = false;
  else if (strlen_P (pstr_description) == 0) extract = false;

  // loop to extract data
  while (extract)
  {
    // init
    period = UINT8_MAX;
    str_period[0] = 0;
    str_level[0]  = 0;
    str_hchp[0]   = 0;
    str_code[0]   = 0;
    str_label[0]  = 0;
    
    // extract data
    GetTextIndexed (str_period, sizeof (str_period), index++, pstr_description);
    if (strlen (str_period) > 0) period = (uint8_t)atoi (str_period) - 1;
    if (period < TIC_PERIOD_MAX) GetTextIndexed (str_level, sizeof (str_level), index++, pstr_description);
    if (strlen (str_level) > 0)  GetTextIndexed (str_hchp,  sizeof (str_hchp),  index++, pstr_description);
    if (strlen (str_hchp)  > 0)  GetTextIndexed (str_code,  sizeof (str_code),  index++, pstr_description);
    if (strlen (str_code)  > 0)  GetTextIndexed (str_label, sizeof (str_label), index++, pstr_description);

    // if data are available
    if (strlen (str_code) > 0)
    {
      // load period data
      teleinfo_contract.arr_period[period].valid     = 1;
      teleinfo_contract.arr_period[period].level     = (uint8_t)atoi (str_level);
      teleinfo_contract.arr_period[period].hchp      = (uint8_t)atoi (str_hchp);
      teleinfo_contract.arr_period[period].str_code  = str_code;
      teleinfo_contract.arr_period[period].str_label = str_label;

      // update number of periods
      period++;
      result = max (result, period);
    }
    else extract = false;
  }

  return result;
}

bool TeleinfoContractPeriod2String (char *pstr_description, const size_t size_description, const uint8_t period)
{
  bool result = true;

  // check parameter
  if (pstr_description == nullptr) result = false;
  else if (period >= TIC_PERIOD_MAX) result = false;
  else if (teleinfo_contract.arr_period[period].valid == 0) result = false;
  
  // generate period description string
  if (result) snprintf_P (pstr_description, size_description, PSTR ("%u|%u|%u|%s|%s"), period + 1, teleinfo_contract.arr_period[period].level, teleinfo_contract.arr_period[period].hchp, teleinfo_contract.arr_period[period].str_code.c_str (), teleinfo_contract.arr_period[period].str_label.c_str ());

  return result;
}

// set contract max power par phase
void TeleinfoContractPowerUpdate (const char *pstr_donnee)
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
  if (value > 0)
  {
    // adjust value in kilo and per phase
    if (value < 1000) value = value * 1000;
    if (teleinfo_contract.phase > 1) value = value / teleinfo_contract.phase;

    // update boundaries
    teleinfo_contract.ssousc = value;                
    teleinfo_contract.isousc = value / TIC_VOLTAGE_REF;
  }
}

// get contract name
void TeleinfoContractGetName (char* pstr_name, size_t size_name)
{
  // check parameter
  if (pstr_name == nullptr) return;
  if (size_name == 0) return;

  // get contract name
  if ((teleinfo_contract.index == TIC_C_UNKNOWN) || (teleinfo_contract.index == UINT8_MAX)) strlcpy (pstr_name, teleinfo_contract.str_code, size_name);
    else GetTextIndexed (pstr_name, size_name, teleinfo_contract.index, kTicContractName);
}

// get period code
void TeleinfoPeriodGetCode (char* pstr_code, size_t size_code) { TeleinfoPeriodGetCode (pstr_code, size_code, teleinfo_contract.period_idx); }
void TeleinfoPeriodGetCode (char* pstr_code, size_t size_code, const uint8_t period)
{
  // check parameter
  if (pstr_code == nullptr) return;
  if (size_code == 0) return;
  if (period >= TIC_PERIOD_MAX) return;

  // look for period code according to contract
  if (teleinfo_contract.period_qty < period + 1) pstr_code[0] = 0;
    else if (teleinfo_contract.arr_period[period].valid == 0) sprintf_P (pstr_code, PSTR ("PERIOD%u"), period + 1);
    else strlcpy (pstr_code, teleinfo_contract.arr_period[period].str_code.c_str (), size_code);
}

// get period name
void TeleinfoPeriodGetLabel (char* pstr_name, const size_t size_name) { TeleinfoPeriodGetLabel (pstr_name, size_name, teleinfo_contract.period_idx); }
void TeleinfoPeriodGetLabel (char* pstr_name, const size_t size_name, const uint8_t period)
{
  // check parameter
  if (pstr_name == nullptr) return;
  if (size_name == 0) return;
  if (period >= TIC_PERIOD_MAX) return;

  // look for period label according to contract
  if (teleinfo_contract.period_qty < period + 1) pstr_name[0] = 0;
    else if (teleinfo_contract.arr_period[period].valid == 0) sprintf_P (pstr_name, PSTR ("Période %u"), period + 1);
    else strlcpy (pstr_name, teleinfo_contract.arr_period[period].str_label.c_str (), size_name);
}

// get period level
uint8_t TeleinfoPeriodGetLevel () { return TeleinfoPeriodGetLevel (teleinfo_contract.period_idx); }
uint8_t TeleinfoPeriodGetLevel (const uint8_t period)
{
  uint8_t value;

  if (period >= TIC_PERIOD_MAX) value = 0;
    else if (teleinfo_contract.period_qty < period + 1) value = 0;
    else if (teleinfo_contract.arr_period[period].valid == 0) value = 0;
    else value = teleinfo_contract.arr_period[period].level;

  return value;
}

// get period HP status
uint8_t TeleinfoPeriodGetHP () { return TeleinfoPeriodGetHP (teleinfo_contract.period_idx); }
uint8_t TeleinfoPeriodGetHP (const uint8_t period)
{
  uint8_t value;

  if (period >= TIC_PERIOD_MAX) value = 1;
    else if (teleinfo_contract.period_qty < period + 1) value = 1;
    else if (teleinfo_contract.arr_period[period].valid == 0) value = 1;
    else value = teleinfo_contract.arr_period[period].hchp;

  return value;
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
  if (teleinfo_config.calendar && TeleinfoDriverMeterReady ()) teleinfo_meter.json.data = 1;
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
  if (teleinfo_config.calendar && TeleinfoDriverMeterReady ()) teleinfo_meter.json.data = 1;
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

  // if apparent power increased more than 5% of the contract power, average on 2 samples to update cosphi very fast
  if (delta >= contract5percent) for (index = 1; index < TIC_COSPHI_SAMPLE; index++) struct_cosphi.arr_value[index] = LONG_MAX;

  // else if apparent power increased more than 2% of the contract power, average on 4 samples to update cosphi quite fast
  else if (delta >= contract2percent) for (index = 3; index < TIC_COSPHI_SAMPLE; index++) struct_cosphi.arr_value[index] = LONG_MAX;

  // else if apparent power decreased more than 5% of the contract power, average on 2 samples to update cosphi very fast
  else if (delta <= -contract5percent) for (index = 1; index < TIC_COSPHI_SAMPLE; index++) struct_cosphi.arr_value[index] = LONG_MAX;

  // if apparent power increased more than 5% of the contract power, cancel current cosphi average calculation to avoid spikes
  if (delta >= contract5percent) return;

  // shift values and update first one
  for (index = TIC_COSPHI_SAMPLE - 1; index > 0; index--) struct_cosphi.arr_value[index] = struct_cosphi.arr_value[index - 1];
  struct_cosphi.arr_value[0] = cosphi;

  // calculate cosphi average
  result = 0;
  sample = 0;
  for (index = 0; index < TIC_COSPHI_SAMPLE; index++)
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
  if (ufs_type) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Partition mounted"));
  else AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Partition could not be mounted"));
#endif  // USE_UFILESYS

  // remove serial logging
  TasmotaGlobal.seriallog_timer = 0;

  // remove standard energy display
  bitWrite (Settings->sensors[1][0], 3, 0);

  // set web refresh to 3 sec (linky ready)
  Settings->web_refresh = 3000;

  // set measure resolution
  Settings->flag2.current_resolution = 1;

  // disable fast cycle power recovery (SetOption65)
  Settings->flag3.fast_power_cycle_disable = true;

  // init hardware energy counter
  Settings->flag3.hardware_energy_total = true;

  // disable automatic data saving
  Settings->save_data = 0;
  TasmotaGlobal.save_data_counter = 0;

  // set default energy parameters
  Energy->voltage_available = true;
  Energy->current_available = true;

  // init first power delta detection value
  if (Settings->energy_power_delta[0] == 0) Settings->energy_power_delta[0] = 100;

  // meter : separator and JSON flags
  teleinfo_meter.sep_line  = ' ';
  teleinfo_meter.json.tic  = 0;
  teleinfo_meter.json.data = 0;

  // meter : stge data
  teleinfo_meter.flag.overload   = 0;
  teleinfo_meter.flag.overvolt   = 0;
  teleinfo_meter.preavis.level   = TIC_PREAVIS_NONE;
  teleinfo_meter.preavis.timeout = UINT32_MAX;
  teleinfo_meter.preavis.str_label[0] = 0;

  // meter : pointe period
  teleinfo_meter.pointe.hour        = UINT8_MAX;
  teleinfo_meter.pointe.day         = UINT8_MAX;
  teleinfo_meter.pointe.start       = UINT32_MAX;
  teleinfo_meter.pointe.crc_start   = 0;
  teleinfo_meter.pointe.crc_profile = 0;

  // meter : calendar days
  for (index = TIC_DAY_YESTERDAY; index < TIC_DAY_MAX; index ++)
  {
    teleinfo_meter.arr_day[index].valid = 0;
    for (slot = 0; slot < 24; slot ++) teleinfo_meter.arr_day[index].arr_period[slot] = TIC_LEVEL_NONE;
  }

  // message data
  teleinfo_message.timestamp = UINT32_MAX;
  teleinfo_message.str_line[0] = 0;
  for (index = 0; index < TIC_LINE_QTY; index ++) 
  {
    // init current message data
    teleinfo_message.arr_line[index].str_etiquette[0] = 0;
    teleinfo_message.arr_line[index].str_donnee[0] = 0;
    teleinfo_message.arr_line[index].checksum = 0;

    // init last message data
    teleinfo_message.arr_last[index].str_etiquette[0] = 0;
    teleinfo_message.arr_last[index].str_donnee[0] = 0;
    teleinfo_message.arr_last[index].checksum = 0;
  }

  // conso data
  teleinfo_conso.total_wh = 0;
  for (index = 0; index < TIC_PERIOD_MAX; index ++) teleinfo_conso.index_wh[index] = 0;
  teleinfo_conso.cosphi.nb_measure   = 0;
  teleinfo_conso.cosphi.last_papp    = 0;
  teleinfo_conso.cosphi.value        = TIC_COSPHI_DEFAULT;     
  teleinfo_conso.cosphi.arr_value[0] = TIC_COSPHI_DEFAULT;
  for (index = 1; index < TIC_COSPHI_SAMPLE; index++) teleinfo_conso.cosphi.arr_value[index] = LONG_MAX;

  // prod data
  teleinfo_prod.total_wh = 0;
  teleinfo_prod.cosphi.nb_measure   = 0;
  teleinfo_prod.cosphi.last_papp    = 0;
  teleinfo_prod.cosphi.value        = TIC_COSPHI_DEFAULT;
  teleinfo_prod.cosphi.arr_value[0] = TIC_COSPHI_DEFAULT;
  for (index = 1; index < TIC_COSPHI_SAMPLE; index++) teleinfo_prod.cosphi.arr_value[index]  = LONG_MAX;

  // conso data per phase
  for (phase = 0; phase < ENERGY_MAX_PHASES; phase++)
  {
    // init energy data 
    Energy->voltage[phase]        = TIC_VOLTAGE;
    Energy->current[phase]        = 0;
    Energy->active_power[phase]   = 0;
    Energy->apparent_power[phase] = 0;

    // init conso data
    teleinfo_conso.phase[phase].volt_set  = false;
    teleinfo_conso.phase[phase].voltage   = TIC_VOLTAGE;
    teleinfo_conso.phase[phase].current   = 0;
    teleinfo_conso.phase[phase].papp      = 0;
    teleinfo_conso.phase[phase].sinsts    = 0;
    teleinfo_conso.phase[phase].pact      = 0;
    teleinfo_conso.phase[phase].preact    = 0;
    teleinfo_conso.phase[phase].papp_last = 0;
    teleinfo_conso.phase[phase].cosphi    = 1000;
  }

  // load configuration
  TeleinfoConfigLoad ();

  // log default method & help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run energyconfig to get help on all Teleinfo commands"));
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

  // reset error flag, line index and current line content
  teleinfo_message.error = 0;
  teleinfo_message.line_idx = 0;
  teleinfo_message.injection = 0;
  teleinfo_message.str_line[0] = 0;

  // init message period and contract to current one
  teleinfo_message.period = UINT8_MAX;
  teleinfo_message.str_period[0] = 0;
  strlcpy (teleinfo_message.str_contract, teleinfo_contract.str_code, sizeof (teleinfo_message.str_contract));

  // reset voltage flags
  for (phase = 0; phase < teleinfo_contract.phase; phase++) teleinfo_conso.phase[phase].volt_set = false;
}

// Conso : calcultate Cosphi and Active Power 
//   from Instant Apparent Power (VA) and Active Power counters (Wh)
void TeleinfoConsoCalculateActivePower_VA_Wh ()
{
  uint8_t phase;
  long    sinsts, current, cosphi;

  // calculate total current
  current = 0;
  for (phase = 0; phase < teleinfo_contract.phase; phase++) current += teleinfo_conso.phase[phase].current;

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
    else if (current > 0) teleinfo_conso.phase[phase].papp = teleinfo_conso.papp * teleinfo_conso.phase[phase].current / current;
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
      if (cosphi < 50) cosphi = 0;
      TeleinfoUpdateCosphi (cosphi, teleinfo_conso.cosphi, teleinfo_conso.papp);
    }

    // else try to forecast cosphi evolution in case of very low power
    else
    {
      cosphi = 1000 * 100 * 1000 / teleinfo_conso.delta_mvah;
      if (cosphi < 50) cosphi = 0;
      if (cosphi <= teleinfo_conso.cosphi.value) TeleinfoUpdateCosphi (cosphi, teleinfo_conso.cosphi, teleinfo_conso.papp);
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
}

// Conso : calcultate Cosphi, Apparent Power (VA) and Active Power (W)
//   from Apparent Power (VAh) and Active Power (Wh) increments
void TeleinfoConsoCalculateApparentandActivePower_VAh_Wh ()
{
  uint8_t phase;
  long    delay, increment, cosphi;

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
}

// Conso : calcultate Cosphi, Apparent Power (VA) and Active Power (W)
//   from Active Power (Wh) and Reactive Power (Varh) increments
void TeleinfoConsoCalculateApparentandActivePower_VAh_Varh ()
{
  uint8_t   phase;
  long      delay, increment, cosphi;
  long long llpact, llpreact, llpapp;

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
      // calculatation
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
}

// Prod : calcultate Cosphi and Active Power (W)
//   from Instant Apparent Power (VA) and Active Power counters (Wh)
void TeleinfoProdCalculateActivePower_VA_Wh ()
{
  long cosphi;
  
  // add apparent power increment according to message time window
  teleinfo_prod.delta_mvah += teleinfo_prod.papp * teleinfo_message.duration / 36;

  // if active power has increased 
  if (teleinfo_prod.delta_mvah > 0)
  {
    // if global counter increased, calculate new cosphi
    if (teleinfo_prod.delta_mwh > 0)
    {
      cosphi = 1000 * 100 * teleinfo_prod.delta_mwh / teleinfo_prod.delta_mvah;
      if (cosphi < 50) cosphi = 0;
      TeleinfoUpdateCosphi (cosphi, teleinfo_prod.cosphi, teleinfo_prod.papp);
    }

    // else try to forecast cosphi drop in case of very low active power production (photovoltaic router ...)
    else
    {
      cosphi = 1000 * 100 * 1000 / teleinfo_prod.delta_mvah;
      if (cosphi < 50) cosphi = 0;
      if (cosphi <= teleinfo_prod.cosphi.value) TeleinfoUpdateCosphi (cosphi, teleinfo_prod.cosphi, teleinfo_prod.papp);
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

// set environment for end of message
void TeleinfoReceptionMessageStop ()
{
  bool     delta_va = false;
  int      index;
  uint8_t  phase;
  uint32_t timestamp;
  long     value;
  
  // get current timestamp
  timestamp = millis ();

  // set next stage
  teleinfo_meter.reception = TIC_RECEPTION_NONE;

  // handle contract type and period update
  if (TeleinfoContractUpdate ()) TeleinfoPeriodUpdate ();

  // calculate delay between 2 messages
  if (teleinfo_message.timestamp != UINT32_MAX) teleinfo_message.duration = (long)(timestamp - teleinfo_message.timestamp);
  teleinfo_message.timestamp = timestamp;

  // increment message counter and declare meter ready after 2nd message
  teleinfo_meter.nb_message++;

  // loop thru message data
  for (index = 0; index < TIC_LINE_QTY; index++)
  {
    // save data from last message
    if (index < teleinfo_message.line_idx)
    {
      strcpy (teleinfo_message.arr_last[index].str_etiquette, teleinfo_message.arr_line[index].str_etiquette);
      strcpy (teleinfo_message.arr_last[index].str_donnee, teleinfo_message.arr_line[index].str_donnee);
      teleinfo_message.arr_last[index].checksum = teleinfo_message.arr_line[index].checksum;
    }
    else
    {
      teleinfo_message.arr_last[index].str_etiquette[0] = 0;
      teleinfo_message.arr_last[index].str_donnee[0] = 0;
      teleinfo_message.arr_last[index].checksum = 0;
    }

    // init next message data
    teleinfo_message.arr_line[index].str_etiquette[0] = 0;
    teleinfo_message.arr_line[index].str_donnee[0] = 0;
    teleinfo_message.arr_line[index].checksum = 0;
  }

  // save number of lines and reset index
  teleinfo_message.line_last   = teleinfo_message.line_idx;
  teleinfo_message.line_max    = max (teleinfo_message.line_last, teleinfo_message.line_max);
  teleinfo_message.line_idx    = 0;
  teleinfo_message.str_line[0] = 0;
  
  // if needed, calculate max contract power from max phase current
  if ((teleinfo_contract.ssousc == 0) && (teleinfo_contract.isousc != 0)) teleinfo_contract.ssousc = teleinfo_contract.isousc * TIC_VOLTAGE_REF;

  // ----------------------
  // counter global indexes

  // if needed, adjust number of phases
  if (Energy->phase_count < teleinfo_contract.phase) Energy->phase_count = teleinfo_contract.phase;

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
      TeleinfoConsoCalculateActivePower_VA_Wh ();
    break;

    // PME/PMI : use apparent power and active power increments, timestamp from published time
    // ---------------------------------------------------------------------------------------
    case TIC_MODE_PMEPMI:
      TeleinfoConsoCalculateApparentandActivePower_VAh_Wh ();
      break;

    // Emeraude : use active power and reactive power increments, timestamp from published time
    // ----------------------------------------------------------------------------------------
    case TIC_MODE_EMERAUDE:
      TeleinfoConsoCalculateApparentandActivePower_VAh_Varh ();
      break;
  }

  // ---------------------------------------
  //    calculate production active power 
  // ---------------------------------------

  if (teleinfo_prod.total_wh > 0) TeleinfoProdCalculateActivePower_VA_Wh ();
  
  // -----------------------------
  //    update publication flags
  // -----------------------------

  if (TeleinfoDriverMeterReady ())
  {
    // loop thru phases to detect overload
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
      if (teleinfo_conso.phase[phase].papp > teleinfo_contract.ssousc * (long)teleinfo_config.percent / 100) teleinfo_meter.json.data = 1;

    // if publication every new message
    if (teleinfo_config.policy == TIC_POLICY_MESSAGE)
    {
      // set publication flags
      if (teleinfo_config.meter || teleinfo_config.calendar || teleinfo_config.relay) teleinfo_meter.json.data = 1;
      if (teleinfo_config.tic) teleinfo_meter.json.tic = 1;
    }

    // else if % power change detected
    else if ((teleinfo_config.policy == TIC_POLICY_DELTA) && (Settings->energy_power_delta[0] > 0))
    {
      // get delta power for dynamic publication
      value = (long)Settings->energy_power_delta[0];

      // loop thru phase to detect % power change (should be > to handle 0 conso)
      for (phase = 0; phase < teleinfo_contract.phase; phase++) 
        if (abs (teleinfo_conso.phase[phase].papp_last - teleinfo_conso.phase[phase].papp) >= value) delta_va = true;

      // detect % power change on production (should be > to handle 0 prod)
      if (abs (teleinfo_prod.papp_last - teleinfo_prod.papp) >= value) delta_va = true;

      // if energy has changed more than the trigger, publish
      if (delta_va)
      {
        // update phases reference
        for (phase = 0; phase < teleinfo_contract.phase; phase++) teleinfo_conso.phase[phase].papp_last = teleinfo_conso.phase[phase].papp;
        teleinfo_prod.papp_last = teleinfo_prod.papp;

        // if deepsleep not enabled, set publication flags
        if (Settings->deepsleep == 0)
        {
          if (teleinfo_config.meter || teleinfo_config.calendar || teleinfo_config.relay) teleinfo_meter.json.data = 1;
          if (teleinfo_config.tic) teleinfo_meter.json.tic = 1;
        } 
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
    Energy->voltage[phase]          = (float)teleinfo_conso.phase[phase].voltage;
    Energy->current[phase]          = (float)teleinfo_conso.phase[phase].current / 1000;
    Energy->apparent_power[phase]   = (float)teleinfo_conso.phase[phase].papp;
    Energy->active_power[phase]     = (float)teleinfo_conso.phase[phase].pact;
    if (teleinfo_conso.phase[phase].preact != 0)
      Energy->reactive_power[phase] = (float)teleinfo_conso.phase[phase].preact;
  }

  // -------------------
  //  update LED state

#ifdef USE_LIGHT
  // set last LED timestamp and status
  if (teleinfo_message.error == 0) TeleinfoLedSetStatus (TIC_LED_STEP_OK);
    else TeleinfoLedSetStatus (TIC_LED_STEP_ERR);
#endif    // USE_LIGHT
}

void TeleinfoReceptionLineStart ()
{
  // set next stage
  teleinfo_meter.reception = TIC_RECEPTION_LINE;

  // reset line content
  teleinfo_message.str_line[0] = 0;
}

void TeleinfoReceptionLineStop ()
{
  uint8_t phase, relay;
  int     index = -1;
  long    value;
  char    checksum;
  char*   pstr_match;
  char    str_etiquette[TIC_KEY_MAX];
  char    str_donnee[TIC_DATA_MAX];
  char    str_text[TIC_DATA_MAX];

  // set next stage
  teleinfo_meter.reception = TIC_RECEPTION_MESSAGE;

  // increment line counter
  teleinfo_meter.nb_line++;

  // if checksum is ok, handle the line
  checksum = TeleinfoCalculateChecksum (teleinfo_message.str_line, str_etiquette, sizeof (str_etiquette), str_donnee, sizeof (str_donnee));
  if (checksum != 0)
  {
    // select etiquette list according to contract
    if (teleinfo_contract.mode < TIC_MODE_MAX)
    {
      index = GetCommandCode (str_text, sizeof (str_text), str_etiquette, arr_kTicEtiquette[teleinfo_contract.mode]);
      if (index != -1) index += arrTicEtiquetteDelta[teleinfo_contract.mode];
      AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: %s = %d"), str_etiquette, index);
    }

    // update data according to etiquette
    switch (index)
    {
      //   Reference
      // ------------- 
      
      // identifiant compteur mode historique
      case TIC_UKN_ADCO:
        teleinfo_contract.mode  = TIC_MODE_HISTORIC;
        AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Mode %s"), GetTextIndexed (str_text, sizeof (str_text), TIC_MODE_HISTORIC, kTeleinfoModeName));
        teleinfo_contract.ident = atoll (str_donnee);
        AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Contrat n° %s"), str_donnee);
        break;
      // identifiant compteur mode standard
      case TIC_UKN_ADSC:
        teleinfo_contract.mode  = TIC_MODE_STANDARD;
        AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Mode %s"), GetTextIndexed (str_text, sizeof (str_text), TIC_MODE_STANDARD, kTeleinfoModeName));
        teleinfo_contract.ident = atoll (str_donnee);
        AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Contrat n° %s"), str_donnee);
        break;

      // identifiant compteur PME/PMI
      case TIC_UKN_ADS:
        teleinfo_contract.mode  = TIC_MODE_PMEPMI;
        AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Mode %s"), GetTextIndexed (str_text, sizeof (str_text), TIC_MODE_PMEPMI, kTeleinfoModeName));
        teleinfo_contract.ident = atoll (str_donnee);
        AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Contrat n° %s"), str_donnee);
        break;

      // contract compteur Emeraude
      case TIC_UKN_CONTRAT:
        teleinfo_contract.mode = TIC_MODE_EMERAUDE;
        AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Meter %s"), GetTextIndexed (str_text, sizeof (str_text), TIC_MODE_EMERAUDE, kTeleinfoModeName));
        strlcpy (teleinfo_message.str_contract, str_donnee, sizeof (teleinfo_message.str_contract));
        break;

      // contract compteur Jaune
      case TIC_UKN_JAUNE:
        teleinfo_contract.mode = TIC_MODE_JAUNE;
        AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Meter %s"), GetTextIndexed (str_text, sizeof (str_text), TIC_MODE_JAUNE, kTeleinfoModeName));
        break;

      //   Timestamp
      // -------------
      
      case TIC_STD_DATE:
      case TIC_PME_DATE:
      case TIC_EME_DATECOUR:
        teleinfo_conso.last_stamp = TeleinfoTimestampCalculate (str_donnee);
        break;

      //   Identifiant compteur
      // -----------------------
      case TIC_HIS_ADCO:
      case TIC_STD_ADSC:
      case TIC_PME_ADS:
        // if not set, set meter identifier
        if (teleinfo_contract.ident == 0) teleinfo_contract.ident = atoll (str_donnee);
        break;

      //   Contract period
      // -------------------

      case TIC_HIS_OPTARIF:
        // handle specificity of Historique Tempo where last char is dynamic (BBRx)
        strlcpy (teleinfo_message.str_contract, str_donnee, 4);
        if (strcmp_P (teleinfo_message.str_contract, PSTR ("BBR")) != 0) strlcpy (teleinfo_message.str_contract, str_donnee, sizeof (teleinfo_message.str_contract));
        break;

      case TIC_STD_NGTF:
      case TIC_PME_MESURES1:
      case TIC_EME_CONTRAT:
        strlcpy (teleinfo_message.str_contract, str_donnee, sizeof (teleinfo_message.str_contract));
        break;

      //   Period
      // -----------

      // switching to production data
      case TIC_EME_APPLI:
        teleinfo_message.injection = 1;
        break;

      // period name
      case TIC_HIS_PTEC:
      case TIC_STD_LTARF:
      case TIC_PME_PTCOUR1:
      case TIC_EME_PTCOUR:
        strlcpy (teleinfo_message.str_period, str_donnee, sizeof (teleinfo_message.str_period));
        break;

      // period index
      case TIC_STD_NTARF:
        teleinfo_message.period = (uint8_t)(atoi (str_donnee) - 1);
        break;

      //   Current
      // -----------

      case TIC_HIS_IINST:
      case TIC_HIS_IINST1:
      case TIC_STD_IRMS1:
        TeleinfoCurrentUpdate (str_donnee, 0);
        break;

      case TIC_HIS_IINST2:
      case TIC_STD_IRMS2:
        TeleinfoCurrentUpdate (str_donnee, 1);
        break;

      case TIC_HIS_IINST3:
      case TIC_STD_IRMS3:
        TeleinfoCurrentUpdate (str_donnee, 2);
        teleinfo_contract.phase = 3; 
        break;

      //   Power
      // ---------

      // instant apparent power, 
      case TIC_HIS_PAPP:
      case TIC_STD_SINSTS:
        TeleinfoConsoApparentPowerUpdate (str_donnee);
        break;

      case TIC_STD_SINSTS1:
        TeleinfoConsoApparentPowerUpdate (str_donnee, 0);
        break;

      case TIC_STD_SINSTS2:
        TeleinfoConsoApparentPowerUpdate (str_donnee, 1);
        break;

      case TIC_STD_SINSTS3:
        TeleinfoConsoApparentPowerUpdate (str_donnee, 2);
        break;

      // if in prod mode, instant apparent power, 
      case TIC_STD_SINSTI:
        TeleinfoProdApparentPowerUpdate (str_donnee);
        break;

      // apparent power counter since last period
      case TIC_PME_EAPPS:
        strcpy (str_text, str_donnee);
        pstr_match = strchr (str_text, 'V');
        if (pstr_match != nullptr) *pstr_match = 0;
        teleinfo_conso.papp_now = atol (str_text);
        break;

      // active power counter since last period
      case TIC_PME_EAS:
      case TIC_EME_EA:
        strcpy (str_text, str_donnee);
        pstr_match = strchr (str_text, 'W');
        if (pstr_match != nullptr) *pstr_match = 0;
        teleinfo_conso.pact_now = atol (str_text);
        break;

      // reactive power counter since last period
      case TIC_EME_ERP:
        strcpy (str_text, str_donnee);
        pstr_match = strchr (str_text, 'v');
        if (pstr_match != nullptr) *pstr_match = 0;
        teleinfo_conso.preact_now = atol (str_text);
        break;

      //   Voltage
      // -----------

      // RMS voltage
      case TIC_STD_URMS1:
        TeleinfoVoltageUpdate (str_donnee, 0, true);
        break;

      case TIC_STD_URMS2:
        TeleinfoVoltageUpdate (str_donnee, 1, true);
        break;

      case TIC_STD_URMS3:
        TeleinfoVoltageUpdate (str_donnee, 2, true);
        teleinfo_contract.phase = 3; 
        break;

      // average voltage
      case TIC_STD_UMOY1:
        TeleinfoVoltageUpdate (str_donnee, 0, false);
        break;

      case TIC_STD_UMOY2:
        TeleinfoVoltageUpdate (str_donnee, 1, false);
        break;

      case TIC_STD_UMOY3:
        TeleinfoVoltageUpdate (str_donnee, 2, false);
        teleinfo_contract.phase = 3; 
        break;

      case TIC_EME_U10MN:         // for the last 10 mn
        for (phase = 0; phase < teleinfo_contract.phase; phase ++) TeleinfoVoltageUpdate (str_donnee, phase, false);
        break;

      //   Contract max values
      // -----------------------

      // Maximum Current
      case TIC_HIS_ISOUSC:
        value = atol (str_donnee);
        if ((value > 0) && (teleinfo_contract.isousc != value))
        {
          teleinfo_contract.isousc = value;
          teleinfo_contract.ssousc = teleinfo_contract.isousc * TIC_VOLTAGE_REF;
        }
        break;

      // Contract maximum Power
      case TIC_STD_PREF:
      case TIC_STD_PCOUP:
      case TIC_PME_PS:
        TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSP:
        if (teleinfo_contract.period_idx == 0) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSPM:
        if (teleinfo_contract.period_idx == 1) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHPH:
        if (teleinfo_contract.period_idx == 2) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHPD:
        if (teleinfo_contract.period_idx == 3) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHCH:
        if (teleinfo_contract.period_idx == 4) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHCD:
        if (teleinfo_contract.period_idx == 5) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHPE:
        if (teleinfo_contract.period_idx == 6) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHCE:
        if (teleinfo_contract.period_idx == 7) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSJA:
        if (teleinfo_contract.period_idx == 8) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHH:
        if (teleinfo_contract.period_idx == 9) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHD:
        if (teleinfo_contract.period_idx == 10) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHM:
        if (teleinfo_contract.period_idx == 11) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSDSM:
        if (teleinfo_contract.period_idx == 12) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSSCM:
        if (teleinfo_contract.period_idx == 13) TeleinfoContractPowerUpdate (str_donnee);
        break;

      //   Counters
      // ------------

      // counter according to current period
      case TIC_PME_EAPS:
        TeleinfoConsoIndexCounterUpdate (str_donnee, teleinfo_contract.period_idx);
        break;

      case TIC_STD_EAIT:
        TeleinfoProdGlobalCounterUpdate (str_donnee);
        break;

      case TIC_HIS_BASE:
      case TIC_HIS_HCHC:
      case TIC_HIS_EJPHN:
      case TIC_HIS_BBRHCJB:
      case TIC_STD_EASF01:
      case TIC_EME_EAPP:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 0);
        break;

      case TIC_HIS_HCHP:
      case TIC_HIS_EJPHPM:
      case TIC_HIS_BBRHPJB:
      case TIC_STD_EASF02:
      case TIC_EME_EAPPM:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 1);
        break;

      case TIC_HIS_BBRHCJW:
      case TIC_STD_EASF03:
      case TIC_EME_EAPHPH:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 2);
        break;

      case TIC_HIS_BBRHPJW:
      case TIC_STD_EASF04:
      case TIC_EME_EAPHPD:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 3);
        break;

      case TIC_HIS_BBRHCJR:
      case TIC_STD_EASF05:
      case TIC_EME_EAPHCH:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 4);
        break;

      case TIC_HIS_BBRHPJR:
      case TIC_STD_EASF06:
      case TIC_EME_EAPHCD:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 5);
        break;

      case TIC_STD_EASF07:
      case TIC_EME_EAPHPE:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 6);
        break;

      case TIC_STD_EASF08:
      case TIC_EME_EAPHCE:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 7);
        break;

      case TIC_STD_EASF09:
      case TIC_EME_EAPJA:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 8);
      break;

      case TIC_STD_EASF10:
      case TIC_EME_EAPHH:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 9);
        break;

      case TIC_EME_EAPHD:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 10);
        break;

      case TIC_EME_EAPHM:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 11);
        break;

      case TIC_EME_EAPDSM:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 12);
        break;

      case TIC_EME_EAPSCM:
        TeleinfoConsoIndexCounterUpdate (str_donnee, 13);
        break;

      //   Flags
      // ---------

      // Overload
      case TIC_HIS_ADPS:
        TeleinfoSetPreavis (TIC_PREAVIS_DANGER, "DEP");
        break;

      case TIC_HIS_ADIR1:
        TeleinfoSetPreavis (TIC_PREAVIS_DANGER, "DEP1");
      break;

      case TIC_HIS_ADIR2:
        TeleinfoSetPreavis (TIC_PREAVIS_DANGER, "DEP2");
        break;

      case TIC_HIS_ADIR3:
        TeleinfoSetPreavis (TIC_PREAVIS_DANGER, "DEP3");
        break;

      case TIC_HIS_PEJP:
        TeleinfoSetPreavis (TIC_PREAVIS_ALERT, "EJP");
        break;

      case TIC_PME_PREAVIS:
      case TIC_EME_PREAVIS:
        TeleinfoSetPreavis (TIC_PREAVIS_ALERT, str_donnee);
        break;

      // start of next pointe period
      case TIC_STD_DPM1:
        TeleinfoCalendarPointeDeclare (str_donnee);
        break;
        
      // next day standard profile
      case TIC_STD_PJOURF1:
        TeleinfoCalendarTomorrowProfile (str_donnee);
        break;
        
      // next pointe profile
      case TIC_STD_PPOINTE:
        TeleinfoCalendarPointeProfile (str_donnee);
        break;
        
      // STGE flags
      case TIC_STD_STGE:
        TeleinfoAnalyseSTGE (str_donnee);
        break;

      case TIC_STD_RELAIS:
        relay = (uint8_t)atoi (str_donnee);
        if ((relay != teleinfo_meter.relay) && TeleinfoDriverMeterReady ()) teleinfo_meter.json.data = 1;
        teleinfo_meter.relay = relay;
        break;
    }
  }

  // if maximum number of lines not reached, save new line
  index = teleinfo_message.line_idx;
  strcpy (teleinfo_message.arr_line[index].str_etiquette, str_etiquette);
  strcpy (teleinfo_message.arr_line[index].str_donnee, str_donnee);
  teleinfo_message.arr_line[index].checksum = checksum;

  // increment line index and stay on last line if reached
  teleinfo_message.line_idx++;
  if (teleinfo_message.line_idx >= TIC_LINE_QTY) teleinfo_message.line_idx = TIC_LINE_QTY - 1;
}

// Handling of received teleinfo data
//   0x04 : Message reset 
//   0x02 : Message start
//   Ox03 : Message stop
//   0x0A : Line start
//   0x0D : Line stop
//   0x09 : Line separator
void TeleinfoReceptionProcess ()
{
  char   character;
  size_t index, buffer;
  char   str_character[2];
  char   str_buffer[1024];

  // check serial port
  if (teleinfo_serial == nullptr) return;

  // read pending data
  buffer = 0;
  if (teleinfo_serial->available ()) buffer = teleinfo_serial->read (str_buffer, 1024);

  // loop thru reception buffer
  for (index = 0; index < buffer; index++)
  {
    // read character
    character = str_buffer[index];

#ifdef USE_TCPSERVER
    // send character thru TCP stream
    TCPSend (character); 
#endif

    // handle according to reception stage
    switch (teleinfo_meter.reception)
    {
      case TIC_RECEPTION_NONE:
        // reset
        if (character == 0x04) TeleinfoReceptionMessageReset ();

        // message start
        else if (character == 0x02) TeleinfoReceptionMessageStart ();
        break;

      case TIC_RECEPTION_MESSAGE:
        // reset
        if (character == 0x04) TeleinfoReceptionMessageReset ();

        // line start
        else if (character == 0x0A) TeleinfoReceptionLineStart ();

        // message stop
        else if (character == 0x03) TeleinfoReceptionMessageStop ();
        break;

      case TIC_RECEPTION_LINE:
        // reset
        if (character == 0x04) TeleinfoReceptionMessageReset ();

        // line stop
        else if (character == 0x0D) TeleinfoReceptionLineStop ();

        // message stop
        else if (character == 0x03) TeleinfoReceptionMessageStop ();

        // append character to line
        else
        {
          // set line separator
          if ((teleinfo_meter.nb_message == 0) && (character == 0x09)) teleinfo_meter.sep_line = 0x09;

          // append separator to line
          str_character[0] = character;
          str_character[1] = 0;
          strlcat (teleinfo_message.str_line, str_character, sizeof (teleinfo_message.str_line));
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

  //   Serial speed auto-detection
  // -------------------------------
  if (Settings->teleinfo.autodetect == 1)
  {

    // if received more than 2 messages, speed has been found
    if (teleinfo_meter.nb_message > 2)
    {
      // disable auto-detection process
      Settings->teleinfo.autodetect = 0;

      // ask for a reboot
      TasmotaGlobal.restart_flag = 2;
    }

    else if ((teleinfo_meter.speed_changed == 0) && ((teleinfo_meter.nb_error > 2) || (teleinfo_meter.nb_reset > 2) || (TasmotaGlobal.uptime > 30)))
    {
      // adjust speed to next one
      if (TasmotaGlobal.baudrate == 1200)       { TasmotaGlobal.baudrate = 9600;   Settings->baudrate = 9600 / 300;   }
      else if (TasmotaGlobal.baudrate == 9600)  { TasmotaGlobal.baudrate = 19200;  Settings->baudrate = 19200 / 300;  }
      else if (TasmotaGlobal.baudrate == 19200) { TasmotaGlobal.baudrate = 115200; Settings->baudrate = 115200 / 300; }

      // if all speed tested, disable auto-detection
      if (TasmotaGlobal.baudrate == 115200) Settings->teleinfo.autodetect = 0;

      TasmotaGlobal.restart_flag = 2;

      teleinfo_meter.speed_changed = 1;
    }
  }

  //   Counters and Totals
  // -----------------------

  // force meter grand total on conso total
  Energy->total[0] = (float)(teleinfo_conso.total_wh / 1000);
  for (phase = 1; phase < teleinfo_contract.phase; phase++) Energy->total[phase] = 0;
  RtcSettings.energy_kWhtotal_ph[0] = (int32_t)(teleinfo_conso.total_wh / 100000);
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
    // update conso counters
    teleinfo_conso.today_wh = 0;
    teleinfo_conso.yesterday_wh = (long)(teleinfo_conso.total_wh - teleinfo_conso.midnight_wh);
    teleinfo_conso.midnight_wh  = teleinfo_conso.total_wh;

    // update prod counters
    teleinfo_prod.today_wh = 0;
    teleinfo_prod.yesterday_wh = (long)(teleinfo_prod.total_wh - teleinfo_prod.midnight_wh);
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
    case FUNC_ENERGY_EVERY_SECOND:
      TeleinfoEnergyEverySecond ();
      break;
  }

  return result;
}

#endif      // USE_TELEINFO
#endif      // USE_ENERGY_SENSOR

