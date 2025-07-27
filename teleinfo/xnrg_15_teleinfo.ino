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
    - Settings->rf_code[16][6]       : Minimum production power level to activate local relay (x50 W)

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
    20/10/2024 - v14.9 - Rework in cosphi calculation
                         Convert contract type to upper case before detection
                         Correct brand new counter index bug
                         Complete rewrite of calendar management
                         Handle calendar for TEMPO and EJP in historic mode
                         First step of generic TEMPO contract detection
                         Avoid NTARF and STGE to detect period as they as out of synchro very often                          
    07/05/2025 - v14.11 - Complete rewrite of speed detection
                          Add period profile
    10/07/2025 - v15.0  - Refactoring based on Tasmota 15
                          Set hardware fallback to 2 on esp8266 for serial port 

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

#define XNRG_15             15

/**************************************************\
 *                  Commands
\**************************************************/

bool TeleinfoEnergyHandleCommand ()
{
  bool    serviced  = true;
  bool    to_save   = false;
  bool    to_reboot = false;
  uint8_t result;
  int     index;
  long    value;
  char   *pstr_next, *pstr_key, *pstr_value;
  char    str_label[32];
  char    str_item[64];
  char    str_line[96];

  serviced = (Energy->command_code == CMND_ENERGYCONFIG);
  if (serviced) 
  {
    // if no parameter, show configuration
    if (XdrvMailbox.data_len == 0) 
    {
      AddLog (LOG_LEVEL_INFO, PSTR ("EnergyConfig Teleinfo parameters :"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  historique     mode historique (redémarrage)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  standard       mode standard (redémarrage)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  noraw          pas d'emission trame TIC"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  full           emission topic TIC"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  live           emission topic LIVE"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  stats          statistiques de reception"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  reset          detection série et raz du contrat (nécessite un redémarrage)"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  calraz         remise a 0 des plages du calendrier"));
      AddLog (LOG_LEVEL_INFO, PSTR ("  calhexa=%u      format des plages horaires Linky [0:decimal/1:hexa]"), teleinfo_config.cal_hexa);
      AddLog (LOG_LEVEL_INFO, PSTR ("  skip=%u         emet les topic TIC/LIVE toutes les xx trames"), teleinfo_config.skip);
      AddLog (LOG_LEVEL_INFO, PSTR ("  error=%u        affiche les compteurs d'erreurs [0/1]"), teleinfo_config.error);
      AddLog (LOG_LEVEL_INFO, PSTR ("  percent=%u    puissance maximale acceptable (%% du contrat)"), teleinfo_config.percent);
      AddLog (LOG_LEVEL_INFO, PSTR ("  trigger=%d      puissance déclenchant le relai de production (W)"), teleinfo_config.prod_trigger);

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
      AddLog (LOG_LEVEL_INFO, PSTR ("  meter=%u        publication sections METER & CONTRACT [0/1]"), teleinfo_config.meter);
      AddLog (LOG_LEVEL_INFO, PSTR ("  calendar=%u     publication section CAL [0/1]"), teleinfo_config.calendar);
      AddLog (LOG_LEVEL_INFO, PSTR ("  relay=%u        publication section RELAY [0/1]"), teleinfo_config.relay);
      AddLog (LOG_LEVEL_INFO, PSTR ("  period=%u       affichage couleur periode en cours [0/1]"), teleinfo_config.led_period);
      AddLog (LOG_LEVEL_INFO, PSTR ("  bright=%u      luminosite d'affichage LED [0..100]"), teleinfo_config.param[TIC_CONFIG_BRIGHT]);
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

        // handle command
        index = GetCommandCode (str_item, sizeof(str_item), pstr_key, kTeleinfoEnergyCommands);
        if (pstr_value != nullptr) value = atol (pstr_value);
          else value = LONG_MAX;
        result = TeleinfoEnergyExecuteCommand (index, value);

        // handle result
        if (result != TIC_COMMAND_NOTHING) to_save   = true;
        if (result == TIC_COMMAND_REBOOT)  to_reboot = true;
      } 
      while (pstr_next != nullptr);
    }

    // if needed, save and restart
    if (to_save)   TeleinfoEnergyConfigSave (true);
    if (to_reboot) TasmotaGlobal.restart_flag = 2;
  }

  return serviced;
}

// execute one energyconfig command
//   1st param : command index
//   2nd param : argument (LONG_MAX if undefined)
uint8_t TeleinfoEnergyExecuteCommand (const int command, const long value)
{
  bool    to_save   = false;
  bool    to_reboot = false;
  uint8_t result, day;
  long    counter;

  // handle command
  switch (command)
  {
    case TIC_CMND_STATS:
      if (teleinfo_meter.nb_line > 0) counter = (long)(teleinfo_meter.nb_error * 10000 / teleinfo_meter.nb_line);
        else counter = 0;

      AddLog (LOG_LEVEL_INFO, PSTR (" - Messages   : %d"), teleinfo_meter.nb_message);
      AddLog (LOG_LEVEL_INFO, PSTR (" - Erreurs    : %d (%d.%02d%%)"), (long)teleinfo_meter.nb_error, counter / 100, counter % 100);
      AddLog (LOG_LEVEL_INFO, PSTR (" - Reset      : %d"), teleinfo_meter.nb_reset);
      AddLog (LOG_LEVEL_INFO, PSTR (" - Cosφ conso : %d"), teleinfo_conso.cosphi.quantity);
      AddLog (LOG_LEVEL_INFO, PSTR (" - Cosφ prod  : %d"), teleinfo_prod.cosphi.quantity);
      break;

    case TIC_CMND_HISTORIQUE:
      to_save   = true;
      to_reboot = true;
      TasmotaGlobal.baudrate = 1200;
      Settings->baudrate = TasmotaGlobal.baudrate / 300;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Mode Historique (%d bauds)"), TasmotaGlobal.baudrate);
      break;

    case TIC_CMND_STANDARD:
      to_save   = true;
      to_reboot = true;
      TasmotaGlobal.baudrate = 9600;
      Settings->baudrate = TasmotaGlobal.baudrate / 300;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Mode Standard (%d bauds)"), TasmotaGlobal.baudrate);
      break;

    case TIC_CMND_NORAW:
      to_save = (teleinfo_config.tic != 0);
      if (to_save) teleinfo_config.tic = 0;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Non emission section TIC"));
      break;

    case TIC_CMND_FULL:
      to_save = (teleinfo_config.tic != 1);
      if (to_save) teleinfo_config.tic = 1;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Emission section TIC"));
      break;

    case TIC_CMND_SKIP:
      to_save = ((value > 0) && (value < 8) && (teleinfo_config.skip != (uint8_t)value));
      if (to_save) teleinfo_config.skip = (uint8_t)value;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: skip=%u"), teleinfo_config.skip);
      break;

    case TIC_CMND_LIVE:
      to_save = ((value < 2) && (teleinfo_config.live != (uint8_t)value));
      if (to_save) teleinfo_config.live = (uint8_t)value;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Emission section TIC"));
      break;

    case TIC_CMND_PERCENT:
      to_save = ((value < TIC_PERCENT_MAX) && (teleinfo_config.percent != (uint8_t)value));
      if (to_save) teleinfo_config.percent = (uint8_t)value;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: percent=%u"), teleinfo_config.percent);
      break;

    case TIC_CMND_ERROR:
      to_save = ((value < 2) && (teleinfo_config.error != (uint8_t)value));
      if (to_save) teleinfo_config.error = (uint8_t)value;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: error=%u"), teleinfo_config.error);
      break;

    case TIC_CMND_RESET:
      to_save   = true;
      TasmotaGlobal.baudrate = 115200;
      Settings->baudrate = TasmotaGlobal.baudrate / 300;
      TeleinfoContractReset ();
#ifdef USE_CURVE
      TeleinfoGraphDataReset ();
#endif    // USE_CURVE
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Port série, contrat et données remis à zéro"));
      break;

    case TIC_CMND_CALRAZ:
      to_save = true;
      for (day = TIC_DAY_TODAY; day < TIC_DAY_MAX; day++) TeleinfoCalendarReset (day);
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Calendrier remis à zéro"));
      break;

    case TIC_CMND_CALHEXA:
      to_save = ((value < 2) && (teleinfo_config.cal_hexa != (uint8_t)value));
      if (to_save)
      {
        teleinfo_config.cal_hexa = (uint8_t)value;
        for (day = TIC_DAY_TODAY; day < TIC_DAY_MAX; day ++) TeleinfoCalendarReset (day);
      }
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: calhexa=%u"), teleinfo_config.cal_hexa);
      break;

    case TIC_CMND_PERIOD:
      to_save = ((value < 2) && (teleinfo_config.led_period != (uint8_t)value));
      if (to_save) teleinfo_config.led_period = (uint8_t)value;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: period=%u"), teleinfo_config.led_period);
      break;

    case TIC_CMND_TRIGGER:
      to_save =  ((value >= 0) && (value <= 12750) && (teleinfo_config.prod_trigger != value));
      if (to_save) teleinfo_config.prod_trigger = value;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: trigger=%d"), teleinfo_config.prod_trigger);
      break;

    case TIC_CMND_BRIGHT:
      to_save = ((value <= 100) && (teleinfo_config.param[TIC_CONFIG_BRIGHT] != value));
      if (to_save) teleinfo_config.param[TIC_CONFIG_BRIGHT] = value;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: bright=%d"), teleinfo_config.param[TIC_CONFIG_BRIGHT]);
      break;

    case TIC_CMND_POLICY:
      to_save = ((value < TIC_POLICY_MAX) && (teleinfo_config.policy != (uint8_t)value));
      if (to_save) teleinfo_config.policy = (uint8_t)value;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: policy=%u"), teleinfo_config.policy);
      break;

    case TIC_CMND_METER:
      to_save = ((value < 2) && (teleinfo_config.meter != (uint8_t)value));
      if (to_save) teleinfo_config.meter = (uint8_t)value;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: meter=%u"), teleinfo_config.meter);
      break;

    case TIC_CMND_CALENDAR:
      to_save = ((value < 2) && (teleinfo_config.calendar != (uint8_t)value));
      if (to_save) teleinfo_config.calendar = (uint8_t)value;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: calendar=%u"), teleinfo_config.calendar);
      break;

    case TIC_CMND_RELAY:
      to_save = ((value < 2) && (teleinfo_config.relay != (uint8_t)value));
      if (to_save) teleinfo_config.relay = (uint8_t)value;
      AddLog (LOG_LEVEL_INFO, PSTR ("TIC: relay=%u"), teleinfo_config.relay);
      break;
  }

  // set result
  if (to_reboot) result = TIC_COMMAND_REBOOT;
    else if (to_save) result = TIC_COMMAND_SAVE;
    else result = TIC_COMMAND_NOTHING;

  return result;
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
void TeleinfoSerialStart ()
{
  bool   is_ready = false;
  size_t buffer_size;

  // if serial port is not already created
  if (teleinfo_serial != nullptr) return;

  // GPIO selection step
  teleinfo_meter.serial = TIC_SERIAL_GPIO;
  if (PinUsed (GPIO_TELEINFO_RX))
  {
    // create and initialise serial port
    // on esp8266, allow GPIO3 AND GPIO13 with hardware fallback to 2
#ifdef ESP8266
    teleinfo_serial = new TasmotaSerial (Pin (GPIO_TELEINFO_RX), -1, 2, 0);
#else
    teleinfo_serial = new TasmotaSerial (Pin (GPIO_TELEINFO_RX), -1, 1, 0);
#endif

    // start reception
    is_ready = teleinfo_serial->begin (TasmotaGlobal.baudrate, SERIAL_7E1);
    if (is_ready)
    {
      // set serial reception buffer
      teleinfo_serial->setRxBufferSize (TIC_BUFFER_MAX);
      buffer_size = teleinfo_serial->getRxBufferSize ();

#ifdef ESP32
      // display UART used and speed
      AddLog (LOG_LEVEL_INFO, PSTR("TIC: Serial UART %d démarré à %ubps (buffer %u)"), teleinfo_serial->getUart (), TasmotaGlobal.baudrate, buffer_size);
#else       // ESP8266
      // force configuration on ESP8266
      if (teleinfo_serial->hardwareSerial ()) ClaimSerial ();

      // display serial speed
      AddLog (LOG_LEVEL_INFO, PSTR("TIC: Serial démarré à %ubps (buffer %d)"), TasmotaGlobal.baudrate, buffer_size);
#endif      // ESP32 & ESP8266

      // flush serail data
      teleinfo_serial->flush ();

      // serial init succeeded
      teleinfo_meter.serial = TIC_SERIAL_ACTIVE;
    }

    // serial init failed
    else teleinfo_meter.serial = TIC_SERIAL_FAILED;
  }

  // log serial port init failure
  if (!is_ready) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Serial port init failed"));
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
  uint8_t index;

  // load standard settings
  teleinfo_config.percent      = Settings->teleinfo.percent;
  teleinfo_config.policy       = Settings->teleinfo.policy;
  teleinfo_config.meter        = Settings->teleinfo.meter;
  teleinfo_config.energy       = Settings->teleinfo.energy;
  teleinfo_config.calendar     = Settings->teleinfo.calendar;
  teleinfo_config.relay        = Settings->teleinfo.relay;
  teleinfo_config.tic          = Settings->teleinfo.tic;
  teleinfo_config.led_period   = Settings->teleinfo.led_period;
  teleinfo_config.live         = Settings->teleinfo.live;
  teleinfo_config.skip         = Settings->teleinfo.skip;
  teleinfo_config.cal_hexa     = Settings->teleinfo.cal_hexa;
  teleinfo_config.prod_trigger = 50 * (long)Settings->rf_code[16][6];

  // validate boundaries
  if ((teleinfo_config.policy < 0) || (teleinfo_config.policy >= TIC_POLICY_MAX)) teleinfo_config.policy = TIC_POLICY_TELEMETRY;
  if (teleinfo_config.meter > 1) teleinfo_config.meter = 1;
  if (teleinfo_config.calendar > 1) teleinfo_config.calendar = 1;
  if (teleinfo_config.relay > 1) teleinfo_config.relay = 1;
  if (teleinfo_config.tic > 1) teleinfo_config.tic = 1;
  if (teleinfo_config.skip == 0) teleinfo_config.skip = 3;
  if ((teleinfo_config.percent < TIC_PERCENT_MIN) || (teleinfo_config.percent > TIC_PERCENT_MAX)) teleinfo_config.percent = 100;

  // init parameters
  for (index = 0; index < TIC_CONFIG_MAX; index ++) teleinfo_config.param[index] = arrTeleinfoConfigDefault[index];

  // init contract data
  teleinfo_contract.period        = UINT8_MAX;
  teleinfo_contract.period_qty    = 0;
  teleinfo_contract.str_code[0]   = 0;
  teleinfo_contract.str_period[0] = 0;
  for (index = 0; index < TIC_INDEX_MAX; index ++) TeleinfoPeriodReset (index);

  // load littlefs settings
#ifdef USE_UFILESYS
  uint8_t slot;
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
void TeleinfoEnergyConfigSave (const bool save2rom) 
{
  uint8_t index;

  // save standard settings
  Settings->teleinfo.percent    = teleinfo_config.percent;
  Settings->teleinfo.policy     = teleinfo_config.policy;
  Settings->teleinfo.meter      = teleinfo_config.meter;
  Settings->teleinfo.energy     = teleinfo_config.energy;
  Settings->teleinfo.calendar   = teleinfo_config.calendar;
  Settings->teleinfo.relay      = teleinfo_config.relay;
  Settings->teleinfo.tic        = teleinfo_config.tic;
  Settings->teleinfo.led_period = teleinfo_config.led_period;
  Settings->teleinfo.live       = teleinfo_config.live;
  Settings->teleinfo.skip       = teleinfo_config.skip;
  Settings->teleinfo.cal_hexa   = teleinfo_config.cal_hexa;
  Settings->rf_code[16][6]      = (uint8_t)(teleinfo_config.prod_trigger / 50);

  // update today and yesterday conso/prod
  teleinfo_config.param[TIC_CONFIG_TODAY_CONSO]     = teleinfo_conso.today_wh;
  teleinfo_config.param[TIC_CONFIG_YESTERDAY_CONSO] = teleinfo_conso.yesterday_wh;
  teleinfo_config.param[TIC_CONFIG_TODAY_PROD]      = teleinfo_prod.today_wh;;
  teleinfo_config.param[TIC_CONFIG_YESTERDAY_PROD]  = teleinfo_prod.yesterday_wh;

  // save calendar
  TeleinfoCalendarSaveToSettings ();

  // save settings
  if (save2rom) SettingsSave (0);

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
  for (index = 0; index < TIC_INDEX_MAX; index ++)
    if (TeleinfoContractPeriod2String (str_key, sizeof (str_key), index))
    {
      sprintf_P (str_text, PSTR ("%s=%s\n"), PSTR (CMND_TIC_CONTRACT_PERIOD), str_key);
      file.print (str_text);
    }

  file.close ();
# endif     // USE_UFILESYS

  // log
  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Configuration saved"));
}

void TeleinfoContractReset ()
{
  uint8_t index;

  // init contract data
  teleinfo_contract.index       = 0;
  teleinfo_contract.period      = UINT8_MAX;
  teleinfo_contract.period_qty  = 0;
  teleinfo_contract.str_code[0] = 0;

  // init periods
  for (index = 0; index < TIC_INDEX_MAX; index ++) TeleinfoPeriodReset (index);

  // init calendar days
  for (index = 0; index < TIC_DAY_MAX; index ++) TeleinfoCalendarReset (index);

  // init conso data
  teleinfo_conso.total_wh = 0;
  for (index = 0; index < TIC_INDEX_MAX; index ++) teleinfo_conso.index_wh[index] = 0;

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
  char     *pstr_token, *pstr_data;
  char     str_line[TIC_LINE_SIZE];

  // check parameters
  if ((pstr_line == nullptr) || (pstr_etiquette == nullptr) || (pstr_donnee == nullptr)) return 0;

  // init result
  pstr_etiquette[0] = 0;
  pstr_donnee[0]    = 0;
  str_line[0]       = 0;

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

    // if network is up, account error
    if (!TasmotaGlobal.global_state.network_down)
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
  if (pstr_data != nullptr)
  {
    *pstr_data = 0;
    strlcpy (pstr_donnee, pstr_data + 1, size_donnee);
  }

  // else, no data
  else
  {
    line_checksum = 0;
    teleinfo_message.error = 1;
    AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: No data [%s]"), str_line);
  }

  // extract etiquette
  strlcpy (pstr_etiquette, str_line, size_etiquette);

  return line_checksum;
}

// get meter manufacturer
void TeleinfoMeterGetManufacturer (char* pstr_text, const size_t size_text)
{
  uint8_t index, range;

  // check parameters
  if (pstr_text == nullptr) return;
  if (size_text < 32) return;

  // look for manufacturer name
  pstr_text[0] = 0;
  range = teleinfo_meter.company / 10;
  index = teleinfo_meter.company - (10 * range);
  switch (range)
  {
    case 0: GetTextIndexed (pstr_text, size_text, index, kTicManufacturer00to09); break;
    case 1: GetTextIndexed (pstr_text, size_text, index, kTicManufacturer10to19); break;
    case 2: GetTextIndexed (pstr_text, size_text, index, kTicManufacturer20to29); break;
    case 3: GetTextIndexed (pstr_text, size_text, index, kTicManufacturer30to39); break;
    case 7: GetTextIndexed (pstr_text, size_text, index, kTicManufacturer70to79); break;
    case 8: GetTextIndexed (pstr_text, size_text, index, kTicManufacturer80to89); break;
  }

  // if not found
  if (strlen (pstr_text) == 0) strcpy_P (pstr_text, PSTR ("Fabricant non référencé")); 
}

// get meter model description
void TeleinfoMeterGetModel (char* pstr_model, const size_t size_model, char* pstr_gene, const size_t size_gene)
{
  uint8_t index, range;
  char   *pstr_separator;

  // check parameters
  if (pstr_model == nullptr) return;
  if (size_model < 32) return;

  // look for manufacturer name
  range = teleinfo_meter.model / 10;
  index = teleinfo_meter.model - (10 * range);
  switch (range)
  {
    case 0: GetTextIndexed (pstr_model, size_model, index, kTicModel00to09); break;
    case 1: GetTextIndexed (pstr_model, size_model, index, kTicModel10to19); break;
    case 2: GetTextIndexed (pstr_model, size_model, index, kTicModel20to29); break;
    case 3: GetTextIndexed (pstr_model, size_model, index, kTicModel30to39); break;
    case 4: GetTextIndexed (pstr_model, size_model, index, kTicModel40to49); break;
    case 6: GetTextIndexed (pstr_model, size_model, index, kTicModel60to69); break;
    case 7: GetTextIndexed (pstr_model, size_model, index, kTicModel70to79); break;
    case 8: GetTextIndexed (pstr_model, size_model, index, kTicModel80to89); break;
    case 9: GetTextIndexed (pstr_model, size_model, index, kTicModel90to99); break;
  }

  // if model found, split model and generation
  pstr_separator = strchr (pstr_model, ',');
  if (pstr_separator != nullptr)
  {
    // trim generation from model
    *pstr_separator = 0;

    // if asked, set generation
    if (pstr_gene != nullptr) strlcpy (pstr_gene, pstr_separator + 1, size_gene);
  }

  // unknown meter
  if (strlen (pstr_model) == 0) strcpy_P (pstr_model, PSTR ("Modèle de compteur inconnu"));
}

// update serial number and meter type
void TeleinfoMeterUpdateType (const char* pstr_donnee, const bool detect)
{
  char str_text[64];

  // check if alread done and parameter
  if (teleinfo_meter.ident != 0) return;
  if (pstr_donnee == nullptr) return;

  // update serial number
  teleinfo_meter.ident = atoll (pstr_donnee);
  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Numéro de série %s"), pstr_donnee);

  // if counter should be detected
  if (detect)
  {
    // manufacturer
    strlcpy (str_text, pstr_donnee, 3);
    teleinfo_meter.company = (uint8_t)atoi (str_text);

    // year
    strlcpy (str_text, pstr_donnee + 2, 3);
    teleinfo_meter.year = 2000 + (uint16_t)atoi (str_text);

    // model
    strlcpy (str_text, pstr_donnee + 4, 3);
    teleinfo_meter.model = (uint8_t)atoi (str_text);

    // log
    TeleinfoMeterGetManufacturer (str_text, sizeof (str_text));
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Fabricant %s (%u)"), str_text, teleinfo_meter.year);
    TeleinfoMeterGetModel (str_text, sizeof (str_text), nullptr, 0);
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: %s"), str_text);
  }
}

// update global consommation counter (avoid any decrease)
void TeleinfoUpdateConsoGlobalCounter ()
{
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

  // if value is abnormal
  if (total < teleinfo_conso.total_wh)
  {
    // init values
    teleinfo_conso.total_wh   = 0;
    teleinfo_conso.delta_mwh  = 0;
    teleinfo_conso.delta_mvah = 0;

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: compteur global anormal, reset"));
  }

  // else update counters
  else
  {
    teleinfo_conso.delta_mwh += 1000 * (long)(total - teleinfo_conso.total_wh);
    teleinfo_conso.total_wh   = total;
    if (teleinfo_conso.midnight_wh > 0) teleinfo_conso.today_wh = (long)(teleinfo_conso.total_wh - teleinfo_conso.midnight_wh);
  }
}

// update global consommation counter (avoid any decrease)
void TeleinfoProdGlobalCounterUpdate (const char* pstr_value)
{
  long long total;

  // check parameter
  if (pstr_value == nullptr) return;

  // convert string and update counter
  total = atoll (pstr_value);
  if (total == LONG_LONG_MAX) return;

  // if total not defined, update total
  if (teleinfo_prod.total_wh == 0) teleinfo_prod.total_wh = total;

  // if needed, update midnight value
  if ((teleinfo_prod.midnight_wh == 0) && (teleinfo_prod.total_wh > 0)) teleinfo_prod.midnight_wh = teleinfo_prod.total_wh - (long long)teleinfo_config.param[TIC_CONFIG_TODAY_PROD];

  // if no increment, return
  if (total == teleinfo_prod.total_wh) return;

  // if value is abnormal, reset global counter
  if (total < teleinfo_prod.total_wh)
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
void TeleinfoConsoIndexCounterUpdate (const char* pstr_value, const uint8_t index)
{
  long long value;
  char     *pstr_kilo;
  char      str_value[16];

  // check parameter
  if (pstr_value == nullptr) return;
  if (index >= TIC_INDEX_MAX) return;

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

  // in case of period counter increment, update current period
  if (value > teleinfo_conso.index_wh[index]) teleinfo_message.period = index;

  // update value
  teleinfo_conso.index_wh[index] = value;
}

// update phase voltage
void TeleinfoVoltageUpdate (const char* pstr_value, const uint8_t phase, const bool is_rms)
{
  long  value;
  char *pstr_unit;
  char  str_value[16];

  // check parameter
  if (pstr_value == nullptr) return;
  if (phase >= TIC_PHASE_MAX) return;

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
  if (phase >= TIC_PHASE_MAX) return;
  
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
  if ((value >= 0) && (value < LONG_MAX))
  {
    // update total apprent power
    teleinfo_conso.papp = value;

    // if only one phase, update first phase apparent power
    if (teleinfo_contract.phase == 1) teleinfo_conso.phase[0].sinsts = value; 
  }
}

// update phase apparent power
void TeleinfoConsoApparentPowerUpdate (const char* pstr_value, const uint8_t phase)
{
  long value;

  // check parameter
  if (pstr_value == nullptr) return;
  if (phase >= TIC_PHASE_MAX) return;
  
  // calculate and update phase apparent power
  value = atol (pstr_value);
  if ((value >= 0) && (value < LONG_MAX)) teleinfo_conso.phase[phase].sinsts = value; 
}

void TeleinfoPreavisSet (const uint8_t level, const char* pstr_label)
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
  uint8_t  value;
  uint32_t stge;

  // convert hexa STGE string
  stge = (uint32_t)strtoul (pstr_donnee, nullptr, 16);

  // get phase over voltage
  value = (uint8_t)((stge >> 6) & 0x01);
  if (TeleinfoDriverMeterReady () && (teleinfo_message.stge.over_volt != value)) teleinfo_meter.json.data = 1;
  teleinfo_message.stge.over_volt = value;

  // get phase overload
  value = (uint8_t)((stge >> 7) & 0x01);
  if (TeleinfoDriverMeterReady () && (teleinfo_message.stge.over_load != value)) teleinfo_meter.json.data = 1;
  teleinfo_message.stge.over_load = value;

  // get preavis pointe mobile signal
  teleinfo_message.stge.preavis = (uint8_t)((stge >> 28) & 0x03);
  if (teleinfo_message.stge.preavis != 0) TeleinfoPreavisSet (TIC_PREAVIS_WARNING, "PM");

  // get active pointe mobile signal
  teleinfo_message.stge.pointe = (uint8_t)((stge >> 30) & 0x03);
}

void TeleinfoEnergyTotalMidnight ()
{
  // update conso counters
  teleinfo_conso.today_wh = 0;
  teleinfo_conso.yesterday_wh = (long)(teleinfo_conso.total_wh - teleinfo_conso.midnight_wh);
  teleinfo_conso.midnight_wh  = teleinfo_conso.total_wh;

  // update prod counters
  teleinfo_prod.today_wh = 0;
  teleinfo_prod.yesterday_wh = (long)(teleinfo_prod.total_wh - teleinfo_prod.midnight_wh);
  teleinfo_prod.midnight_wh  = teleinfo_prod.total_wh;
}

/*********************************************\
 *                Calendar
\*********************************************/

// calculate current slot
uint8_t TeleinfoCalendarGetSlot (const uint8_t hour, const uint8_t minute)
{
  uint8_t slot;

  // check parameter
  if (hour > 23) return UINT8_MAX;
  if (minute > 59) return UINT8_MAX;

  // calculate slot
  slot = hour * 2 + (minute / 30);
 
  return slot;
}

// calculate current slot
uint8_t TeleinfoCalendarGetSlot (const char* pstr_time, const uint8_t is_hexa)
{
  uint8_t slot, hour, minute;
  char    str_value[4];

  // check parameter
  if (pstr_time == nullptr) return UINT8_MAX;
  if (strlen (pstr_time) < 4) return UINT8_MAX;

  // calculate hours
  strlcpy (str_value, pstr_time, 3);
  if (is_hexa == 1) hour = (uint8_t)strtol (str_value, NULL, 16);
    else hour = (uint8_t)atoi (str_value);

  // calculate minutes
  strlcpy (str_value, pstr_time + 2, 3);
  if (is_hexa == 1) minute = (uint8_t)strtol (str_value, NULL, 16);
    else minute = (uint8_t)atoi (str_value);

  // calculate slot
  slot = TeleinfoCalendarGetSlot (hour, minute);

  return slot;
}

// calculate current date (format : YYYYMMDDSS)
uint32_t TeleinfoCalendarGetDay (const char* pstr_date, const bool add_slot)
{
  uint32_t date = 0;
  char     str_text[8];

  // check parameter
  if (pstr_date == nullptr) return 0;

  // calculate date stamp
  if (strlen (pstr_date) >= 6)
  {
    strlcpy (str_text, pstr_date, 7);
    date = 100 * (20000000 + (uint32_t)atoi (str_text));
  }

  // if needed, include day slot
  if (add_slot && (date != 0) && (strlen (pstr_date) >= 10))
  {
    // handle hours
    strlcpy (str_text, pstr_date + 6, 3);
    date += 2 * (uint8_t)atoi (str_text);

    // handle minutes
    strlcpy (str_text, pstr_date + 8, 3);
    date += (uint8_t)atoi (str_text) / 30;
  }

  return date;
}

// calculate current date (format : YYYYMMDDSS)
uint32_t TeleinfoCalendarGetDay (const uint16_t year, const uint8_t month, const uint8_t day_of_month, const uint8_t hour, const uint8_t minute)
{
  uint32_t date;

  // calculate today's date
  date  = 1000000 * (2000 + (uint32_t)year);
  date += 10000 * (uint32_t)month;
  date += 100 * (uint32_t)day_of_month;

  // append daily slot
  date += hour * 2;
  date += minute / 30;

  return date;
}

// set current date (format : YYYYMMDD00)
uint32_t TeleinfoCalendarGetNextDay (const uint32_t date)
{
  uint32_t date_next;
  TIME_T   date_dst;

  // check parameter
  if (date == 0) return 0;

  // set given date
  date_dst.year         = (uint16_t)(date / 1000000 - 1970);
  date_dst.month        = (uint8_t)(date / 10000 % 100);
  date_dst.day_of_month = (uint8_t)(date / 100 % 100);
  date_dst.hour         = 0;
  date_dst.minute       = 0;
  date_dst.second       = 0;

  // calculate next day
  date_next = MakeTime (date_dst) + 86400;
  BreakTime (date_next, date_dst);

  // generate new date timestamp
  date_next  = 1000000 * ((uint32_t)date_dst.year + 1970);
  date_next += 10000 * (uint32_t)date_dst.month;
  date_next += 100 * (uint32_t)date_dst.day_of_month;

  return date_next;
}

// remise a zero des donnees de calendrier
void TeleinfoCalendarReset (const uint8_t day)
{
  // check parameter
  if (day >= TIC_DAY_MAX) return;

  // reset all data
  memset (&teleinfo_calendar[day], 0, sizeof (tic_cal_day));

  // reset settings
  SettingsUpdateText (SET_TIC_CAL_TODAY + day, "");
}

// save current calendar to settings
void TeleinfoCalendarSaveToSettings ()
{
  uint8_t day, count, slot, period;
  char    str_value[8];
  String  str_setting;

  // loop thru days to save in settings
  for (day = TIC_DAY_TODAY; day < TIC_DAY_MAX; day ++)
  {
    // date
    ultoa ((ulong)teleinfo_calendar[day].date, str_value, 16);
    str_setting = str_value;

    // loop thru slots
    count = 0;
    period = teleinfo_calendar[day].arr_slot[0];
    for (slot = 0; slot < TIC_DAY_SLOT_MAX; slot ++)
    {
      // if period has not changed, increase counter
      if (teleinfo_calendar[day].arr_slot[slot] == period) count ++;

      // else save previous one
      else
      {
        // append previous period
        str_setting += period;
        sprintf_P (str_value, PSTR ("%02u"), count);
        str_setting += str_value;

        // set new period
        period = teleinfo_calendar[day].arr_slot[slot];
        count = 1;
      }
    }

    // append last period
    str_setting += period;
    sprintf_P (str_value, PSTR ("%02u"), count);
    str_setting += str_value;    

    // update string
    SettingsUpdateText (SET_TIC_CAL_TODAY + day, str_setting.c_str ());
  }
}

// load current calendar from settings
bool TeleinfoCalendarLoadFromSettings (const uint8_t day)
{
  bool     done = false;
  uint8_t  index, period, slot, start, stop, length;
  uint16_t value;
  uint32_t date;
  char    *pstr_setting;
  char     str_text[8];

  // check parameters
  if (day >= TIC_DAY_MAX) return done;

  // loop thru setting strings
  for (index = SET_TIC_CAL_TODAY; index <= SET_TIC_CAL_AFTER; index ++)
  {
    // retrieve date of current day setting
    pstr_setting = SettingsText (index);
    strlcpy (str_text, pstr_setting, 5);
    date = (uint32_t)strtoul (str_text, nullptr, 16);

    // if date is matching, populate slots
    if (date == teleinfo_calendar[day].date)
    {
      // init
      done  = true;
      index = UINT8_MAX;
      start = 0;
      pstr_setting += 4;

      // loop thru slots
      while (strlen (pstr_setting) >= 3)
      {
        // get current slot description
        strlcpy (str_text, pstr_setting, 4);
        value = atoi (str_text);
        period = (uint8_t)(value / 100); 
        length = (uint8_t)(value % 100);

        // update daily slot
        stop = min (start + length, TIC_DAY_SLOT_MAX);
        for (slot = start; slot < stop; slot ++) teleinfo_calendar[day].arr_slot[slot] = period;

        // go to next slot
        pstr_setting += 3;
        start += length;
      }
    }
  }

  return done;
}

// set current date (year has offset from 2000 : 24 is 2024)
void TeleinfoCalendarSetDate (const uint16_t year, const uint8_t month, const uint8_t day_of_month, const uint8_t hour, const uint8_t minute)
{
  uint8_t day;

  // update current slot
  teleinfo_meter.date = TeleinfoCalendarGetDay (year, month, day_of_month, hour, minute);
  teleinfo_meter.slot = TeleinfoCalendarGetSlot (hour, minute);
  
  // if today is set, nothing else
  if (teleinfo_calendar[TIC_DAY_TODAY].date > 0) return;
  
  // reset all calendar days
  for (day = TIC_DAY_TODAY; day < TIC_DAY_MAX; day ++) TeleinfoCalendarReset (day);

  // set today, tomorrow and day after tomorrow
  teleinfo_calendar[TIC_DAY_TODAY].date = TeleinfoCalendarGetDay (year, month, day_of_month, 0, 0);
  teleinfo_calendar[TIC_DAY_TMROW].date = TeleinfoCalendarGetNextDay (teleinfo_calendar[TIC_DAY_TODAY].date);
  teleinfo_calendar[TIC_DAY_AFTER].date = TeleinfoCalendarGetNextDay (teleinfo_calendar[TIC_DAY_TMROW].date);
}

// set period on current slot
void TeleinfoCalendarSetDailyCalendar (const uint8_t day, const uint8_t period)
{
  uint8_t index, period_hc, period_hp;

  // check parameters
  if (day >= TIC_DAY_AFTER) return;
  if (period >= TIC_INDEX_MAX) return;
  
  // if dealing with EJP calendar and announcing pointe
  if (((teleinfo_contract.index == TIC_C_HIS_EJP) || (teleinfo_contract.index == TIC_C_STD_EJP)) && (period == 1))
  {
    // if today before 1h -> set period today 0h-1h
    if ((day == TIC_DAY_TODAY) && (teleinfo_meter.slot < 2)) for (index = 0; index < 2; index ++) teleinfo_calendar[TIC_DAY_TODAY].arr_slot[index] = period;

    // else if today after 7h -> set period today 7h-24h and tomorrow 0h-1h
    else if ((day == TIC_DAY_TODAY) && (teleinfo_meter.slot >= 14))
    {
      for (index = 14; index < TIC_DAY_SLOT_MAX; index ++) teleinfo_calendar[TIC_DAY_TODAY].arr_slot[index] = period;
      for (index = 0; index < 2; index ++) teleinfo_calendar[TIC_DAY_TMROW].arr_slot[index] = period;
      teleinfo_calendar[TIC_DAY_TODAY].level = teleinfo_contract.arr_period[period].level;
    }

    // else if tomorrow before 1h -> set period today 0h-1h
    else if ((day == TIC_DAY_TMROW) && (teleinfo_meter.slot < 2)) for (index = 0; index < 2; index ++) teleinfo_calendar[TIC_DAY_TODAY].arr_slot[index] = period;

    // else if tomorrow after 8h -> set period tomorrow 7h-24h and day after 0h-1h
    else if ((day == TIC_DAY_TMROW) && (teleinfo_meter.slot >= 16))
    {
      for (index = 14; index < TIC_DAY_SLOT_MAX; index ++) teleinfo_calendar[TIC_DAY_TMROW].arr_slot[index] = period;
      for (index = 0; index < 2; index ++) teleinfo_calendar[TIC_DAY_AFTER].arr_slot[index] = period;
      teleinfo_calendar[TIC_DAY_TMROW].level = teleinfo_contract.arr_period[period].level;
    }
  }

  // check if default Tempo calendar should be applied
  else if (((teleinfo_contract.index == TIC_C_HIS_TEMPO) || (teleinfo_contract.index == TIC_C_STD_TEMPO)) && (period >= 2))
  {
    // calculate HC and HP periods
    period_hc = period - (period % 2);
    period_hp = period_hc + 1;

    // if today before 6h -> set period today 0h-6h hc
    if ((day == TIC_DAY_TODAY) &&  (teleinfo_meter.slot < 12)) for (index = 0; index < 12; index ++) teleinfo_calendar[TIC_DAY_TODAY].arr_slot[index] = period_hc;

    // else if today after 6h -> set period today 6h-22h hp, today 22h-24h hc and tomorrow 0h-6h hc
    else if ((day == TIC_DAY_TODAY) &&  (teleinfo_meter.slot >= 12))
    {
      for (index = 12; index < 44; index ++) teleinfo_calendar[TIC_DAY_TODAY].arr_slot[index] = period_hp;
      for (index = 44; index < TIC_DAY_SLOT_MAX; index ++) teleinfo_calendar[TIC_DAY_TODAY].arr_slot[index] = period_hc;
      for (index = 0; index < 12; index ++) teleinfo_calendar[TIC_DAY_TMROW].arr_slot[index] = period_hc;
      teleinfo_calendar[TIC_DAY_TODAY].level = teleinfo_contract.arr_period[period].level;
    }

    // else if tomorrow before 6h -> set period today 0h-6h hc
    else if ((day == TIC_DAY_TMROW) &&  (teleinfo_meter.slot < 12)) for (index = 0; index < 12; index ++) teleinfo_calendar[TIC_DAY_TODAY].arr_slot[index] = period_hc;

    // else if tomorrow after 8h -> set period tomorrow 6h-22h hp, tomorrow 22h-24h hc and day after 0h-6h hc
    else if ((day == TIC_DAY_TMROW) &&  (teleinfo_meter.slot >= 16))
    {
      for (index = 12; index < 44; index ++) teleinfo_calendar[TIC_DAY_TMROW].arr_slot[index] = period_hp;
      for (index = 44; index < TIC_DAY_SLOT_MAX; index ++) teleinfo_calendar[TIC_DAY_TMROW].arr_slot[index] = period_hc;
      for (index = 0; index < 12; index ++) teleinfo_calendar[TIC_DAY_AFTER].arr_slot[index] = period_hc;
      teleinfo_calendar[TIC_DAY_TMROW].level = teleinfo_contract.arr_period[period].level;
    }
  }
}

void TeleinfoCalendarSetDemain (const char* pstr_color)
{
  uint8_t level, period;

  // check parameters
  if (pstr_color == nullptr) return;
  if (!RtcTime.valid) return;

  // detect color
  level = TeleinfoPeriodDetectLevel (pstr_color);
  if (level == TIC_LEVEL_NONE) return;

  // check if contract is compatible, calculate period accordingly
  if (teleinfo_contract.index == TIC_C_HIS_TEMPO) period = 2 * level - 2;
    else if (teleinfo_contract.index == TIC_C_HIS_EJP) period = level / 3;
    else return;

  // set period
  TeleinfoCalendarSetDailyCalendar (TIC_DAY_TMROW, period);
}

// calendar midnight shift
void TeleinfoEnergyCalendarMidnight ()
{
  char str_text[16];

  // shift tomorrow and day after tomorrow
  teleinfo_calendar[TIC_DAY_TODAY] = teleinfo_calendar[TIC_DAY_TMROW];
  teleinfo_calendar[TIC_DAY_TMROW] = teleinfo_calendar[TIC_DAY_AFTER];

  // init day after
  TeleinfoCalendarReset (TIC_DAY_AFTER);
  teleinfo_calendar[TIC_DAY_AFTER].date = TeleinfoCalendarGetNextDay (teleinfo_calendar[TIC_DAY_TMROW].date);
  GetTextIndexed (str_text, sizeof (str_text), TIC_DAY_AFTER, kTeleinfoPeriodDay);
  AddLog (LOG_LEVEL_INFO, PSTR ("CAL: %u %s"), teleinfo_calendar[TIC_DAY_AFTER].date / 100, str_text);
}

// set calendar current pointe start
void TeleinfoCalendarPointeBegin (const uint8_t index, const char *pstr_horodatage)
{
  // check parameter
  if (!RtcTime.valid) return;
  if (index >= TIC_POINTE_MAX) return;
  if (pstr_horodatage == nullptr) return;
  if (strlen (pstr_horodatage) < 12) return;

  // set index, day and slot
  teleinfo_message.arr_pointe[index].start = TeleinfoCalendarGetDay (pstr_horodatage, true);
}

// set calendar current pointe stop
void TeleinfoCalendarPointeEnd (const uint8_t index, const char *pstr_horodatage)
{
  // check parameter
  if (!RtcTime.valid) return;
  if (index >= TIC_POINTE_MAX) return;
  if (pstr_horodatage == nullptr) return;
  if (strlen (pstr_horodatage) < 12) return;

  // check if date is within known days
  teleinfo_message.arr_pointe[index].stop = TeleinfoCalendarGetDay (pstr_horodatage, true);
}

// set default profile if not defined
void TeleinfoCalendarDefaultProfile (char *pstr_donnee)
{
  bool    update;
  uint8_t day, start, slot, period, level;
  uint8_t arr_slot[TIC_DAY_SLOT_MAX];
  char    str_text[16];

  // check parameters
  if (!RtcTime.valid) return;
  if (pstr_donnee == nullptr) return;
  if (strlen (pstr_donnee) < 8) return;

  // check if a day calendar needs to be updated
  update = false;
  for (day = TIC_DAY_TODAY; day < TIC_DAY_MAX; day ++) update |= (teleinfo_calendar[day].level == TIC_LEVEL_NONE);
  if (!update) return;

  // loop thru segments to load daily calendar
  level = TIC_LEVEL_NONE;
  memset (&arr_slot, UINT8_MAX, TIC_DAY_SLOT_MAX);
  while ((strlen (pstr_donnee) >= 8) && isdigit (pstr_donnee[0]))
  {
    // extract data and populate slot
    strlcpy (str_text, pstr_donnee, 5);
    start = TeleinfoCalendarGetSlot (str_text, teleinfo_config.cal_hexa);
    strlcpy (str_text, pstr_donnee + 7, 2);
    period = (uint8_t)atoi (str_text) - 1;
    level = max (level, teleinfo_contract.arr_period[period].level);
    for (slot = start; slot < TIC_DAY_SLOT_MAX; slot ++) arr_slot[slot] = period;

    // increment to next segment
    pstr_donnee += 8;
    if (pstr_donnee[0] == ' ') pstr_donnee ++;
  }

  // loop thru days : if day's calendar not set, set default one
  for (day = TIC_DAY_TODAY; day < TIC_DAY_MAX; day ++)
    if ((teleinfo_calendar[day].level == TIC_LEVEL_NONE) && (level != TIC_LEVEL_NONE))
    {
      teleinfo_calendar[day].level = level;
      for (slot = 0; slot < TIC_DAY_SLOT_MAX; slot ++) 
        if (teleinfo_calendar[day].arr_slot[slot] < arr_slot[slot]) teleinfo_calendar[day].arr_slot[slot] = arr_slot[slot];
      GetTextIndexed (str_text, sizeof (str_text), day, kTeleinfoPeriodDay);
      AddLog (LOG_LEVEL_INFO, PSTR ("CAL: default calendar applied to %s"), str_text);
    }
}

// set pointe profile
void TeleinfoCalendarPointeProfile (const char *pstr_donnee)
{
  uint8_t  day, start, slot, period, index, pointe;
  uint32_t date;
  uint8_t  arr_slot[TIC_DAY_SLOT_MAX];
  char     str_value[8];

  // check parameters
  if (!RtcTime.valid) return;
  if (pstr_donnee == nullptr) return;
  if (strlen (pstr_donnee) < 8) return;

  // detect pointe to apply
  pointe = UINT8_MAX;
  date   = UINT32_MAX;
  for (index = 0; index < TIC_POINTE_MAX; index ++) 
    if ((teleinfo_message.arr_pointe[index].start > teleinfo_meter.date) && (teleinfo_message.arr_pointe[index].start < date))
    {
      date   = teleinfo_message.arr_pointe[index].start;
      pointe = index;
    }
  if (pointe >= TIC_POINTE_MAX) return;
  if (teleinfo_message.arr_pointe[pointe].stop == 0) return;

  // loop thru segments to load daily calendar
  for (slot = 0; slot < TIC_DAY_SLOT_MAX; slot ++) arr_slot[slot] = UINT8_MAX;
  while ((strlen (pstr_donnee) >= 8) && isdigit (pstr_donnee[0]))
  {
    // extract data and populate slot
    strlcpy (str_value, pstr_donnee, 5);
    start = TeleinfoCalendarGetSlot (str_value, teleinfo_config.cal_hexa);
    strlcpy (str_value, pstr_donnee + 7, 2);
    period = (uint8_t)atoi (str_value) - 1;
    for (slot = start; slot < TIC_DAY_SLOT_MAX; slot ++) arr_slot[slot] = period;

    // increment to next segment
    pstr_donnee += 8;
    if (pstr_donnee[0] == ' ') pstr_donnee ++;
  }

  // loop thru days to update their slots with current pointe period
  for (day = TIC_DAY_TODAY; day < TIC_DAY_MAX; day ++)
    for (slot = 0; slot < TIC_DAY_SLOT_MAX; slot ++)
    {
      date = teleinfo_calendar[day].date + slot;
      if ((date >= teleinfo_message.arr_pointe[pointe].start) && (date < teleinfo_message.arr_pointe[pointe].stop) && (arr_slot[slot] != UINT8_MAX)) teleinfo_calendar[day].arr_slot[slot] = arr_slot[slot];
    }
}

/*********************************************\
 *          Date and Timestamp
\*********************************************/

// calculate date and timestamp
void TeleinfoTimestampFromDate (const char* pstr_donnee)
{
  uint8_t  arr_date[3], arr_time[3];
  uint32_t epoch, offset, delay;
  int      size;
  char     str_text[4];
  TIME_T   tm_meter;

  // check parameters
  if (pstr_donnee == nullptr) return;
  if (strlen (pstr_donnee) < 12) return;

  // set conversion parameters according to string size
  size = strlen (pstr_donnee);
  if (size == 12)      { arr_date[0] = 0; arr_date[1] = 2; arr_date[2] = 4; arr_time[0] = 6; arr_time[1] = 8;  arr_time[2] = 10; }      // string format is 240413181235 
  else if (size == 13) { arr_date[0] = 1; arr_date[1] = 3; arr_date[2] = 5; arr_time[0] = 7; arr_time[1] = 9;  arr_time[2] = 11; }      // string format is H240413181235
  else if (size == 17) { arr_date[0] = 0; arr_date[1] = 3; arr_date[2] = 6; arr_time[0] = 9; arr_time[1] = 12; arr_time[2] = 15; }      // string format is 13/04/24 18:12:35 or 13/04/24 18/12/35
  else return;

  // year, month, day of month, hour, minute and second
  strlcpy (str_text, pstr_donnee + arr_date[0], 3);
  tm_meter.year = (uint16_t)atoi (str_text);
  strlcpy (str_text, pstr_donnee + arr_date[1], 3);
  tm_meter.month = (uint8_t)atoi (str_text);
  strlcpy (str_text, pstr_donnee + arr_date[2], 3);
  tm_meter.day_of_month = (uint8_t)atoi (str_text);
  strlcpy (str_text, pstr_donnee + arr_time[0], 3);
  tm_meter.hour = (uint8_t)atoi (str_text);
  strlcpy (str_text, pstr_donnee + arr_time[1], 3);
  tm_meter.minute = (uint8_t)atoi (str_text);
  strlcpy (str_text, pstr_donnee + arr_time[2], 3);
  tm_meter.second = (uint8_t)atoi (str_text);

  // set current date
  TeleinfoCalendarSetDate (tm_meter.year, tm_meter.month, tm_meter.day_of_month, tm_meter.hour, tm_meter.minute);

  // calculate timestamp
  teleinfo_conso.last_stamp = 3600 * (long)tm_meter.hour + 60 * (long)tm_meter.minute + (long)tm_meter.second;

  // if time is not set after some time, set it from meter
  if (TeleinfoDriverIsPowered ()) delay = TIC_RTC_TIMEOUT_MAINS;
    else delay = TIC_RTC_TIMEOUT_BATTERY;
  if (!RtcTime.valid && (TasmotaGlobal.uptime > delay))
  {
    // allow RTC manual update
    Rtc.time_synced = true;

    // set Rtc time from meter local time (year+30 as it starts from 1970)
    tm_meter.year += 30;
    epoch  = MakeTime (tm_meter);
    RtcGetDaylightSavingTimes (epoch);
    offset = RtcTimeZoneOffset (epoch);
    Rtc.utc_time = epoch - offset;

    // set local time from UTC time
    Rtc.time_timezone = RtcTimeZoneOffset(Rtc.utc_time);
    Rtc.local_time = Rtc.utc_time + Rtc.time_timezone;
    Rtc.time_timezone /= 60;
    RtcSetTimeOfDay (Rtc.local_time);

    // set RtcTime
    BreakNanoTime (Rtc.local_time, 0, RtcTime);
    RtcTime.year += 1970;
    RtcTime.valid = 1;

    // allow next NTP update and log
    Rtc.user_time_entry = false;
    AddLog (LOG_LEVEL_INFO, PSTR ("RTC: Date set from meter (%u/%02u/%02u %02u:%02u:%02u)"), tm_meter.year + 1970, tm_meter.month, tm_meter.day_of_month, tm_meter.hour, tm_meter.minute, tm_meter.second);
  }
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

/*********************************************\
 *           Contract and Period
\*********************************************/

// set contract period
bool TeleinfoContractUpdate ()
{
  bool    result = true;
  bool    is_kw;
  int     index;
  char    str_text[64];
  String  str_contract;

  // if message contract code not defined, ignore
  if (strlen (teleinfo_message.str_contract) == 0) result = false;

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
    str_contract = teleinfo_message.str_contract;
//    str_contract.toUpperCase ();
//    strlcpy (str_text, str_contract.c_str (), 4);
//    if (strcmp (str_text, "BBR") == 0) str_contract = str_text;

    // look for known contract
    index = GetCommandCode (str_text, sizeof (str_text), str_contract.c_str (), kTicContractCode);

    // detect non standard TEMPO
    if ((index == -1) && (strstr_P (str_contract.c_str (), PSTR ("TEMPO")) != nullptr)) index = TIC_C_STD_TEMPO;
    if ((index == -1) && (strstr_P (str_contract.c_str (), PSTR ("TEMP0")) != nullptr)) index = TIC_C_STD_TEMPO;

    // if not found, contract unknown
    if (index == -1) index = TIC_C_UNKNOWN;

    // set contract index and code
    teleinfo_contract.index = (uint8_t)index;
    strcpy (teleinfo_contract.str_code, str_contract.c_str ());

    // init current period
    teleinfo_contract.period = UINT8_MAX;
    teleinfo_contract.str_period[0] = 0;

    // load contract periods
    teleinfo_contract.period_qty = TeleinfoContractString2Period (arr_kTicPeriod[index]);

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Nouveau contrat %s, %d périodes connues"), str_contract.c_str (), teleinfo_contract.period_qty);
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

uint8_t TeleinfoPeriodDetectHcHp (const char* pstr_name)
{
  uint8_t hchp = 1;

  // detect HC
  if (strstr_P (pstr_name, PSTR ("HC")) != nullptr) hchp = 0;
    else if (strstr_P (pstr_name, PSTR ("CREUSE")) != nullptr) hchp = 0;

  return hchp;
}

uint8_t TeleinfoPeriodDetectLevel (const char* pstr_name)
{
  uint8_t level = TIC_LEVEL_BLUE;       // by default, blue period

  if (strstr_P (pstr_name, PSTR ("NORMAL"))        != nullptr) level = TIC_LEVEL_BLUE;
    else if (strstr_P (pstr_name, PSTR ("BLEU"))   != nullptr) level = TIC_LEVEL_BLUE;
    else if (strstr_P (pstr_name, PSTR ("BLAN"))   != nullptr) level = TIC_LEVEL_WHITE;
    else if (strstr_P (pstr_name, PSTR ("ROUG"))   != nullptr) level = TIC_LEVEL_RED;
    else if (strstr_P (pstr_name, PSTR ("JB"))     != nullptr) level = TIC_LEVEL_BLUE;
    else if (strstr_P (pstr_name, PSTR ("JW"))     != nullptr) level = TIC_LEVEL_WHITE;
    else if (strstr_P (pstr_name, PSTR ("JR"))     != nullptr) level = TIC_LEVEL_RED;
    else if (strstr_P (pstr_name, PSTR ("PM"))     != nullptr) level = TIC_LEVEL_RED;
    else if (strstr_P (pstr_name, PSTR ("POINTE")) != nullptr) level = TIC_LEVEL_RED;
    else if (strcmp_P (pstr_name, PSTR ("P"))      == 0)       level = TIC_LEVEL_RED;

  return level;
}

// update contract period
void TeleinfoPeriodUpdate ()
{
  bool    new_period = false;
  uint8_t index, period, hchp, level;

  // if message period is undefined, ignore
  if (strlen (teleinfo_message.str_period) == 0) return;

  // look for period index in period code list
  period = UINT8_MAX;
  for (index = 0; index < TIC_INDEX_MAX; index ++)
    if (teleinfo_contract.arr_period[index].str_code == teleinfo_message.str_period) period = index;

  // detect period specs
  hchp  = TeleinfoPeriodDetectHcHp  (teleinfo_message.str_period);
  level = TeleinfoPeriodDetectLevel (teleinfo_message.str_period);

  // if not found and dealing with generic TEMPO contract, set period according to standard Tempo periods
  if ((period == UINT8_MAX) && (teleinfo_contract.index == TIC_C_STD_TEMPO) && (level > TIC_LEVEL_NONE)) period = 2 * level - 2 + hchp;

  // if period not detected, set to messsage period
  if (period == UINT8_MAX) period = teleinfo_message.period;

  // if still ne priod index, ignore
  if (period == UINT8_MAX) return;
  
  // update today's slot
  TeleinfoCalendarSetDailyCalendar (TIC_DAY_TODAY, period);

  // if contract period and message period are identical, ignore
  if (teleinfo_contract.period == period) return;

  // check for an unknown period index
  new_period = (teleinfo_contract.arr_period[period].valid == 0) ;

  // update contract current period
  teleinfo_contract.period = period;
  strcpy (teleinfo_contract.str_period, teleinfo_message.str_period);

  // if a new period is detected, update contract period
  if (new_period)
  {
    // update contract period validity, code and label
    teleinfo_contract.arr_period[period].valid     = 1;
    teleinfo_contract.arr_period[period].str_code  = teleinfo_message.str_period;
    teleinfo_contract.arr_period[period].str_label = teleinfo_message.str_period;

    // update cpntract period level, hchp and period quantity
    teleinfo_contract.arr_period[period].hchp  = hchp;
    teleinfo_contract.arr_period[period].level = level;
    teleinfo_contract.period_qty = max (teleinfo_contract.period_qty, (uint8_t)(period + 1));

    // publish updated calendar
    if (teleinfo_config.calendar) teleinfo_meter.json.data = 1;

    // log
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Nouvelle période %s (index %u, level %u, hchp %u), %u périodes connues"), teleinfo_message.str_period, period + 1, level, hchp, teleinfo_contract.period_qty);

    // save new period
    TeleinfoEnergyConfigSave (false);
  }

  // else log period change
  else  AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Changement de période %s (index %u, level %u, hchp %u)"), teleinfo_contract.str_period, period + 1, teleinfo_contract.arr_period[period].level, teleinfo_contract.arr_period[period].hchp);
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
    if (period < TIC_INDEX_MAX)  GetTextIndexed (str_level, sizeof (str_level), index++, pstr_description);
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
  else if (period >= TIC_INDEX_MAX) result = false;
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

// get period profile
void TeleinfoPeriodGetProfile (char* pstr_profile, size_t size_profile) { TeleinfoPeriodGetProfile (pstr_profile, size_profile, teleinfo_contract.period); }
void TeleinfoPeriodGetProfile (char* pstr_profile, size_t size_profile, const uint8_t period)
{
  char str_level[4];

  // check parameter
  if (pstr_profile == nullptr) return;
  if (size_profile < 6) return;
  if (period >= TIC_INDEX_MAX) return;

  // look for period code according to contract
  if (teleinfo_contract.period_qty < period + 1) GetTextIndexed (pstr_profile, sizeof (pstr_profile), TIC_LEVEL_NONE, kTeleinfoLevelShort);
    else if (teleinfo_contract.arr_period[period].valid == 0) GetTextIndexed (pstr_profile, sizeof (pstr_profile), TIC_LEVEL_NONE, kTeleinfoLevelShort);
    else 
    {
      GetTextIndexed (pstr_profile, sizeof (pstr_profile), teleinfo_contract.arr_period[period].hchp, kTeleinfoHourShort);
      GetTextIndexed (str_level, sizeof (str_level), teleinfo_contract.arr_period[period].level, kTeleinfoLevelShort);
      strcat (pstr_profile, str_level);
    }
}

// get period code
void TeleinfoPeriodGetCode (char* pstr_code, size_t size_code) { TeleinfoPeriodGetCode (pstr_code, size_code, teleinfo_contract.period); }
void TeleinfoPeriodGetCode (char* pstr_code, size_t size_code, const uint8_t period)
{
  // check parameter
  if (pstr_code == nullptr) return;
  if (size_code == 0) return;
  if (period >= TIC_INDEX_MAX) return;

  // look for period code according to contract
  if (teleinfo_contract.period_qty < period + 1) pstr_code[0] = 0;
    else if (teleinfo_contract.arr_period[period].valid == 0) sprintf_P (pstr_code, PSTR ("PERIOD%u"), period + 1);
    else strlcpy (pstr_code, teleinfo_contract.arr_period[period].str_code.c_str (), size_code);
}

// get period name
void TeleinfoPeriodGetLabel (char* pstr_name, const size_t size_name) { TeleinfoPeriodGetLabel (pstr_name, size_name, teleinfo_contract.period); }
void TeleinfoPeriodGetLabel (char* pstr_name, const size_t size_name, const uint8_t period)
{
  // check parameter
  if (pstr_name == nullptr) return;
  if (size_name == 0) return;
  if (period >= TIC_INDEX_MAX) return;

  // look for period label according to contract
  if (teleinfo_contract.period_qty < period + 1) pstr_name[0] = 0;
    else if (teleinfo_contract.arr_period[period].valid == 0) sprintf_P (pstr_name, PSTR ("Période %u"), period + 1);
    else strlcpy (pstr_name, teleinfo_contract.arr_period[period].str_label.c_str (), size_name);
}

// get period level
uint8_t TeleinfoPeriodGetLevel () { return TeleinfoPeriodGetLevel (teleinfo_contract.period); }
uint8_t TeleinfoPeriodGetLevel (const uint8_t period)
{
  uint8_t value;

  if (period >= TIC_INDEX_MAX) value = TIC_LEVEL_NONE;
    else if (teleinfo_contract.period_qty < period + 1) value = TIC_LEVEL_NONE;
    else if (teleinfo_contract.arr_period[period].valid == 0) value = TIC_LEVEL_NONE;
    else value = teleinfo_contract.arr_period[period].level;

  return value;
}

// get period HP status
uint8_t TeleinfoPeriodGetHP () { return TeleinfoPeriodGetHP (teleinfo_contract.period); }
uint8_t TeleinfoPeriodGetHP (const uint8_t period)
{
  uint8_t value;

  if (period >= TIC_INDEX_MAX) value = 1;
    else if (teleinfo_contract.period_qty < period + 1) value = 1;
    else if (teleinfo_contract.arr_period[period].valid == 0) value = 1;
    else value = teleinfo_contract.arr_period[period].hchp;

  return value;
}

void TeleinfoPeriodReset (const uint8_t period)
{
  // check parameter
  if (period >= TIC_INDEX_MAX) return;

  // reset validity
  teleinfo_contract.arr_period[period].valid     = 0;
  teleinfo_contract.arr_period[period].level     = TIC_LEVEL_NONE;
  teleinfo_contract.arr_period[period].hchp      = 1;
  teleinfo_contract.arr_period[period].str_code  = "";
  teleinfo_contract.arr_period[period].str_label = "";
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

void TeleinfoUpdateCosphi (long cosphi, struct tic_cosphi &struct_cosphi, const long papp)
{
  uint8_t index;
  long    page, page_size, page_low, page_high;
  long    result, sample;

  // ignore first measure as duration was not good
  struct_cosphi.quantity++;
  if (struct_cosphi.quantity == 1) return;

  // if contract not defined, ignore
  if (teleinfo_contract.ssousc == 0) return;

  // calculate numbre of messsages to get cosphi
  result = teleinfo_meter.nb_message - struct_cosphi.nb_message;
  struct_cosphi.nb_message = teleinfo_meter.nb_message;

  // cosphi under 0.05 is lowered to 0 (used in production mode)
  if (cosphi < 50) cosphi = 0;

  // caculate current page limoits (with acceptable delta of 50VA)
  page      = struct_cosphi.page;
  page_size = teleinfo_contract.ssousc / TIC_COSPHI_PAGE;
  page_low  = page_size * page - 50;
  page_high = page_low + page_size + 50;

  // if apparent power is outside of current page, calculate new power page
  if ((papp < page_low) || (papp > page_high))
  {
    page = papp * TIC_COSPHI_PAGE / teleinfo_contract.ssousc;
    if (page >= TIC_COSPHI_PAGE) page = TIC_COSPHI_PAGE - 1;
  }

  // log for debug
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: [upd] %d msg, %d ms, %d mwah, %d mwh"), result, teleinfo_message.duration, teleinfo_conso.delta_mvah, teleinfo_conso.delta_mwh);
  AddLog (LOG_LEVEL_DEBUG, PSTR ("TIC: [cos] %d VA, %d page, new %d, avg %d"), papp, page, cosphi, struct_cosphi.value);


  // if page has changed, load array of values for new page (current cosphi is ignored as it is a transition one)
  if (struct_cosphi.page != page)
  {
    struct_cosphi.index = TIC_COSPHI_SAMPLE / 2;
    for (index = 0; index < struct_cosphi.index; index++) struct_cosphi.arr_value[index] = struct_cosphi.arr_page[page];
    for (index = struct_cosphi.index; index < TIC_COSPHI_SAMPLE; index++) struct_cosphi.arr_value[index] = LONG_MAX;
  }

  // if page has not changed, update array of values
  else
  {
    if (struct_cosphi.index >= TIC_COSPHI_SAMPLE) struct_cosphi.index = 0;
    struct_cosphi.arr_value[struct_cosphi.index] = cosphi;
    struct_cosphi.index++;
  }

  // calculate current cosphi average
  result = 0;
  sample = 0;
  for (index = 0; index < TIC_COSPHI_SAMPLE; index++) if (struct_cosphi.arr_value[index] != LONG_MAX)
  {
    result += struct_cosphi.arr_value[index];
    sample++;
  }
  if (sample > 0) struct_cosphi.value = min (1000L, result / sample);

  // update current page and value
  struct_cosphi.page           = page;
  struct_cosphi.arr_page[page] = struct_cosphi.value;
}

uint32_t TeleinfoDetectTeleinfo ()
{
  bool     detect = true;
  uint8_t  pin_rx, level_now, level_prev;
  uint16_t win_count, win_level;
  uint32_t baudrate, time_stop;
  int64_t  ts_low, ts_length, ts_mini, ts_window;

  // if TinfoRx pin not set, return current baud rate
  if (!PinUsed (GPIO_TELEINFO_RX)) return TasmotaGlobal.baudrate;

  // init levels
  pin_rx = Pin (GPIO_TELEINFO_RX);
  pinMode (pin_rx, INPUT);  
  level_now  = digitalRead (pin_rx);
  level_prev = level_now;

  // init timings
  time_stop = millis () + 500;

  // loop to detect speed
  ts_low = ts_mini = 0;
  while (detect)
  {
    // collect data on a 34 µs window (1/3 of 9600bps bit length)
    win_count = win_level = 0;
    ts_window = esp_timer_get_time () + 34;
    while (ts_window > esp_timer_get_time ())
    {
      if (digitalRead (pin_rx) == LOW) win_level++;
      win_count++;
    }

    // average data collected on the xindow (to avoid spikes)
    if (win_level > win_count / 2) level_now = LOW;
      else level_now = HIGH;

    // handle switch to low state
    if ((level_now == LOW) && (level_prev == HIGH)) ts_low = esp_timer_get_time ();

    // handle switch to high state
    else if ((level_now == HIGH) && (level_prev == LOW) && (ts_low > 0))
    {
      // calculate low level duration
      ts_length = esp_timer_get_time () - ts_low;

      // update duration, dropping first one
      if (ts_mini == 0) ts_mini = ts_length;
        else ts_mini = min (ts_mini, ts_length);

      // update data
      ts_low = 0;
    }

    // else if no capture running, check if time to exit or to yield 
    else if (ts_low == 0)
    {
      if (millis () >= time_stop) detect = false;
    }

    // update previous level
    level_prev = level_now;
  }

  // calculate baud rate according to low level signal length (19200 = 52 µs, 9600 = 104 µs, 4800 = 208 µs, 2400 = 416 µs and 1200 = 833 µs)
  if (ts_mini == 0) baudrate = 115200;
    else if (ts_mini <= 170) baudrate = 9600;
    else if (ts_mini <= 340) baudrate = 4800;
    else if (ts_mini <= 646) baudrate = 2400;
    else baudrate = 1200;

  // log
  if (baudrate == 115200) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Teleinfo speed not detected"));
    else AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Teleinfo detected at %u bauds (%u µs)"), baudrate, (uint32_t)ts_mini);

  return baudrate;
}

/*
uint32_t TeleinfoDetectTeleinfo ()
{
  bool     detect = true;
  uint8_t  count, pin_rx, level_now, level_prev;
  uint16_t win_count, win_level;
  uint32_t baudrate, time_stop, time_yield, time_now;
  int64_t  ts_low, ts_length, ts_minimum, ts_window;

  // init
  baudrate = 115200;

  // if TinfoRx pin not set, return current baud rate
  if (!PinUsed (GPIO_TELEINFO_RX)) return baudrate;

  // init levels
  pin_rx = Pin (GPIO_TELEINFO_RX);
  pinMode (pin_rx, INPUT);  
  level_now  = digitalRead (pin_rx);
  level_prev = level_now;

  // init timings
  time_now   = millis ();
  time_yield = time_now + 500;
  time_stop  = time_now + 2000;

  // loop to detect speed
  count = 0;
  ts_low = 0;
  ts_minimum = 0;
  while (detect)
  {
    // collect data on 10 µs window
    win_count = win_level = 0;
    ts_window = esp_timer_get_time () + 30;
    while (ts_window > esp_timer_get_time ())
    {
      if (digitalRead (pin_rx) == LOW) win_level++;
      win_count++;
    }

    // set level according to average read
    if (win_level > win_count / 2) level_now = LOW;
      else level_now = HIGH;

    // handle switch to low state
    if ((level_now == LOW) && (level_prev == HIGH)) ts_low = esp_timer_get_time ();

        // handle switch to high state
    else if ((level_now == HIGH) && (level_prev == LOW) && (ts_low > 0))
    {
      // calculate low level duration
      ts_length = esp_timer_get_time () - ts_low;

      // update duration, dropping first one
      if (ts_minimum == 0) ts_minimum = ts_length;
        else ts_minimum = min (ts_minimum, ts_length);

      // update data
      ts_low = 0;
      count ++;
    }

    // else if no capture running, check if time to exit or to yield 
    else if (ts_low == 0)
    {
      time_now = millis ();
      if (count > 10) detect = false;
      else if (time_now >= time_stop) detect = false;
      else if (time_now >= time_yield) { yield (); time_yield = time_now + 500; }
    }

    // update previous level
    level_prev = level_now;
  }

  // calculate baud rate according to low level signal length (19200 = 52 µs, 9600 = 104 µs, 4800 = 208 µs, 2400 = 416 µs and 1200 = 833 µs)
  if (ts_minimum == 0) baudrate = 115200;
    else if (ts_minimum < 78) baudrate = 19200;
    else if (ts_minimum < 156) baudrate = 9600;
    else if (ts_minimum < 312) baudrate = 4800;
    else if (ts_minimum < 624) baudrate = 2400;
    else baudrate = 1200;

  // log
  if (baudrate == 115200) AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Teleinfo speed not detected"));
    else AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Teleinfo detected at %u bauds (%u µs)"), baudrate, (uint32_t)ts_minimum);

  return baudrate;
}
*/

/*********************************************\
 *                   Callback
\*********************************************/

// Teleinfo GPIO initilisation
void TeleinfoEnergyPreInit ()
{
  uint32_t baudrate;

  // if needed, detect teleinfo speed (only on ESP32 family)
  if (TasmotaGlobal.baudrate == 115200) baudrate = TeleinfoDetectTeleinfo ();
    else baudrate = TasmotaGlobal.baudrate;

  // if new speed detected, save settings
  if (baudrate != TasmotaGlobal.baudrate)
  {
    TasmotaGlobal.baudrate = baudrate;
    Settings->baudrate = (uint16_t)(baudrate / 300);
    SettingsSave (0);
  }
  
  // declare energy driver
  TasmotaGlobal.energy_driver = XNRG_15;
  AddLog (LOG_LEVEL_DEBUG, PSTR("TIC: Teleinfo driver enabled"));

  // if baud rate is valid, start serial reception
  if (TasmotaGlobal.baudrate <= 19200) TeleinfoSerialStart ();
}

// Teleinfo driver initialisation
void TeleinfoEnergyInit ()
{
  uint8_t index, phase;

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
  memset (&teleinfo_meter.json, 0, sizeof (tic_json));

  // meter : stge data
  memset (&teleinfo_message.stge, 0, sizeof (tic_stge));
  teleinfo_meter.preavis.level = TIC_PREAVIS_NONE;
  teleinfo_meter.preavis.timeout = UINT32_MAX;
  teleinfo_meter.preavis.str_label[0] = 0;

  // init calendar slots to 0
  for (index = TIC_DAY_TODAY; index < TIC_DAY_MAX; index ++) teleinfo_calendar[index].level = TIC_LEVEL_NONE;

  // message data
  teleinfo_message.timestamp_last = UINT32_MAX;
  teleinfo_message.str_line[0]    = 0;
  for (index = 0; index < TIC_LINE_QTY; index ++) 
  {
    // init current message data
    teleinfo_message.arr_line[index].str_etiquette[0] = 0;
    teleinfo_message.arr_line[index].str_donnee[0]    = 0;
    teleinfo_message.arr_line[index].checksum         = 0;

    // init last message data
    teleinfo_message.arr_last[index].str_etiquette[0] = 0;
    teleinfo_message.arr_last[index].str_donnee[0]    = 0;
    teleinfo_message.arr_last[index].checksum         = 0;
  }

  // conso data
  teleinfo_conso.total_wh          = 0;
  teleinfo_conso.cosphi.quantity   = 0;
  teleinfo_conso.cosphi.index      = 0;
  teleinfo_conso.cosphi.page       = 0;
  teleinfo_conso.cosphi.nb_message = 0;
  teleinfo_conso.cosphi.value      = TIC_COSPHI_DEFAULT;     
  for (index = 0; index < TIC_INDEX_MAX;     index++) teleinfo_conso.index_wh[index]         = 0;
  for (index = 0; index < TIC_COSPHI_PAGE;   index++) teleinfo_conso.cosphi.arr_page[index]  = TIC_COSPHI_DEFAULT;
  for (index = 0; index < TIC_COSPHI_SAMPLE; index++) teleinfo_conso.cosphi.arr_value[index] = LONG_MAX;

  // prod data
  teleinfo_prod.total_wh          = 0;
  teleinfo_prod.cosphi.quantity   = 0;
  teleinfo_prod.cosphi.index      = 0;
  teleinfo_prod.cosphi.page       = 0;
  teleinfo_prod.cosphi.nb_message = 0;
  teleinfo_prod.cosphi.value      = TIC_COSPHI_DEFAULT;
  for (index = 0; index < TIC_COSPHI_PAGE;   index++) teleinfo_prod.cosphi.arr_page[index]  = TIC_COSPHI_DEFAULT;
  for (index = 0; index < TIC_COSPHI_SAMPLE; index++) teleinfo_prod.cosphi.arr_value[index] = LONG_MAX;

  // conso data per phase
  for (phase = 0; phase < TIC_PHASE_MAX; phase++)
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
    teleinfo_conso.phase[phase].pact_last = 0;
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
  uint8_t index;

  // set next stage
  teleinfo_meter.reception = TIC_RECEPTION_MESSAGE;

  // reset error flag, line index and current line content
  teleinfo_message.error = 0;
  teleinfo_message.line_idx = 0;
  teleinfo_message.injection = 0;
  teleinfo_message.str_line[0] = 0;

  // init calendar data                                                                                          // current slot
  for (index = 0; index < TIC_POINTE_MAX; index ++) memset (&teleinfo_message.arr_pointe[index], 0, sizeof (tic_pointe));     // pointe dates
  teleinfo_message.stge.pointe  = 0;
  teleinfo_message.stge.preavis = 0;

  // init contract data
  teleinfo_message.period = UINT8_MAX;
  teleinfo_message.str_period[0] = 0;
  strlcpy (teleinfo_message.str_contract, teleinfo_contract.str_code, sizeof (teleinfo_message.str_contract));

  // reset voltage flags
  for (index = 0; index < teleinfo_contract.phase; index ++) teleinfo_conso.phase[index].volt_set = false;
}

// Conso : calcultate Cosphi and Active Power 
//   from Instant Apparent Power (VA) and Active Power counters (Wh)
void TeleinfoConsoCalculateActivePower_VA_Wh ()
{
  uint8_t phase;
  long    sinsts, papp, current, cosphi, total_papp;

  // calculate total current
  current = 0;
  for (phase = 0; phase < teleinfo_contract.phase; phase++) current += teleinfo_conso.phase[phase].current;

  // check if sinsts per phase should be used (+/- 5VA difference with PAPP)
  if (!teleinfo_meter.use_sinsts && (teleinfo_contract.mode == TIC_MODE_STANDARD) && (teleinfo_conso.papp > 5))
  {
    sinsts = 0;
    for (phase = 0; phase < teleinfo_contract.phase; phase++) sinsts += teleinfo_conso.phase[phase].sinsts;
    if (abs (sinsts - teleinfo_conso.papp) <= 5) teleinfo_meter.use_sinsts = 1;
  }  

  // loop thru phase to update apparent power
  total_papp = 0;
  for (phase = 0; phase < teleinfo_contract.phase; phase++)
  {
    // calculate new apparent power
    papp = teleinfo_conso.phase[phase].papp;
    if (teleinfo_meter.use_sinsts) papp = teleinfo_conso.phase[phase].sinsts;
    else if (current > 0) papp = teleinfo_conso.papp * teleinfo_conso.phase[phase].current / current;
    else papp = teleinfo_conso.papp / teleinfo_contract.phase;

    // update average and total apparent power
    teleinfo_conso.phase[phase].papp = papp;
    total_papp += papp;

    // update mvah increment (order is important not to overflow long max)
    teleinfo_conso.delta_mvah += teleinfo_message.duration * papp / 36;
  }

  // if active power has increased
  if (teleinfo_conso.delta_mvah > 0)
  {
    // if global counter increased
    if (teleinfo_conso.delta_mwh > 0)
    {
      // calculate new cosphi
      cosphi = 1000 * 100 * teleinfo_conso.delta_mwh / teleinfo_conso.delta_mvah;
      TeleinfoUpdateCosphi (cosphi, teleinfo_conso.cosphi, total_papp);
    }

    // else try to forecast cosphi evolution
    else
    {
      // forecast cosphi of next increase of 1 Wh
      cosphi = 1000 * 100 * 1000 / teleinfo_conso.delta_mvah;

      // if forecast is lower than current cosphi, update
      if (cosphi <= teleinfo_conso.cosphi.value) TeleinfoUpdateCosphi (cosphi, teleinfo_conso.cosphi, total_papp);
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
      TeleinfoUpdateCosphi (cosphi, teleinfo_conso.cosphi, teleinfo_conso.papp);

      // increase counter
      teleinfo_conso.cosphi.quantity++;
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
      // calculatation, averaging with last value 
      cosphi = 1000 * teleinfo_conso.phase[0].pact / teleinfo_conso.phase[0].papp;
      TeleinfoUpdateCosphi (cosphi, teleinfo_conso.cosphi, teleinfo_conso.phase[0].papp);

      // update counter
      teleinfo_conso.cosphi.quantity++;
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

  // if total is O, ignore
  if (teleinfo_prod.total_wh == 0) return;

  // add apparent power increment according to message time window
  teleinfo_prod.delta_mvah += teleinfo_message.duration * teleinfo_prod.papp / 36;
//  teleinfo_conso.delta_mvah += teleinfo_message.duration / 1000 * teleinfo_prod.papp / 36;

  // if active power has increased 
  if (teleinfo_prod.delta_mvah > 0)
  {
    // if global counter increased, calculate new cosphi
    if (teleinfo_prod.delta_mwh > 0)
    {
      cosphi = 1000 * 100 * teleinfo_prod.delta_mwh / teleinfo_prod.delta_mvah;
      TeleinfoUpdateCosphi (cosphi, teleinfo_prod.cosphi, teleinfo_prod.papp);
    }

    // else try to forecast cosphi drop in case of very low active power production (photovoltaic router ...)
    else
    {
      cosphi = 1000 * 100 * 1000 / teleinfo_prod.delta_mvah;
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

void TeleinfoProdCalculateAverageActivePower ()
{
  uint8_t relay;
  float   power;

  // if no production, ignore
  if (teleinfo_prod.total_wh == 0) return;

  // calculate average active production power
  power = (float)teleinfo_prod.pact;
  if (teleinfo_prod.pact_avg == 0) teleinfo_prod.pact_avg = power;
    else teleinfo_prod.pact_avg = (teleinfo_prod.pact_avg * (TIC_AVERAGE_PROD_SAMPLE - 1) + power) / TIC_AVERAGE_PROD_SAMPLE;

  // save current relay state
  relay = teleinfo_prod.relay;

  // if average production is beyond trigger, production relay should be ON
  if ((long)teleinfo_prod.pact_avg >= teleinfo_config.prod_trigger) teleinfo_prod.relay = 1;

  // else if production average is down 100w, production relay should be OFF
  else if (teleinfo_prod.pact_avg < 100) teleinfo_prod.relay = 0;

  // if relay state has changed, ask for JSON publication
  if (relay != teleinfo_prod.relay) teleinfo_meter.json.data = 1;
}

// ----------------------
//   message management
// ----------------------

// handle end of message
void TeleinfoReceptionMessageStop ()
{
  bool     delta_va = false;
  uint8_t  index, phase;
  uint32_t timestamp;
  long     value, duration, quantity, delta;
  
  // set next stage and get current timestamp
  teleinfo_meter.reception = TIC_RECEPTION_NONE;
  timestamp = millis ();

  // handle contract type and period update
  if (TeleinfoContractUpdate ()) TeleinfoPeriodUpdate ();

  // if at least one full message received,
  if (teleinfo_message.timestamp_last != UINT32_MAX) 
  {
    // calculate duration with previous message
    duration = (long)TimeDifference (teleinfo_message.timestamp_last, timestamp);
    delta    = duration - teleinfo_message.duration;
    quantity = min (teleinfo_meter.nb_message - 1, 49L);

    // average messages on maximum 50 samples
    if (delta < -quantity) teleinfo_message.duration = (duration + (quantity * teleinfo_message.duration)) / (quantity + 1);
    else if (delta < 0) teleinfo_message.duration--;
    else if (delta > quantity) teleinfo_message.duration = (duration + (quantity * teleinfo_message.duration)) / (quantity + 1);
    else if (delta > 0) teleinfo_message.duration++;
  }

  // update message timestamp and increment messages counter
  teleinfo_message.timestamp_last = timestamp;
  teleinfo_meter.nb_message++;

  // ----------------------
  // counter global indexes

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

  // ----------------------
  // counter global indexes

  // if needed, calculate max contract power from max phase current
  if ((teleinfo_contract.ssousc == 0) && (teleinfo_contract.isousc != 0)) teleinfo_contract.ssousc = teleinfo_contract.isousc * TIC_VOLTAGE_REF;

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

  TeleinfoProdCalculateActivePower_VA_Wh ();

  // ---------------------------------------
  //    calculate average active power 
  // ---------------------------------------

  TeleinfoProdCalculateAverageActivePower ();

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
    }

    // else if % power change detected
    else if ((teleinfo_config.policy == TIC_POLICY_DELTA) && (Settings->energy_power_delta[0] > 0))
    {
      // get delta power for dynamic publication
      value = (long)Settings->energy_power_delta[0];

      // loop thru phase to detect % power change (should be > to handle 0 conso)
      for (phase = 0; phase < teleinfo_contract.phase; phase++) 
        if (abs (teleinfo_conso.phase[phase].pact_last - teleinfo_conso.phase[phase].pact) >= value) delta_va = true;

      // detect % power change on production (should be > to handle 0 prod)
      if (abs (teleinfo_prod.papp_last - teleinfo_prod.papp) >= value) delta_va = true;

      // if energy has changed more than the trigger, publish
      if (delta_va)
      {
        // update phases reference
        for (phase = 0; phase < teleinfo_contract.phase; phase++) teleinfo_conso.phase[phase].pact_last = teleinfo_conso.phase[phase].pact;
        teleinfo_prod.papp_last = teleinfo_prod.papp;

        // if deepsleep not enabled
        if (Settings->deepsleep == 0) teleinfo_meter.json.data = 1;
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

// handle start of line
void TeleinfoReceptionLineStart ()
{
  // set next stage
  teleinfo_meter.reception = TIC_RECEPTION_LINE;

  // reset line content
  teleinfo_message.str_line[0] = 0;
}

// handle end of line
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
        break;

      // identifiant compteur mode standard
      case TIC_UKN_ADSC:
        teleinfo_contract.mode  = TIC_MODE_STANDARD;
        AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Mode %s"), GetTextIndexed (str_text, sizeof (str_text), TIC_MODE_STANDARD, kTeleinfoModeName));
        break;

      // identifiant compteur PME/PMI
      case TIC_UKN_ADS:
        teleinfo_contract.mode  = TIC_MODE_PMEPMI;
        AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Mode %s"), GetTextIndexed (str_text, sizeof (str_text), TIC_MODE_PMEPMI, kTeleinfoModeName));
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
        TeleinfoTimestampFromDate (str_donnee);
        break;

      //   Identifiant compteur
      // -----------------------
      case TIC_HIS_ADCO:
      case TIC_STD_ADSC:
        TeleinfoMeterUpdateType (str_donnee, true);
        break;

      case TIC_PME_ADS:
        TeleinfoMeterUpdateType (str_donnee, false);
        break;

      //   Contract period
      // -------------------

      case TIC_HIS_OPTARIF:
        // handle specificity of Historique Tempo where last char is dynamic (BBRx)
        strlcpy (teleinfo_message.str_contract, str_donnee, 4);
        if (strcmp_P (teleinfo_message.str_contract, PSTR ("BBR")) != 0) strlcpy (teleinfo_message.str_contract, str_donnee, sizeof (teleinfo_message.str_contract));

        // set date according to RTC time as there is no timestamp in historic mode
        if (RtcTime.valid) TeleinfoCalendarSetDate (RtcTime.year % 100, RtcTime.month, RtcTime.day_of_month, RtcTime.hour, RtcTime.minute);
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

      // period index
      case TIC_STD_NTARF:
        value = atol (str_donnee);
        if (value > 0) teleinfo_message.period = (uint8_t)value - 1;
        break;

      // period name
      case TIC_HIS_PTEC:
      case TIC_STD_LTARF:
      case TIC_PME_PTCOUR1:
      case TIC_EME_PTCOUR:
        strlcpy (teleinfo_message.str_period, str_donnee, sizeof (teleinfo_message.str_period));
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
        if (teleinfo_contract.period == 0) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSPM:
        if (teleinfo_contract.period == 1) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHPH:
        if (teleinfo_contract.period == 2) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHPD:
        if (teleinfo_contract.period == 3) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHCH:
        if (teleinfo_contract.period == 4) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHCD:
        if (teleinfo_contract.period == 5) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHPE:
        if (teleinfo_contract.period == 6) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHCE:
        if (teleinfo_contract.period == 7) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSJA:
        if (teleinfo_contract.period == 8) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHH:
        if (teleinfo_contract.period == 9) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHD:
        if (teleinfo_contract.period == 10) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSHM:
        if (teleinfo_contract.period == 11) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSDSM:
        if (teleinfo_contract.period == 12) TeleinfoContractPowerUpdate (str_donnee);
        break;

      case TIC_EME_PSSCM:
        if (teleinfo_contract.period == 13) TeleinfoContractPowerUpdate (str_donnee);
        break;

      //   Counters
      // ------------

      // counter according to current period
      case TIC_PME_EAPS:
        TeleinfoConsoIndexCounterUpdate (str_donnee, teleinfo_contract.period);
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
        TeleinfoPreavisSet (TIC_PREAVIS_DANGER, "DEP");
        break;

      case TIC_HIS_ADIR1:
        TeleinfoPreavisSet (TIC_PREAVIS_DANGER, "DEP1");
      break;

      case TIC_HIS_ADIR2:
        TeleinfoPreavisSet (TIC_PREAVIS_DANGER, "DEP2");
        break;

      case TIC_HIS_ADIR3:
        TeleinfoPreavisSet (TIC_PREAVIS_DANGER, "DEP3");
        break;

      //   Calendar
      // ------------

      case TIC_HIS_PEJP:
        TeleinfoPreavisSet (TIC_PREAVIS_ALERT, "EJP");
        break;

      case TIC_HIS_DEMAIN:
        TeleinfoCalendarSetDemain (str_donnee);
        break;

      case TIC_PME_PREAVIS:
      case TIC_EME_PREAVIS:
        TeleinfoPreavisSet (TIC_PREAVIS_ALERT, str_donnee);
        break;

      // begin of next pointe period
      case TIC_STD_DPM1:
        TeleinfoCalendarPointeBegin (0, str_donnee);
        break;
      case TIC_STD_DPM2:
        TeleinfoCalendarPointeBegin (1, str_donnee);
        break;
      case TIC_STD_DPM3:
        TeleinfoCalendarPointeBegin (2, str_donnee);
        break;

      // end of next pointe period
      case TIC_STD_FPM1:
        TeleinfoCalendarPointeEnd (0, str_donnee);
        break;
      case TIC_STD_FPM2:
        TeleinfoCalendarPointeEnd (1, str_donnee);
        break;
      case TIC_STD_FPM3:
        TeleinfoCalendarPointeEnd (2, str_donnee);
        break;
        
      // day standard profile
      case TIC_STD_PJOURF1:
        TeleinfoCalendarDefaultProfile (str_donnee);
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
        if ((relay != teleinfo_conso.relay) && TeleinfoDriverMeterReady ()) teleinfo_meter.json.data = 1;
        teleinfo_conso.relay = relay;
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

// get relay status
uint8_t TeleinfoRelayStatus (const uint8_t index)
{
  uint8_t result;

  result = teleinfo_conso.relay >> index;
  result = result & 0x01;

  return result;
}

// Treatments called every second
void TeleinfoEnergyEverySecond ()
{
  uint8_t  phase;
  uint32_t time_now;
  TIME_T   time_dst;

  // check time validity
  if (!RtcTime.valid) return;

  // get current time
  time_now = LocalTime ();
  BreakTime (time_now, time_dst);

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
  if (time_dst.second % 30 == 0) EnergyUpdateToday ();
#endif    // USE_WINKY 

  //   Midnight day change
  // -----------------------

  // if day change, update daily counters and rotate daily calendars
  if (teleinfo_meter.day == 0) teleinfo_meter.day = time_dst.day_of_month;
  if (teleinfo_meter.day != time_dst.day_of_month)
  {
    TeleinfoEnergyTotalMidnight ();
    TeleinfoEnergyCalendarMidnight ();
    AddLog (LOG_LEVEL_INFO, PSTR ("TIC: Midnight shift (%u->%u)"), teleinfo_meter.day, time_dst.day_of_month);
  }
  teleinfo_meter.day = time_dst.day_of_month;

  //   Reset Preavis & Pointe data
  // -------------------------------

  // check if preavis is over
  if (teleinfo_meter.preavis.timeout < time_now) TeleinfoPreavisSet (TIC_PREAVIS_NONE, "");
}

/***************************************\
 *              Interface
\***************************************/

bool Xnrg15 (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_COMMAND:
      result = TeleinfoEnergyHandleCommand ();
      break;

    case FUNC_PRE_INIT:
      TeleinfoEnergyPreInit ();
      break;

    case FUNC_INIT:
      TeleinfoEnergyInit ();
      break;

    case FUNC_ENERGY_EVERY_SECOND:
      TeleinfoEnergyEverySecond ();
      break;
  }

  return result;
}

#endif      // USE_TELEINFO
#endif      // USE_ENERGY_SENSOR

