/*
  xdrv_98_motion.ino - Motion detector management with tempo and timers (~16.5 kb)
  
  Copyright (C) 2020  Nicolas Bernaerts
    27/03/2020 - v1.0   - Creation
    10/04/2020 - v1.1   - Add detector configuration for low/high level
    15/04/2020 - v1.2   - Add detection auto rearm flag management
    18/04/2020 - v1.3   - Handle Toggle button and display motion icon
    15/05/2020 - v1.4   - Add /json page to get latest motion JSON
    20/05/2020 - v1.5   - Add configuration for first NTP server
    26/05/2020 - v1.6   - Add Information JSON page
    07/07/2020 - v1.6.1 - Enable discovery (mDNS)
    21/09/2020 - v1.7   - Add switch and icons on control page
                          Based on Tasmota 8.4
                   
  Input devices should be configured as followed :
   - Switch2 = Motion detector

  Settings are stored using weighting scale parameters :
   - Settings.energy_voltage_calibration = Motion detection level (0 = low, 1 = high)
   - Settings.energy_current_calibration = Motion detection auto rearm flag
   - Settings.energy_power_calibration   = Motion detection tempo (s)

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
 *                Motion detector
\*************************************************/

#ifdef USE_MOTION

#define XDRV_98                      98
#define XSNS_98                      98

#define MOTION_BUTTON                1           // switch2

#define MOTION_BUFFER_SIZE           128

#define MOTION_JSON_UPDATE           5000        // update JSON every 5 sec
#define MOTION_WEB_UPDATE            5           // update Graph Web page every 5 sec

#define MOTION_GRAPH_STEP            2           // collect motion status every 2mn
#define MOTION_GRAPH_SAMPLE          720         // 24 hours display with collect every 2 mn
#define MOTION_GRAPH_WIDTH           800      
#define MOTION_GRAPH_HEIGHT          400 
#define MOTION_GRAPH_PERCENT_START   10      
#define MOTION_GRAPH_PERCENT_STOP    90      

#define D_PAGE_MOTION_CONFIG         "config"
#define D_PAGE_MOTION_TOGGLE         "toggle"
#define D_PAGE_MOTION_CONTROL        "control"
#define D_PAGE_MOTION_JSON           "json"

#define D_CMND_MOTION_ENABLE         "enable"
#define D_CMND_MOTION_FORCE          "force"
#define D_CMND_MOTION_TOGGLE         "toggle"
#define D_CMND_MOTION_LEVEL          "level"
#define D_CMND_MOTION_REARM          "rearm"
#define D_CMND_MOTION_TEMPO          "tempo"

#define D_CMND_MOTION_MN             "mn"
#define D_CMND_MOTION_SEC            "sec"

#define D_JSON_MOTION                "Motion"
#define D_JSON_MOTION_STATUS         "Status"
#define D_JSON_MOTION_ENABLED        "Enabled"
#define D_JSON_MOTION_LEVEL          "Level"
#define D_JSON_MOTION_HIGH           "High"
#define D_JSON_MOTION_LOW            "Low"
#define D_JSON_MOTION_ON             "ON"
#define D_JSON_MOTION_OFF            "OFF"
#define D_JSON_MOTION_REARM          "Rearm"
#define D_JSON_MOTION_FORCED         "Forced"
#define D_JSON_MOTION_TIMELEFT       "Timeleft"
#define D_JSON_MOTION_DETECTED       "Detected"
#define D_JSON_MOTION_LIGHT          "Light"
#define D_JSON_MOTION_TEMPO          "Tempo"

#define D_MOTION                     "Motion Detector"
#define D_MOTION_CONFIG              "Configure"
#define D_MOTION_DETECTION           "Detection"
#define D_MOTION_CONTROL             "Control"
#define D_MOTION_TEMPO               "Temporisation"
#define D_MOTION_MOTION              "Motion"
#define D_MOTION_DETECTOR            "Detector"
#define D_MOTION_REARM               "Rearm"
#define D_MOTION_COMMAND             "Light"
#define D_MOTION_ENABLE              "Enable"
#define D_MOTION_ENABLED             "Enabled"
#define D_MOTION_DISABLE             "Disable"
#define D_MOTION_DISABLED            "Disabled"
#define D_MOTION_ON                  "On"
#define D_MOTION_OFF                 "Off"
#define D_MOTION_LEVEL               "Level"
#define D_MOTION_HIGH                "High"
#define D_MOTION_LOW                 "Low"
#define D_MOTION_FORCED              "Forced"

// offloading commands
enum MotionCommands { CMND_MOTION_ENABLE, CMND_MOTION_FORCE, CMND_MOTION_TOGGLE, CMND_MOTION_LEVEL, CMND_MOTION_REARM, CMND_MOTION_TEMPO };
const char kMotionCommands[] PROGMEM = D_CMND_MOTION_ENABLE "|" D_CMND_MOTION_FORCE "|" D_CMND_MOTION_TOGGLE "|" D_CMND_MOTION_LEVEL "|" D_CMND_MOTION_REARM "|" D_CMND_MOTION_TEMPO ;

// form topic style
const char MOTION_TOPIC_STYLE[] PROGMEM = "style='float:right;font-size:0.7rem;'";

// graph data structure
struct graph_value {
    uint8_t is_enabled : 1;
    uint8_t is_detected : 1;
    uint8_t is_active : 1;
}; 

