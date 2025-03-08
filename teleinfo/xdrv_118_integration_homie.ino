/*
  xdrv_118_integration_homie.ino - Homie auto-discovery integration for Teleinfo

  Copyright (C) 2024  Nicolas Bernaerts

  Version history :
    01/04/2024 - v1.0 - Creation
    14/04/2024 - v1.1 - switch to rf_code configuration
                        Change key 2DAY to TDAY
    20/06/2024 - v1.2 - Add meter serial number and global conso counter
                        
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

static const char PSTR_HOMIE_URL_CONNECT[]    PROGMEM = "|/$homie|/$name|/$state|/$nodes|END";
static const char PSTR_HOMIE_URL_DISCONNECT[] PROGMEM = "|/$state|END";
static const char PSTR_HOMIE_URL_GROUP[]      PROGMEM = "|/$name|/$properties|END";
static const char PSTR_HOMIE_URL_DEFAULT[]    PROGMEM = "|/$name|/$datatype|/$unit|END";

static const char PSTR_HOMIE_PUB_CONNECT[]         PROGMEM = "|3.0|%s|ready|ctr,cal,prod,conso,ph1,ph2,ph3,relay,total";
static const char PSTR_HOMIE_PUB_CONTRACT[]        PROGMEM = "/ctr"        "|Contrat"      "|name"    "|";
static const char PSTR_HOMIE_PUB_CONTRACT_NAME[]   PROGMEM = "/ctr/name"   "|Type contrat" "|string"  "|";
static const char PSTR_HOMIE_PUB_CONTRACT_SERIAL[] PROGMEM = "/ctr/serial" "|N° série"     "|integer" "|";

static const char PSTR_HOMIE_PUB_CALENDAR[]        PROGMEM = "/cal|Calendrier|period,color,hour,2day,tmrw|";
static const char PSTR_HOMIE_PUB_CALENDAR_PERIOD[] PROGMEM = "/cal/period" "|Période"    "|string" "|";
static const char PSTR_HOMIE_PUB_CALENDAR_COLOR[]  PROGMEM = "/cal/color"  "|Couleur"    "|string" "|";
static const char PSTR_HOMIE_PUB_CALENDAR_HOUR[]   PROGMEM = "/cal/hour"   "|Heure"      "|string" "|";
static const char PSTR_HOMIE_PUB_CALENDAR_TODAY[]  PROGMEM = "/cal/tday"   "|Aujourdhui" "|string" "|";
static const char PSTR_HOMIE_PUB_CALENDAR_TOMRW[]  PROGMEM = "/cal/tmrw"   "|Demain"     "|string" "|";

static const char PSTR_HOMIE_PUB_PROD[]       PROGMEM = "/prod|Production|yday,2day,p,w,c|";
static const char PSTR_HOMIE_PUB_PROD_P[]     PROGMEM = "/prod/p"    "|Prod Puissance apparente" "|integer" "|VA";
static const char PSTR_HOMIE_PUB_PROD_W[]     PROGMEM = "/prod/w"    "|Prod Puissance active"    "|integer" "|W";
static const char PSTR_HOMIE_PUB_PROD_C[]     PROGMEM = "/prod/c"    "|Prod Cos φ"               "|float"   "|";
static const char PSTR_HOMIE_PUB_PROD_YTDAY[] PROGMEM = "/prod/yday" "|Prod Hier"                "|integer" "|Wh";
static const char PSTR_HOMIE_PUB_PROD_TODAY[] PROGMEM = "/prod/tday" "|Prod Aujourdhui"          "|integer" "|Wh";

static const char PSTR_HOMIE_PUB_CONSO[]       PROGMEM = "/conso|Conso|yday,2day,p,w,c,i,u|";
static const char PSTR_HOMIE_PUB_CONSO_P[]     PROGMEM = "/conso/p"    "|Conso Puissance apparente" "|integer" "|VA";
static const char PSTR_HOMIE_PUB_CONSO_W[]     PROGMEM = "/conso/w"    "|Conso Puissance active"    "|integer" "|W";
static const char PSTR_HOMIE_PUB_CONSO_C[]     PROGMEM = "/conso/c"    "|Conso Cos φ"               "|float"   "|";
static const char PSTR_HOMIE_PUB_CONSO_I[]     PROGMEM = "/conso/i"    "|Conso Courant"             "|float"   "|A";
static const char PSTR_HOMIE_PUB_CONSO_U[]     PROGMEM = "/conso/u"    "|Conso Tension"             "|integer" "|V";
static const char PSTR_HOMIE_PUB_CONSO_YTDAY[] PROGMEM = "/conso/yday" "|Conso hier"                "|integer" "|Wh";
static const char PSTR_HOMIE_PUB_CONSO_TODAY[] PROGMEM = "/conso/2day" "|Conso aujourd'hui"         "|integer" "|Wh";

static const char PSTR_HOMIE_PUB_PH1[]   PROGMEM = "/ph1|Phase 1|u,i,p,w|";
static const char PSTR_HOMIE_PUB_PH1_U[] PROGMEM = "/ph1/u" "|Ph1 Tension"             "|integer" "|V";
static const char PSTR_HOMIE_PUB_PH1_I[] PROGMEM = "/ph1/i" "|Ph1 Courant"             "|float"   "|A";
static const char PSTR_HOMIE_PUB_PH1_P[] PROGMEM = "/ph1/p" "|Ph1 Puissance apparente" "|integer" "|VA";
static const char PSTR_HOMIE_PUB_PH1_W[] PROGMEM = "/ph1/w" "|Ph1 Puissance active"    "|integer" "|W";

static const char PSTR_HOMIE_PUB_PH2[]   PROGMEM = "/ph2|Phase 2|u,i,p,w|";
static const char PSTR_HOMIE_PUB_PH2_U[] PROGMEM = "/ph2/u" "|Ph2 Tension"             "|integer" "|V";
static const char PSTR_HOMIE_PUB_PH2_I[] PROGMEM = "/ph2/i" "|Ph2 Courant"             "|float"   "|A";
static const char PSTR_HOMIE_PUB_PH2_P[] PROGMEM = "/ph2/p" "|Ph2 Puissance apparente" "|integer" "|VA";
static const char PSTR_HOMIE_PUB_PH2_W[] PROGMEM = "/ph2/w" "|Ph2 Puissance active"    "|integer" "|W";

static const char PSTR_HOMIE_PUB_PH3[]   PROGMEM = "/ph3|Phase 3|u,i,p,w|";
static const char PSTR_HOMIE_PUB_PH3_U[] PROGMEM = "/ph3/u" "|Ph3 Tension"             "|integer" "|V";
static const char PSTR_HOMIE_PUB_PH3_I[] PROGMEM = "/ph3/i" "|Ph3 Courant"             "|float"   "|A";
static const char PSTR_HOMIE_PUB_PH3_P[] PROGMEM = "/ph3/p" "|Ph3 Puissance apparente" "|integer" "|VA";
static const char PSTR_HOMIE_PUB_PH3_W[] PROGMEM = "/ph3/w" "|Ph3 Puissance active"    "|integer" "|W";

static const char PSTR_HOMIE_PUB_RELAY[]      PROGMEM = "/relay|Relais|r1,r2,r3,r4,r5,r6,r7,r8|";
static const char PSTR_HOMIE_PUB_RELAY_DATA[] PROGMEM = "/relay/r%u" "|Relai virtual %u" "|boolean" "|";

static const char PSTR_HOMIE_PUB_TOTAL[]        PROGMEM = "/total|Totaux|%s|";
static const char PSTR_HOMIE_PUB_TOTAL_PROD[]   PROGMEM = "/total/prod"  "|Total Production" "|integer" "|Wh";
static const char PSTR_HOMIE_PUB_TOTAL_CONSO[]  PROGMEM = "/total/conso" "|Total Conso"      "|integer" "|Wh";
static const char PSTR_HOMIE_PUB_TOTAL_PERIOD[] PROGMEM = "/total/c%u"   "|Total %s"         "|integer" "|Wh";

static const char PSTR_HOMIE_PUB_DISCONNECT[] PROGMEM = "|disconnected";

// Commands
const char kHomieIntegrationCommands[] PROGMEM = "" "|" "homie";
void (* const HomieIntegrationCommand[])(void) PROGMEM = { &CmndHomieIntegrationEnable };

/********************************\
 *              Data
\********************************/

