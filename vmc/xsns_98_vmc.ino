/*
  xsns_98_vmc.ino - Ventilation Motor Controled support 
  for Sonoff TH, Sonoff Basic or SonOff Dual
  
  Copyright (C) 2019 Nicolas Bernaerts
    15/03/2019 - v1.0 - Creation
    01/03/2020 - v2.0 - Functions rewrite for Tasmota 8.x compatibility
    07/03/2020 - v2.1 - Add daily humidity / temperature graph
    13/03/2020 - v2.2 - Add time on graph
    17/03/2020 - v2.3 - Handle Sonoff Dual and remote humidity sensor
    05/04/2020 - v2.4 - Add Timezone management
    15/05/2020 - v2.5 - Add /json page
    20/05/2020 - v2.6 - Add configuration for first NTP server
    26/05/2020 - v2.7 - Add Information JSON page
    15/09/2020 - v2.8 - Remove /json page, based on Tasmota 8.4
                        Add status icons and mode control

  Settings are stored using weighting scale parameters :
    - Settings.weight_reference   = VMC mode
    - Settings.weight_max         = Target humidity level (%)
    - Settings.weight_calibration = Humidity thresold (%) 
    
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

#ifdef USE_VMC

/*********************************************************************************************\
 * Fil Pilote
\*********************************************************************************************/

#define XSNS_98                  98

// web configuration page
#define D_PAGE_VMC_CONFIG       "vmc"
#define D_PAGE_VMC_CONTROL      "control"

// commands
#define D_CMND_VMC_MODE         "mode"
#define D_CMND_VMC_TARGET       "target"
#define D_CMND_VMC_THRESHOLD    "thres"
#define D_CMND_VMC_LOW          "low"
#define D_CMND_VMC_HIGH         "high"
#define D_CMND_VMC_AUTO         "auto"

// JSON data
#define D_JSON_VMC              "VMC"
#define D_JSON_VMC_MODE         "Mode"
#define D_JSON_VMC_STATE        "State"
#define D_JSON_VMC_LABEL        "Label"
#define D_JSON_VMC_TARGET       "Target"
#define D_JSON_VMC_HUMIDITY     "Humidity"
#define D_JSON_VMC_TEMPERATURE  "Temperature"
#define D_JSON_VMC_THRESHOLD    "Threshold"
#define D_JSON_VMC_RELAY        "Relay"

#define D_VMC_MODE              "VMC Mode"
#define D_VMC_STATE             "VMC State"
#define D_VMC_CONTROL           "Control"
#define D_VMC_LOCAL             "Local"
#define D_VMC_REMOTE            "Remote"
#define D_VMC_HUMIDITY          "Humidity"
#define D_VMC_SENSOR            "Sensor"
#define D_VMC_DISABLED          "Disabled"
#define D_VMC_LOW               "Low speed"
#define D_VMC_HIGH              "High speed"
#define D_VMC_AUTO              "Automatic"
#define D_VMC_BTN_LOW           "Low"
#define D_VMC_BTN_HIGH          "High"
#define D_VMC_BTN_AUTO          "Auto"
#define D_VMC_PARAMETERS        "VMC Parameters"
#define D_VMC_TARGET            "Target Humidity"
#define D_VMC_THRESHOLD         "Humidity Threshold"
#define D_VMC_CONFIGURE         "Configure VMC"
#define D_VMC_TIME              "Time"

// graph data
#define VMC_GRAPH_STEP          5           // collect graph data every 5 mn
#define VMC_GRAPH_SAMPLE        288         // 24 hours if data is collected every 5mn
#define VMC_GRAPH_WIDTH         800      
#define VMC_GRAPH_HEIGHT        500 
#define VMC_GRAPH_PERCENT_START 12      
#define VMC_GRAPH_PERCENT_STOP  88
#define VMC_GRAPH_TEMP_MIN      15      
#define VMC_GRAPH_TEMP_MAX      25  
#define VMC_GRAPH_HUMIDITY_MIN  0      
#define VMC_GRAPH_HUMIDITY_MAX  100  

// VMC data
#define VMC_TARGET_MAX          99
#define VMC_TARGET_DEFAULT      50
#define VMC_THRESHOLD_MAX       10
#define VMC_THRESHOLD_DEFAULT   2
#define VMC_ICON_MAX            3

// buffer
#define VMC_BUFFER_SIZE         128

// vmc humidity source
enum VmcSources { VMC_SOURCE_NONE, VMC_SOURCE_LOCAL, VMC_SOURCE_REMOTE };

// vmc states and modes
enum VmcStates { VMC_STATE_OFF, VMC_STATE_LOW, VMC_STATE_HIGH };
enum VmcModes { VMC_MODE_DISABLED, VMC_MODE_LOW, VMC_MODE_HIGH, VMC_MODE_AUTO };

// vmc commands
enum VmcCommands { CMND_VMC_MODE, CMND_VMC_TARGET, CMND_VMC_THRESHOLD, CMND_VMC_LOW, CMND_VMC_HIGH, CMND_VMC_AUTO };
const char kVmcCommands[] PROGMEM = D_CMND_VMC_MODE "|" D_CMND_VMC_TARGET "|" D_CMND_VMC_THRESHOLD "|" D_CMND_VMC_LOW "|" D_CMND_VMC_HIGH "|" D_CMND_VMC_AUTO;

// HTML chains
const char VMC_LINE_DASH[] PROGMEM = "<line class='dash' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n";
const char VMC_TEXT_TEMPERATURE[] PROGMEM = "<text class='temperature' x='%d%%' y='%d%%'>%s°C</text>\n";
const char VMC_TEXT_HUMIDITY[] PROGMEM = "<text class='humidity' x='%d%%' y='%d%%'>%d %%</text>\n";
const char VMC_TEXT_TIME[] PROGMEM = "<text class='time' x='%d' y='%d%%'>%sh</text>\n";
const char VMC_INPUT_BUTTON[] PROGMEM = "<input name='%s' class='button mode' value='%s' type='submit' %s />\n";

/****************************************\
 *               Icons
\****************************************/