// light off icon
const char light_off_0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAIAAAACAAQMAAAD58POIAAAC4HpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHja7ZdbktwgDEX/WUWWgCSEYDmYR1V2kOXnghn3YzqpyiQf+WhoG6PGV0IHmGnXf3wf7hsKZWEX1FLMMXqUkEPmgofkz3K25MO635lW/8Huri8YJkErZzf2Pb7ArrcXLGz78Wh3VrdO2kJ0Ca8i0/N8bjuiLSR82mn3Xd4vlHg3nX1x3bJb/LkfDMloCj3kiLuQ+HXn05OcV8GVcWdhDPTLoqhFRF7kz12pe5HA6+kpf/4jMrml4xT6mFZ8ytO2kz7Z5XLDDxERX575PiKTy8Wn/I3R0hj9nF0J0SFdcU/qYyrrCQMPpFPWaxHVcCmebdWMmnzxFdQapno4f6CTiZHxQYEaFRrUV1upIsTAnQ0tc2VZtiTGmSvSTxJmpcHmQKZJApsKcgIzX7HQ8punPzhL8NwII5kgRpPmfXXPhq/WB6Ex5jIn8unKFeLiub4QxiQ37xgFIDR2TnXll9zZ+OcywQoI6kpzwgSLP06JQ+m2tmRxFq8OQ4M/9wtZ2wJIEXwrgiEBAR9JlCJ5YzYi5DGBT0HkLIEPECB1yg1RchCJgJN4+sY7RmssK59mHC8AoRLFgAZbB7BC0BCx3xKWUHHYPkFVo5omzVqixBA1xmhxnlPFxIKpRTNLlq0kSSFpislSSjmVzFlwjKnLMVtO";
const char light_off_1[] PROGMEM = "OedS4LSEAq2C8QWGgw85wqFHPOxIRz5KxfKpoWqN1WqquZbGTRqOANdis5ZabqVTx1LqoWuP3XrquZeBtTZkhKEjDhtp5FEuapvqI7Vncr+nRpsaL1BznN2owWz2IUHzONHJDMQ4EIjbJIAFzZOZTxQCT3KTmc8sTkQZUeqE02gSA8HQiXXQxe5G7pfcHLL7p9z4FTk30f0Lcm6iuyP3mdsLaq2s41YWoLkLkVOckILtN0ItnPDBn5Ovtc7/pcBb6C30FnoLvYXeQm+h/0pIBv55yPg59RMYVJG1wNq/pAAAAYRpQ0NQSUNDIHByb2ZpbGUAAHicfZE9SMNAHMVfU6UqFQeriDhkqE4WpIo4ahWKUCHUCq06mFz6BU0akhQXR8G14ODHYtXBxVlXB1dBEPwAcXJ0UnSREv/XFFrEeHDcj3f3HnfvAKFWYprVMQFoum0m4zExnVkVA6/oxgAGEUVEZpYxJ0kJeI6ve/j4ehfhWd7n/hy9atZigE8knmWGaRNvEE9v2gbnfeIQK8gq8TnxuEkXJH7kuuLyG+d8gwWeGTJTyXniELGYb2OljVnB1IiniMOqplO+kHZZ5bzFWStVWPOe/IXBrL6yzHWaI4hjEUuQIEJBBUWUYCNCq06KhSTtxzz8ww2/RC6FXEUwciygDA1yww/+B7+7tXKTUTcpGAM6XxznYxQI7AL1quN8HztO/QTwPwNXestfrgEzn6RXW1r4COjbBi6uW5qy";
const char light_off_2[] PROGMEM = "B1zuAENPhmzKDclPU8jlgPcz+qYM0H8L9Ky5vTX3cfoApKirxA1wcAiM5Sl73ePdXe29/Xum2d8P2Why0HatJjcAAAAGUExURQAAAMzMzMhPwDIAAAABdFJOUwBA5thmAAAAAWJLR0QAiAUdSAAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+QJFRY3Hll1dH0AAAEXSURBVEjH7ZRBEoMgDEVhXLjkCNykuVnxaB7FI7hk4ZgKjuWHOMV20emiWckTkg/8YMw/fjl4leOeeRSAmGe5gnnBsd0AyxSz5wGAW7dJEwAfjQmYlaYdPiNsInoskyp0oNXmbwBdnh0A5HxUhPRzDbImX/brMnAAxoJ35UNZCKArmyEFzCmwZbu3GtzPgWmD5SugqePV5ugigBNTh6yvYayAujlXX/ZuBwDKMMpSdjmseMT6tCL6FI0btnw9WpvGozZod9geaeCxC9N0AdJRELZY0k6iK6NQvmu/i0Y+A6Kz0+82iBWwEtyagH4F1C+dPwGTAK4G6vm08m38KFRS1wRqyaUqw5tVFOjqHBcixBosTRAvZ38AMfuyChOeJEwAAAAASUVORK5CYII=";

// light off icon
const char light_on_0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAIAAAACAAQMAAAD58POIAAAC5npUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHja7ZdLkt0gDEXnrCJLQBJCsBw+pio7yPJzwTy/T3dS6VQGGTy7bYwaX4l7MEm748f34b7hoCzBBbUUc4weR8ghc8FD8udxtuTDuj+EVv8p7q5fMEKCVs5uPPb4grjeX7Cw4/U57qxtnbSF6BJeh8zM87nviraQ8Bmn3Xd5v1Diw3T2xW3LbvHXfjCY0RV6wo4PIfHrzmcmOa+CK+POwhjoV0TRKxIkfPTPXdZ9YuD19OKfv1UmdztOodu04otPO076EpcrDT9VRHxl5seKTK8UH/wbo6cxjnN2JUQHu+Ke1G0q6wkDK+yU9VrEabgUz7bOjDP54huodUy1Ol/RycRwfFCgToUGHatt1FBi4IMNLXNjWbEkxpkb7KdpvAQabA5kuiTQaCAnCPNVC628eeZDsoTMnTCSCWI0aT6e7jXwt+eT0BhzmRP5dHmFuniuL5Qxyc07RgEIje2pLn/JnY1/PSZYAUFdNidMsPh6SlSl+9qSxVm8OgwN/vxeyPoWgEXIrSiGBAR8JFGK5I3ZiOBjAp+CylkCVxAgdcodVXIQiYCTeObGO0ZrLCufYWwvAKESxYAGnw5ghaAh4ntLWELFqWhQ1aimSbOWKDFEjTFanPtUMbFgatHMkmUrSVJImmKylFJOJXMWbGPqcsyW";
const char light_on_1[] PROGMEM = "U865FCQtoUCrYHxBoHKVGqrWWK2mmmtpWD4tNG2xWUstt9K5S8cW4Hrs1lPPvRx0YCkd4dAjHnakIx9lYK0NGWHoiMNGGnmUi9qm+kztldzvqdGmxgvUHGd3agib3SRobic6mYEYBwJxmwSwoHky84lC4EluMvOZxYkoo0qdcDpNYiAYDmIddLG7k/slNwd3v8qNPyPnJrp/Qc5NdA/kPnL7hFova7uVBWh+hfAUO6Tg8xsxFcZPHXCoNl69+W/UH7fuqy+8hd5Cb6G30FvoLfQW+v+FBv4DkfEn1U9ib5Md89WKuQAAAYRpQ0NQSUNDIHByb2ZpbGUAAHicfZE9SMNAHMVfU6UqFQeriDhkqE4WpIo4ahWKUCHUCq06mFz6BU0akhQXR8G14ODHYtXBxVlXB1dBEPwAcXJ0UnSREv/XFFrEeHDcj3f3HnfvAKFWYprVMQFoum0m4zExnVkVA6/oxgAGEUVEZpYxJ0kJeI6ve/j4ehfhWd7n/hy9atZigE8knmWGaRNvEE9v2gbnfeIQK8gq8TnxuEkXJH7kuuLyG+d8gwWeGTJTyXniELGYb2OljVnB1IiniMOqplO+kHZZ5bzFWStVWPOe/IXBrL6yzHWaI4hjEUuQIEJBBUWUYCNCq06KhSTtxzz8ww2/RC6FXEUwciygDA1yww/+B7+7tXKTUTcpGAM6XxznYxQI7AL1quN8HztO/QTwPwNXestfrgEzn6RXW1r4COjb";
const char light_on_2[] PROGMEM = "Bi6uW5qyB1zuAENPhmzKDclPU8jlgPcz+qYM0H8L9Ky5vTX3cfoApKirxA1wcAiM5Sl73ePdXe29/Xum2d8P2Why0HatJjcAAAAGUExURQAAAP//MzcLEUYAAAABdFJOUwBA5thmAAAAAWJLR0QAiAUdSAAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+QJFRY0LLqPdj4AAAGKSURBVEjH3ZRLcsMgDIbl8YJNJtwg3CRcpSexfTTnJu4NWLJgTJENQoBTu111qg0zn6PXL0UAb6yfKiD+K7gl8BHBI7bfmQhU0oRA9BdLBHKObwLpi5orX51Sd3Z/RyrE7c9KYADwK/FgTxDez8lzK1V7b6hQgDt47x0sub0uAF80743yXAC5hh8xD1AhwWgY0MsOycbQlnAMYIZ+ZVnXshXoXdks9LaQI8QzNVgKBZO6koG5ED0NRtSgz83oBsAhYCI/azAcA7AkcAJ6E1p4G4HTfj4GJoOpCOrqLOfAnoHvmtMXAVOsEbkdw1yBZnKyHva+Dgw0C9OsVOfSKiZbaRX5nvLFHUM8wVb7rnG1TU77wBLkZ27uiaWqF9MUS1WvHNWhFHoi0FmsXadpoFQo5xiV3JTB2gc6LKjPBmjZwxeHoyYN1TZMl2V/REDX4LaBkIj9twc+yH3+Z0D/FRAuXQHUAVgKIGuA57MAXXkbf2VNUHkKGpdLWaYfZmlAX8e4YKOtgTsF9nL0LxjZ7CIu";
const char light_on_3[] PROGMEM = "/K0ZAAAAAElFTkSuQmCC";

