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
    08/10/2020 - v3.0 - Handle graph with js auto update

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
#define D_PAGE_VMC_BASE_SVG     "base.svg"
#define D_PAGE_VMC_DATA_SVG     "data.svg"

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
#define VMC_GRAPH_WIDTH         800      
#define VMC_GRAPH_HEIGHT        500 
#define VMC_GRAPH_SAMPLE        800
#define VMC_GRAPH_REFRESH       108         // collect data every 108 sec to get 24h graph with 800 samples
#define VMC_GRAPH_MODE_LOW      20 
#define VMC_GRAPH_MODE_HIGH     40 
#define VMC_GRAPH_PERCENT_START 12      
#define VMC_GRAPH_PERCENT_STOP  88
#define VMC_GRAPH_TEMP_MIN      15      
#define VMC_GRAPH_TEMP_MAX      25       

// VMC data
#define VMC_HUMIDITY_MAX        100  
#define VMC_TARGET_MAX          99
#define VMC_TARGET_DEFAULT      50
#define VMC_THRESHOLD_MAX       10
#define VMC_THRESHOLD_DEFAULT   2

// buffer
#define VMC_BUFFER_SIZE         128

// vmc humidity source
enum VmcSources { VMC_SOURCE_NONE, VMC_SOURCE_LOCAL, VMC_SOURCE_REMOTE };

// vmc states and modes
enum VmcStates { VMC_STATE_OFF, VMC_STATE_LOW, VMC_STATE_HIGH, VMC_STATE_NONE, VMC_STATE_MAX };
enum VmcModes { VMC_MODE_DISABLED, VMC_MODE_LOW, VMC_MODE_HIGH, VMC_MODE_AUTO, VMC_MODE_MAX };

// vmc commands
enum VmcCommands { CMND_VMC_MODE, CMND_VMC_TARGET, CMND_VMC_THRESHOLD, CMND_VMC_LOW, CMND_VMC_HIGH, CMND_VMC_AUTO };
const char kVmcCommands[] PROGMEM = D_CMND_VMC_MODE "|" D_CMND_VMC_TARGET "|" D_CMND_VMC_THRESHOLD "|" D_CMND_VMC_LOW "|" D_CMND_VMC_HIGH "|" D_CMND_VMC_AUTO;

// HTML chains
const char VMC_LINE_DASH[] PROGMEM = "<line class='dash' x1='%d' y1='%d%%' x2='%d' y2='%d%%' />\n";
const char VMC_TEXT_TEMPERATURE[] PROGMEM = "<text class='temperature' x='%d%%' y='%d%%'>%s°C</text>\n";
const char VMC_TEXT_HUMIDITY[] PROGMEM = "<text class='humidity' x='%d%%' y='%d%%'>%d %%</text>\n";
const char VMC_TEXT_TIME[] PROGMEM = "<text class='time' x='%d' y='%d%%'>%sh</text>\n";
const char VMC_INPUT_BUTTON[] PROGMEM = "<button name='%s' class='button %s'>%s</button>\n";
const char VMC_INPUT_NUMBER[] PROGMEM = "<p>%s<br/><input type='number' name='%s' min='0' max='%d' step='1' value='%d'></p>\n";

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
  {fan_icon_off_0,  fan_icon_off_1,  nullptr,         nullptr        },    // VMC_STATE_OFF
  {fan_icon_low_0,  fan_icon_low_1,  fan_icon_low_2,  nullptr        },    // VMC_STATE_SLOW
  {fan_icon_high_0, fan_icon_high_1, fan_icon_high_2, fan_icon_high_3}     // VMC_STATE_FAST
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
String VmcGetStateLabel (uint8_t state)
{
  String str_label;

  // get label
  switch (state)
  {
   case VMC_MODE_DISABLED:          // Disabled
     str_label = D_VMC_DISABLED;
     break;
   case VMC_MODE_LOW:               // Forced Low speed
     str_label = D_VMC_LOW;
     break;
   case VMC_MODE_HIGH:              // Forced High speed
     str_label = D_VMC_HIGH;
     break;
   case VMC_MODE_AUTO:              // Automatic mode
     str_label = D_VMC_AUTO;
     break;
  }
  
  return str_label;
}

