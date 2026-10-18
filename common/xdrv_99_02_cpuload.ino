/*
  xdrv_99_03_cpuload.ino - CPU load on main page
  
  Copyright (C) 2020-2025  Nicolas Bernaerts
    22/10/2025 v1.0 - Creation 
    29/03/2026 v2.0 - Separate form misc options

  Settings are stored using :
    - Settings->rf_code[0][8] = Web display of load average

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

/*************************************************\
 *                IP Address
\*************************************************/

#ifdef USE_CPU_LOAD

#define CPULOAD_SAMPLE     60

// data publication commands
static const char kCpuLoadCommands[]  PROGMEM = "|" "cpuload"   ;
void (* const CpuLoadCommand[])(void) PROGMEM = { &CmndCpuLoad };

// load color levels
enum LoadLevelColor                  { LOAD_HIGH , LOAD_MEDIUM,  LOAD_LOW, LOAD_MAX };
const char kLoadLevelColor[] PROGMEM = "#b00" "|" "#c80" "|" "#090";     

/**************************************************\
 *                  Variables
\**************************************************/

static struct {
  bool    enabled = false;
  uint8_t second  = UINT8_MAX;
  uint8_t arr_load[CPULOAD_SAMPLE];
} cpu_load;

/**************************************************\
 *                 Configuration
\**************************************************/

void CpuLoadConfig ()
{
  cpu_load.enabled = (Settings->rf_code[0][8] != 0);
}

void CpuSaveConfig ()
{
  Settings->rf_code[0][8] = cpu_load.enabled;
}

/**************************************************\
 *                    Commands
\**************************************************/

// display web load on main page
void CmndCpuLoad ()
{
  // update flag
  if (XdrvMailbox.data_len > 0)
  {
    cpu_load.enabled = ((XdrvMailbox.payload != 0) || (strcasecmp_P (XdrvMailbox.data, PSTR ("ON")) == 0));
    CpuSaveConfig ();
  }

  ResponseCmndNumber (cpu_load.enabled);
}

/**************************************************\
 *                  Call back
\**************************************************/

// init main status
void CpuLoadInit ()
{
  // init array
  memset (&cpu_load.arr_load, 0, sizeof (cpu_load.arr_load));

  // load config
  CpuLoadConfig ();

  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Run cpuload to display average load graph on main page"));
}

void CpuLoadEvery250ms ()
{
  uint8_t second;
  uint8_t load_avg;

  // if no valid time, ignore
  if (!RtcTime.valid) return;

  // reset load average for new slot
  second = RtcTime.second;
  if (cpu_load.second != second) cpu_load.arr_load[second] = 0;

  // save peak average load
  if (TasmotaGlobal.loop_load_avg < 100) load_avg = (uint8_t)TasmotaGlobal.loop_load_avg;
    else load_avg = 100;
  cpu_load.arr_load[second] = max (cpu_load.arr_load[second], load_avg);

  // update current slot
  cpu_load.second = second;
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// main page display style
void CpuLoadWebMainButton ()
{
  WSContentSend_P (PSTR ("<style>\n"));

  // style : cpu load icon
  WSContentSend_P (PSTR ("div#l1 .cpu{padding:0px;width:16px;height:20px;margin-left:5px;border-radius:2px;}\n"));
  WSContentSend_P (PSTR ("div#l1 .cpu div{padding:0px;background:#444;border-radius:2px;}\n"));

  // style : cpu load graph
  WSContentSend_P (PSTR ("div.avg{padding:0px 6px;border:#444 solid 1px;border-radius:12px;}\n"));
  WSContentSend_P (PSTR ("svg.avg{width:100%%;margin:4px 0px;height:80px;}\n"));
  WSContentSend_P (PSTR ("polyline{stroke-width:4;fill:none;}\n"));

  WSContentSend_P (PSTR ("</style>\n"));
}

// CPU load status icon
void CpuLoadWebStatusLeft ()
{
  uint8_t level;
  int32_t load;
  char    str_color[8];

  // load average
  load = min ((uint32_t)100, TasmotaGlobal.loop_load_avg);
  if (load < 60) level = LOAD_LOW;
    else if (load < 90) level = LOAD_MEDIUM;
    else level = LOAD_HIGH;
  GetTextIndexed (str_color, sizeof (str_color), level, kLoadLevelColor);
  WSContentSend_P (PSTR ("<div class='cpu' style='background:%s;'><div style='height:%u%%;'></div></div><span class='top span'>%u%%</span>\n"), str_color, 100 - load, load);
}

// load average graph
void CpuLoadWebSensor ()
{
  uint8_t  second, index, slot;
  uint16_t pos_x, pos_y;

  // if RTC not set, ignore
  if (!RtcTime.valid) return;
  if (!cpu_load.enabled) return;

  // start of section
  WSContentSend_P (PSTR ("<div class='avg'>\n"));

  // curve declaration
  WSContentSend_P (PSTR ("<svg class='avg' viewBox='0 0 %u 200' preserveAspectRatio='none'>\n"), 10 * CPULOAD_SAMPLE - 10);
  WSContentSend_P (PSTR ("<linearGradient id='avg' x1=0 y1=0 x2=0 y2=1>\n"));
  WSContentSend_P (PSTR ("<stop stop-color='red' offset=%u%% /><stop stop-color='red' offset=%u%% />\n"),       100 - 100, 100 - 92);
  WSContentSend_P (PSTR ("<stop stop-color='orange' offset=%u%% /><stop stop-color='orange' offset=%u%% />\n"), 100 - 88,  100 - 72);
  WSContentSend_P (PSTR ("<stop stop-color='green' offset=%u%% /><stop stop-color='green' offset=%u%% />\n"),   100 - 68,  100 - 0);
  WSContentSend_P (PSTR ("</linearGradient>\n"));

  // curve data
  WSContentSend_P (PSTR ("<polyline stroke='url(#avg)' points='"));
  second = (uint16_t)RtcTime.second + 1;
  for (index = 0; index < CPULOAD_SAMPLE; index ++)
  {
    // display slot if valid
    slot = (second + index) % CPULOAD_SAMPLE;
    if (cpu_load.arr_load[slot] > 0)
    {
      // display point
      pos_x = (uint16_t)index * 10;
      pos_y = 200 - (uint16_t)cpu_load.arr_load[slot] * 2;
      WSContentSend_P (PSTR (" %u,%u"), pos_x, pos_y);

      // last point
      if (index == CPULOAD_SAMPLE - 1) WSContentSend_P (PSTR (" %u,%u %u,0"), pos_x + 10, pos_y, pos_x + 10);
    }
  }
  WSContentSend_P (PSTR ("' />\n"));

  // end of section
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentSend_P (PSTR ("</div>\n"));
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool XdrvCpuLoad (uint32_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_INIT:
      CpuLoadInit ();
      break;

    case FUNC_COMMAND:
      result = DecodeCommand (kCpuLoadCommands, CpuLoadCommand);
      break;

    case FUNC_EVERY_250_MSECOND:
      CpuLoadEvery250ms ();
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_MAIN_BUTTON:
      CpuLoadWebMainButton ();
      break;
      
    case FUNC_WEB_SENSOR:
      CpuLoadWebSensor ();
      break;
#endif  // USE_WEBSERVER
  }

  return result;
}

#endif    // USE_CPU_LOAD