static struct {
  uint8_t enabled   = 0;
  uint8_t stage     = TIC_PUB_CONNECT;      // auto-discovery publication stage
  uint8_t sub_stage = 0;                    // sub index within stage if multiple data
  uint8_t sub_step  = 1;                    // auto-discovery step within a stage
  uint8_t data      = 0;                    // first data index to publish
  uint8_t arr_data[TIC_PUB_MAX];            // array of data publication flags
} homie_integration;

/******************************************\
 *             Configuration
\******************************************/

// load configuration
void HomieIntegrationLoadConfig () 
{
  uint8_t index;

  homie_integration.enabled = Settings->rf_code[16][1];
  for (index = 0; index < TIC_PUB_MAX; index++) homie_integration.arr_data[index] = 0;
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
  if (!enabled) HomieIntegrationPublishStage (TIC_PUB_DISCONNECT);

  // reset publication flags
  homie_integration.stage     = TIC_PUB_CONNECT; 
  homie_integration.sub_stage = 0; 
  homie_integration.sub_step  = 1; 
  homie_integration.data      = 0; 

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
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run homie to set Homie auto-discovery [%u]"), homie_integration.enabled);
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
  if (!RtcTime.valid) return;
  if (!MqttIsConnected ()) return;

  // publish current data
  result = HomieIntegrationPublishSubStage (homie_integration.stage, homie_integration.sub_stage, homie_integration.sub_step);

  // if publication done, increase step within stage, else increase stage
  if (result) homie_integration.sub_step++;
  else 
  {
    // reset step
    homie_integration.sub_step = 1;

    // handle increment
    if ((homie_integration.stage == TIC_PUB_RELAY_DATA) || (homie_integration.stage == TIC_PUB_TOTAL_INDEX)) homie_integration.sub_stage++;
      else homie_integration.stage++;
    if ((homie_integration.stage == TIC_PUB_RELAY_DATA) && (homie_integration.sub_stage >= 8)) homie_integration.stage++;
    if ((homie_integration.stage == TIC_PUB_TOTAL_INDEX) && (homie_integration.sub_stage >= teleinfo_contract.period_qty)) homie_integration.stage++;
    if ((homie_integration.stage != TIC_PUB_RELAY_DATA) && (homie_integration.stage != TIC_PUB_TOTAL_INDEX)) homie_integration.sub_stage = 0; 

    // handle data publication
    if ((homie_integration.stage == TIC_PUB_CALENDAR)    && (!teleinfo_config.calendar))    homie_integration.stage = TIC_PUB_PROD;
    if ((homie_integration.stage == TIC_PUB_PROD)        && (!teleinfo_config.meter))       homie_integration.stage = TIC_PUB_RELAY;
    if ((homie_integration.stage == TIC_PUB_PROD)        && (teleinfo_prod.total_wh == 0))  homie_integration.stage = TIC_PUB_CONSO;
    if ((homie_integration.stage == TIC_PUB_CONSO)       && (teleinfo_conso.total_wh == 0)) homie_integration.stage = TIC_PUB_RELAY;
    if ((homie_integration.stage == TIC_PUB_PH1)         && (teleinfo_contract.phase == 1)) homie_integration.stage = TIC_PUB_RELAY;
    if ((homie_integration.stage == TIC_PUB_RELAY)       && (!teleinfo_config.relay))       homie_integration.stage = TIC_PUB_TOTAL;
    if ((homie_integration.stage == TIC_PUB_TOTAL)       && (!teleinfo_config.meter))       homie_integration.stage = TIC_PUB_MAX;
    if ((homie_integration.stage == TIC_PUB_TOTAL_PROD)  && (teleinfo_prod.total_wh == 0))  homie_integration.stage = TIC_PUB_TOTAL_CONSO;
    if ((homie_integration.stage == TIC_PUB_TOTAL_CONSO) && (teleinfo_conso.total_wh == 0)) homie_integration.stage = TIC_PUB_MAX;
    if (homie_integration.stage == TIC_PUB_MAX)
    {
      homie_integration.stage     = UINT8_MAX;
      homie_integration.sub_stage = 0;
    }
  }
}