// icon : fan off
const char fan_icon_off_0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAIAAAACAAQMAAAD58POIAAABhWlDQ1BJQ0MgcHJvZmlsZQAAKJF9kT1Iw0AcxV9bpSoVh1YQcchQxcGCqIijVqEIFUKt0KqDyaVf0KQhSXFxFFwLDn4sVh1cnHV1cBUEwQ8QJ0cnRRcp8X9JoUWMB8f9eHfvcfcO8NfLTDU7xgFVs4xUIi5ksqtC8BXdCKMfo5iWmKnPiWISnuPrHj6+3sV4lve5P0evkjMZ4BOIZ5luWMQbxNObls55nzjCipJCfE48ZtAFiR+5Lrv8xrngsJ9nRox0ap44QiwU2lhuY1Y0VOIp4qiiapTvz7iscN7irJarrHlP/sJQTltZ5jrNISSwiCWIECCjihLKsBCjVSPFRIr24x7+QccvkksmVwmMHAuoQIXk+MH/4He3Zn5ywk0KxYHOF9v+GAaCu0CjZtvfx7bdOAECz8CV1vJX6sDMJ+m1lhY9Avq2gYvrlibvAZc7wMCTLhmSIwVo+vN54P2MvikLhG+BnjW3t+Y+Th+ANHWVvAEODoGRAmWve7y7q723f880+/sBz1ByzGUugtQAAAAGUExURWyJf/8AAOtjICoAAAABdFJOUwBA5thmAAAAAWJLR0QAiAUdSAAAAAlwSFlzAAAN1wAADdcBQiibeAAAAAd0SU1FB+QJExU1Jf1lHl4AAAIASURBVEjHzZYxkuQgDEWhHDj0EbiJfTGXcbbX8g32CswNqNqEwIVWX3i6LTEzTjZYqoPmFSAh6SM7980YiwGRdjUfiJLeQaT3BKJTgaVOVZ+ZHGkgv9vg9UHZPQ3wbHM6DBjvYMiPIP0bMD2";
const char fan_icon_off_1[] PROGMEM = "D3YDwCBZngAoQ7rLdgc8So5+BV4niWQdw7m0UsazSMHbgUGDVt4ebPdAlNZtc8917oKsy7E9gOjqwmFJP83t7eYFR6juSRBlA6tsTHRfggq+ig4yQMViIWCRNB/6csRarpyJlz4tHFgTX/JQGiClWlsjhQnLhcDDGgH9uzLh2LLDocfhQUAWBcOAv7PQC+DQ2/ru2BM2u2btUeAKw9xipga1pUpyUBAHInnplbJX40KdutwawqXwN5qv4fgItuIhNvoPYgvMC/uX71mp+pE/XLtB8x5611UX8w7OA6zXg6QN3wyGnFNtEO3H+QkXEpneEBwLgJ27gtVGSc3DFD5XdTC3lsUwZucWB67KL9YSMIDYrrI3iShS8wtoggQ2w2ICTv62OVtFLeOvsNAJyp9GcKx3w5QloqeOWBuQrhTewOfOidGB25hFajIAsOIxOORpayTzrgH4NFvcENvPAolnoR+g0oOsNPcj/cffQvSFbUEyzwOVUs/BVYnQb5EZdHzFH07O/6Opd39fvuqdqvh1C+u4z4y+xi3oCpU+XTQAAAABJRU5ErkJggg==";

// icon : fan slow speed
const char fan_icon_low_0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAIAAAACAAQMAAAD58POIAAAC5XpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHja7ZdbsuMqDEX/GUUPAUkIieFgHlU9gx5+b2zHeZz0rTq3+qM/YsoGFCyJvYAkYfz6OcMPXFQkhaTmueQccaWSClc0PB7XUVNM+/PBtPef7OH6gGES1HJ08zjHV9j1/oKl074924O104+fjs4Pbg5lRWY0+pnR6Uj4sNPZD4WPRs0P0znveUvXjuq1nwxidIU/4cBDSOL+5COSHHfFXfAkKWugCNoq6bB81S9c0r0R8Gq96BfbaZe7HIej27Tyi06nnfS9frtKjxkRX5H5MSPTK8RX/Wb3Occxu5pygFz5nNRtKnsLAzfIKftrGcVwK9q2l4LiscYGah1T3ULc0CnEUHxSok6VJo29btSQYuLBhpq5sew2F+PCDTAI4qPQZAsg08VBpYGcwMxXLrTHLSsegjkid8JIJjgD4+cSXg3/tzw5mnMtc6Lol1bIi9f6QhqL3HpiFIDQPDXVXV8KRxVfrwVWQFB3mR0TrHE7XGxK97UlO2eJGjA0xWO/kPXTASRCbEUyJCAQM4lSpmjMRgQdHXwqMmdJvIEAaVDuyJKTSAYc5xUb7xjtY1n5MON4AQiVLAY02DqAlZKmjP3mWEI1qGhS1aymrkVrlpyy5pwtr3OqmlgytWxmbsWqiydXz27uXrwWLoJjTEPJxYqXUmpF0JoqfFWMrzBsvMmWNt3yZptvZasNy6elpi03a95Kq527dBwBoedu3XvpddDAUhpp6MjDho8y6sRamzLT1JmnTZ9l1ov";
const char fan_icon_low_1[] PROGMEM = "aSfWZ2iu5/6ZGJzXeQa1xdqcGs9nNBa3jRBczEONEIG6LABY0L2bRKSVe5BazWFiCiDKy1AWn0yIGgmkQ66SL3Z3cH7kFqPtdbvyOXFjo/ga5sNA9kPvK7Q21XvfjVnZAaxdCU5yQgu1Xhlf2Otto02tcbXw/facO333h4+jj6OPo4+jj6OPo4+ifdyQTPyDwpy/8BuLlk7Z8Fy7MAAABhWlDQ1BJQ0MgcHJvZmlsZQAAeJx9kT1Iw0AcxV9bpSoVh1YQcchQxcGCqIijVqEIFUKt0KqDyaVf0KQhSXFxFFwLDn4sVh1cnHV1cBUEwQ8QJ0cnRRcp8X9JoUWMB8f9eHfvcfcO8NfLTDU7xgFVs4xUIi5ksqtC8BXdCKMfo5iWmKnPiWISnuPrHj6+3sV4lve5P0evkjMZ4BOIZ5luWMQbxNObls55nzjCipJCfE48ZtAFiR+5Lrv8xrngsJ9nRox0ap44QiwU2lhuY1Y0VOIp4qiiapTvz7iscN7irJarrHlP/sJQTltZ5jrNISSwiCWIECCjihLKsBCjVSPFRIr24x7+QccvkksmVwmMHAuoQIXk+MH/4He3Zn5ywk0KxYHOF9v+GAaCu0CjZtvfx7bdOAECz8CV1vJX6sDMJ+m1lhY9Avq2gYvrlibvAZc7wMCTLhmSIwVo+vN54P2MvikLhG+BnjW3t+Y+Th+ANHWVvAEODoGRAmWve7y7q723f880+/sBz1ByzG39SBMAAAAGUExURQhFfzPMAKsZn9oAAAABdFJOUwBA5thmAAAAAWJLR0QAiAUdSAAAAAlwSFlzAAAN1wAADdcBQiibeAAAAAd0SU";
const char fan_icon_low_2[] PROGMEM = "1FB+QJExU2FIeWTacAAAJHSURBVEjHzZY9bh0hEMcHUZA04QKWuYaL1e6VXLpY7ZIqZXKDXIUb+ArkBkRuiISYzMB71g5r+zUpgl7D7/ExH/9hFuCdYfIAdvRirhGj3IEo9zjEIsBSbZVnRkAJ2u8waL0T95YBKLrThgGYI9DpJoj/BtjbwA/A3QQLDEAEiH3ZjkClFqOPgRKJotkJ8LmHkdvNIg3mBIIAq/SezTwDKal5yDX57n6MYJIa824Wpv/0Tgb5OThhmfkeFnW03Zo4w9VdR4G4Z/BEeo5N9gCTTjNM0PStEAM8MLgjwddWBwkeVZ7h04JIRdLqIKky81pebTPJnpRe6a/CmrdR49cAe6USCeAiuAAYG6AfmMRx2OkyREWHg86sAod84DcuQ9UAnUaXP9eeoBn6fZcqLAzIeh6xg63XZDOyJYhB21MvGVtbPPBat1sHvCm/DeaL+D4CPT8cm3QEew/OK1Cvtm9d8wavpl1At533rF0o+wvNHLvXgcJf7BsfUprYLHqk/LnKEbNcOy3CWiMDeuI0rWXgMZDidSUzI6fcUPht4tyy1eviLd8eOSNYHmF1/gvbF5ob8QFWG+54tW9WklYITLy6+RHhHlYTn7q8uuigmHisMeOL/h1FqWf9Ih62lNUfUR9rlnULU5alDp9HQMW/jmCD4UU5gRmGR2iRL/8JkKoFoGTI0qbZCcjXYIFbYBseWG4W8hEqAzj1hjNI/3H3kL0hjSAPzYKdE81C1RajwyClS33saR969htd/dT35buusA7fDi6+95nxF/d4jePtG0K3AAAAAElFTkSuQmCC";

