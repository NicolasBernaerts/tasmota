/*
xdrv_98_14_teleinfo_prometheus.ino - Publish Prometheus metrics

  Copyright (C) 2026  Nicolas Bernaerts
    04/01/2026 v1.0 - Creation

  Prometheus metrics are available thru :

    your.device.ip.addr/api/prometheus/metrics

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

/**********************************************************\
 *             Prometheus Metrics API
\**********************************************************/

#ifdef USE_TELEINFO
#ifdef USE_TELEINFO_PROMETHEUS

// path and url
#define TIC_PROMETHEUS_PAGE_METRICS   "/api/prometheus/metrics"

/***********************************************\
 *                  Functions
\***********************************************/

char* TeleinfoPrometheusCleanupString (char *pstr_string)
{
  bool is_ok;
  char *pstr_letter;

  // check parameter
  if (pstr_string == nullptr) return nullptr;

  // loop to cleanup string
  pstr_letter = pstr_string;
  while (*pstr_letter != 0)
  { 
    is_ok  = ((*pstr_letter >= 48) && (*pstr_letter <=  57));               // digit
    if (!is_ok) is_ok = ((*pstr_letter >= 65) && (*pstr_letter <=  90));    // upper
    if (!is_ok) is_ok = ((*pstr_letter >= 97) && (*pstr_letter <= 122));    // lower
    if (!is_ok) *pstr_letter = '_';
    pstr_letter++;
  }

  return pstr_string;
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// Append METER and PROD
void TeleinfoPrometheusAppendMeter ()
{
  uint8_t phase, value;
  long    voltage, current, power_app, power_act;

  // METER basic data
  WSContentSend_P (PSTR ("phase_number %u\n"), teleinfo_contract.phase);
  WSContentSend_P (PSTR ("current_max_amperes %u\n"), teleinfo_contract.isousc);
  WSContentSend_P (PSTR ("power_max_voltamperes %u\n"), teleinfo_contract.ssousc);

  // conso 
  if (teleinfo_conso.enabled)
  {
    // conso : loop thru phases
    voltage = 0;
    current = 0;
    power_app = 0;
    power_act = 0;
    for (phase = 0; phase < teleinfo_contract.phase; phase++)
    {
      // calculate parameters
      voltage   += teleinfo_conso.phase[phase].voltage;
      current   += teleinfo_conso.phase[phase].current;
      power_app += teleinfo_conso.phase[phase].papp;
      power_act += teleinfo_conso.phase[phase].pact;

      // if needed, phase data
      if (teleinfo_contract.phase > 1) 
      {
        value = phase + 1;
        WSContentSend_P (PSTR ("phase%u_voltage_volts_u%u %d\n"), value, teleinfo_conso.phase[phase].voltage);
        WSContentSend_P (PSTR ("phase%u_current_amperes %d.%03d\n"), value, teleinfo_conso.phase[phase].current / 1000, teleinfo_conso.phase[phase].current % 1000);
        WSContentSend_P (PSTR ("phase%u_apparent_voltamperes %d\n"), value, teleinfo_conso.phase[phase].papp);
        WSContentSend_P (PSTR ("phase%u_active_watts %d\n"), value, teleinfo_conso.phase[phase].pact);
      }
    } 

    // conso : values and cosphi
    if (teleinfo_contract.phase > 1) voltage = voltage / (long)teleinfo_contract.phase;
    WSContentSend_P (PSTR ("conso_voltage_volts %d\n"), voltage);
    WSContentSend_P (PSTR ("conso_current_amperes %d.%03d\n"), current / 1000, current % 1000);
    WSContentSend_P (PSTR ("conso_apparent_voltamperes %d\n"), power_app);
    WSContentSend_P (PSTR ("conso_active_watts %d\n"), power_act);
    if (teleinfo_conso.cosphi.quantity >= TIC_COSPHI_MIN) WSContentSend_P (PSTR ("conso_cosphi_ratio %d.%02d\n"), teleinfo_conso.cosphi.value / 1000, teleinfo_conso.cosphi.value % 1000 / 10);

    // conso : total of yesterday and today
    WSContentSend_P (PSTR ("conso_yesterday_watthours %d\n"), teleinfo_conso_wh.yesterday);
    WSContentSend_P (PSTR ("conso_today_watthours %d\n"), teleinfo_conso_wh.today);
  }
  
  // production 
  if (teleinfo_prod.enabled)
  {
    // prod : global values
    WSContentSend_P (PSTR ("prod_apparent_voltamperes %d\n"), teleinfo_prod.papp);
    WSContentSend_P (PSTR ("prod_active_watts %d\n"), teleinfo_prod.pact);

    // prod : cosphi
    if (teleinfo_prod.cosphi.quantity >= TIC_COSPHI_MIN) WSContentSend_P (PSTR ("prod_cosphi_ratio %d.%02d\n"), teleinfo_prod.cosphi.value / 1000, teleinfo_prod.cosphi.value % 1000 / 10);

    // prod : average power
    WSContentSend_P (PSTR ("prod_average_watts %d\n"), (long)teleinfo_prod.pact_avg);

    // prod : total of yesterday and today
    WSContentSend_P (PSTR ("prod_yesterday_watthours %d\n"), teleinfo_prod_wh.yesterday);
    WSContentSend_P (PSTR ("prod_today_watthours %d\n"), teleinfo_prod_wh.today);
  }
}

// Append CONTRACT
void TeleinfoPrometheusAppendGeneral ()
{
  uint8_t index;
  char    str_value[16];
  char    str_serial[16];
  char    str_contract[24];
  char    str_period[24];

  // meter serial number
  strcpy_P (str_value, PSTR (EXTENSION_VERSION));
  lltoa (teleinfo_meter.ident, str_serial, 10);
  TeleinfoContractGetName (str_contract, sizeof (str_contract));
  TeleinfoPrometheusCleanupString (str_contract);
  TeleinfoPeriodGetLabel (str_period, sizeof (str_period));
  TeleinfoPrometheusCleanupString (str_period);

  // meter general data
  WSContentSend_P (PSTR ("meter_info{version=\"%s\",serial=\"%s\",contract=\"%s\",period=\"%s\"} 1\n"), str_value, str_serial, str_contract, str_period);

  // conso
  if (teleinfo_conso.enabled)
  {
    // contract period
    WSContentSend_P (PSTR ("meter_period_index %u\n"), teleinfo_contract.period + 1);

    // period level
    WSContentSend_P (PSTR ("meter_level_index %u\n"), TeleinfoPeriodGetLevel ());

    // period type
    WSContentSend_P (PSTR ("meter_hchp_index %u\n"), TeleinfoPeriodGetHP ());

    // total conso counter
    lltoa (teleinfo_conso_wh.total, str_value, 10);
    WSContentSend_P (PSTR ("conso_watthours_total %s\n"), str_value);

    // loop to publish conso counters
    for (index = 0; index < teleinfo_contract_db.period_qty; index ++)
    {
      lltoa (teleinfo_conso_wh.index[index], str_value, 10);
      WSContentSend_P (PSTR ("period%u_watthours_total %s\n"), index + 1, str_value);
    }
  }

  // prod
  if (teleinfo_prod.enabled)
  {
    // total production counter
    lltoa (teleinfo_prod_wh.total, str_value, 10) ;
    WSContentSend_P (PSTR ("prod_watthours_total %s\n"), str_value);
  }
}

// Append RELAY
void TeleinfoPrometheusAppendRelay ()
{
  bool    status, first;
  uint8_t index, level, hchp;
  char    str_name[32];

  // production relay
  if (teleinfo_prod.enabled)
  {
    WSContentSend_P (PSTR ("relay_prod_state %u\n"), teleinfo_prod.relay);
    WSContentSend_P (PSTR ("relay_prod_trigger_watts %d\n"), teleinfo_config.prod_trigger);
  }

  // virtual relays
  if (teleinfo_conso.enabled)
    for (index = 0; index < 8; index ++) WSContentSend_P (PSTR ("relay_virtual%u_state %u\n"), index + 1, TeleinfoRelayStatus (index));

  // contract period relays
  if (teleinfo_conso.enabled)
    for (index = 0; index < teleinfo_contract_db.period_qty; index ++) WSContentSend_P (PSTR ("relay_period%u %u\n"), index + 1, (index == teleinfo_contract.period));
}

// Append ALERT
void TeleinfoPrometheusAppendAlert ()
{
  uint8_t index;
  char    str_name[8];

  // loop thru alert types
  for (index = 0; index < TIC_ALERT_MAX; index ++)
  {
    GetTextIndexed (str_name, sizeof (str_name), index, kTeleinfoAlert);  
    WSContentSend_P (PSTR ("alert_%s_index %u\n"), str_name, (teleinfo_meter.arr_alert[index].timeout != UINT32_MAX));
  } 
}

#ifdef USE_TELEINFO_SOLAR

// Append SOLAR
void TeleinfoPrometheusAppendSolar ()
{
  char str_value[16];

  lltoa (teleinfo_solar.total_wh, str_value, 10);
  WSContentSend_P (PSTR ("solar_prod_power_watts %d\n"), teleinfo_solar.pact);
  WSContentSend_P (PSTR ("solar_prod_watthours_total %s\n"), str_value);
}

// Append FORECAST
void TeleinfoPrometheusAppendForecast ()
{
  WSContentSend_P (PSTR ("solar_forecast_power_watts %u\n"), teleinfo_forecast.pact);
  WSContentSend_P (PSTR ("solar_forecast_tday_watthours %u\n"), teleinfo_forecast.today.total);
  WSContentSend_P (PSTR ("solar_forecast_tmrw_watthours %u\n"), teleinfo_forecast.tomorrow.total);
}

#endif  // USE_TELEINFO_SOLAR

#ifdef USE_TELEINFO_RTE

// Append TEMPO
void TeleinfoPrometheusAppendTempo ()
{
  uint8_t day;

  // today and tomorrow
  WSContentSend_P (PSTR ("tempo_today_level %u\n"), rte_tempo_status.arr_day[RTE_DAY_TODAY].level);
  WSContentSend_P (PSTR ("tempo_tomorrow_level %u\n"), rte_tempo_status.arr_day[RTE_DAY_TOMORROW].level);

  // days after
  for (day = RTE_DAY_PLUS2; day < RTE_DAY_PLUS7; day ++) WSContentSend_P (PSTR ("tempo_day%u_level %u\n"), day - 1, rte_tempo_status.arr_day[day].level);
}

#endif  // USE_TELEINFO_RTE

// Prometheus Metrics API
void TeleinfoPrometheusWebMetrics ()
{
  // page start
  WSContentBegin (200, CT_PLAIN);

  // main data
  if (teleinfo_config.meter) TeleinfoPrometheusAppendGeneral ();
  if (teleinfo_config.meter) TeleinfoPrometheusAppendMeter ();
  if (teleinfo_config.relay) TeleinfoPrometheusAppendRelay ();
  TeleinfoPrometheusAppendAlert ();

#ifdef USE_TELEINFO_SOLAR
  // solar and forecast data
  if (teleinfo_solar.enabled) TeleinfoPrometheusAppendSolar ();
  if (teleinfo_forecast.enabled) TeleinfoPrometheusAppendForecast ();
#endif  // USE_TELEINFO_SOLAR

#ifdef USE_TELEINFO_RTE
  // tempo data
  if (rte_config.tempo.enabled) TeleinfoPrometheusAppendTempo ();
#endif  // USE_TELEINFO_RTE

  // page end
  WSContentEnd ();
}

#endif  // USE_WEBSERVER

/***************************************\
 *              Interface
\***************************************/

bool XdrvTeleinfoPrometheus (const uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on (F (TIC_PROMETHEUS_PAGE_METRICS), TeleinfoPrometheusWebMetrics);
      break;
#endif    // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_TELEINFO_PROMETHEUS
#endif    // USE_TELEINFO