// motion icon
const char motion_0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAIAAAACAAQMAAAD58POIAAAC5npUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHja7ZdbstwqDEX/GUWGgCSEYDg8qzKDDD8b7HY/TudWnVv5yEebsgEZS2IvoLrd+PVzuh+4KEtwQS3FHKPHFXLIXNBI/riOmnzYzwfT7j/Z3fWCYRLUcnTjOMcX2PX+gYXTXp/tztrpJ52Ozhc3h7IiMxr9zOh0JHzY6ey7zEejxIfpnPdsnJdJ6/HqtR8MYnSFP2HHQ0j8fvIRSY674M54skQMJIloq9i2h6/6uUu6NwJerRf9fDvtcpfjcHSbVnzR6bSTvtdvq/SYEfEVmR8zMr1CfNVv9jTnOGZXQnSQK56Tuk1ltzAQkgbZn0UUw61o2y4ZJfniG6h1TLU6X9HJxFB8UqBOhSaNXTdqSDHwYEPN3Fi2LYlx5iYLQViFJpsDmS4JbBrICcx85UI7bl7xECwhcieMZIIzMH4u7tXwf8uToznXMify6dIKefFaX0hjkVtPjAIQmqemuvUld1T+9VpgBQR1y5wwweLr4aIq3deWbM7i1WFo8Md+IeunA0iE2IpkSEDARxKlSN6YjQg6JvApyJwlcAUBUqfckSUHwU4wTrxi4xujPZaVDzOOF4BQbBQDGmwdwApBQ8R+S1hCxaloUNWopkmzligxRI0xWlznVDGxYGrRzJJlK0lSSJpispRSTiVzFhxj6nL";
const char motion_1[] PROGMEM = "MllPOuRQELaHAV8H4AkPlKjVUrbFaTTXX0rB8WmjaYrOWWm6lc5eOI8D12K2nnnsZNLCURhg64rCRRh5lYq1NmWHqjNNmmnmWi9pJ9ZnaK7n/pkYnNd6g1ji7U4PZ7OaC1nGiixmIcSAQt0UAC5oXM58oBF7kFjOfWZyIMrLUBafTIgaCYRDrpIvdndwfuTmo+11u/I6cW+j+Bjm30D2Q+8rtDbVe9nErG9DahdAUJ6Rg+42M+JzKbKXNihOLV9d/o3bf/eDj6OPo4+jj6OPo4+jj6N93JPgBgf+Q7jcd8pR3c5XVzAAAAYRpQ0NQSUNDIHByb2ZpbGUAAHicfZE9SMNAHMVfW6UqVQc7SHHIUJ0siBZx1CoUoUKoFVp1MLn0C5o0JCkujoJrwcGPxaqDi7OuDq6CIPgB4uTopOgiJf4vKbSI8eC4H+/uPe7eAf5Ghalm1wSgapaRTiaEbG5VCL6iFxEMIA6fxEx9ThRT8Bxf9/Dx9S7Gs7zP/Tn6lbzJAJ9APMt0wyLeIJ7etHTO+8RhVpIU4nPicYMuSPzIddnlN85Fh/08M2xk0vPEYWKh2MFyB7OSoRLHiaOKqlG+P+uywnmLs1qpsdY9+QtDeW1lmes0R5DEIpYgQoCMGsqowEKMVo0UE2naT3j4I45fJJdMrjIYORZQhQrJ8YP/we9uzcLUpJsUSgDdL7b9MQoEd4Fm3ba/j227eQIEnoErre2vNoCZT9LrbS16BA";
const char motion_2[] PROGMEM = "xuAxfXbU3eAy53gOEnXTIkRwrQ9BcKwPsZfVMOGLoF+tbc3lr7OH0AMtRV6gY4OATGipS97vHuns7e/j3T6u8HaXFyo/QMiGIAAAAGUExURQAAAP+ZALt8vHEAAAABdFJOUwBA5thmAAAAAWJLR0QAiAUdSAAAAAlwSFlzAAAuIwAALiMBeKU/dgAAAAd0SU1FB+QJGg45BHDCQhcAAAHvSURBVEjH7dVBit0wDAZgGw9kU/AFCp4jdNlFaS5WxoFZzLJXMvQihl7AQzcuDVH1S07iJK99FLqcQHjke8mTLMl5xrwd/+uYT9eWTuBoOsJA6Qj+LgTK/wr+CulOHsM5U3dei1n+XIjH/uKBzw/6G0U+3vH5RfPQin3mM2rqy/bTGi/qB23xqPkNsLMmqqmPk3GzLkUXNybjqk1YPcBNAYCcRkKBfFKoiAoIgKEMFUEEss+Awp0l4ge9QGbgqFQBA8AXRBUoDHKbV5DHFX4SzSiKQjL+lWhpEBJD+K6AnCSZ8I0WOsHMy11hEojZWAWuQXhm4ITnFcZnquPcwwvVQCfwNO0QGRylFbg+X6larsgOnstxgfEM/ghcqmS2KE+YW9eFZcDA7PApoIId8KjPOwQBTIXdCtRgrxiafYGpgbQBkLa+4A4dh6GunTPxFzKTVkqzTfyxNbvBKzLTccBtJhYFnSCMVInLOlJVII8kQ5fbWFIOnFkbS8tNpcxtMYMM7mwx+1k27sSjLcPPd";
const char motion_3[] PROGMEM = "zgZdq6NlUJw1wTipPsFXyu0DYSNvgH1gIsneVkgULv42MP7ttWxawUe2pbn+TKxf+cIlA5ivQFjD6hwD/YK+Qiy9LH+BeStdoFwAHMHHKrme0DlfZ8YYCjnP4Pp7Q/yxvEbGrauux+HTt4AAAAASUVORK5CYII=";