// icon : fan fast speed
const char fan_icon_high_0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAIAAAACAAQMAAAD58POIAAAC5npUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHja7ZdbsuMqDEX/GUUPAUkIieFgHlU9gx5+b2zHeZz0rTq3+qM/YsqAFSyJvYAkYfz6OcMPXFQkhaTmueQccaWSCld0PB7X0VJMe/1g2p+f7OH6gGEStHI85nGOr7Dr/QVLp317tgdrpx8/HZ0f3BzKiszo9DOj05HwYafzORQ+OjU/TOe85y1dO5rX52QQoyv8CQceQhL3mo9IctwVd0FNUtZAEfRVEuok8lW/cEn3RsCr96JfbKdd7nIcjm7Tyi86nXbS9/rtKj1mRHxF5seMTK8QX/Wb3eccx+xqygFy5XNSt6nsPQzcIOehRkYx3Iq+7aWgeKyxgVrHVLcQNzwUYig+KVGnSpPG3jZqSDHxYEPL3Fh2m4tx4QYYBOFRaLIFkOnioNJATmDmKxfa45YVD8EckTthJBOcgfFzCa+G/1ueHM25ljlR9Esr5MVrfSGNRW7VGAUgNE9NddeXwtHE12uBFRDUXWbHBGvcDheb0n1tyc5ZogYMTfHYL2T9dACJEFuRDAkIxEyilCkasxFBRwefisxZEm8gQBqUO7JkLPsMOM4rNt4x2sey8mHG8QIQKlkMaLB1ACslTRn7zbGEalDRpKpZTV2L1iw5Zc05W17nVDWxZGrZzNyKVRdPrp7d3L14LVwEx5iGkosVL6XUiqA1VfiqGF9h2HiTLW265c0238pWG5ZPS01bbta8lVY7d+k4AkLP3br30uuggaU00tCRhw0fZdSJtTZlpqkzT5s+y6w";
const char fan_icon_high_1[] PROGMEM = "XtZPqM7VXcv9NjU5qvINa4+xODWazmwtax4kuZiDGiUDcFgEsaF7MolNKvMgtZrGwBBFlZKkLTqdFDATTINZJF7s7uT9yC1D3u9z4Hbmw0P0NcmGheyD3ldsbar3ux63sgNYuhKY4IQXbrwyv7HW20abXuPr4fvpOG777wsfRx9HH0cfRx9HH0cfRP+9IJn5AFPyl+g25AJOz9gVREgAAAYVpQ0NQSUNDIHByb2ZpbGUAAHicfZE9SMNAHMVfW6UqFYdWEHHIUMXBgqiIo1ahCBVCrdCqg8mlX9CkIUlxcRRcCw5+LFYdXJx1dXAVBMEPECdHJ0UXKfF/SaFFjAfH/Xh373H3DvDXy0w1O8YBVbOMVCIuZLKrQvAV3QijH6OYlpipz4liEp7j6x4+vt7FeJb3uT9Hr5IzGeATiGeZbljEG8TTm5bOeZ84woqSQnxOPGbQBYkfuS67/Ma54LCfZ0aMdGqeOEIsFNpYbmNWNFTiKeKoomqU78+4rHDe4qyWq6x5T/7CUE5bWeY6zSEksIgliBAgo4oSyrAQo1UjxUSK9uMe/kHHL5JLJlcJjBwLqECF5PjB/+B3t2Z+csJNCsWBzhfb/hgGgrtAo2bb38e23TgBAs/AldbyV+rAzCfptZYWPQL6toGL65Ym7wGXO8DAky4ZkiMFaPrzeeD9jL4pC4RvgZ41t7fmPk4fgDR1lbwBDg6BkQJlr3u8u6u9t3/PNPv7Ac9Qcsxt/UgTAAAABlBMVEWZiH8zzABspnZWAAAAAXRSTlMAQObYZgAAAAFiS0dEAIgFHUgAAAAJcEhZcwAADdcAAA3XAUIom3gAAAAHdE";
const char fan_icon_high_2[] PROGMEM = "lNRQfkCRMVNi3Yk8WvAAADWUlEQVRIx32WX4rbMBDGxxVULZRVDxCiK/QxBWP3KD1CHlMwsfu0j9sDlPYqgj3AXkF7Ay95ccFY/Wb0J3HSbQiE/CLNjOT5vgnRKy89XYE+DKvvKgS/3hHCeo8NYV6BdjHLOqansAbyvnhhvV3lna9AhZzGXQF9CU4jwNNF4RGcl5hHHEQ/juc6FQN3Lr6LoDnniKBUojwD40piPURQ5TsyJGCgnGYrABEOOUkGOc0ooOW9MasX0HP0CBwphDt+IRUBPiqAeeSfpAyizwxwFJ/KoG/4dWooFbKl6mUCMLmQmtTzhLg4Si1gh3QAHuk3AvaIMpP2iHCXCt0ycIgQS/XY2vFjqbkCKfRARwbbWCrAyGDgexmkcoRv+PR6kNrVoBzObn+QcgI0LwSoebMXoPm6bMP5GLxFsOr3YOWSvwq4I/XkLCrbCyCcQD+4Frk+pUvekNG+KU/FTjvaMjign720/Y5qNTY4ivR3FYKjHYMNGn4RHYy05xt914YAkYgOxmpueC2vNhPaHp2+4KeZe954Fb476hdIxJH1ZB0FLwBv0iPfQ49kIVQITmriLrCBA96zDCsBiIbkTyKpibtG8iUVztJGvQAfwTFqUoqUfmMge5bUgJ20SMi6PUbAm6Z/g0Zi/B/Edua7GS9BHy+ngKrUfuTDiZ+k0hKItfOeLnpAf8I3y8eLoArPfDYOMospmDAEGIxd+MYMa0duWKnAABansJbBEByUpBaU6VEZOqufzMjPlqvu2sFwds9PJMx76uxwx/U5OYbfUWfchlcPUiV3v3E1r5ZzeLR9p/2BPtqp2MGs/";
const char fan_icon_high_3[] PROGMEM = "QhtxdcHtP2sXnwC0rh+UqfcuNLa41T9ya0tzd9NbHY+qQHyqCccPMlDBPSeQRKQSAz20GWJiQgr0WFR5YEd5ZhlKkIGaLKQReowobZInc1AM8hmwHYBwdliF4jg8TDsfTYUWM6Ix2UfsuUgEm6UzK9sSsjVsu/9zLaFakwr9pmMDR9vjuL8yfrYaBdxfl3sk4cFl1IMNs2GulhwAofyVNg+tSt2ABsXUGwcRi+gGP3NKLgZFrfj5GbgqBPPhouRRBFcjlt/Nfi4jjWYKOrkchKupmm1yB1dvNDp6xHdj/3VzP7HVL+Z+24FqrBc/Xew/rW/GX8BCz3v9cz3fz0AAAAASUVORK5CYII=";

