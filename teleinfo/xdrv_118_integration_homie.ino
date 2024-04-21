/*
  xdrv_118_integration_homie.ino - Homie auto-discovery integration for Teleinfo

  Copyright (C) 2024  Nicolas Bernaerts

  Version history :
    01/04/2024 - v1.0 - Creation
    14/04/2024 - v1.1 - switch to rf_code configuration
                        Change key 2DAY to TDAY
                        
  Configuration values are stored in :
    - Settings->rf_code[16][1]  : Flag en enable/disable integration

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

#ifdef USE_TELEINFO
#ifdef USE_TELEINFO_HOMIE

// declare teleinfo homie integration
#define XDRV_118                    118

/*************************************************\
 *               Variables
\*************************************************/

#define TELEINFO_HOMIE_NB_MESSAGE    5

// Commands
const char kHomieIntegrationCommands[] PROGMEM = "homie_" "|" "enable";
void (* const HomieIntegrationCommand[])(void) PROGMEM = { &CmndHomieIntegrationEnable };

/********************************\
 *              Data
\********************************/

static struct {
  uint8_t enabled = 0;
  uint8_t stage   = TIC_PUB_CONNECT;      // auto-discovery publication stage
  uint8_t index   = 0;                    // index within multi-data
  uint8_t step    = 1;                    // auto-discovery step within a stage
  uint8_t data[TIC_PUB_MAX];              // data to be published
} homie_integration;

/******************************************\
 *             Configuration
\******************************************/

// load configuration
void HomieIntegrationLoadConfig () 
{
  uint8_t index;

  homie_integration.enabled = Settings->rf_code[16][1];
  for (index = 0; index < TIC_PUB_MAX; index++) homie_integration.data[index] = 0;
}

// save configuration
void HomieIntegrationSaveConfig () 
{
  Settings->rf_code[16][1] = homie_integration.enabled;
}

/***************************************\
 *               Function
\***************************************/

// set integration
void HomieIntegrationSet (const bool enabled) 
{
  // update status
  homie_integration.enabled = enabled;

  // if disabled, set offline
  if (!enabled) HomieIntegrationPublishData (TIC_PUB_DISCONNECT);

  // reset publication flags
  homie_integration.stage = TIC_PUB_CONNECT; 
  homie_integration.index = 0; 
  homie_integration.step  = 1; 

  // save configuration
  HomieIntegrationSaveConfig ();
}

// get integration
bool HomieIntegrationGet () 
{
  return (homie_integration.enabled == 1);
}

/*******************************\
 *          Command
\*******************************/

// Enable homie auto-discovery
void CmndHomieIntegrationEnable ()
{
  if (XdrvMailbox.data_len > 0) HomieIntegrationSet ((XdrvMailbox.payload != 0));
  ResponseCmndNumber (homie_integration.enabled);
}

/*******************************\
 *          Callback
\*******************************/

// trigger publication
void HomieIntegrationInit ()
{
  // load config
  HomieIntegrationLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: homie_enable to enable Homie auto-discovery [%u]"), homie_integration.enabled);
}