// publish current message (return true if message fully published)
bool HomieIntegrationPublishStage (const uint8_t stage)
{
  // check parameter
  if (stage >= TIC_PUB_MAX) return false;

  // reset sub-index for most data
  if ((stage != TIC_PUB_RELAY_DATA) && (stage != TIC_PUB_TOTAL_INDEX)) homie_integration.sub_stage = 0;

  // if needed, publish data
  if (homie_integration.arr_data[stage])
  {
    // publish current index
    HomieIntegrationPublishSubStage (stage, homie_integration.sub_stage, 0);

    // decide if current data is fully published
    if ((stage == TIC_PUB_RELAY_DATA) || (stage == TIC_PUB_TOTAL_INDEX)) homie_integration.sub_stage++;
    if ((stage == TIC_PUB_RELAY_DATA) && (homie_integration.sub_stage >= 8)) homie_integration.sub_stage = 0;
    if ((stage == TIC_PUB_TOTAL_INDEX) && (homie_integration.sub_stage >= teleinfo_contract.period_qty)) homie_integration.sub_stage = 0;

    // if no sub-index left, data is fully published
    if (homie_integration.sub_stage == 0) homie_integration.arr_data[stage] = 0; 
  }

  return (homie_integration.arr_data[stage] == 0);
}

// publish all pending messages
void HomieIntegrationPublishAllData ()
{
  // if not enabled, ignore
  if (!homie_integration.enabled) return;

  // loop to publish next available value
  homie_integration.stage = UINT8_MAX;
  homie_integration.data  = 0;
  while (homie_integration.data < TIC_PUB_MAX)
  {
    if (HomieIntegrationPublishStage (homie_integration.data)) homie_integration.data++;
  }
}