// get VMC state from relays state
uint8_t VmcGetRelayState ()
{
  uint8_t vmc_relay1 = 0;
  uint8_t vmc_relay2 = 1;
  uint8_t relay_state = VMC_STATE_OFF;

  // set number of relay to read status
  devices_present = vmc_devices_present;

  // read relay states
  vmc_relay1 = bitRead (power, 0);
  if (vmc_devices_present == 2) vmc_relay2 = bitRead (power, 1);

  // convert to vmc state
  if ((vmc_relay1 == 0) && (vmc_relay2 == 1)) relay_state = VMC_STATE_LOW;
  else if (vmc_relay1 == 1) relay_state = VMC_STATE_HIGH;
  
  // reset number of relay
  devices_present = 0;

  return relay_state;
}

// set relays state
void VmcSetRelayState (uint8_t new_state)
{
  // set number of relay to start command
  devices_present = vmc_devices_present;

  // set relays
  switch (new_state)
  {
    case VMC_STATE_OFF:  // VMC disabled
      ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
      if (devices_present == 2) ExecuteCommandPower (2, POWER_OFF, SRC_IGNORE);
      break;
    case VMC_STATE_LOW:  // VMC low speed
      ExecuteCommandPower (1, POWER_OFF, SRC_IGNORE);
      if (devices_present == 2) ExecuteCommandPower (2, POWER_ON, SRC_IGNORE);
      break;
    case VMC_STATE_HIGH:  // VMC high speed
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
  if (actual_mode >= VMC_MODE_MAX) actual_mode = VMC_MODE_DISABLED;

  return actual_mode;
}

// set vmc mode
void VmcSetMode (uint8_t new_mode)
{
  // if outvalue, set to disabled
  if (new_mode >= VMC_MODE_MAX) new_mode = VMC_MODE_DISABLED;
  Settings.weight_reference = (unsigned long) new_mode;

  // if forced mode, set relay state accordingly
  if (new_mode == VMC_MODE_LOW) VmcSetRelayState (VMC_STATE_LOW);
  else if (new_mode == VMC_MODE_HIGH) VmcSetRelayState (VMC_STATE_HIGH);
}

// get current temperature
float VmcGetTemperature ()
{
  float temperature = NAN;

#ifdef USE_DHT
  // if dht sensor present, read it 
  if (Dht[0].t != 0) temperature = Dht[0].t;
#endif

  //if temperature mesured, round at 0.1 °C
  if (!isnan (temperature)) temperature = floor (temperature * 10) / 10;

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

  // convert to integer and keep in range 0..100
  if (isnan (humidity) == false)
  {
    result = (uint8_t) humidity;
    if (result > VMC_HUMIDITY_MAX) result = VMC_HUMIDITY_MAX;
  }

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
  uint8_t humidity, vmc_value, vmc_mode;
  float   temperature;
  String  str_mqtt, str_json, str_text;

  // save MQTT data
  str_mqtt = mqtt_data;

  // get mode and humidity
  vmc_mode = VmcGetMode ();
  str_text = VmcGetStateLabel (vmc_mode);

  // vmc mode  -->  "VMC":{"Relay":2,"Mode":4,"Label":"Automatic","Humidity":70.5,"Target":50,"Temperature":18.4}
  str_json  = "\"" + String (D_JSON_VMC) + "\":{";
  str_json += "\"" + String (D_JSON_VMC_RELAY) + "\":" + String (vmc_devices_present) + ",";
  str_json += "\"" + String (D_JSON_VMC_MODE) + "\":" + String (vmc_mode) + ",";
  str_json += "\"" + String (D_JSON_VMC_LABEL) + "\":\"" + str_text + "\",";

  // temperature
  temperature = VmcGetTemperature ();
  if (!isnan(temperature)) str_text = String (temperature, 1);
  else str_text = "n/a";
  str_json += "\"" + String (D_JSON_VMC_TEMPERATURE) + "\":" + str_text + ",";

  // humidity level
  humidity = VmcGetHumidity ();
  if (humidity != UINT8_MAX) str_text = String (humidity);
  else str_text = "n/a";
  str_json += "\"" + String (D_JSON_VMC_HUMIDITY) + "\":" + str_text + ",";

  // target humidity
  vmc_value = VmcGetTargetHumidity ();
  str_json += "\"" + String (D_JSON_VMC_TARGET) + "\":" + String (vmc_value) + ",";

  // humidity thresold
  vmc_value = VmcGetThreshold ();
  str_json += "\"" + String (D_JSON_VMC_THRESHOLD) + "\":" + String (vmc_value) + "}";

  // if VMC mode is enabled
  if (vmc_mode != VMC_MODE_DISABLED)
  {
    // get relay state and label
    vmc_value = VmcGetRelayState ();
    str_text  = VmcGetStateLabel (vmc_value);

    // relay state  -->  ,"State":{"Mode":1,"Label":"On"}
    str_json += ",";
    str_json += "\"" + String (D_JSON_VMC_STATE) + "\":{";
    str_json += "\"" + String (D_JSON_VMC_MODE) + "\":" + String (vmc_value) + ",";
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
void VmcUpdateGraphData ()
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
  uint8_t humidity, threshold, vmc_mode, target, actual_state, target_state;
  float   temperature, compar_current, compar_read, compar_diff;

  // update current temperature
  temperature = VmcGetTemperature ( );
  if (!isnan(temperature))
  {
    // if temperature was previously mesured, compare
    if (!isnan(vmc_current_temperature))
    {
      // update JSN if temperature is at least 0.2°C different
      compar_current = floor (vmc_current_temperature * 20);
      compar_read    = floor (temperature * 20);
      compar_diff    = abs (compar_current - compar_read);
      if (compar_diff > 2)
      {
        vmc_current_temperature = temperature;
        need_update = true;
      }
    }

    // else, temperature is the mesured temperature
    else
    {
      vmc_current_temperature = temperature;
      need_update = true;
    }

    // update graph value
    if (!isnan(vmc_graph_temperature)) vmc_graph_temperature = min (vmc_graph_temperature, temperature);
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

  // get VMC mode and consider as low if mode disabled and single relay
  vmc_mode = VmcGetMode ();
  if ((vmc_mode == VMC_MODE_DISABLED) && (vmc_devices_present == 1)) vmc_mode = VMC_MODE_LOW;

  // determine relay target state according to vmc mode
  target_state = actual_state;
  switch (vmc_mode)
  {
    case VMC_MODE_LOW: 
      target_state = VMC_STATE_LOW;
      break;
    case VMC_MODE_HIGH:
      target_state = VMC_STATE_HIGH;
      break;
    case VMC_MODE_AUTO: 
      // read humidity threshold
      threshold = VmcGetThreshold ();

      // if humidity is low enough, target VMC state is low speed
      if (humidity + threshold < target) target_state = VMC_STATE_LOW;
      
      // else, if humidity is too high, target VMC state is high speed
      else if (humidity > target + threshold) target_state = VMC_STATE_HIGH;
      break;
  }

  // if VMC state is different than target state, set relay
  if (actual_state != target_state)
  {
    VmcSetRelayState (target_state);
    need_update = true;
  }
  
  // if JSON update needed, publish
  if (need_update == true) VmcShowJSON (false);

  // increment delay counter and if delay reached, update history data
  if (vmc_graph_counter == 0) VmcUpdateGraphData ();
  vmc_graph_counter ++;
  vmc_graph_counter = vmc_graph_counter % VMC_GRAPH_REFRESH;
}

void VmcInit ()
{
  int index;

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
  vmc_graph_state       = VMC_STATE_NONE;
  vmc_graph_index       = 0;
  vmc_graph_counter     = 0;

  // initialise graph data
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    arr_temperature[index] = NAN;
    arr_humidity[index] = UINT8_MAX;
    arr_target[index] = UINT8_MAX;
    arr_state[index] = VMC_STATE_NONE;
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
  if (icon_index < VMC_STATE_MAX)
  {
    // start of img declaration
    WSContentSend_P (PSTR ("<img "));
    if (icon_height != 0) WSContentSend_P (PSTR ("height=%d "), icon_height);
    WSContentSend_P (PSTR ("src='data:image/png;base64,"));

    // loop thru base64 parts
    nbrItem = sizeof (vmc_fan_icon[icon_index]) / sizeof (char*);
    for (index=0; index<nbrItem; index++)
      if (vmc_fan_icon[icon_index][index] != nullptr) WSContentSend_P (vmc_fan_icon[icon_index][index]);

    // end of img declaration
    WSContentSend_P (PSTR ("' >"));
  }
}

// VMC mode select combo
void VmcWebSelectMode (bool autosubmit)
{
  uint8_t vmc_mode, humidity;
  char    argument[VMC_BUFFER_SIZE];

  // get mode and humidity
  vmc_mode = VmcGetMode ();
  humidity = VmcGetHumidity ();

  // selection : beginning
  WSContentSend_P (PSTR ("<select name='%s'"), D_CMND_VMC_MODE);
  if (autosubmit == true) WSContentSend_P (PSTR (" onchange='this.form.submit()'"));
  WSContentSend_P (PSTR (">"));
  
  // selection : disabled
  if (vmc_mode == VMC_MODE_DISABLED) strcpy (argument, "selected"); else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), VMC_MODE_DISABLED, argument, D_VMC_DISABLED);

  // selection : low speed
  if (vmc_mode == VMC_MODE_LOW) strcpy (argument, "selected"); else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), VMC_MODE_LOW, argument, D_VMC_LOW);

  // selection : high speed
  if (vmc_mode == VMC_MODE_HIGH) strcpy (argument, "selected"); else strcpy (argument, "");
  WSContentSend_P (PSTR ("<option value='%d' %s>%s</option>"), VMC_MODE_HIGH, argument, D_VMC_HIGH);

  // selection : automatic
  if (humidity != UINT8_MAX) 
  {
    if (vmc_mode == VMC_MODE_AUTO) strcpy (argument, "selected"); else strcpy (argument, "");
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
    target = VmcGetTargetHumidity ();

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
  WSContentSend_P (VMC_INPUT_NUMBER, D_VMC_TARGET, D_CMND_VMC_TARGET, VMC_TARGET_MAX, value);

  // humidity threshold label and input
  value = VmcGetThreshold ();
  WSContentSend_P (VMC_INPUT_NUMBER, D_VMC_THRESHOLD, D_CMND_VMC_THRESHOLD, VMC_THRESHOLD_MAX, value);

  WSContentSend_P (PSTR("</fieldset></p>\n"));

  // save button
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>"));

  // configuration button
  WSContentSpaceButton (BUTTON_CONFIGURATION);

  // end of page
  WSContentStop ();
}