// icons associated to vmc
const char* vmc_fan_icon[][4] PROGMEM = { 
  {fan_icon_off_0,  fan_icon_off_1,  nullptr,         nullptr        },    // VMC_OFF
  {fan_icon_low_0,  fan_icon_low_1,  fan_icon_low_2,  nullptr        },    // VMC_SLOW
  {fan_icon_high_0, fan_icon_high_1, fan_icon_high_2, fan_icon_high_3}     // VMC_FAST
};

/*************************************************\
 *               Variables
\*************************************************/

// variables
uint8_t  vmc_devices_present;            // number of relays on the devices
uint8_t  vmc_humidity_source;            // humidity source
float    vmc_current_temperature;        // last temperature read
uint8_t  vmc_current_humidity;           // last read humidity level
uint8_t  vmc_current_target;             // last target humidity level

// graph variables
int      vmc_graph_refresh;
uint32_t vmc_graph_index;
uint32_t vmc_graph_counter;
float    vmc_graph_temperature;
uint8_t  vmc_graph_humidity;
uint8_t  vmc_graph_target;
uint8_t  vmc_graph_state;
float    arr_temperature[VMC_GRAPH_SAMPLE];
uint8_t  arr_humidity[VMC_GRAPH_SAMPLE];
uint8_t  arr_target[VMC_GRAPH_SAMPLE];
uint8_t  arr_state[VMC_GRAPH_SAMPLE];

/**************************************************\
 *                  Accessors
\**************************************************/

// get VMC label according to state
const char* VmcGetStateLabel (uint8_t state)
{
  const char* label = NULL;
    
  // get label
  switch (state)
  {
   case VMC_MODE_DISABLED:          // Disabled
     label = D_VMC_DISABLED;
     break;
   case VMC_MODE_LOW:               // Forced Low speed
     label = D_VMC_LOW;
     break;
   case VMC_MODE_HIGH:              // Forced High speed
     label = D_VMC_HIGH;
     break;
   case VMC_MODE_AUTO:              // Automatic mode
     label = D_VMC_AUTO;
     break;
  }
  
  return label;
}

// get VMC state from relays state
uint8_t VmcGetRelayState ()
{
  uint8_t relay1 = 0;
  uint8_t relay2 = 1;
  uint8_t state;

  // read relay states
  relay1 = bitRead (power, 0);
  if (devices_present == 2) relay2 = bitRead (power, 1);

  // convert to vmc state
  if ((relay1 == 0 ) && (relay2 == 1 )) state = VMC_STATE_LOW;
  else if (relay1 == 1) state = VMC_STATE_HIGH;
  else state  = VMC_STATE_OFF;
  
  return state;
}

// set relays state
void VmcSetRelayState (uint8_t new_state)
{
  // set number of relay to start command
  devices_present = vmc_devices_present;

  // set relays
  switch (new_state)
  {
    case VMC_MODE_DISABLED:  // VMC disabled
      ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
      if (devices_present == 2) ExecuteCommandPower (2, POWER_OFF, SRC_IGNORE);
      break;
    case VMC_MODE_LOW:  // VMC low speed
      ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
      if (devices_present == 2) ExecuteCommandPower (2, POWER_ON, SRC_IGNORE);
      break;
    case VMC_MODE_HIGH:  // VMC high speed
      if (devices_present == 2) ExecuteCommandPower (2, POWER_OFF, SRC_IGNORE);
      ExecuteCommandPower (1, POWER_ON, SRC_IGNORE);
      break;
  }

  // reset number of relay
  devices_present = 0;
}

// get vmc actual mode
uint8_t VmcGetMode ()
{
  uint8_t actual_mode;

  // read actual VMC mode
  actual_mode = (uint8_t) Settings.weight_reference;

  // if outvalue, set to disabled
  if (actual_mode > VMC_MODE_AUTO) actual_mode = VMC_MODE_DISABLED;

  return actual_mode;
}

// set vmc mode
void VmcSetMode (uint8_t new_mode)
{
  // if outvalue, set to disabled
  if (new_mode > VMC_MODE_AUTO) new_mode = VMC_MODE_DISABLED;

  // if forced mode, set relay state accordingly
  if (new_mode == VMC_MODE_LOW) VmcSetRelayState (VMC_STATE_LOW);
  else if (new_mode == VMC_MODE_HIGH) VmcSetRelayState (VMC_STATE_HIGH);

  // if within range, set mode
  Settings.weight_reference = (unsigned long) new_mode;
}

// get current temperature
float VmcGetTemperature ()
{
  float temperature = NAN;

#ifdef USE_DHT
  // if dht sensor present, read it 
  if (Dht[0].t != 0) temperature = Dht[0].t;
#endif

  return temperature;
}

// get current humidity level
uint8_t VmcGetHumidity ()
{
  uint8_t result   = UINT8_MAX;
  float   humidity = NAN;

  // read humidity from local sensor
  vmc_humidity_source = VMC_SOURCE_LOCAL;

#ifdef USE_DHT
  // if dht sensor present, read it 
  if (Dht[0].h != 0) humidity = Dht[0].h;
#endif

  // if not available, read MQTT humidity
  if (isnan (humidity))
  {
    vmc_humidity_source = VMC_SOURCE_REMOTE;
    humidity = HumidityGetValue ();
  }

  // convert to integer
  if (isnan (humidity) == false) result = int (humidity);

  return result;
}

// set target humidity
void VmcSetTargetHumidity (uint8_t new_target)
{
  // if in range, save target humidity level
  if (new_target <= VMC_TARGET_MAX) Settings.weight_max = (uint16_t) new_target;
}