// icons associated to motion detection
enum MotionIcons { MOTION_LIGHT_OFF, MOTION_LIGHT_ON, MOTION_DETECTED };
const char* motion_icon[][4] PROGMEM = { 
  {light_off_0, light_off_1, light_off_2, nullptr   },    // LIGHT_OFF
  {light_on_0,  light_on_1,  light_on_2,  light_on_3},    // LIGHT_ON
  {motion_0,    motion_1,    motion_2,    motion_3  }     // MOTION_ON
};

/*************************************************\
 *               Variables
\*************************************************/

// detector states
enum MotionStates { MOTION_OFF, MOTION_ON };
enum MotionForce  { MOTION_FORCE_NONE, MOTION_FORCE_OFF, MOTION_FORCE_ON };

// variables
bool    motion_enabled     = true;                 // is motion detection enabled
bool    motion_last_state  = false;                // last motion detection status
bool    motion_update_json = false;                // data has been updated, needs JSON update
uint8_t motion_forced      = MOTION_FORCE_NONE;    // relay state forced
unsigned long motion_time_start  = ULONG_MAX;      // when tempo started
unsigned long motion_time_update = ULONG_MAX;      // when JSON was last updated

// graph data
int  motion_graph_index;
int  motion_graph_counter;
int  motion_graph_refresh;
bool motion_graph_enabled;
bool motion_graph_detected;
bool motion_graph_active;
graph_value arr_graph_data[MOTION_GRAPH_SAMPLE];

/**************************************************\
 *                  Accessors
\**************************************************/

// get motion detection level (0 or 1)
uint8_t MotionGetDetectionLevel ()
{
  return (uint8_t)Settings.energy_voltage_calibration;
}

// set motion detection level (0 or 1)
void MotionSetDetectionLevel (uint8_t level)
{
  Settings.energy_voltage_calibration= (unsigned long)level;
  motion_update_json = true;
}

// get detection temporisation (in s)
unsigned long MotionGetTempo ()
{
  return Settings.energy_power_calibration;
}
 
// set detection temporisation (in s)
void MotionSetTempo (unsigned long new_tempo)
{
  Settings.energy_power_calibration = new_tempo;
  motion_update_json = true;
}

// get if tempo should be rearmed on detection
bool MotionGetAutoRearm ()
{
  return (Settings.energy_current_calibration == 1);
}
 
// set if tempo should be rearmed on detection
void MotionSetAutoRearm (const char* str_string)
{
  String str_command = str_string;

  // check if state in ON
  if (str_command.equals ("1") || str_command.equalsIgnoreCase (D_JSON_MOTION_ON)) Settings.energy_current_calibration = 1;
  else if (str_command.equals ("0") || str_command.equalsIgnoreCase (D_JSON_MOTION_OFF)) Settings.energy_current_calibration = 0;
  motion_update_json = true;
}

/***************************************\
 *               Functions
\***************************************/

// check if motion is detected
bool MotionIsDetected ()
{
  bool motion_detected;

  // check if motion is detected according to detection level
  motion_detected = (SwitchGetVirtual (MOTION_BUTTON) == MotionGetDetectionLevel ());

  // if change, ask for JSON update
  if (motion_last_state != motion_detected) motion_update_json = true;
  motion_last_state = motion_detected;

  return motion_detected;
}

// get relay state
bool MotionIsRelayActive ()
{
  uint8_t relay_condition;

  // read relay state
  relay_condition = bitRead (power, 0);

  return (relay_condition == 1);
}

// set relay state
void MotionSetRelay (bool new_state)
{
  // set relay state
  if (new_state == false) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
  else ExecuteCommandPower (1, POWER_ON, SRC_MAX);
  motion_update_json = true;
}

// enable or disable motion detector (POWER_OFF = disable, POWER_ON = enable)
void MotionEnable (uint32_t state)
{
  if (state == POWER_ON) motion_enabled = true;
  else if (state == POWER_OFF) motion_enabled = false;
  motion_update_json = true;
}

// enable or disable motion detector
void MotionEnable (const char* str_string)
{
  String str_command = str_string;

  // check if state in ON
  if (str_command.equals ("1") || str_command.equalsIgnoreCase (D_JSON_MOTION_ON)) motion_enabled = true;
  else if (str_command.equals ("0") || str_command.equalsIgnoreCase (D_JSON_MOTION_OFF)) motion_enabled = false;
  motion_update_json = true;
}

// switch forced state (ON or OFF)
void MotionToggleSwitch ()
{
  // toggle motion forced state
  if (MotionIsRelayActive ())
  {
    motion_forced = MOTION_FORCE_OFF;
    MotionSetRelay (false);
  }
  else 
  {
    motion_forced = MOTION_FORCE_ON;
    MotionSetRelay (true);
  }
  motion_update_json = true;
}

// force state ON or OFF
void MotionForce (const char* str_string)
{
  String str_command = str_string;

  // check if state is forced ON
  if (str_command.equals ("1") || str_command.equalsIgnoreCase (D_JSON_MOTION_ON))
  {
    motion_forced      = MOTION_FORCE_ON;
    motion_update_json = true;
  }

  // else if state is forced OFF
  else if (str_command.equals ("0") || str_command.equalsIgnoreCase (D_JSON_MOTION_OFF))
  {
    motion_forced      = MOTION_FORCE_OFF;
    motion_update_json = true;
  } 
}

// get motion status in readable format
String MotionGetStatus ()
{
  String str_status;

  // if relay is OFF
  if (!MotionIsRelayActive ()) str_status = D_JSON_MOTION_OFF;

  // if tempo is forced ON
  else if (motion_forced == MOTION_FORCE_ON) str_status = D_JSON_MOTION_FORCED;

  // if motion is currently detected
  else if (MotionIsDetected () && MotionGetAutoRearm ()) str_status = D_JSON_MOTION;

  // else, temporisation is runnning
  else str_status = D_JSON_MOTION_TEMPO;

  return str_status;
}

// get temporisation left in readable format
String MotionGetTempoLeft ()
{
  TIME_T   tempo_dst;
  unsigned long time_tempo, time_now, time_left;
  char     str_number[8];
  String   str_timeleft;

  // if tempo is forced ON
  if (motion_forced == MOTION_FORCE_ON) str_timeleft = D_MOTION_FORCED;

  // else, if temporisation is runnning,
  else if (motion_time_start != ULONG_MAX)
  {
    // get current time and current temporisation (convert from s to ms)
    time_now   = millis () / 1000;
    time_tempo = MotionGetTempo ();
 
    // if temporisation is not over
    if (time_now < motion_time_start + time_tempo)
    {
      // convert to readable format
      time_left = motion_time_start + time_tempo - time_now;
      BreakTime ((uint32_t) time_left, tempo_dst);
      sprintf (str_number, "%02d", tempo_dst.minute);
      str_timeleft = str_number + String (":");
      sprintf (str_number, "%02d", tempo_dst.second);
      str_timeleft += str_number;
    }
    else str_timeleft = "0:00";
  }

  return str_timeleft;
}