// trigger publication
void HomieIntegrationEvery100ms ()
{
  // check publication validity
  if (!homie_integration.enabled) return;
  if (homie_integration.data == UINT8_MAX) return;

  // do not publish value before end of auto-discovery declaration
  if (homie_integration.stage != UINT8_MAX) return;

  // publish current data
  if (HomieIntegrationPublishStage (homie_integration.data)) homie_integration.data++;
  if (homie_integration.data >= TIC_PUB_MAX) homie_integration.data = UINT8_MAX;
}

/***************************************\
 *           JSON publication
\***************************************/

void HomieIntegrationDataConsoProd ()
{
  uint8_t index;

  // ask for data publication
  homie_integration.data = 0;

  // conso data
  if (teleinfo_conso.total_wh > 0)
  {
    for (index = TIC_PUB_CONSO_P; index <= TIC_PUB_CONSO_TODAY; index++) homie_integration.arr_data[index] = 1;
    if (teleinfo_contract.phase > 1)
    {
      for (index = TIC_PUB_PH1_U; index <= TIC_PUB_PH1_W; index++) homie_integration.arr_data[index] = 1;
      for (index = TIC_PUB_PH2_U; index <= TIC_PUB_PH3_W; index++) homie_integration.arr_data[index] = 1;
      for (index = TIC_PUB_PH3_U; index <= TIC_PUB_PH3_W; index++) homie_integration.arr_data[index] = 1;
    }
  }

  // production data
  if (teleinfo_prod.total_wh > 0)
    for (index = TIC_PUB_PROD_P; index <= TIC_PUB_PROD_TODAY; index++) homie_integration.arr_data[index] = 1;

  // total data
  if (teleinfo_conso.total_wh > 0)
  {
    homie_integration.arr_data[TIC_PUB_TOTAL_CONSO] = 1;
    homie_integration.arr_data[TIC_PUB_TOTAL_INDEX] = 1;
  }
  if (teleinfo_prod.total_wh > 0)  homie_integration.arr_data[TIC_PUB_TOTAL_PROD] = 1;
}