// get target humidity
uint8_t VmcGetTargetHumidity ()
{
  uint8_t target;

  // get target temperature
  target = (uint8_t) Settings.weight_max;
  if (target > VMC_TARGET_MAX) target = VMC_TARGET_DEFAULT;
  
  return target;
}

// set vmc humidity threshold
void VmcSetThreshold (uint8_t new_threshold)
{
  // if within range, save threshold
  if (new_threshold <= VMC_THRESHOLD_MAX) Settings.weight_calibration = (unsigned long) new_threshold;
}

// get vmc humidity threshold
uint8_t VmcGetThreshold ()
{
  uint8_t threshold;

  // get humidity threshold
  threshold = (uint8_t) Settings.weight_calibration;
   if (threshold > VMC_THRESHOLD_MAX) threshold = VMC_THRESHOLD_DEFAULT;

  return threshold;
}

// Show JSON status (for MQTT)
void VmcShowJSON (bool append)
{
  uint8_t humidity, value, mode;
  float   temperature;
  String  str_mqtt, str_json, str_text;

  // save MQTT data
  str_mqtt = mqtt_data;

  // get mode and humidity
  mode     = VmcGetMode ();
  str_text = VmcGetStateLabel (mode);

  // vmc mode  -->  "VMC":{"Relay":2,"Mode":4,"Label":"Automatic","Humidity":70.5,"Target":50,"Temperature":18.4}
  str_json  = "\"" + String (D_JSON_VMC) + "\":{";
  str_json += "\"" + String (D_JSON_VMC_RELAY) + "\":" + String (devices_present) + ",";
  str_json += "\"" + String (D_JSON_VMC_MODE) + "\":" + String (mode) + ",";
  str_json += "\"" + String (D_JSON_VMC_LABEL) + "\":\"" + str_text + "\",";

  // temperature
  temperature = VmcGetTemperature ();
  if (isnan(temperature) == false) str_text = String (temperature, 1);
  else str_text = "n/a";
  str_json += "\"" + String (D_JSON_VMC_TEMPERATURE) + "\":" + str_text + ",";

  // humidity level
  humidity = VmcGetHumidity ();
  if (humidity != UINT8_MAX) str_text = String (humidity);
  else str_text = "n/a";
  str_json += "\"" + String (D_JSON_VMC_HUMIDITY) + "\":" + str_text + ",";

  // target humidity
  value = VmcGetTargetHumidity ();
  str_json += "\"" + String (D_JSON_VMC_TARGET) + "\":" + String (value) + ",";

  // humidity thresold
  value = VmcGetThreshold ();
  str_json += "\"" + String (D_JSON_VMC_THRESHOLD) + "\":" + String (value) + "}";

  // if VMC mode is enabled
  if (mode != VMC_MODE_DISABLED)
  {
    // get relay state and label
    value    = VmcGetRelayState ();
    str_text = VmcGetStateLabel (value);

    // relay state  -->  ,"State":{"Mode":1,"Label":"On"}
    str_json += ",";
    str_json += "\"" + String (D_JSON_VMC_STATE) + "\":{";
    str_json += "\"" + String (D_JSON_VMC_MODE) + "\":" + String (value) + ",";
    str_json += "\"" + String (D_JSON_VMC_LABEL) + "\":\"" + str_text + "\"}";
  }

  // add remote humidity to JSON
  snprintf_P(mqtt_data, sizeof(mqtt_data), str_json.c_str());
  HumidityShowJSON (true);
  str_json = mqtt_data;

  // generate MQTT message and publish if needed
  if (append == false) 
  {
    str_mqtt = "{" + str_json + "}";
    snprintf_P(mqtt_data, sizeof(mqtt_data), str_mqtt.c_str());
    MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
  }
  else
  {
    str_mqtt += "," + str_json;
    snprintf_P(mqtt_data, sizeof(mqtt_data), str_mqtt.c_str());
  }
}

// Handle VMC MQTT commands
bool VmcCommand ()
{
  bool command_serviced = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kVmcCommands);

  // handle command
  switch (command_code)
  {
    case CMND_VMC_MODE:        // set mode
      VmcSetMode (XdrvMailbox.payload);
      break;
    case CMND_VMC_LOW:         // set mode to low
      VmcSetMode (VMC_MODE_LOW);
      break;
    case CMND_VMC_HIGH:         // set mode to high
      VmcSetMode (VMC_MODE_HIGH);
      break;
    case CMND_VMC_AUTO:         // set mode to auto
      VmcSetMode (VMC_MODE_AUTO);
      break;
    case CMND_VMC_TARGET:     // set target humidity 
      VmcSetTargetHumidity ((uint8_t) XdrvMailbox.payload);
      break;
    case CMND_VMC_THRESHOLD:  // set humidity threshold 
      VmcSetThreshold ((uint8_t) XdrvMailbox.payload);
      break;
    default:
      command_serviced = false;
      break;
  }

  // if command processed, publish JSON
  if (command_serviced == true) VmcShowJSON (false);

  return command_serviced;
}

// update graph history data
void VmcUpdateHistory ()
{
  // set indexed graph values with current values
  arr_temperature[vmc_graph_index] = vmc_graph_temperature;
  arr_humidity[vmc_graph_index] = vmc_graph_humidity;
  arr_target[vmc_graph_index] = vmc_graph_target;
  arr_state[vmc_graph_index] = vmc_graph_state;

  // init current values
  vmc_graph_temperature = NAN;
  vmc_graph_humidity    = UINT8_MAX;
  vmc_graph_target      = UINT8_MAX;
  vmc_graph_state       = VMC_STATE_OFF;

  // increase temperature data index and reset if max reached
  vmc_graph_index ++;
  vmc_graph_index = vmc_graph_index % VMC_GRAPH_SAMPLE;
}