// Save data for graph use
void MotionSetGraphData (int index, bool set_enabled, bool set_detected, bool set_active)
{
  // force index in graph window
  index = index % MOTION_GRAPH_SAMPLE;

  // generate stored value
  if (set_enabled == true) arr_graph_data[index].is_enabled = 1;
  else arr_graph_data[index].is_enabled = 0;
  if (set_detected == true) arr_graph_data[index].is_detected = 1;
  else arr_graph_data[index].is_detected = 0;
  if (set_active == true) arr_graph_data[index].is_active = 1;
  else arr_graph_data[index].is_active = 0;
}

// Retrieve enabled state from graph data
bool MotionGetGraphEnabled (int index)
{
  // force index in graph window and return result
  index = index % MOTION_GRAPH_SAMPLE;
  return (arr_graph_data[index].is_enabled == 1);
}

// Retrieve detected state from graph data
bool MotionGetGraphDetected (int index)
{
  // force index in graph window and return result
  index = index % MOTION_GRAPH_SAMPLE;
  return (arr_graph_data[index].is_detected == 1);
}

// Retrieve active state from graph data
bool MotionGetGraphActive (int index)
{
  // force index in graph window and return result
  index = index % MOTION_GRAPH_SAMPLE;
  return (arr_graph_data[index].is_active == 1);
}

/**************************************************\
 *                  Functions
\**************************************************/

// Show JSON status (for MQTT)
void MotionShowJSON (bool append)
{
  TIME_T  tempo_dst;
  uint8_t  motion_level;
  unsigned long time_total;
  String   str_json, str_mqtt, str_text;

  // Motion detection section  -->  "Motion":{"Level":"High","Enabled":"ON","Detected":"ON","Light":"ON","Tempo":120,"Timeout":240,"Status":"Timer","Timeleft":"2:15"}
  str_json = "\"" + String (D_JSON_MOTION) + "\":{";

  // detector active level (high or low)
  motion_level = MotionGetDetectionLevel ();
  str_json += "\"" + String (D_JSON_MOTION_LEVEL) + "\":\"";
  if (motion_level == 0) str_json += D_JSON_MOTION_LOW;
  else str_json += D_JSON_MOTION_HIGH;
  str_json += "\",";

  // motion enabled
  str_json += "\"" + String (D_JSON_MOTION_ENABLED) + "\":\"";
  if (motion_enabled == true) str_json += D_JSON_MOTION_ON;
  else str_json += D_JSON_MOTION_OFF;
  str_json += "\",";

  // auto rearm on detection
  MotionGetAutoRearm ();
  str_json += "\"" + String (D_JSON_MOTION_REARM) + "\":\"";
  if (MotionGetAutoRearm ()) str_json += D_JSON_MOTION_ON;
  else str_json += D_JSON_MOTION_OFF;
  str_json += "\",";

  // motion detection status
  str_json += "\"" + String (D_JSON_MOTION_DETECTED) + "\":\"";
  if (MotionIsDetected ()) str_json += D_JSON_MOTION_ON;
  else str_json += D_JSON_MOTION_OFF;
  str_json += "\",";

  // light status (relay)
  str_json += "\"" + String (D_JSON_MOTION_LIGHT) + "\":\"";
  if (MotionIsRelayActive ()) str_json += D_JSON_MOTION_ON;
  else str_json += D_JSON_MOTION_OFF;
  str_json += "\",";

  // total temporisation
  time_total = MotionGetTempo ();
  str_json += "\"" + String (D_JSON_MOTION_TEMPO) + "\":" + String (time_total) + ",";

  // motion status (off, timer, forced, ...)
  str_text = MotionGetStatus ();
  str_json += "\"" + String (D_JSON_MOTION_STATUS) + "\":\"" + str_text + "\",";

  // tempo left before switch off
  str_text = MotionGetTempoLeft ();
  str_json += "\"" + String (D_JSON_MOTION_TIMELEFT) + "\":\"" + str_text + "\"}";

  // generate MQTT message according to append mode
  if (append == true) str_mqtt = mqtt_data + String (",") + str_json;
  else str_mqtt = "{" + str_json + "}";

  // place JSON back to MQTT data and publish it if not in append mode
  snprintf_P (mqtt_data, sizeof(mqtt_data), str_mqtt.c_str ());
  if (append == false) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));

  // reset need for update
  motion_update_json = false;
}

// Handle detector MQTT commands
bool MotionMqttCommand ()
{
  bool command_handled = true;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kMotionCommands);

  // handle command
  switch (command_code)
  {
    case CMND_MOTION_ENABLE:  // enable or disable detector
      MotionEnable (XdrvMailbox.data);
      break;
    case CMND_MOTION_FORCE:  // force detector state 
      MotionForce (XdrvMailbox.data);
      break;
    case CMND_MOTION_TOGGLE:  // set detector tempo
      MotionToggleSwitch ();
      break;
    case CMND_MOTION_LEVEL:  // set detector level (high or low)
      MotionSetDetectionLevel (XdrvMailbox.payload);
      break;
    default:
    case CMND_MOTION_REARM:  // set auto-rearm flag
      MotionSetAutoRearm (XdrvMailbox.data);
      break;
    case CMND_MOTION_TEMPO:  // set detector tempo
      MotionSetTempo (XdrvMailbox.payload);
      break;
      command_handled = false;
  }

  // if needed, send updated status
  if (command_handled == true) MotionShowJSON (false);
  
  return command_handled;
}

void MotionUpdateGraph ()
{
  // set current graph value
  MotionSetGraphData (motion_graph_index, motion_graph_enabled, motion_graph_detected, motion_graph_active);

  // init current values
  motion_graph_enabled  = false;
  motion_graph_detected = false;
  motion_graph_active   = false;

  // increase temperature data index and reset if max reached
  motion_graph_index ++;
  motion_graph_index = motion_graph_index % MOTION_GRAPH_SAMPLE;
}