// trigger publication
void HomieIntegrationEvery250ms ()
{
  bool result;

  // if on battery, no publication of persistent data
  if (teleinfo_config.battery) homie_integration.stage = UINT8_MAX;

  // check publication validity 
  if (!homie_integration.enabled) return;
  if (homie_integration.stage == UINT8_MAX) return;
  if (teleinfo_meter.nb_message <= TELEINFO_HOMIE_NB_MESSAGE) return;
  if (!RtcTime.valid) return;
  if (!MqttIsConnected ()) return;

  // publish current data
  result = HomieIntegrationPublish (homie_integration.stage, homie_integration.index, homie_integration.step);

  // if publication done, increase step within stage, else increase stage
  if (result) homie_integration.step++;
  else 
  {
    // reset step
    homie_integration.step = 1;

    // handle increment
    if ((homie_integration.stage == TIC_PUB_RELAY_DATA) || (homie_integration.stage == TIC_PUB_TOTAL_CONSO)) homie_integration.index++;
      else homie_integration.stage++;
    if ((homie_integration.stage == TIC_PUB_RELAY_DATA) && (homie_integration.index >= 8)) homie_integration.stage++;
    if ((homie_integration.stage == TIC_PUB_TOTAL_CONSO) && (homie_integration.index >= teleinfo_contract.period_qty)) homie_integration.stage++;
    if ((homie_integration.stage != TIC_PUB_RELAY_DATA) && (homie_integration.stage != TIC_PUB_TOTAL_CONSO)) homie_integration.index = 0; 

    // handle data publication
    if ((homie_integration.stage == TIC_PUB_CALENDAR)    && (!teleinfo_config.calendar))    homie_integration.stage = TIC_PUB_PROD;
    if ((homie_integration.stage == TIC_PUB_PROD)        && (!teleinfo_config.meter))       homie_integration.stage = TIC_PUB_RELAY;
    if ((homie_integration.stage == TIC_PUB_PROD)        && (teleinfo_prod.total_wh == 0))  homie_integration.stage = TIC_PUB_CONSO;
    if ((homie_integration.stage == TIC_PUB_CONSO)       && (teleinfo_conso.total_wh == 0)) homie_integration.stage = TIC_PUB_RELAY;
    if ((homie_integration.stage == TIC_PUB_PH1)         && (teleinfo_contract.phase == 1)) homie_integration.stage = TIC_PUB_RELAY;
    if ((homie_integration.stage == TIC_PUB_RELAY)       && (!teleinfo_config.relay))       homie_integration.stage = TIC_PUB_TOTAL;
    if ((homie_integration.stage == TIC_PUB_TOTAL)       && (!teleinfo_config.contract))    homie_integration.stage = TIC_PUB_MAX;
    if ((homie_integration.stage == TIC_PUB_TOTAL_PROD)  && (teleinfo_prod.total_wh == 0))  homie_integration.stage = TIC_PUB_TOTAL_CONSO;
    if ((homie_integration.stage == TIC_PUB_TOTAL_CONSO) && (teleinfo_conso.total_wh == 0)) homie_integration.stage = TIC_PUB_MAX;
    if (homie_integration.stage == TIC_PUB_MAX)
    {
      homie_integration.stage = UINT8_MAX;
      homie_integration.index = 0;
    }
  }
}

// publish current message
uint8_t HomieIntegrationPublishData (const uint8_t index)
{
  // check parameter
  if (index >= TIC_PUB_MAX) return 0;

  // reset sub-index for most data
  if ((index != TIC_PUB_RELAY_DATA) && (index != TIC_PUB_TOTAL_CONSO)) homie_integration.index = 0;

  // if needed, publish data
  if (homie_integration.data[index])
  {
    // publish current index
    HomieIntegrationPublish (index, homie_integration.index, 0);

    // decide if current data is fully published
    if ((index == TIC_PUB_RELAY_DATA) || (index == TIC_PUB_TOTAL_CONSO)) homie_integration.index++;
    if ((index == TIC_PUB_RELAY_DATA) && (homie_integration.index >= 8)) homie_integration.index = 0;
    if ((index == TIC_PUB_TOTAL_CONSO) && (homie_integration.index >= teleinfo_contract.period_qty)) homie_integration.index = 0;

    // if no sub-index left, data is fully published
    if (homie_integration.index == 0) homie_integration.data[index] = 0; 
  }

  return homie_integration.index;
}

// publish all pending messages
void HomieIntegrationPublishAllData ()
{
  uint8_t index, result;

  // check publication validity
  if (!homie_integration.enabled) return;

  // loop to publish next available value
  homie_integration.stage = UINT8_MAX;
  for (index = 0; index < TIC_PUB_MAX; index ++)
  {
    result = HomieIntegrationPublishData (index);
    while (result != 0) result = HomieIntegrationPublishData (index);
  }
}