void VmcEverySecond ()
{
  bool    need_update = false;
  uint8_t humidity, threshold, mode, target, actual_state, target_state;
  float   temperature;

  // update current temperature
  temperature = VmcGetTemperature ( );
  if (isnan(temperature) == false)
  {
    // save current value and ask for JSON update if any change
    if (vmc_current_temperature != temperature) need_update = true;
    vmc_current_temperature = temperature;

    // update graph value
    if (isnan(vmc_graph_temperature) == false) vmc_graph_temperature = min (vmc_graph_temperature, temperature);
    else vmc_graph_temperature = temperature;
  }

  // update current humidity
  humidity = VmcGetHumidity ( );
  if (humidity != UINT8_MAX)
  {
    // save current value and ask for JSON update if any change
    if (vmc_current_humidity != humidity) need_update = true;
    vmc_current_humidity = humidity;

    // update graph value
    if (vmc_graph_humidity != UINT8_MAX) vmc_graph_humidity = max (vmc_graph_humidity, humidity);
    else vmc_graph_humidity = humidity;
  }

  // update target humidity
  target = VmcGetTargetHumidity ();
  if (target != UINT8_MAX)
  {
    // save current value and ask for JSON update if any change
    if (vmc_current_target != target) need_update = true;
    vmc_current_target = target;

    // update graph value
    if (vmc_graph_target != UINT8_MAX) vmc_graph_target = min (vmc_graph_target, target);
    else vmc_graph_target = target;
  } 

  // update relay state
  actual_state = VmcGetRelayState ();
  if (vmc_graph_state != VMC_STATE_HIGH) vmc_graph_state = actual_state;

  // get VMC mode
  mode = VmcGetMode ();

  // if only one relay and mode is disabled, it is considered as low
  if ((devices_present == 1) && (mode == VMC_MODE_DISABLED)) mode = VMC_MODE_LOW;

  // if VMC mode is automatic
  if (mode == VMC_MODE_AUTO)
  {
    // get current and target humidity
    threshold = VmcGetThreshold ();

    // if humidity is low enough, target VMC state is low speed
    if (humidity < (target - threshold)) target_state = VMC_STATE_LOW;
      
    // else, if humidity is too high, target VMC state is high speed
    else if (humidity > (target + threshold)) target_state = VMC_STATE_HIGH;

    // else, keep current state
    else target_state = actual_state;
  }

  // else, convert target mode to target state
  else target_state = mode;

  // if VMC state is different than target state, set relay
  if (actual_state != target_state)
  {
    VmcSetRelayState (target_state);
    need_update = true;
  }
  
  // if JSON update needed, publish
  if (need_update == true) VmcShowJSON (false);

  // increment delay counter and if delay reached, update history data
  if (vmc_graph_counter == 0) VmcUpdateHistory ();
  vmc_graph_counter ++;
  vmc_graph_counter = vmc_graph_counter % vmc_graph_refresh;
}

void VmcInit ()
{
  int    index;

  // save number of devices and set it to 0 to avoid direct command
  vmc_devices_present = devices_present;
  devices_present = 0;

  // init default values
  vmc_humidity_source     = VMC_SOURCE_NONE;
  vmc_current_temperature = NAN;
  vmc_current_humidity    = UINT8_MAX;
  vmc_current_target      = UINT8_MAX;
  vmc_graph_temperature = NAN;
  vmc_graph_humidity    = UINT8_MAX;
  vmc_graph_target      = UINT8_MAX;
  vmc_graph_state       = VMC_STATE_OFF;
  vmc_graph_index       = 0;
  vmc_graph_counter     = 0;
  vmc_graph_refresh     = 60 * VMC_GRAPH_STEP;

  // initialise graph data
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    arr_temperature[index] = NAN;
    arr_humidity[index] = UINT8_MAX;
    arr_target[index] = UINT8_MAX;
    arr_state[index] = VMC_MODE_LOW;
  }
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// display base64 embeded icon
void VmcWebDisplayIcon (uint8_t icon_height, uint8_t icon_index)
{
  uint8_t nbrItem, index;

  // display icon
  if (icon_index < VMC_ICON_MAX)
  {
    WSContentSend_P (PSTR ("<img height=%d src='data:image/png;base64,"), icon_height);
    nbrItem = sizeof (vmc_fan_icon[icon_index]) / sizeof (char*);
    for (index=0; index<nbrItem; index++)
      if (vmc_fan_icon[icon_index][index] != nullptr) WSContentSend_P (vmc_fan_icon[icon_index][index]);
    WSContentSend_P (PSTR ("' >"));
  }
}

// VMC mode select combo
void VmcWebSelectMode (bool autosubmit)
{
  uint8_t mode, humidity;
  char    argument[VMC_BUFFER_SIZE];

  // get mode and humidity
  mode     = VmcGetMode ();
  humidity = VmcGetHumidity ();

  // selection : beginning
  WSContentSend_P (PSTR ("<select name='%s'"), D_CMND_VMC_MODE);
  if (autosubmit == true) WSContentSend_P (PSTR (" onchange='this.form.submit()'"));
  WSContentSend_P (PSTR (">"));
  
  // selection : disabled
  if (mode == VMC_MODE_DISABLED) strcpy (argument, "selected"); 
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), VMC_MODE_DISABLED, argument, D_VMC_DISABLED);

  // selection : low speed
  if (mode == VMC_MODE_LOW) strcpy (argument, "selected");
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), VMC_MODE_LOW, argument, D_VMC_LOW);

  // selection : high speed
  if (mode == VMC_MODE_HIGH) strcpy (argument, "selected");
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), VMC_MODE_HIGH, argument, D_VMC_HIGH);

  // selection : automatic
  if (humidity != UINT8_MAX) 
  {
    if (mode == VMC_MODE_AUTO) strcpy (argument, "selected");
    else strcpy (argument, "");
    WSContentSend_P (PSTR("<option value='%d' %s>%s</option>"), VMC_MODE_AUTO, argument, D_VMC_AUTO);
  }

  // selection : end
  WSContentSend_P (PSTR ("</select>"));
}

// append VMC control button to main page
void VmcWebMainButton ()
{
    // VMC control page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_VMC_CONTROL, D_VMC_CONTROL);
}

// append VMC configuration button to configuration page
void VmcWebConfigButton ()
{
  // VMC configuration button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>"), D_PAGE_VMC_CONFIG, D_VMC_CONFIGURE);
}

// append VMC state to main page
bool VmcWebSensor ()
{
  uint8_t  mode, state, humidity, target;
  String   str_title, str_text, str_value, str_source, str_color;

  // get VMC mode
  mode = VmcGetMode ();

  // -----------------------
  //   Humidity and Target 
  // -----------------------

  // if automatic mode, display humidity and target humidity
  if (mode == VMC_MODE_AUTO)
  {
    // read current and target humidity
    humidity = VmcGetHumidity ();
    target   = VmcGetTargetHumidity ();

    // handle sensor source
    switch (vmc_humidity_source)
    {
      case VMC_SOURCE_NONE:  // no humidity source available 
        str_value  = "--";
        break;
      case VMC_SOURCE_LOCAL:  // local humidity source used 
        str_source = D_VMC_LOCAL;
        str_value  = String (humidity);
        break;
      case VMC_SOURCE_REMOTE:  // remote humidity source used 
        str_source = D_VMC_REMOTE;
        str_value  = String (humidity);
        break;
    }

    // set title and text
    str_title = D_VMC_HUMIDITY;
    if (str_source.length() > 0) str_title += " (" + str_source + ")";
    str_text  = "<b>" + str_value + "</b> / " + String (target) + "%";

    // display
    WSContentSend_PD (PSTR("{s}%s{m}%s{e}"), str_title.c_str(), str_text.c_str());
  }

  // --------------
  //   VMC state 
  // --------------

  // display vmc icon status
  state = VmcGetRelayState ();
  WSContentSend_PD (PSTR("<tr><td colspan=2 style='width:100%;text-align:center;padding:10px;'>"));
  VmcWebDisplayIcon (64, state);
  WSContentSend_PD (PSTR("</td></tr>\n"));
}