// update motion and relay state according to status
void MotionEvery250ms ()
{
  bool relay_target = true;
  unsigned long time_now, time_tempo;

  // if relay is forced ON : reset timer and keep ON
  if (motion_forced == MOTION_FORCE_ON)
  {
    motion_time_start = ULONG_MAX;
    relay_target      = true;
  }

  // else, if relay is forced OFF : reset timer and switch OFF
  else if (motion_forced == MOTION_FORCE_OFF)
  {
    motion_time_start = ULONG_MAX;
    relay_target      = false;
    motion_forced     = MOTION_FORCE_NONE;
  }

  // else check timeout
  else
  {
    // get current time and tempo (convert mn in ms)
    time_now   = millis () / 1000;
    time_tempo = MotionGetTempo (); 

    // if motion enabled and detected, update timer and keep ON
    if ((motion_enabled == true) && MotionIsDetected ())
    {
      // update start time on first detection
      if (motion_time_start == ULONG_MAX) motion_time_start = time_now;

      // or if auto-rearm is set
      else if (MotionGetAutoRearm ()) motion_time_start = time_now;
    }

    // else, if temporisation is reached, switch OFF
    else if ((motion_time_start != ULONG_MAX) && (time_now > motion_time_start + time_tempo))
    {
      motion_time_start = ULONG_MAX;
      relay_target      = false;
    }

    // else, if tempo not started, relay should be OFF
    else if (motion_time_start == ULONG_MAX) relay_target = false;
  }

  // if no timer, no JSON update timer
  if (motion_time_start == ULONG_MAX) motion_time_update = ULONG_MAX;

  // else, check for JSON update timer
  else
  {
    // if no JSON update started, start it
    if (motion_time_update == ULONG_MAX) motion_time_update = time_now;
    
    // if JSON update delay is reached, ask for update
    if (time_now > motion_time_update + MOTION_JSON_UPDATE)
    {
      motion_time_update = time_now;
      motion_update_json = true;
    }
  }

  // if needed, change relay state
  if (relay_target != MotionIsRelayActive ()) MotionSetRelay (relay_target);

  // if JSON update asked, do it
  if (motion_update_json == true) MotionShowJSON (false);
}

// update graph data
void MotionEverySecond ()
{
  // check if motion is enabled
  if (motion_graph_enabled == false) motion_graph_enabled = motion_enabled;

  // check if motion is detected
  if (motion_graph_detected == false) motion_graph_detected = MotionIsDetected ();

  // check if relay is active
  if (motion_graph_active == false) motion_graph_active = MotionIsRelayActive ();

  // increment delay counter and if delay reached, update history data
  if (motion_graph_counter == 0) MotionUpdateGraph ();
  motion_graph_counter ++;
  motion_graph_counter = motion_graph_counter % motion_graph_refresh;
}

// pre init main status
void MotionPreInit ()
{
  int index;

  // set switch mode
  Settings.switchmode[MOTION_BUTTON] = FOLLOW;

  // disable serial log
  Settings.seriallog_level = 0;
  
  // initialise graph data
  motion_graph_index   = 0;
  motion_graph_counter = 0;
  motion_graph_refresh = 60 * MOTION_GRAPH_STEP;
  motion_graph_enabled  = false;  
  motion_graph_detected = false;  
  motion_graph_active   = false;  
  for (index = 0; index < MOTION_GRAPH_SAMPLE; index++) MotionSetGraphData (index, false, false, false);
}

/***********************************************\
 *                    Web
\***********************************************/

#ifdef USE_WEBSERVER

// display base64 embeded icon
void MotionWebDisplayIcon (uint8_t icon_index)
{
  uint8_t nbrItem, index;
  String  str_class;

  // set image class
  if (icon_index == MOTION_DETECTED) str_class = "motion";
  else str_class = "light";

  // display icon
  WSContentSend_P (PSTR ("<img class='%s' src='data:image/png;base64,"), str_class.c_str ());
  nbrItem = sizeof (motion_icon[icon_index]) / sizeof (char*);
  for (index=0; index<nbrItem; index++) if (motion_icon[icon_index][index] != nullptr) WSContentSend_P (motion_icon[icon_index][index]);
  WSContentSend_P (PSTR ("' >"));
}

// detector main page switch button
void MotionWebMainButton ()
{
  String str_state;

  if (motion_enabled == false) str_state = D_MOTION_ENABLE;
  else str_state = D_MOTION_DISABLE;
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>"), D_PAGE_MOTION_TOGGLE, str_state.c_str (), D_MOTION_MOTION);

  // Motion control page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_MOTION_CONTROL, D_MOTION_CONTROL);
}

// detector configuration page button
void MotionWebConfigButton ()
{
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>"), D_PAGE_MOTION_CONFIG, D_MOTION_CONFIG, D_MOTION_MOTION);
}

// append detector state to main page
bool MotionWebSensor ()
{
  String str_motion, str_time;

  // dislay motion detector state
  if (motion_enabled == false) str_motion = D_MOTION_DISABLED;
  else if (MotionIsDetected ()) str_motion = D_MOTION_ON;
  else str_motion = D_MOTION_OFF;
  WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_MOTION_MOTION, str_motion.c_str ());

  // display timeleft
  str_time = MotionGetTempoLeft ();
  if (str_time != "") WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_MOTION_TEMPO, str_time.c_str ());
}