void HomieIntegrationDataCalendar ()
{
  uint8_t index;

  // ask for data publication
  homie_integration.data = 0;

  // calendar data
  for (index = TIC_PUB_CALENDAR_PERIOD; index <= TIC_PUB_CALENDAR_TOMRW; index++) homie_integration.arr_data[index] = 1;
}

void HomieIntegrationDataRelay ()
{
  homie_integration.arr_data[TIC_PUB_RELAY_DATA] = 1;
}

// publish homie topic (value if index = 0)
bool HomieIntegrationPublishSubStage (const uint8_t stage, const uint8_t index, const uint8_t data)
{
  uint8_t     count, length;
  long        value;
  const char* pstr_url;
  const char* pstr_param;
  char        str_id[32];
  char        str_temp[32];
  char        str_name[64];
  char        str_text[128];

  // check parameters
  if (stage == UINT8_MAX) return false;
  if ((stage == TIC_PUB_RELAY_DATA) && (index >= 8)) return false;
  if ((stage == TIC_PUB_TOTAL_INDEX) && (index >= TIC_INDEX_MAX)) return false;

  // init
  pstr_url   = nullptr;
  pstr_param = nullptr;
  strcpy (str_id,   "");
  strcpy (str_temp, "");
  strcpy (str_name, "");
  strcpy (str_text, "");
  Response_P ("");

  // set url leaves
  switch (stage)
  {
    case TIC_PUB_CONNECT:    pstr_url = PSTR_HOMIE_URL_CONNECT; break;
    case TIC_PUB_DISCONNECT: pstr_url = PSTR_HOMIE_URL_DISCONNECT; break;
    case TIC_PUB_CONTRACT:
    case TIC_PUB_PROD:
    case TIC_PUB_CONSO:
    case TIC_PUB_PH1:
    case TIC_PUB_PH2:
    case TIC_PUB_PH3:
    case TIC_PUB_RELAY:
    case TIC_PUB_TOTAL:      pstr_url = PSTR_HOMIE_URL_GROUP; break;
    default:                 pstr_url = PSTR_HOMIE_URL_DEFAULT; break;
  }

  // check if anything to publish
  if (pstr_url == nullptr) return false;
  GetTextIndexed (str_temp, sizeof (str_temp), data, pstr_url);
  if (strcmp (str_temp, "END") == 0) return false;

  // load parameters according to stage/data
  switch (stage)
  {
    // declaration
    case TIC_PUB_CONNECT:
      pstr_param = PSTR_HOMIE_PUB_CONNECT;
      if (data == 2) sprintf_P (str_text, GetTextIndexed (str_temp, sizeof (str_temp), data, pstr_param), SettingsText (SET_DEVICENAME));
      break;

    // contract period
    case TIC_PUB_CONTRACT:        pstr_param = PSTR_HOMIE_PUB_CONTRACT; break;
    case TIC_PUB_CONTRACT_NAME:   pstr_param = PSTR_HOMIE_PUB_CONTRACT_NAME; break;


    case TIC_PUB_CALENDAR:        pstr_param = PSTR_HOMIE_PUB_CALENDAR; break;
    case TIC_PUB_CALENDAR_PERIOD: pstr_param = PSTR_HOMIE_PUB_CALENDAR_PERIOD; break;
    case TIC_PUB_CALENDAR_COLOR:  pstr_param = PSTR_HOMIE_PUB_CALENDAR_COLOR; break;
    case TIC_PUB_CALENDAR_HOUR:   pstr_param = PSTR_HOMIE_PUB_CALENDAR_HOUR; break;
    case TIC_PUB_CALENDAR_TODAY:  pstr_param = PSTR_HOMIE_PUB_CALENDAR_TODAY; break;
    case TIC_PUB_CALENDAR_TOMRW:  pstr_param = PSTR_HOMIE_PUB_CALENDAR_TOMRW; break;

    case TIC_PUB_PROD:            pstr_param = PSTR_HOMIE_PUB_PROD; break;
    case TIC_PUB_PROD_P:          pstr_param = PSTR_HOMIE_PUB_PROD_P; break;
    case TIC_PUB_PROD_W:          pstr_param = PSTR_HOMIE_PUB_PROD_W; break;
    case TIC_PUB_PROD_C:          pstr_param = PSTR_HOMIE_PUB_PROD_C; break;
    case TIC_PUB_PROD_YTDAY:      if (!teleinfo_config.battery) pstr_param = PSTR_HOMIE_PUB_PROD_YTDAY; break;
    case TIC_PUB_PROD_TODAY:      if (!teleinfo_config.battery) pstr_param = PSTR_HOMIE_PUB_PROD_TODAY; break;

    case TIC_PUB_CONSO:           pstr_param = PSTR_HOMIE_PUB_CONSO; break;
    case TIC_PUB_CONSO_P:         pstr_param = PSTR_HOMIE_PUB_CONSO_P; break;
    case TIC_PUB_CONSO_W:         pstr_param = PSTR_HOMIE_PUB_CONSO_W; break;
    case TIC_PUB_CONSO_C:         pstr_param = PSTR_HOMIE_PUB_CONSO_C; break;
    case TIC_PUB_CONSO_I:         pstr_param = PSTR_HOMIE_PUB_CONSO_I; break;
    case TIC_PUB_CONSO_U:         pstr_param = PSTR_HOMIE_PUB_CONSO_U; break;
    case TIC_PUB_CONSO_YTDAY:     if (!teleinfo_config.battery) pstr_param = PSTR_HOMIE_PUB_CONSO_YTDAY; break;
    case TIC_PUB_CONSO_TODAY:     if (!teleinfo_config.battery) pstr_param = PSTR_HOMIE_PUB_CONSO_TODAY; break;

    case TIC_PUB_PH1:             pstr_param = PSTR_HOMIE_PUB_PH1; break;
    case TIC_PUB_PH1_U:           pstr_param = PSTR_HOMIE_PUB_PH1_U; break;
    case TIC_PUB_PH1_I:           pstr_param = PSTR_HOMIE_PUB_PH1_I; break;
    case TIC_PUB_PH1_P:           pstr_param = PSTR_HOMIE_PUB_PH1_P; break;
    case TIC_PUB_PH1_W:           pstr_param = PSTR_HOMIE_PUB_PH1_W; break;

    case TIC_PUB_PH2:             pstr_param = PSTR_HOMIE_PUB_PH2; break;
    case TIC_PUB_PH2_U:           pstr_param = PSTR_HOMIE_PUB_PH2_U; break;
    case TIC_PUB_PH2_I:           pstr_param = PSTR_HOMIE_PUB_PH2_I; break;
    case TIC_PUB_PH2_P:           pstr_param = PSTR_HOMIE_PUB_PH2_P; break;
    case TIC_PUB_PH2_W:           pstr_param = PSTR_HOMIE_PUB_PH2_W; break;

    case TIC_PUB_PH3:             pstr_param = PSTR_HOMIE_PUB_PH3; break;
    case TIC_PUB_PH3_U:           pstr_param = PSTR_HOMIE_PUB_PH3_U; break;
    case TIC_PUB_PH3_I:           pstr_param = PSTR_HOMIE_PUB_PH3_I; break;
    case TIC_PUB_PH3_P:           pstr_param = PSTR_HOMIE_PUB_PH3_P; break;
    case TIC_PUB_PH3_W:           pstr_param = PSTR_HOMIE_PUB_PH3_W; break;

    case TIC_PUB_RELAY: pstr_param = PSTR_HOMIE_PUB_RELAY; break;
    case TIC_PUB_RELAY_DATA:
      pstr_param = PSTR_HOMIE_PUB_RELAY_DATA;
      sprintf_P (str_id, GetTextIndexed (str_temp, sizeof (str_temp), 0, pstr_param), index + 1);
      if (data > 2) sprintf_P (str_text, GetTextIndexed (str_temp, sizeof (str_temp), data, pstr_param), index + 1);
    break;

    case TIC_PUB_TOTAL:
      pstr_param = PSTR_HOMIE_PUB_TOTAL;
      if (data == 2)
      {
        if (teleinfo_prod.total_wh > 0) strcpy_P (str_text, PSTR ("prod"));
        for (count = 0; count < teleinfo_contract.period_qty; count ++)
        {
          sprintf_P (str_temp, PSTR ("c%u"), count);
          if (strlen (str_text) > 0) strcat_P (str_text, PSTR (","));
          strcat (str_text, str_temp);
        }
      }
      break;

    case TIC_PUB_TOTAL_PROD: 
      if (teleinfo_prod.total_wh > 0) pstr_param = PSTR_HOMIE_PUB_TOTAL_PROD;
      break;

    case TIC_PUB_TOTAL_CONSO: 
      if (teleinfo_conso.total_wh > 0) pstr_param = PSTR_HOMIE_PUB_TOTAL_CONSO;
      break;

    case TIC_PUB_TOTAL_INDEX: 
      pstr_param = PSTR_HOMIE_PUB_TOTAL_PERIOD;
      sprintf_P (str_id, GetTextIndexed (str_temp, sizeof (str_temp), 0, pstr_param), index);
      TeleinfoPeriodGetLabel (str_name, sizeof (str_name), index);
      if (data == 0) sprintf_P (str_text, GetTextIndexed (str_temp, sizeof (str_temp), data, pstr_param), index);
      else if (data == 1) sprintf_P (str_text, GetTextIndexed (str_temp, sizeof (str_temp), data, pstr_param), str_name);
      break;

    // disconnexion
    case TIC_PUB_DISCONNECT: pstr_param = PSTR_HOMIE_PUB_DISCONNECT; break;
  }

  // if no valid data, ignore
  if (pstr_param == nullptr) return false;

  // if publishing metadata
  if (data > 0)
  {
    if (strlen (str_text) > 0) Response_P (PSTR ("%s"), str_text);
      else Response_P (GetTextIndexed (str_text, sizeof (str_text), data, pstr_param));
  }

  // else, publishing value
  else switch (stage)
  {
    // contract
    case TIC_PUB_CONTRACT_NAME: TeleinfoContractGetName (str_text, sizeof (str_text)); Response_P (PSTR ("%s"), str_text); break;
    case TIC_PUB_CONTRACT_SERIAL: lltoa (teleinfo_meter.ident, str_text, 10); Response_P (PSTR ("%s"), str_text); break;

    // calendar
    case TIC_PUB_CALENDAR_PERIOD: TeleinfoPeriodGetLabel (str_text, sizeof (str_text)); Response_P (PSTR ("%s"), str_text); break;
    case TIC_PUB_CALENDAR_COLOR:  GetTextIndexed (str_text, sizeof (str_text), TeleinfoPeriodGetLevel (), kTeleinfoLevelLabel); Response_P (PSTR ("%s"), str_text); break;
    case TIC_PUB_CALENDAR_HOUR:   GetTextIndexed (str_text, sizeof (str_text), TeleinfoPeriodGetHP (), kTeleinfoHourLabel);  Response_P (PSTR ("%s"), str_text); break;
    case TIC_PUB_CALENDAR_TODAY:  GetTextIndexed (str_text, sizeof (str_text), TeleinfoDriverCalendarGetLevel (TIC_DAY_TODAY, 12 * 2, true), kTeleinfoLevelLabel); Response_P (PSTR ("%s"), str_text); break;
    case TIC_PUB_CALENDAR_TOMRW:  GetTextIndexed (str_text, sizeof (str_text), TeleinfoDriverCalendarGetLevel (TIC_DAY_TMROW, 12 * 2, true), kTeleinfoLevelLabel); Response_P (PSTR ("%s"), str_text); break;

    // production
    case TIC_PUB_PROD_YTDAY: Response_P (PSTR ("%d"), teleinfo_prod.yesterday_wh); break;
    case TIC_PUB_PROD_TODAY: Response_P (PSTR ("%d"), teleinfo_prod.today_wh); break;
    case TIC_PUB_PROD_P:     Response_P (PSTR ("%d"), teleinfo_prod.papp); break;
    case TIC_PUB_PROD_W:     Response_P (PSTR ("%d"), teleinfo_prod.pact); break;
    case TIC_PUB_PROD_C:     Response_P (PSTR ("%d.%02d"), teleinfo_prod.cosphi.value / 1000, teleinfo_prod.cosphi.value % 1000 / 10); break;
  
    // conso
    case TIC_PUB_CONSO_YTDAY: Response_P (PSTR ("%d"), teleinfo_conso.yesterday_wh); break;
    case TIC_PUB_CONSO_TODAY: Response_P (PSTR ("%d"), teleinfo_conso.today_wh); break;
    case TIC_PUB_CONSO_P:     Response_P (PSTR ("%d"), teleinfo_conso.papp); break;
    case TIC_PUB_CONSO_W:     Response_P (PSTR ("%d"), teleinfo_conso.pact); break;
    case TIC_PUB_CONSO_U:     Response_P (PSTR ("%d"), teleinfo_conso.phase[0].voltage); break;
    case TIC_PUB_CONSO_C:     Response_P (PSTR ("%d.%02d"), teleinfo_conso.cosphi.value / 1000, teleinfo_conso.cosphi.value % 1000 / 10); break;
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

    // counters
    case TIC_PUB_TOTAL_PROD:  lltoa (teleinfo_prod.total_wh, str_text, 10); Response_P (PSTR ("%s"), str_text); break;
    case TIC_PUB_TOTAL_CONSO: lltoa (teleinfo_conso.total_wh, str_text, 10); Response_P (PSTR ("%s"), str_text); break;
    case TIC_PUB_TOTAL_INDEX: lltoa (teleinfo_conso.index_wh[index], str_text, 10); Response_P (PSTR ("%s"), str_text); break;
  }

  // if nothing to publish, ignore
  if (ResponseLength () > 0)
  {
    // generate topic
    strcpy_P (str_text, PSTR ("homie/"));
    strlcat (str_text, TasmotaGlobal.hostname, sizeof (str_text));
    if (strlen (str_id) > 0) strlcat (str_text, str_id, sizeof (str_text));
      else strlcat (str_text, GetTextIndexed (str_temp, sizeof (str_temp), 0, pstr_param), sizeof (str_text));
    strlcat (str_text, GetTextIndexed (str_temp, sizeof (str_temp), data, pstr_url), sizeof (str_text));

    // set topic to lowercase and publish (in mode retain if not data)
    length = (uint8_t)strlen (str_text);
    for (count = 0; count < length; count ++) str_text[count] = tolower (str_text[count]);
    MqttPublish (str_text, (data != 0));
  }

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
  }

  return result;
}

#endif      // USE_TELEINFO_HOMIE
#endif      // USE_TELEINFO