// VMC web page
void VmcWebPageConfig ()
{
  uint8_t value;
  char    argument[VMC_BUFFER_SIZE];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg("save"))
  {
    // get VMC mode according to MODE parameter
    WebGetArg (D_CMND_VMC_MODE, argument, VMC_BUFFER_SIZE);
    if (strlen(argument) > 0) VmcSetMode ((uint8_t) atoi (argument)); 

    // get VMC target humidity according to TARGET parameter
    WebGetArg (D_CMND_VMC_TARGET, argument, VMC_BUFFER_SIZE);
    if (strlen(argument) > 0) VmcSetTargetHumidity ((uint8_t) atoi (argument));

    // get VMC humidity threshold according to THRESHOLD parameter
    WebGetArg (D_CMND_VMC_THRESHOLD, argument, VMC_BUFFER_SIZE);
    if (strlen(argument) > 0) VmcSetThreshold ((uint8_t) atoi (argument));
  }

  // beginning of form
  WSContentStart_P (D_VMC_CONFIGURE);
  WSContentSendStyle ();
  WSContentSend_P (PSTR("<form method='get' action='%s'>\n"), D_PAGE_VMC_CONFIG);

  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>\n"), D_VMC_PARAMETERS);

  // select mode
  WSContentSend_P (PSTR ("<p>%s<br/>"), D_VMC_MODE);
  VmcWebSelectMode (false);
  WSContentSend_P (PSTR ("</p>\n"));

  // target humidity label and input
  value = VmcGetTargetHumidity ();
  WSContentSend_P (PSTR ("<p>%s<br/><input type='number' name='%s' min='0' max='%d' step='1' value='%d'></p>\n"), D_VMC_TARGET, D_CMND_VMC_TARGET, VMC_TARGET_MAX, value);

  // humidity threshold label and input
  value = VmcGetThreshold ();
  WSContentSend_P (PSTR ("<p>%s<br/><input type='number' name='%s' min='0' max='%d' step='1' value='%d'></p>\n"), D_VMC_THRESHOLD, D_CMND_VMC_THRESHOLD, VMC_THRESHOLD_MAX, value);

  WSContentSend_P (PSTR("</fieldset></p>\n"));

  // save button
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Humidity and temperature graph
void VmcWebDisplayGraph ()
{
  TIME_T   current_dst;
  uint32_t current_time;
  int      index, array_idx;
  int      graph_x, graph_y, graph_x1, graph_x2, graph_left, graph_right, graph_width, graph_pos, graph_hour;
  uint8_t  humidity, target, state_curr, state_prev;
  float    temperature, temp_min, temp_max, temp_scope;
  char     str_hour[4];
  String   str_value;

// A RECODER

  // loop to adjust min and max temperature
  temp_min   = VMC_GRAPH_TEMP_MIN;
  temp_max   = VMC_GRAPH_TEMP_MAX;
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if needed, adjust minimum and maximum temperature
    temperature = arr_temperature[index];
    if (isnan (temperature) == false)
    {
      if (temperature < temp_min) temp_min = floor (temperature);
      if (temperature > temp_max) temp_max = ceil (temperature);
    }
  }
  temp_scope = temp_max - temp_min;

  // boundaries of SVG graph
  graph_left  = VMC_GRAPH_PERCENT_START * VMC_GRAPH_WIDTH / 100;
  graph_right = VMC_GRAPH_PERCENT_STOP * VMC_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentSend_P (PSTR ("<svg viewBox='0 0 %d %d'>\n"), VMC_GRAPH_WIDTH, VMC_GRAPH_HEIGHT);

  // graph curve zone
  WSContentSend_P (PSTR ("<rect x='%d%%' y='%d%%' width='%d%%' height='%d%%' rx='10' />\n"), VMC_GRAPH_PERCENT_START, 0, VMC_GRAPH_PERCENT_STOP - VMC_GRAPH_PERCENT_START, 100);

  // graph separation lines
  WSContentSend_P (VMC_LINE_DASH, graph_left, 25, graph_right, 25);
  WSContentSend_P (VMC_LINE_DASH, graph_left, 50, graph_right, 50);
  WSContentSend_P (VMC_LINE_DASH, graph_left, 75, graph_right, 75);

  // temperature units
  str_value = String (temp_max, 1);
  WSContentSend_P (VMC_TEXT_TEMPERATURE, 1, 5, str_value.c_str ());
  str_value = String (temp_min + temp_scope * 0.75, 1);
  WSContentSend_P (VMC_TEXT_TEMPERATURE, 1, 27, str_value.c_str ());
  str_value = String (temp_min + temp_scope * 0.50, 1);
  WSContentSend_P (VMC_TEXT_TEMPERATURE, 1, 52, str_value.c_str ());
  str_value = String (temp_min + temp_scope * 0.25, 1);
  WSContentSend_P (VMC_TEXT_TEMPERATURE, 1, 77, str_value.c_str ());
  str_value = String (temp_min, 1);
  WSContentSend_P (VMC_TEXT_TEMPERATURE, 1, 99, str_value.c_str ());

  // humidity units
  WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, 5, 100);
  WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, 27, 75);
  WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, 52, 50);
  WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, 77, 25);
  WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, 99, 0);

  // ---------------
  //   Relay state
  // ---------------

  // loop for the relay state as background red color
  state_prev = VMC_STATE_LOW;
  graph_x1 = 0;
  graph_x2 = 0;
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // get target temperature value and set to minimum if not defined
    array_idx  = (index + vmc_graph_index) % VMC_GRAPH_SAMPLE;
    state_curr = arr_state[array_idx];

    // last graph point, force draw
    if ((index == VMC_GRAPH_SAMPLE - 1) && (state_prev == VMC_STATE_HIGH)) state_curr = VMC_STATE_LOW;

    // if relay just switched on, record start point x1
    if ((state_prev != VMC_STATE_HIGH) && (state_curr == VMC_STATE_HIGH)) graph_x1  = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
    
    // esle, if relay just switched off, record end point x2
    else if ((state_prev == VMC_STATE_HIGH) && (state_curr != VMC_STATE_HIGH)) graph_x2  = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);

    // if both point recorded,
    if ((graph_x1 != 0) && (graph_x2 != 0))
    {
      // display graph
      WSContentSend_P (PSTR ("<rect class='relay' x='%d' y='95%%' width='%d' height='5%%' />\n"), graph_x1, graph_x2 - graph_x1);

      // reset records
      graph_x1 = 0;
      graph_x2 = 0;
    }

    // update previous state
    state_prev = state_curr;
  }

  // --------------------
  //   Target Humidity
  // --------------------

  // loop for the target humidity graph
  WSContentSend_P (PSTR ("<polyline class='target' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // get target temperature value and set to minimum if not defined
    array_idx = (index + vmc_graph_index) % VMC_GRAPH_SAMPLE;
    target    = arr_target[array_idx];

    // if target value is defined,
    if (target != UINT8_MAX)
    {
      // calculate current position
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - (target * VMC_GRAPH_HEIGHT / 100);

      // add the point to the line
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // ----------------
  //   Temperature
  // ----------------

  // loop for the temperature graph
  WSContentSend_P (PSTR ("<polyline class='temperature' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if temperature value is defined
    array_idx   = (index + vmc_graph_index) % VMC_GRAPH_SAMPLE;
    temperature = arr_temperature[array_idx];
    if (isnan (temperature) == false)
    {
      // calculate current position
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - int (((temperature - temp_min) / temp_scope) * VMC_GRAPH_HEIGHT);

      // add the point to the line
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // ---------------
  //    Humidity
  // ---------------

  // loop for the humidity graph
  WSContentSend_P (PSTR ("<polyline class='humidity' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if temperature value is defined
    array_idx = (index + vmc_graph_index) % VMC_GRAPH_SAMPLE;
    humidity  = arr_humidity[array_idx];
    if (humidity != UINT8_MAX)
    {
      // adjust current temperature to acceptable range
      humidity = min (max ((int) humidity, 0), 100);

      // calculate current position
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - (humidity * VMC_GRAPH_HEIGHT / 100);

      // add the point to the line
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

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
  WSContentSend_P (VMC_TEXT_TIME, graph_x, 51, str_hour);

  // dislay next 5 time marks (every 4 hours)
  for (index = 0; index < 5; index++)
  {
    current_dst.hour = (current_dst.hour + 4) % 24;
    graph_x += graph_width / 6;
    sprintf(str_hour, "%02d", current_dst.hour);
    WSContentSend_P (VMC_TEXT_TIME, graph_x, 51, str_hour);
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
}

// VMC control public web page
void VmcWebPageControl ()
{
  uint8_t mode, state, humidity;
  float   temperature;
  String  str_text;

  // check if vmc state has changed
  if (Webserver->hasArg(D_CMND_VMC_LOW)) VmcSetMode (VMC_MODE_LOW);
  else if (Webserver->hasArg(D_CMND_VMC_HIGH)) VmcSetMode (VMC_MODE_HIGH);
  else if (Webserver->hasArg(D_CMND_VMC_AUTO)) VmcSetMode (VMC_MODE_AUTO);

  // beginning of form without authentification with 60 seconds auto refresh
  WSContentStart_P (D_VMC_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%d;URL=/%s' />\n"), 60, D_PAGE_VMC_CONTROL);
  
  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {color:white;background-color:#303030;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));

  WSContentSend_P (PSTR (".title {font-size:5vh;}\n"));
  WSContentSend_P (PSTR (".value {font-size:4vh;}\n"));
  WSContentSend_P (PSTR (".bold {font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".button {border:none;border-radius:6px;padding:auto;margin:12px;font-size:2vh;color:white;}\n"));
  WSContentSend_P (PSTR (".mode {background-color:#a569bd;border:2px solid #a569bd;padding:6px;margin:16px 6px;width:80px;}\n"));
  WSContentSend_P (PSTR (".mode:disabled {border:2px solid white;font-weight:bold;}\n"));

  WSContentSend_P (PSTR (".graph {margin-top:20px;max-width:%dpx;}\n"), VMC_GRAPH_WIDTH);
  WSContentSend_P (PSTR (".yellow {color:#FFFF33;}\n"));
  WSContentSend_P (PSTR (".orange {color:#FF8C00;}\n"));

  WSContentSend_P (PSTR (".temperature {font-size:24px;}\n"));
  WSContentSend_P (PSTR (".humidity {font-size:24px;}\n"));
  WSContentSend_P (PSTR (".time {font-size:20px;}\n"));

  WSContentSend_P (PSTR ("rect.graph {stroke:grey;stroke-dasharray:1;fill:#101010;}\n"));
  WSContentSend_P (PSTR ("rect.relay {fill:red;fill-opacity:50%%;}\n"));
  WSContentSend_P (PSTR ("polyline.temperature {fill:none;stroke:yellow;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.humidity {fill:none;stroke:orange;stroke-width:4;}\n"));
  WSContentSend_P (PSTR ("polyline.target {fill:none;stroke:orange;stroke-width:2;stroke-dasharray:4;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("text.temperature {stroke:yellow;fill:yellow;}\n"));
  WSContentSend_P (PSTR ("text.humidity {stroke:orange;fill:orange;}\n"));
  WSContentSend_P (PSTR ("text.time {stroke:white;fill:white;}\n"));

  WSContentSend_P (PSTR ("</style>\n</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<form name='control' method='get' action='%s'>\n"), D_PAGE_VMC_CONTROL);

  // room name
  WSContentSend_P (PSTR ("<div class='title bold'>%s</div>\n"), SettingsText(SET_DEVICENAME));

  // display icon
  state = VmcGetRelayState ();
  WSContentSend_P (PSTR ("<div class='icon'>"));
  VmcWebDisplayIcon (92, state);
  WSContentSend_P (PSTR ("</div>\n"));

  // display temperature
  temperature = VmcGetTemperature ();
  if (isnan (temperature) == false) WSContentSend_P (PSTR ("<div class='value bold yellow'>%s °C</div>\n"), String (temperature, 1).c_str());

  // display humidity
  humidity = VmcGetHumidity ();
  WSContentSend_P (PSTR ("<div class='value bold orange'>%d %%</div>\n"), humidity);

  // display vmc mode selector
  mode = VmcGetMode ();
  WSContentSend_P (PSTR ("<div>\n"));
  str_text="";
  if (mode == VMC_MODE_AUTO) str_text="disabled";
  WSContentSend_P (VMC_INPUT_BUTTON, D_CMND_VMC_AUTO, D_VMC_BTN_AUTO, str_text.c_str ());
  str_text="";
  if (mode == VMC_MODE_LOW) str_text="disabled";
  WSContentSend_P (VMC_INPUT_BUTTON, D_CMND_VMC_LOW, D_VMC_BTN_LOW, str_text.c_str ());
  str_text="";
  if (mode == VMC_MODE_HIGH) str_text="disabled";
  WSContentSend_P (VMC_INPUT_BUTTON, D_CMND_VMC_HIGH, D_VMC_BTN_HIGH, str_text.c_str ());
  WSContentSend_P (PSTR ("</div>\n"));

  // display humidity / temperature graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  VmcWebDisplayGraph ();
  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
  WSContentSend_P (PSTR ("</form>\n"));
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/*******************************************************\
 *                      Interface
\*******************************************************/

bool Xsns98 (byte callback_id)
{
  bool result = false;

  // main callback switch
  switch (callback_id)
  {
    case FUNC_INIT:
      VmcInit ();
      break;
    case FUNC_COMMAND:
      result = VmcCommand ();
      break;
    case FUNC_EVERY_SECOND:
      VmcEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      VmcShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on ("/" D_PAGE_VMC_CONFIG,  VmcWebPageConfig);
      Webserver->on ("/" D_PAGE_VMC_CONTROL, VmcWebPageControl);
      break;
    case FUNC_WEB_SENSOR:
      VmcWebSensor ();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      VmcWebMainButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      VmcWebConfigButton ();
      break;
#endif  // USE_WEBSERVER

  }
  return result;
}

#endif // USE_VMC