// Movement detector mode toggle 
void MotionWebPageToggle ()
{
  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // invert mode
  motion_enabled = !motion_enabled;

  // refresh immediatly on main page
  WSContentStart_P (D_MOTION_DETECTION, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='0;URL=/' />\n"));
  WSContentSend_P (PSTR ("</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body bgcolor='#303030' >\n"));
  WSContentStop ();
}

// Motion config web page
void MotionWebPageConfigure ()
{
  bool     rearm;
  uint8_t  tempo_mn  = 0;
  uint8_t  tempo_sec = 0;
  uint8_t  level;
  unsigned long time_current;
  char     argument[MOTION_BUFFER_SIZE];
  String   str_checked;
  
  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // page comes from save button on configuration page
  if (Webserver->hasArg ("save"))
  {
    // get detection level,according to 'level' parameter
    WebGetArg (D_CMND_MOTION_LEVEL, argument, MOTION_BUFFER_SIZE);
    if (strlen(argument) > 0) MotionSetDetectionLevel (atoi (argument));
    
    // get auto-rearm flag according to 'rearm' parameter
    WebGetArg (D_CMND_MOTION_REARM, argument, MOTION_BUFFER_SIZE);
    if (strlen(argument) > 0) MotionSetAutoRearm (D_MOTION_ON);
    else MotionSetAutoRearm (D_MOTION_OFF);
    
    // get number of minutes according to 'mn' parameter
    WebGetArg (D_CMND_MOTION_MN, argument, MOTION_BUFFER_SIZE);
    if (strlen(argument) > 0) tempo_mn = atoi (argument);
    
    // get number of seconds according to 'temposec' parameter
    WebGetArg (D_CMND_MOTION_SEC, argument, MOTION_BUFFER_SIZE);
    if (strlen(argument) > 0) tempo_sec = atoi (argument);

    // save total temporisation
    time_current = 60 * tempo_mn + tempo_sec;
    MotionSetTempo (time_current);
  }

  // beginning of form
  WSContentStart_P (D_MOTION_CONFIG);
  WSContentSendStyle ();
  WSContentSend_P (PSTR ("<form method='get' action='%s'>\n"), D_PAGE_MOTION_CONFIG);

  // motion detector section  
  // -----------------------
  level = MotionGetDetectionLevel ();
  rearm = MotionGetAutoRearm ();

  // get temporisation
  time_current = MotionGetTempo ();
  tempo_mn  = time_current / 60;
  tempo_sec = time_current % 60;

  // level (high or low)
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s %s&nbsp;</b></legend>"), D_MOTION_DETECTION, D_MOTION_LEVEL);
  if (level == 0) str_checked = "checked";
  else str_checked = "";
  WSContentSend_P (PSTR ("<p><input type='radio' id='low' name='level' value=0 %s><label for='low'>Low<span %s>%s</span></label></p>\n"), str_checked.c_str (), MOTION_TOPIC_STYLE, D_CMND_MOTION_LEVEL);
  if (level == 1) str_checked = "checked";
  else str_checked = "";
  WSContentSend_P (PSTR ("<p><input type='radio' id='high' name='level' value=1 %s><label for='high'>High</label></p>\n"), str_checked.c_str ());
  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // temporisation
  WSContentSend_P (PSTR ("<p><fieldset><legend><b>&nbsp;%s&nbsp;</b></legend>"), D_MOTION_TEMPO);
  WSContentSend_P (PSTR ("<p>minutes<span %s>%s</span><br><input type='number' name='%s' min='0' max='120' step='1' value='%d'></p>\n"), MOTION_TOPIC_STYLE, D_CMND_MOTION_TEMPO, D_CMND_MOTION_MN, tempo_mn);
  WSContentSend_P (PSTR ("<p>seconds<br><input type='number' name='%s' min='0' max='59' step='1' value='%d'></p>\n"), D_CMND_MOTION_SEC, tempo_sec);
  if (rearm == true) str_checked = "checked";
  else str_checked = "";
  WSContentSend_P (PSTR ("<br>\n"));
  WSContentSend_P (PSTR ("<p><input name='%s' type='checkbox' %s>%s %s %s<span %s>%s</span></p>\n"), D_CMND_MOTION_REARM, str_checked.c_str (), D_MOTION_REARM, D_MOTION_ON, D_MOTION_DETECTION, MOTION_TOPIC_STYLE, D_CMND_MOTION_REARM);
  WSContentSend_P (PSTR ("</fieldset></p>\n"));

  // save button  
  // --------------
  WSContentSend_P (PSTR ("<p><button name='save' type='submit' class='button bgrn'>%s</button></p>\n"), D_SAVE);
  WSContentSend_P (PSTR ("</form>\n"));

  // configuration button and end of page
  WSContentSpaceButton(BUTTON_CONFIGURATION);
  WSContentStop();
}

// Motion status graph display
void MotionWebDisplayGraph ()
{
  TIME_T   current_dst;
  uint32_t current_time;
  int      index, array_idx;
  int      graph_x, graph_y, graph_left, graph_right, graph_width, graph_low, graph_high, graph_hour;
  bool     state_curr;
  char     str_hour[4];

  // boundaries of SVG graph
  graph_left  = MOTION_GRAPH_PERCENT_START * MOTION_GRAPH_WIDTH / 100;
  graph_right = MOTION_GRAPH_PERCENT_STOP * MOTION_GRAPH_WIDTH / 100;
  graph_width = graph_right - graph_left;

  // start of SVG graph
  WSContentSend_P (PSTR ("<svg viewBox='0 0 %d %d'>\n"), MOTION_GRAPH_WIDTH, MOTION_GRAPH_HEIGHT);

  // graph curve zone
  WSContentSend_P (PSTR ("<rect x='%d%%' y='0%%' width='%d%%' height='100%%' rx='10' />\n"), MOTION_GRAPH_PERCENT_START, MOTION_GRAPH_PERCENT_STOP - MOTION_GRAPH_PERCENT_START);

  // graph label
  WSContentSend_P (PSTR ("<text class='active' x='%d%%' y='%d%%'>%s</text>\n"), 0, 27, D_MOTION_COMMAND);
  WSContentSend_P (PSTR ("<text class='enabled' x='%d%%' y='%d%%'>%s</text>\n"), 0, 59, D_MOTION_ENABLED);
  WSContentSend_P (PSTR ("<text class='detected' x='%d%%' y='%d%%'>%s</text>\n"), 0, 92, D_MOTION_DETECTOR);

  // ------------------
  //   Detector state
  // ------------------

  // loop for the sensor state graph
  graph_high = MOTION_GRAPH_HEIGHT * 83 / 100;
  graph_low  = MOTION_GRAPH_HEIGHT * 99 / 100;
  WSContentSend_P (PSTR ("<polyline class='detected' points='"));
  for (index = 0; index < MOTION_GRAPH_SAMPLE; index++)
  {
    // get current detection status
    array_idx  = (index + motion_graph_index) % MOTION_GRAPH_SAMPLE;
    state_curr = MotionGetGraphDetected (array_idx);

    // calculate current position
    graph_x = graph_left + (graph_width * index / MOTION_GRAPH_SAMPLE);
    if (state_curr == true) graph_y = graph_high;
    else graph_y = graph_low;

    // add the point to the line
    WSContentSend_P (PSTR("%d,%d "), graph_x, graph_y);
  }
  WSContentSend_P (PSTR("'/>\n"));

  // -----------------
  //   Enabled state
  // -----------------

  // loop for the sensor state graph
  graph_high = MOTION_GRAPH_HEIGHT * 50 / 100;
  graph_low  = MOTION_GRAPH_HEIGHT * 66 / 100;
  WSContentSend_P (PSTR ("<polyline class='enabled' points='"));
  for (index = 0; index < MOTION_GRAPH_SAMPLE; index++)
  {
    // get current detection status
    array_idx  = (index + motion_graph_index) % MOTION_GRAPH_SAMPLE;
    state_curr = MotionGetGraphEnabled (array_idx);

    // calculate current position
    graph_x = graph_left + (graph_width * index / MOTION_GRAPH_SAMPLE);
    if (state_curr == true) graph_y = graph_high;
    else graph_y = graph_low;

    // add the point to the line
    WSContentSend_P (PSTR ("%d,%d "), graph_x, graph_y);
  }
  WSContentSend_P (PSTR ("'/>\n"));

  // ------------------
  //    Light state
  // ------------------

  // loop for the relay state graph
  graph_high = MOTION_GRAPH_HEIGHT * 17 / 100;
  graph_low  = MOTION_GRAPH_HEIGHT * 33 / 100;
  WSContentSend_P (PSTR ("<polyline class='active' points='"));
  for (index = 0; index < MOTION_GRAPH_SAMPLE; index++)
  {
    // get current detection status
    array_idx  = (index + motion_graph_index) % MOTION_GRAPH_SAMPLE;
    state_curr = MotionGetGraphActive (array_idx);

    // calculate current position
    graph_x = graph_left + (graph_width * index / MOTION_GRAPH_SAMPLE);
    if (state_curr == true) graph_y = graph_high;
    else graph_y = graph_low;

    // add the point to the line
    WSContentSend_P (PSTR ("%d,%d "), graph_x, graph_y);
  }
  WSContentSend_P (PSTR ("'/>\n"));

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
  sprintf (str_hour, "%02d", current_dst.hour);
  WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%sh</text>\n"), graph_x, 75, str_hour);

  // dislay next 5 time marks (every 4 hours)
  for (index = 0; index < 5; index++)
  {
    current_dst.hour = (current_dst.hour + 4) % 24;
    graph_x += graph_width / 6;
    sprintf (str_hour, "%02d", current_dst.hour);
    WSContentSend_P (PSTR ("<text class='time' x='%d' y='%d%%'>%sh</text>\n"), graph_x, 75, str_hour);
  }

  // end of SVG graph
  WSContentSend_P (PSTR ("</svg>\n"));
}

// Motion control public page
void MotionWebPageControl ()
{
  bool   is_light;
  String str_time;

  // handle light switch
  if (Webserver->hasArg(D_CMND_MOTION_TOGGLE)) MotionToggleSwitch (); 

  // beginning of form without authentification with 5 seconds auto refresh
  WSContentStart_P (D_MOTION, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%d;URL=/%s' />\n"), MOTION_WEB_UPDATE, D_PAGE_MOTION_CONTROL);
  
  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {color:white;background-color:#303030;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));

  WSContentSend_P (PSTR (".title {font-size:5vh;}\n"));
  WSContentSend_P (PSTR (".info {margin-top:10px; height:60px;}\n"));

  WSContentSend_P (PSTR ("div.status {width:128px;position:relative;padding:16px;}\n"));
  WSContentSend_P (PSTR ("img.light {position:relative;top:0;left:0;height:92px;z-index:1;}\n"));
  WSContentSend_P (PSTR ("img.motion {position:absolute;top:72px;left:-32px;height:48px;z-index:2;}\n"));
  WSContentSend_P (PSTR ("span.timeleft {position:absolute;font-style:italic;top:88px;left:136px;z-index:2;font-size:24px;}\n"));

  WSContentSend_P (PSTR (".time {font-size:20px;}\n"));
  WSContentSend_P (PSTR (".bold {font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".yellow {color:#FFFF33;}\n"));

  WSContentSend_P (PSTR (".switch {text-align:left;font-size:24px;font-weight:bold;}\n"));
  WSContentSend_P (PSTR (".toggle input {display:none;}\n"));
  WSContentSend_P (PSTR (".toggle {position:relative;display:inline-block;width:140px;height:40px;margin:10px auto;}\n"));
  WSContentSend_P (PSTR (".slide-off {position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#ff0000;border:1px solid #aaa;border-radius:5px;}\n"));
  WSContentSend_P (PSTR (".slide-off:before {position:absolute;content:'';width:64px;height:34px;left:2px;top:2px;background-color:#eee;border-radius:5px;}\n"));
  WSContentSend_P (PSTR (".slide-off:after {position:absolute;content:'Off';top:5px;right:20px;color:#fff;}\n"));
  WSContentSend_P (PSTR (".slide-on {position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#8cbc13;border:1px solid #aaa;border-radius:5px;}\n"));
  WSContentSend_P (PSTR (".slide-on:before {position:absolute;content:'';width:64px;height:34px;left:72px;top:2px;background-color:#eee;border-radius:5px;}\n"));
  WSContentSend_P (PSTR (".slide-on:after {position:absolute;content:'On';top:5px;left:15px;color:#fff;}\n"));

  WSContentSend_P (PSTR (".graph {max-width:%dpx;}\n"), MOTION_GRAPH_WIDTH);

  WSContentSend_P (PSTR ("rect.graph {stroke:grey;stroke-dasharray:1;fill:#101010;}\n"));
  WSContentSend_P (PSTR ("line.dash {stroke:grey;stroke-width:1;stroke-dasharray:8;}\n"));
  WSContentSend_P (PSTR ("polyline.active {fill:none;stroke:yellow;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.detected {fill:none;stroke:orange;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("polyline.enabled {fill:none;stroke:red;stroke-width:2;}\n"));
  WSContentSend_P (PSTR ("text.active {stroke:yellow;fill:yellow;}\n"));
  WSContentSend_P (PSTR ("text.detected {stroke:orange;fill:orange;}\n"));
  WSContentSend_P (PSTR ("text.enabled {stroke:red;fill:red;}\n"));
  WSContentSend_P (PSTR ("text.time {stroke:white;fill:white;}\n"));

  WSContentSend_P (PSTR ("</style>\n</head>\n"));

  // page body
  WSContentSend_P (PSTR ("<body>\n"));
  WSContentSend_P (PSTR ("<form name='control' method='get' action='%s'>\n"), D_PAGE_MOTION_CONTROL);

  // room name
  WSContentSend_P (PSTR ("<div class='title bold'>%s</div>\n"), SettingsText(SET_FRIENDLYNAME1));

  // light icon, motion and tempo
  is_light = MotionIsRelayActive ();
  str_time = MotionGetTempoLeft ();
  WSContentSend_P (PSTR ("<div class='status'>\n"));
  if (is_light == true) MotionWebDisplayIcon (MOTION_LIGHT_ON);
  else MotionWebDisplayIcon (MOTION_LIGHT_OFF);
  if (MotionIsDetected ()) MotionWebDisplayIcon (MOTION_DETECTED);
  if (str_time != "") WSContentSend_P (PSTR ("<span class='timeleft yellow'>%s</span>\n"), str_time.c_str ());
  WSContentSend_P (PSTR ("</div>\n"));

  // toggle switch
  WSContentSend_P (PSTR ("<div>"));
  if (is_light == true) WSContentSend_P (PSTR ("<label class='toggle'><input name='%s' type='submit'/><span class='slide-on switch'></span></label>"), D_CMND_MOTION_TOGGLE);
  else WSContentSend_P (PSTR ("<label class='toggle'><input name='%s' type='submit'/><span class='slide-off switch'></span></label>"), D_CMND_MOTION_TOGGLE);
  WSContentSend_P (PSTR ("</div>\n"));

  // display graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  MotionWebDisplayGraph ();
  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
  WSContentSend_P (PSTR ("</form>\n"));
  WSContentStop ();
}

#endif  // USE_WEBSERVER

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv98 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_PRE_INIT:
      MotionPreInit ();
      break;
    case FUNC_COMMAND:
      result = MotionMqttCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      MotionEvery250ms ();
      break;
  }
  
  return result;
}

bool Xsns98 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_EVERY_SECOND:
      MotionEverySecond ();
      break;
    case FUNC_JSON_APPEND:
      MotionShowJSON (true);
      break;

#ifdef USE_WEBSERVER
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on ("/" D_PAGE_MOTION_CONFIG, MotionWebPageConfigure);
      Webserver->on ("/" D_PAGE_MOTION_TOGGLE, MotionWebPageToggle);
      Webserver->on ("/" D_PAGE_MOTION_CONTROL, MotionWebPageControl);
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      MotionWebMainButton ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      MotionWebConfigButton ();
      break;
    case FUNC_WEB_SENSOR:
      MotionWebSensor ();
      break;
#endif  // USE_WEBSERVER

  }
  
  return result;
}

#endif // USE_MOTION