// Temperature & humidity graph frame
void VmcWebGraphFrame ()
{
  int   index;
  int   graph_left, graph_right, graph_width;
  float temperature, temp_min, temp_max, temp_scope;

  // start of SVG graph
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d' preserveAspectRatio='xMinYMinmeet'>\n"), 0, 0, VMC_GRAPH_WIDTH, VMC_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("rect {stroke:grey;fill:none;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("text.temperature {font-size:18px;fill:yellow;}\n"));
  WSContentSend_P (PSTR ("text.humidity {font-size:18px;fill:orange;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // loop to adjust min and max temperature
  temp_min = VMC_GRAPH_TEMP_MIN;
  temp_max = VMC_GRAPH_TEMP_MAX;
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if needed, adjust minimum and maximum temperature
    if (!isnan (arr_temperature[index]))
    {
      if (temp_min > arr_temperature[index]) temp_min = floor (arr_temperature[index]);
      if (temp_max < arr_temperature[index]) temp_max = ceil (arr_temperature[index]);
    } 
  }
  temp_scope = temp_max - temp_min;

  // boundaries of SVG graph
  graph_left  = VMC_GRAPH_PERCENT_START * VMC_GRAPH_WIDTH / 100;
  graph_right = VMC_GRAPH_PERCENT_STOP * VMC_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // graph curve zone
  WSContentSend_P (PSTR ("<rect x='%d%%' y='%d%%' width='%d%%' height='%d%%' rx='10' />\n"), VMC_GRAPH_PERCENT_START, 0, VMC_GRAPH_PERCENT_STOP - VMC_GRAPH_PERCENT_START, 100);

  // graph separation lines
  WSContentSend_P (VMC_LINE_DASH, graph_left, 25, graph_right, 25);
  WSContentSend_P (VMC_LINE_DASH, graph_left, 50, graph_right, 50);
  WSContentSend_P (VMC_LINE_DASH, graph_left, 75, graph_right, 75);

  // temperature units
  WSContentSend_P (VMC_TEXT_TEMPERATURE, 2, 4,  String (temp_max,                     1).c_str ());
  WSContentSend_P (VMC_TEXT_TEMPERATURE, 2, 27, String (temp_min + temp_scope * 0.75, 1).c_str ());
  WSContentSend_P (VMC_TEXT_TEMPERATURE, 2, 52, String (temp_min + temp_scope * 0.50, 1).c_str ());
  WSContentSend_P (VMC_TEXT_TEMPERATURE, 2, 77, String (temp_min + temp_scope * 0.25, 1).c_str ());
  WSContentSend_P (VMC_TEXT_TEMPERATURE, 2, 99, String (temp_min,                     1).c_str ());

  // humidity units
  WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, 4, 100);
  WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, 27, 75);
  WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, 52, 50);
  WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, 77, 25);
  WSContentSend_P (VMC_TEXT_HUMIDITY, VMC_GRAPH_PERCENT_STOP + 2, 99, 0);

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd();
}