// trigger publication
void HomieIntegrationEvery100ms ()
{
  uint8_t index = 0;

  // check publication validity
  if (!homie_integration.enabled) return;

  // do not publish value before end of auto-discovery declaration
  if (homie_integration.stage != UINT8_MAX) return;

  // loop to publish next available value
  while (index < TIC_PUB_MAX)
  {
    if (homie_integration.data[index])
    {
      HomieIntegrationPublishData (index);
      index = TIC_PUB_MAX;
    }
    else index++;
  }
}

void HomieIntegrationBeforeRestart ()
{
  // if enabled, publish disconnexion
  if (homie_integration.enabled) HomieIntegrationPublishData (TIC_PUB_DISCONNECT);
}

/***************************************\
 *           JSON publication
\***************************************/

void HomieIntegrationTriggerMeter ()
{
  uint8_t index;

  // conso data
  if (teleinfo_conso.total_wh > 0)
  {
    for (index = TIC_PUB_CONSO_U; index <= TIC_PUB_CONSO_2DAY; index++) homie_integration.data[index] = 1;
    if (teleinfo_contract.phase > 1)
    {
      for (index = TIC_PUB_PH1_U; index <= TIC_PUB_PH1_W; index++) homie_integration.data[index] = 1;
      for (index = TIC_PUB_PH2_U; index <= TIC_PUB_PH3_W; index++) homie_integration.data[index] = 1;
      for (index = TIC_PUB_PH3_U; index <= TIC_PUB_PH3_W; index++) homie_integration.data[index] = 1;
    }
  }

  // production data
  if (teleinfo_prod.total_wh > 0)
    for (index = TIC_PUB_PROD_P; index <= TIC_PUB_PROD_2DAY; index++) homie_integration.data[index] = 1;
}

void HomieIntegrationTriggerTotal ()
{
  if (teleinfo_conso.total_wh > 0) homie_integration.data[TIC_PUB_TOTAL_CONSO] = 1;
  if (teleinfo_prod.total_wh > 0)  homie_integration.data[TIC_PUB_TOTAL_PROD] = 1;
}

void HomieIntegrationTriggerRelay ()
{
  homie_integration.data[TIC_PUB_RELAY_DATA] = 1;
}

void HomieIntegrationTriggerCalendar ()
{
  uint8_t index;

  for (index = TIC_PUB_CALENDAR_PERIOD; index <= TIC_PUB_CALENDAR_TOMRW; index++) homie_integration.data[index] = 1;
}

// publish homie topic (value if index = 0)
bool HomieIntegrationPublish (const uint8_t stage, const uint8_t index, const uint8_t data)
{
  bool     result = true;
  uint8_t  count;
  uint32_t ip_address;
  long     value;
  char     str_url[64];
  char     str_text[64];
  char     str_param[128];
  String   str_topic;

  // check parameters
  if (stage == UINT8_MAX) return false;
  if ((stage == TIC_PUB_RELAY_DATA) && (index >= 8)) return false;
  if ((stage == TIC_PUB_TOTAL_CONSO) && (index >= TELEINFO_INDEX_MAX)) return false;

  // init published data
  Response_P ("");

  // set url leaves
  switch (stage)
  {
    case TIC_PUB_CONNECT:
      strcpy_P (str_url, PSTR ("|/$homie|/$name|/$state|/$nodes|END"));
      break;
    case TIC_PUB_DISCONNECT:
      strcpy_P (str_url, PSTR ("|/$state|END"));
      break;
    case TIC_PUB_CONTRACT:
    case TIC_PUB_PROD:
    case TIC_PUB_CONSO:
    case TIC_PUB_PH1:
    case TIC_PUB_PH2:
    case TIC_PUB_PH3:
    case TIC_PUB_RELAY:
    case TIC_PUB_TOTAL:
      strcpy_P (str_url, PSTR ("|/$name|/$properties|END"));
      break;
    default:
      strcpy_P (str_url, PSTR ("|/$name|/$datatype|/$unit|END"));
      break;
  }

  // check if anything to publish
  GetTextIndexed (str_text, sizeof (str_text), data, str_url);
  if (strcmp (str_text, "END") == 0) return false;

  // load parameters according to stage/data
  strcpy (str_param, "");
  switch (stage)
  {
    // declaration
    case TIC_PUB_CONNECT:
      snprintf_P (str_param, sizeof (str_param), PSTR ("|3.0|%s|ready|ctr,cal,prod,conso,ph1,ph2,ph3,relay,total"), SettingsText (SET_DEVICENAME));
      break;

    // contract period
    case TIC_PUB_CONTRACT:        strcpy_P (str_param, PSTR ("/ctr"          "|Contrat|name|")); break;
    case TIC_PUB_CONTRACT_NAME:   strcpy_P (str_param, PSTR ("/ctr/name"     "|Type contrat"    "|string"  "|")); break;


    case TIC_PUB_CALENDAR:        strcpy_P (str_param, PSTR ("/cal"        "|Calendrier|period,color,hour,2day,tmrw|")); break;
    case TIC_PUB_CALENDAR_PERIOD: strcpy_P (str_param, PSTR ("/cal/period" "|Période"     "|string"  "|")); break;
    case TIC_PUB_CALENDAR_COLOR:  strcpy_P (str_param, PSTR ("/cal/color"  "|Couleur"     "|string"  "|")); break;
    case TIC_PUB_CALENDAR_HOUR:   strcpy_P (str_param, PSTR ("/cal/hour"   "|Heure"       "|string"  "|")); break;
    case TIC_PUB_CALENDAR_TODAY:  strcpy_P (str_param, PSTR ("/cal/tday"   "|Aujourdhui" "|string"  "|")); break;
    case TIC_PUB_CALENDAR_TOMRW:  strcpy_P (str_param, PSTR ("/cal/tmrw"   "|Demain"      "|string"  "|")); break;

    // production
    case TIC_PUB_PROD:      strcpy_P (str_param, PSTR ("/prod"      "|Production|yday,2day,p,w,c|")); break;
    case TIC_PUB_PROD_P:    strcpy_P (str_param, PSTR ("/prod/p"    "|Prod Puissance apparente" "|integer" "|VA")); break;
    case TIC_PUB_PROD_W:    strcpy_P (str_param, PSTR ("/prod/w"    "|Prod Puissance active"    "|integer" "|W" )); break;
    case TIC_PUB_PROD_C:    strcpy_P (str_param, PSTR ("/prod/c"    "|Prod Cos φ"               "|float"   "|"  )); break;
    case TIC_PUB_PROD_YDAY: if (!teleinfo_config.battery) strcpy_P (str_param, PSTR ("/prod/yday" "|Prod Hier"         "|integer" "|Wh")); break;
    case TIC_PUB_PROD_2DAY: if (!teleinfo_config.battery) strcpy_P (str_param, PSTR ("/prod/tday" "|Prod Aujourdhui"  "|integer" "|Wh")); break;

    // conso
    case TIC_PUB_CONSO:      strcpy_P (str_param, PSTR ("/conso"      "|Conso|yday,2day,p,w,c,i,u|")); break;
    case TIC_PUB_CONSO_P:    strcpy_P (str_param, PSTR ("/conso/p"    "|Conso Puissance apparente" "|integer" "|VA")); break;
    case TIC_PUB_CONSO_W:    strcpy_P (str_param, PSTR ("/conso/w"    "|Conso Puissance active"    "|integer" "|W" )); break;
    case TIC_PUB_CONSO_C:    strcpy_P (str_param, PSTR ("/conso/c"    "|Conso Cos φ"               "|float"   "|"  )); break;
    case TIC_PUB_CONSO_I:    strcpy_P (str_param, PSTR ("/conso/i"    "|Conso Courant"             "|float"   "|A" )); break;
    case TIC_PUB_CONSO_U:    strcpy_P (str_param, PSTR ("/conso/u"    "|Conso Tension"             "|integer" "|V" )); break;
    case TIC_PUB_CONSO_YDAY: if (!teleinfo_config.battery) strcpy_P (str_param, PSTR ("/conso/yday" "|Conso hier"         "|integer" "|Wh")); break;
    case TIC_PUB_CONSO_2DAY: if (!teleinfo_config.battery) strcpy_P (str_param, PSTR ("/conso/2day" "|Conso aujourd'hui"  "|integer" "|Wh")); break;

    // 3 phases
    case TIC_PUB_PH1:   strcpy_P (str_param, PSTR ("/ph1"   "|Phase 1|u,i,p,w|")); break;
    case TIC_PUB_PH1_U: strcpy_P (str_param, PSTR ("/ph1/u" "|Ph1 Tension"             "|integer" "|V" )); break;
    case TIC_PUB_PH1_I: strcpy_P (str_param, PSTR ("/ph1/i" "|Ph1 Courant"             "|float"   "|A" )); break;
    case TIC_PUB_PH1_P: strcpy_P (str_param, PSTR ("/ph1/p" "|Ph1 Puissance apparente" "|integer" "|VA")); break;
    case TIC_PUB_PH1_W: strcpy_P (str_param, PSTR ("/ph1/w" "|Ph1 Puissance active"    "|integer" "|W" )); break;

    case TIC_PUB_PH2:   strcpy_P (str_param, PSTR ("/ph2"   "|Phase 2|u,i,p,w|")); break;
    case TIC_PUB_PH2_U: strcpy_P (str_param, PSTR ("/ph2/u" "|Ph2 Tension"             "|integer" "|V" )); break;
    case TIC_PUB_PH2_I: strcpy_P (str_param, PSTR ("/ph2/i" "|Ph2 Courant"             "|float"   "|A" )); break;
    case TIC_PUB_PH2_P: strcpy_P (str_param, PSTR ("/ph2/p" "|Ph2 Puissance apparente" "|integer" "|VA")); break;
    case TIC_PUB_PH2_W: strcpy_P (str_param, PSTR ("/ph2/w" "|Ph2 Puissance active"    "|integer" "|W" )); break;

    case TIC_PUB_PH3:   strcpy_P (str_param, PSTR ("/ph3"   "|Phase 3|u,i,p,w|")); break;
    case TIC_PUB_PH3_U: strcpy_P (str_param, PSTR ("/ph3/u" "|Ph3 Tension"             "|integer" "|V" )); break;
    case TIC_PUB_PH3_I: strcpy_P (str_param, PSTR ("/ph3/i" "|Ph3 Courant"             "|float"   "|A" )); break;
    case TIC_PUB_PH3_P: strcpy_P (str_param, PSTR ("/ph3/p" "|Ph3 Puissance apparente" "|integer" "|VA")); break;
    case TIC_PUB_PH3_W: strcpy_P (str_param, PSTR ("/ph3/w" "|Ph3 Puissance active"    "|integer" "|W" )); break;

    // relay
    case TIC_PUB_RELAY: strcpy_P (str_param, PSTR ("/relay" "|Relais|r1,r2,r3,r4,r5,r6,r7,r8|")); break;
    case TIC_PUB_RELAY_DATA:
      GetTextIndexed (str_text, sizeof (str_text), index, kTeleinfoRelayName);
      snprintf_P (str_param, sizeof (str_param), PSTR ("/relay/r%u" "|Relai %u (%s)" "|boolean" "|" ), index + 1, index + 1, str_text);
    break;

    // total counters
    case TIC_PUB_TOTAL:
      str_topic = "";
      if (teleinfo_prod.total_wh > 0) str_topic += PSTR ("prod");
      for (count = 0; count < teleinfo_contract.period_qty; count ++)
      {
        if (str_topic.length () > 0) str_topic += ",";
        str_topic += "c";
        str_topic += count;
      }
      snprintf_P (str_param, sizeof (str_param), PSTR ("/total" "|Totaux|%s|"), str_topic.c_str ());
      break;
    case TIC_PUB_TOTAL_PROD:
      if (teleinfo_prod.total_wh > 0) strcpy_P (str_param, PSTR ("/total/prod" "|Total Production" "|integer" "|Wh"));
      break;
    case TIC_PUB_TOTAL_CONSO: 
      TeleinfoPeriodGetName (index, str_text, sizeof (str_text));
      snprintf_P (str_param, sizeof (str_param), PSTR ("/total/conso%u" "|Total %s" "|integer" "|Wh"), index, str_text);
      break;

    // disconnexion
    case TIC_PUB_DISCONNECT:
      snprintf_P (str_param, sizeof (str_param), PSTR ("|disconnected"));
      break;
  }

  // if no valid data, ignore
  if (strlen (str_param) == 0) return false;

  // if publishing metadata
  if (data > 0) Response_P (PSTR ("%s"), GetTextIndexed (str_text, sizeof (str_text), data, str_param));

  // else, publishing value
  else switch (stage)
  {
    // contract
    case TIC_PUB_CONTRACT_NAME:   TeleinfoContractGetName (str_text, sizeof (str_text)); Response_P (PSTR ("%s"), str_text); break;

    // calendar
    case TIC_PUB_CALENDAR_PERIOD: TeleinfoPeriodGetName   (str_text, sizeof (str_text)); Response_P (PSTR ("%s"), str_text); break;
    case TIC_PUB_CALENDAR_COLOR:  GetTextIndexed (str_text, sizeof (str_text), TeleinfoPeriodGetLevel (), kTeleinfoPeriodLevel); Response_P (PSTR ("%s"), str_text); break;
    case TIC_PUB_CALENDAR_HOUR:   GetTextIndexed (str_text, sizeof (str_text), TeleinfoPeriodGetHP (),    kTeleinfoPeriodHour);  Response_P (PSTR ("%s"), str_text); break;
    case TIC_PUB_CALENDAR_TODAY:  GetTextIndexed (str_text, sizeof (str_text), TeleinfoDriverCalendarGetLevel (TIC_DAY_TODAY,    12, true), kTeleinfoPeriodLevel); Response_P (PSTR ("%s"), str_text); break;
    case TIC_PUB_CALENDAR_TOMRW:  GetTextIndexed (str_text, sizeof (str_text), TeleinfoDriverCalendarGetLevel (TIC_DAY_TOMORROW, 12, true), kTeleinfoPeriodLevel); Response_P (PSTR ("%s"), str_text); break;

    // production
    case TIC_PUB_PROD_YDAY: Response_P (PSTR ("%d"), teleinfo_prod.yesterday_wh); break;
    case TIC_PUB_PROD_2DAY: Response_P (PSTR ("%d"), teleinfo_prod.today_wh); break;
    case TIC_PUB_PROD_P:    Response_P (PSTR ("%d"), teleinfo_prod.papp); break;
    case TIC_PUB_PROD_W:    Response_P (PSTR ("%d"), teleinfo_prod.pact); break;
    case TIC_PUB_PROD_C:    Response_P (PSTR ("%d.%02d"), teleinfo_prod.cosphi.value / 1000, teleinfo_prod.cosphi.value % 1000 / 10); break;
  
    // conso
    case TIC_PUB_CONSO_YDAY: Response_P (PSTR ("%d"), teleinfo_conso.yesterday_wh); break;
    case TIC_PUB_CONSO_2DAY: Response_P (PSTR ("%d"), teleinfo_conso.today_wh); break;
    case TIC_PUB_CONSO_P:    Response_P (PSTR ("%d"), teleinfo_conso.papp); break;
    case TIC_PUB_CONSO_W:    Response_P (PSTR ("%d"), teleinfo_conso.pact); break;
    case TIC_PUB_CONSO_U:    Response_P (PSTR ("%d"), teleinfo_conso.phase[0].voltage); break;
    case TIC_PUB_CONSO_C:    Response_P (PSTR ("%d.%02d"), teleinfo_conso.cosphi.value / 1000, teleinfo_conso.cosphi.value % 1000 / 10); break;
    case TIC_PUB_CONSO_I:
    {
      value = 0;
      for (count = 0; count < teleinfo_contract.phase; count++) value += teleinfo_conso.phase[count].current;
      Response_P (PSTR ("%d.%02d"), value / 1000, value % 1000 / 10); break;
    }

    // 3 phases
    case TIC_PUB_PH1_U: Response_P (PSTR ("%d"), teleinfo_conso.phase[0].voltage); break;
    case TIC_PUB_PH1_P: Response_P (PSTR ("%d"), teleinfo_conso.phase[0].papp); break;
    case TIC_PUB_PH1_W: Response_P (PSTR ("%d"), teleinfo_conso.phase[0].pact); break;
    case TIC_PUB_PH1_I: Response_P (PSTR ("%d.%02d"), teleinfo_conso.phase[0].current / 1000, teleinfo_conso.phase[0].current % 1000 / 10); break;

    case TIC_PUB_PH2_U: Response_P (PSTR ("%d"), teleinfo_conso.phase[1].voltage); break;
    case TIC_PUB_PH2_P: Response_P (PSTR ("%d"), teleinfo_conso.phase[1].papp); break;
    case TIC_PUB_PH2_W: Response_P (PSTR ("%d"), teleinfo_conso.phase[1].pact); break;
    case TIC_PUB_PH2_I: Response_P (PSTR ("%d.%02d"), teleinfo_conso.phase[1].current / 1000, teleinfo_conso.phase[1].current % 1000 / 10); break;

    case TIC_PUB_PH3_U: Response_P (PSTR ("%d"), teleinfo_conso.phase[2].voltage); break;
    case TIC_PUB_PH3_P: Response_P (PSTR ("%d"), teleinfo_conso.phase[2].papp); break;
    case TIC_PUB_PH3_W: Response_P (PSTR ("%d"), teleinfo_conso.phase[2].pact); break;
    case TIC_PUB_PH3_I: Response_P (PSTR ("%d.%02d"), teleinfo_conso.phase[2].current / 1000, teleinfo_conso.phase[2].current % 1000 / 10); break;

    // relay
    case TIC_PUB_RELAY_DATA: if (TeleinfoRelayStatus (index + 1)) Response_P (PSTR ("true")); else Response_P (PSTR ("false")); break;

    // production counter
    case TIC_PUB_TOTAL_PROD: lltoa (teleinfo_prod.total_wh, str_text, 10); Response_P (PSTR ("%s"), str_text); break;
    
    // loop thru conso counters
    case TIC_PUB_TOTAL_CONSO: lltoa (teleinfo_conso.index_wh[index], str_text, 10); Response_P (PSTR ("%s"), str_text); break;
  }

  // set topic to lower case
  str_topic  = PSTR ("homie/");
  str_topic += TasmotaGlobal.hostname;
  GetTextIndexed (str_text, sizeof (str_text), 0, str_param);
  if (strlen (str_text) > 0) str_topic += str_text;
  GetTextIndexed (str_text, sizeof (str_text), data, str_url);
  if (strlen (str_text) > 0) str_topic += str_text;
  str_topic.toLowerCase ();

  // publish
  if (data == 0) MqttPublish (str_topic.c_str (), false);
    else MqttPublish (str_topic.c_str (), true);

  return true;
}

/***************************************\
 *              Interface
\***************************************/

// Teleinfo homie driver
bool Xdrv118 (uint32_t function)
{
  bool result = false;

  // swtich according to context
  switch (function)
  {
    case FUNC_INIT:
      HomieIntegrationInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kHomieIntegrationCommands, HomieIntegrationCommand);
      break;
    case FUNC_EVERY_250_MSECOND:
      HomieIntegrationEvery250ms ();
      break;
    case FUNC_EVERY_100_MSECOND:
      HomieIntegrationEvery100ms ();
      break;
    case FUNC_SAVE_BEFORE_RESTART:
      HomieIntegrationBeforeRestart ();
      break;
  }

  return result;
}

#endif      // USE_TELEINFO_HOMIE
#endif      // USE_TELEINFO