// Temperature & humidity graph data
void VmcWebGraphData ()
{
  bool     draw;
  int      index, array_idx;
  int      graph_left, graph_right, graph_width;
  int      graph_x, graph_y, graph_off, graph_low, graph_high;
  int      unit_width, shift_unit, shift_width;
  uint8_t  humidity;
  float    temperature, temp_min, temp_max, temp_scope;
  TIME_T   current_dst;
  uint32_t current_time;
  String   str_text;

  // vmc state graph position
  graph_off  = VMC_GRAPH_HEIGHT;
  graph_low  = VMC_GRAPH_HEIGHT - VMC_GRAPH_MODE_LOW;
  graph_high = VMC_GRAPH_HEIGHT - VMC_GRAPH_MODE_HIGH;

  // loop to adjust min and max temperature
  temp_min = VMC_GRAPH_TEMP_MIN;
  temp_max = VMC_GRAPH_TEMP_MAX;
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if needed, adjust minimum and maximum temperature
    if (!isnan (arr_temperature[index]))
    {
      if (temp_min > arr_temperature[index]) temp_min = floor (arr_temperature[index]);
      if (temp_max < arr_temperature[index]) temp_max = ceil (arr_temperature[index]);
    } 
  }
  temp_scope = temp_max - temp_min;

  // boundaries of SVG graph
  graph_left  = VMC_GRAPH_PERCENT_START * VMC_GRAPH_WIDTH / 100;
  graph_right = VMC_GRAPH_PERCENT_STOP * VMC_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("<svg viewBox='%d %d %d %d' preserveAspectRatio='xMinYMinmeet'>\n"), 0, 0, VMC_GRAPH_WIDTH, VMC_GRAPH_HEIGHT);

  // SVG style 
  WSContentSend_P (PSTR ("<style type='text/css'>\n"));
  WSContentSend_P (PSTR ("polyline {fill:none;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("polyline.vmc {stroke:white;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.target {stroke:orange;stroke-dasharray:4;}\n"));
  WSContentSend_P (PSTR ("polyline.temperature {stroke:yellow;}\n"));
  WSContentSend_P (PSTR ("polyline.humidity {stroke:orange;}\n"));
  WSContentSend_P (PSTR ("line.time {stroke:white;stroke-width:1;}\n"));
  WSContentSend_P (PSTR ("text.time {font-size:16px;fill:white;}\n"));
  WSContentSend_P (PSTR ("rect {fill:#333;stroke-width:2;opacity:0.5;}\n"));
  WSContentSend_P (PSTR ("rect.temperature {stroke:yellow;}\n"));
  WSContentSend_P (PSTR ("rect.humidity {stroke:orange;}\n"));
  WSContentSend_P (PSTR ("text.temperature {font-size:28px;fill:yellow;stroke:yellow;}\n"));
  WSContentSend_P (PSTR ("text.humidity {font-size:28px;fill:orange;stroke:orange;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  // ---------------
  //     Curves
  // ---------------

  // loop to display vmc state curve
  WSContentSend_P (PSTR ("<polyline class='vmc' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // get array index and read vmc state
    array_idx  = (index + vmc_graph_index) % VMC_GRAPH_SAMPLE;
    if (arr_state[array_idx] != VMC_STATE_NONE)
    {
      // calculate current position and add the point to the line
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      if (arr_state[array_idx] == VMC_STATE_HIGH) graph_y = graph_high;
      else if (arr_state[array_idx] == VMC_STATE_LOW) graph_y = graph_low;
      else graph_y = graph_off;
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // loop to display target humidity curve
  WSContentSend_P (PSTR ("<polyline class='target' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if humidity has been mesured
    array_idx = (index + vmc_graph_index) % VMC_GRAPH_SAMPLE;
    if (arr_target[array_idx] != UINT8_MAX)
    {
      // calculate current position and add the point to the line
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - (arr_target[array_idx] * VMC_GRAPH_HEIGHT / 100);
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // loop to display temperature curve
  WSContentSend_P (PSTR ("<polyline class='temperature' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if temperature has been mesured
    array_idx   = (index + vmc_graph_index) % VMC_GRAPH_SAMPLE;
    if (!isnan (arr_temperature[array_idx]))
    {
      // calculate current position and add the point to the line
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - int (((arr_temperature[array_idx] - temp_min) / temp_scope) * VMC_GRAPH_HEIGHT);
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // loop to display humidity curve
  WSContentSend_P (PSTR ("<polyline class='humidity' points='"));
  for (index = 0; index < VMC_GRAPH_SAMPLE; index++)
  {
    // if temperature value is defined
    array_idx = (index + vmc_graph_index) % VMC_GRAPH_SAMPLE;
    if (arr_humidity[array_idx] != UINT8_MAX)
    {
      // calculate current position and add the point to the line
      graph_x = graph_left + (graph_width * index / VMC_GRAPH_SAMPLE);
      graph_y = VMC_GRAPH_HEIGHT - (arr_humidity[array_idx] * VMC_GRAPH_HEIGHT / 100);
      WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
    }
  }
  WSContentSend_P (PSTR("'/>\n"));

  // ---------------
  //   Time line
  // ---------------

  // get current time
  current_time = LocalTime();
  BreakTime (current_time, current_dst);

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

  // ---------------
  //     Values
  // ---------------

  // current temperature
  temperature = VmcGetTemperature ();
  if (!isnan (temperature))
  {
    graph_x = 22;
    graph_y = 2;
    str_text = String (temperature, 1) + " °C";
    unit_width = 2 + 2 * str_text.length ();
    WSContentSend_P ("<rect class='temperature' x='%d%%' y='%d%%' width='%d%%' height='%d%%' rx='%d' ry='%d' />\n", graph_x, graph_y, unit_width, 10, 10, 10);
    WSContentSend_P ("<text class='temperature' x='%d%%' y='%d%%'>%s</text>\n", graph_x + 2, graph_y + 7,str_text.c_str ());
  }

  // current humidity
  humidity = VmcGetHumidity ();
  if (humidity != UINT8_MAX) 
  {
    graph_x = 66;
    graph_y = 2;
    str_text = String (humidity) + " %";
    unit_width = 5 + 2 * str_text.length ();
    WSContentSend_P ("<rect class='humidity' x='%d%%' y='%d%%' width='%d%%' height='%d%%' rx='%d' ry='%d' />\n", graph_x, graph_y, unit_width, 10, 10, 10);
    WSContentSend_P ("<text class='humidity' x='%d%%' y='%d%%'>%d %%</text>\n", graph_x + 2, graph_y + 7, humidity);
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
  WSContentEnd();
}

// VMC control public web page
void VmcWebPageControl ()
{
  uint8_t vmc_mode, relay_state;
  String  str_text;

  // check if vmc state has changed
  if (Webserver->hasArg(D_CMND_VMC_LOW)) VmcSetMode (VMC_MODE_LOW);
  else if (Webserver->hasArg(D_CMND_VMC_HIGH)) VmcSetMode (VMC_MODE_HIGH);
  else if (Webserver->hasArg(D_CMND_VMC_AUTO)) VmcSetMode (VMC_MODE_AUTO);

  // beginning of form without authentification with 60 seconds auto refresh
  WSContentStart_P (D_VMC_CONTROL, false);
  WSContentSend_P (PSTR ("</script>\n"));

  // page data refresh script
  WSContentSend_P (PSTR ("<script type='text/javascript'>\n"));
  WSContentSend_P (PSTR ("function updateData() {\n"));
  WSContentSend_P (PSTR ("dataId='data';\n"));
  WSContentSend_P (PSTR ("now=new Date();\n"));
  WSContentSend_P (PSTR ("svgObject=document.getElementById(dataId);\n"));
  WSContentSend_P (PSTR ("svgObjectURL=svgObject.data;\n"));
  WSContentSend_P (PSTR ("svgObject.data=svgObjectURL.substring(0,svgObjectURL.indexOf('ts=')) + 'ts=' + now.getTime();\n"));
  WSContentSend_P (PSTR ("}\n"));
  WSContentSend_P (PSTR ("setInterval(function() {updateData();},%d);\n"), 10000);
  WSContentSend_P (PSTR ("</script>\n"));

  // page style
  WSContentSend_P (PSTR ("<style>\n"));
  WSContentSend_P (PSTR ("body {color:white;background-color:#303030;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%;margin:6px auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR ("img {height:12vh;}\n"));
  WSContentSend_P (PSTR ("span {display:inline-block;font-size:5vw;width:20vw;vertical-align:middle;}\n"));
  WSContentSend_P (PSTR (".title {font-size:6vw;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".button {font-size:3vw;padding:0.5rem 1rem;border:1px #666 solid;background:none;color:#fff;border-radius:8px;}\n"));
  WSContentSend_P (PSTR (".active {background:#666;}\n"));
  WSContentSend_P (PSTR (".svg-container {position:relative;vertical-align:middle;overflow:hidden;width:100%%;max-width:%dpx;padding-bottom:65%%;}\n"), VMC_GRAPH_WIDTH);
  WSContentSend_P (PSTR (".svg-content {display:inline-block;position:absolute;top:0;left:0;}\n"));
  WSContentSend_P (PSTR ("</style>\n"));

  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<form name='%s' method='get' action='%s'>\n"), D_PAGE_VMC_CONTROL, D_PAGE_VMC_CONTROL);

  // room name
  WSContentSend_P (PSTR ("<div class='title'>%s</div>\n"), SettingsText(SET_DEVICENAME));

  // display icon
  relay_state = VmcGetRelayState ();
  WSContentSend_P (PSTR ("<div>"));
  VmcWebDisplayIcon (64, relay_state);
  WSContentSend_P (PSTR ("</div>\n"));

  // display vmc mode selector
  vmc_mode = VmcGetMode ();
  WSContentSend_P (PSTR ("<div>\n"));
  if (vmc_mode == VMC_MODE_AUTO) str_text="active"; else str_text="";
  WSContentSend_P (VMC_INPUT_BUTTON, D_CMND_VMC_AUTO, str_text.c_str (), D_VMC_BTN_AUTO);
  if (vmc_mode == VMC_MODE_LOW) str_text="active"; else str_text="";
  WSContentSend_P (VMC_INPUT_BUTTON, D_CMND_VMC_LOW, str_text.c_str (), D_VMC_BTN_LOW);
  if (vmc_mode == VMC_MODE_HIGH) str_text="active"; else str_text="";
  WSContentSend_P (VMC_INPUT_BUTTON, D_CMND_VMC_HIGH, str_text.c_str (), D_VMC_BTN_HIGH);
  WSContentSend_P (PSTR ("</div>\n"));

  // display graph base and data
  WSContentSend_P (PSTR ("<div class='svg-container'>\n"));
  WSContentSend_P (PSTR ("<object class='svg-content' id='base' type='image/svg+xml' width='%d%%' height='%d%%' data='%s'></object>\n"), 100, 100, D_PAGE_VMC_BASE_SVG);
  WSContentSend_P (PSTR ("<object class='svg-content' id='data' type='image/svg+xml' width='%d%%' height='%d%%' data='%s?ts=0'></object>\n"), 100, 100, D_PAGE_VMC_DATA_SVG);
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
      Webserver->on ("/" D_PAGE_VMC_BASE_SVG, VmcWebGraphFrame);
      Webserver->on ("/" D_PAGE_VMC_DATA_SVG, VmcWebGraphData);
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
