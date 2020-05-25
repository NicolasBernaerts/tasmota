/*
  xdrv_98_motion.ino - Motion detector management with tempo and timers (~16.5 kb)
  
  Copyright (C) 2020  Nicolas Bernaerts
    27/03/2020 - v1.0 - Creation
    10/04/2020 - v1.1 - Add detector configuration for low/high level
    15/04/2020 - v1.2 - Add detection auto rearm flag management
    18/04/2020 - v1.3 - Handle Toggle button and display motion icon
    15/05/2020 - v1.4 - Add /json page to get latest motion JSON
    20/05/2020 - v1.5 - Add configuration for first NTP server
                   
  Input devices should be configured as followed :
   - Switch2 = Motion detector

  Settings are stored using weighting scale parameters :
   - Settings.energy_voltage_calibration = Moton detection status (0 = low, 1 = high)
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
#define D_PAGE_MOTION_GRAPH          "graph"
#define D_PAGE_MOTION_JSON           "json"

#define D_CMND_MOTION_ENABLE         "enable"
#define D_CMND_MOTION_FORCE          "force"
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
#define D_MOTION_GRAPH               "Graph"
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
enum MotionCommands { CMND_MOTION_TEMPO, CMND_MOTION_REARM, CMND_MOTION_LEVEL, CMND_MOTION_ENABLE, CMND_MOTION_FORCE };
const char kMotionCommands[] PROGMEM = D_CMND_MOTION_TEMPO "|" D_CMND_MOTION_REARM "|" D_CMND_MOTION_LEVEL "|" D_CMND_MOTION_ENABLE "|" D_CMND_MOTION_FORCE;

// form topic style
const char MOTION_TOPIC_STYLE[] PROGMEM = "style='float:right;font-size:0.7rem;'";

// graph data structure
struct graph_value {
    uint8_t is_enabled : 1;
    uint8_t is_detected : 1;
    uint8_t is_active : 1;
}; 

// motion icon in base64
const char strIcon0[]  PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAAEAAAAAyCAMAAADbXS0mAAAUMnpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjarZppluuqkoX/M4oaguhhOLRr1Qze8OsLQLYzj503z62XjWXLEgTR7NgRSI3//O9U/8NPCNEo52MKOYSLH5ddNoU36do/+6gvt17Xjztf8fnLeWXN+cJwynK0+2MY5/rCef+8IZ6RdP16XsV2xklnoPPFPaCVmWWyfoQ8A1mzz+vzWeUjUQkvyzn/s5ksp3zdX33/7CLK6J7xrFFmWG2v9Wr2THb/F/4dr9oGuZCjvM+8Ohv+1J96qO6NAlt6r7+rnSvsUx17oHtZ4Zueznnt3+tvaelVIm3OJeb5xdJJ1LdMf+pv9jTn2KsrLijUFc6i7iWud1yISp1dtwV+I/+e93H9Zn7TVa6G1TpLreqqfMjaoPGpne666KnHOjbdENGZYSJHYxoal3PJRpNNwxgaxfOrp4kKO3SbsErDcpbT5iGLXvNmmY/JEjN3zZVGMxg2/vqrvp/4t79fBppT3FxrUWZLS1fIZcS/EEMsJ69chUH0PDr1S79a7cP1/UcMa7GgX2pOLLBcdQ9RvX76ll12tpdXXOpOSOvYzwCoiLk9wmiLBa6grddBX9GYqDV6TNinILmxzlQsoL3ypiOlcZZIiCYZmZt78CC51nizTwMvGMLbYCOmIVAwlnPeBeIt4UJFeeud9z746JPPvgQb";
const char strIcon1[]  PROGMEM = "XPABrAqCUyXa6KKPIcaYYo4l2eSSTyHFlFJOJZtsgTGvcsgxp5xzKUxaXGGswvWFE9VUW131NdRYU821NNynueZbaLGlllvpptsOBKgeeuyp516GHrjScMOPMOJII48y8bVpp5t+hhlnmnmWh9WOVb9a7bvlfraaPlYzy1ByXXxajdMx3kNogRMvNsNixmksHsUCOLQRm11JO2fEcmKzKxurrPUGKb0Yp2uxGBZ0Qxs/9cN2T8t9tJtCu39rN/POckpM99+wnBLTvVjuT7u9sVovC27tMpBEIToFIe0ULJzF1BEn73sJdQ4WNydC7DPWD0k8muu69pzy3z+pn7787adYalQp2mYzAKxnyHO6wkoa0CaSmDbD6L3mGe2oGCKUht5qtaBrZdElp15yqwCqMr3P0EcvvsdgB9fJEi2vuY5aJ183N1ls91kGL7LaLNCl7UhNrkaOlBSn7dXlEuyCVoyPF+r3rsQ2tTcWxemawiylx+x1ZVJsE3r3baLLVGYi3xfVr+HtWHMxuLym6998Vo8T6G72nCMGC2sRNTqxdCd+4ujWyzlrKiquawWDTDPPjSTIH4wtxvG/PSoxZG+z2SKD4N8yo1kDZpRd8Mg8r0EwLClZRrdoRnwuM/UcRAGGrQrDtthD7b7UVhthV0qOsbtZc0uct3kAyTbVHmydjXu2JL6NWIP2Q2c/RlTrTe/JzOonNoIJZtaKMyxdlHNXJ65K+mFtar/Z2hmtrSMJ";
const char strIcon2[]  PROGMEM = "V9JtQfAeXhT6YotlCdHnbQuFMZ6meDXEixXEglXOPK3gR+lDN9HeBEN7UQxhaltyEOCz4f61RoIfC5QOjNt98ZaorqsTOohxXbsutixcFR8Nyh628ZK5ey/CAxejWSJniqS+o+hYe8Wp52VbCakQPFpPYnWKLVXrc6D2ZUxRch5rFo0hsOVVKohJmvSgZKp+JGIHBO8W2Gv9WAK41UrU7oXlfjveSmBSJAyiIYGNuNa1Ftqq2ysDFCOZlhh9aO1cftRwLrf35S0WgR7uAEijjmMAnFxZSIlTNYvndNBmdJRvXURXtUySQZxxOQ5pCLdeoLWcI6wJAC7IWUJRE9iKQaGSBOlET427MqAlsJQRahQyV8049bQERqwRDoj42FHW+7TjWq86hnS3IVkuMogksVqd0GnlTotfojVWlnh1MQaoJAhSCjRfjkala785R0SVCBWbil/bZVIXs17xmceyqR3DdlMCxtQkvFCdSQpzTj5aMtnoGesuKP89krujQLW+Ff29aG9dHMOL+vr0FVV54i/ipXP6OCqOyDTxAFuObsw+SABEf7tAn89YKZHmF8zOahdmtcbqxZJDSSQDRaXtaGOYmbPHxxl8XZJ6TGPea90OkF8doC4HUGsFea9gND+xH1Ykn8+VpEUeF/CFSDUgXziZQ6Rtk0QXs03dBoJbJVgI/tIjrIADunZL+GwtOIsdm1mxCdJKbEpo3jhbHziLGGrhbMuJcXLP5GE0Ab4W";
const char strIcon3[]  PROGMEM = "jWc5vKZl3eyyjItPw/hXw3R8eCixTJB1LctHQR/BM3jWDBsvQ8RBCqRFYzJX8XsU53CjAJBzMpsefFeIlsmbFgpnBfhG9JIxbzwj/g6agaTzoNlCPruQLwAv2uDgqlofbKn2apAz+CJrd6lVrO/hTwGj9+jr8lQJG5eHeeaAF99Vt/Muc97u+4fzfnX0d9eqm7PUKjSN0g5sGqS6IJnQQDdgKDPgjDmuQEcVrAZC6We8E1yhiFF3bqKYwzCoOKcNEDg6/m0gIrg6efhqZTpqUFsblLRSZ/rmNBcHlOADtAYHsLAo2PRWAzH+mvqQ9TcIql4hFL03V0rAeSA/AQqwLGCGWCA7QC66bnA1ohNH0JKk9zSQ0eUpkuE2tIuRH9B+ElwSM4/4aeHdGFkaTgaDthn3FqjqCX8vFAGAJXoVahYreBY3dmO94+CP0D1OLtRv+7iE7nJxsZ+DHJIqGuxukMfInBHko0jAt5sQnZfUuDOj+pQaEY2wksXkRSYO7qDJ3LZ5hU2J+qvfjI2Fdv3pJsiGoRBwQEVs1whZgOIiwJER0zjQJptLCId6wzjcKOMjaywR4E0diCRJSbW4NJWj6CiMuRSxY/VNznqXspa/AGZukHdzDapPimcGACqMpk5zw0wKpgVupvrEwtuoREETDkj4bE6+aBbDZT1c4FTGIZe3YJ4kiYQqwWq8vBdXC0Xo8FehDCVGsgGUcKgplPFP+dXvFqCp9UDVSnTalIBQ";
const char strIcon4[]  PROGMEM = "Cj+bFkU1whsbzD/sTFu5ZDhW0329V0F2kjXICqaof6mVJYjXsYQMcBLqKQp9VNrsOoQPhB/oAOkaBqQj/8FNehzOZsIaFwiG0hLbxgy3m5SL5IET6JpMCwP7g2R9ghTJK2Vkh/O44qKhUKYggplro4qNwo3KQACCSuc9RTVxhHYPHXaGlDC72YB/ZQP4q9qIGgQkhyTlzeNlfZT42ZIuMU9lHLTncGf5g4pYIr0NQHIjXrSqvA3BDxGYmm7ZkHNwYOQjJ7mRQQ2gQcF8uBUoueI36RdEPCAe6d8L78ISXYnsF8YWzyZb8+emo1Lt0+LTRqLubWFBzLGGZ6irl1j34apwxUZa+1rYaBap+4OPisg3kZshbe9SO0KmhJR4Mqg5iG0YHaig9xQr40smy6VFytwAjcyEKHW2LhKfyXmvDJGZl7dwQdvlV38LXQeiRbvOQGQCCkpVUhKKy4qcBNFskJ/auwmfaMQYpSZ/wuHPaGhVMdww8NRkzYIGgX6xVc+3e7+XSwJJqn84D8ovU82VxKD/MQC4ySVgoi00QsugEIo4mZOMlN1HiRUiX9/DfUPWCfhv4d6ZHb22TCa1mKxrwBh8VSbBZVA9QIJYsqTkb8TaqfMA1oMfuZsfpVUZnnSu7ix7qNQjn7+UkWTGT1hAQV7J5vBvhTqMNYVAg8t4twgyOBDNzNGaHgV1EORUHERjB3zfEFT1jqEugkr4ib3TDg/pw6wuDGY3XqoPUM5r";
const char strIcon5[]  PROGMEM = "/CXPLJCssrNwnpKB6gz8jZBwmUS+wqo6SJfcGQnasLy8avGoAnMs/pop81FXXZ3VVJDEMwECrmsiVW6T3OPiw4w4ym3E8QW14zKjJB7YqEpCTKEdOoegBZybwA1RwxdYXNp4ZtTFhU7h9L6aVK/l5LMcImtBQSDRDZSu8Egz4b0wE3IOpKdVMUvskLEhjZRmqpDRFUCy3WREoeu9hJA0tk4b59nE6XmUCCo0Q72XTZx1eou1hnLtjse39lkR+YmmSgppFKYQtqgwzrDgu8eK9oXBjrf1UPeJSaVt0aW5SZneHObgapRdByjcS8tV6iEkH5eLJxdIZfvwekZ6LUVH2NGHA8KwcEhBw9ZWrZd282cKBfLZeo9aC3kEvcKwCCpqbAfd1DJMBzTIyQIlZmG21M9NMpD9hrLhmHijrD/IFVq2szfyzO7xhHhVocfrrn0DGhFsHrpUseV75YstO1oferW/AO4ZR6GoEVKu35pZoptIeafyvvojcVJfgn5LRHVkvCW8BVzirWW9Cviuh3rEU7d80ilDQhHwFD1QCq8XVddrM5fv0+eGy4cQgbSLG75dLhBjpOkjnUSut0wOs4BEuEky11A/RAAPiR/ph0FDVxcFTig87iACGCw4vhCB+4WJHkRQeXPRWK/DRf+GnVQyR261Qa/U7Jf4OvQdnTUdIeLJjlXQQCanJBJrL+Bj8WOR6GaWZr4ySyQ6uUYvarlYKcgjexOoXjvhV9Ke807q";
const char strIcon6[]  PROGMEM = "uL61L0RXMpxs1lWxdNHS0Mx2pR4KA3DQktkzs2NBEIEq92IYs3zISEHLSqEhwlk3SQBQGmOAkBIRrm2vsZJzw0y+eIpyiI8eIIQ09nWiRMZSSALNmlQsoDNA8Mi86nvqPe73hiyELNGmLWxZaAJutUhCzsvR1as7v8Tbj2AHw4MmFGkcCk0ggktUyAIv/wSrN/F4F29HPiJDBFQrNF5khHc3c7rtq1LTd7e9jSAl4nv8skpCzki7oZ6I0zviumZpQhwPz13E8XSOFnHMYXuOsEYWqbrQxvihKeZt6dQZ2uUSBCojFkJoMjPIjjsUajXqrk7dpZ5cn6O/O/aU2GGXmH61yEb6yrCYRgx9OKEEovrq99LVnE+K9cXtJRCF1Wqp2yllNqudcHNYrVPkUJ/GRl9i9/rgDrjXKkmFnr10Wrhbdilrz+jIM9nUjNyDu0GNvJEeZdcPTFcoE14EmVUxW/g7xclVpfTyRWupvGCKhQQjUbBGvmSD8jSI3rbO1KfeGd5SM1Hs5o2tUuuEZ9+2vybLNNVqZ41E8j29rMoZ0lfcvSzpRawhPaGPDaP8yEMX/luzSe2m2yctHB0DNGQIJ/rNyzAX0cHyhWOsKLiicpSBrjPW9Tl8vwK2kJ4vkC1uUtUTsKlhwSgcOHrgN1RZ2Sp9Vg0HhyMaCOFbewKSHMNplChrE2xG2ucCCOB7Aa3gTpd59JzkX8Js12dxY/ROGtPG3Wgwqj8KQb5+dn2o";
const char strIcon7[]  PROGMEM = "A+/2yT91T3acqneBWvNrN1q2VxDjczda9pthI+TKGGx7ZV/fyddvUp169sL8qpMOwX5a57R2xq6TTjaVzs4AzihcIV4YxSoqPpxPM1QhMVJfg4OADGoHCA2HvKjywZnPu5WqkOAuiWVIufg+iTavrmmXCjVSocIBwyu5/bOilNWrs/y95ynK6w9Tn0pnJXstm22yLU1mlSq7R2e70Kza8R6t+3raJ3gmp6R5dK1LIteuEPoUPG0wSjaGGaVdcHUlTZupd89mSs+GackmCztku5sqaIXPvJP8Xpxkecnxd+72StKzzl3SMyrKI1jqC3SPabmA8K8jCUFG3cywe2tn/+5Li1c9NjG/9HiltxZAuVSFg7QpzzdBp/OwUE4SKs6JPWok/Up/V3bXb1ImZYP5mRaP5o3QFU9N30hTXzbv1XP3flO9tW3/gUYsjfz90oAxiEKljsAKsjOQL85HCJSh0sJwQKXkq93UUndvQnamTndi5fPdnpBHWnSQcGQAIOCScCQ6ioQjbkGOzUQGzqIefdO743h6prt2/dY0xbnv/cCzX38Hb1RPomp38FKm56iBQYKsWLCyIDjeKanqqstuVfbtiIFeW129IkZWf9P8/qn3Tbnuw1MPyJmS1YhNai4WOmnFJ4tr/7CJ59Qvu3yft/CA2xI8tMb4QKKv7wn1zzsPrxsPau08uMdO3MnLT+n6U7p0pHOxmhxtNL6SbpwTlnDJQOQsCl1SqexFAo8V";
const char strIcon8[]  PROGMEM = "YfZW8x9bbOK+b4lCUGP+87bgy66g9ETNCMP71eG6NOmRr/qghBCqb10NYmQcpL8E3XN3Wq765IvLFdWrL171QwFLDC9uLkagRMnMMR0Z0KdCDukI5BVMTvYGHw+INGp883d9NkhU/zc6GryD7AtlFCVlkmkkHXn0YgjJy9cA4cA44O2URs7ZRXiSmQfrvfdCDpNosjuupELO0pKirvX85ZyJJMknBKOTThlEnaJYts2lM5UfdXem+EiPulv9/KTDt9bUybcjhWu4XoL0kIeTnZKkZJ/EyR9+tbdKHjslCJzL0wN+fjhBfXk6ob6rgn+qBsiToIYhM6suOVML+QuwdtJnk+YfN5XQpVRbusMbbsDfz/h8eRZlbROpx6MoXj82lV4d5xFd31wHZCm+9YalgfH9sNcQ7vlD6P5qx02e9qk6/qtHmDxFTjC4DiM30lElWyFpodT3mimu+fFJMr8h9h1WqgWWJbXTU5Gn4v88vtkr+948V182yxZ1kAckMz6Hf8fuGsocVHQpQyPBmrA7rrW+ujd2Vf4vbMWUd7n0RPEDzOoZEl8Dva4gt61KCTdIEDPJbkK/4HaG8eV5RdCi4NT4sL1UWaqYnokej+uAxGOfvoB/L30ou3dHP3cnlKTHGACYeadHnKebZPA6SY9RmvqUc2dCqFGqX54POkd1IPknb3mafT0Lvh6nkV1IOQOpuz5uQb88ivO+Irm+SSNH9ZIo7icfTsKQPYaDFzfx";
const char strIcon9[]  PROGMEM = "jk/ibTfxTlNnjUsgUaFSF2/zexOQVwoCyqc6pLBb/uLTPzuJeonoywUI+JAHyrI8kpX7VcJ6JCu4JDu7n2iqzKUINZNICp4cTuaCqkLSJZV5eFLBkakR892UfE2a9muTX712+b+S1l89dPFgrepXtFWayF9201YHfOXgO2LUbxKjCzqY3l0tI2mim9muZnYHCRY5M/xX2esOkmum8M5nf+ey6keftSN9HvqjQ/7/jn8MhEyz5+tS/weWZuHzw2hj5QAAAYRpQ0NQSUNDIHByb2ZpbGUAAHicfZE9SMNAGIbfppWKVBTaoYhDhupkQVTEUatQhAqhVmjVweT6C00akhQXR8G14ODPYtXBxVlXB1dBEPwBcXJ0UnSREr9LCi1ivOO4h/e+9+XuO0BoVplqBsYBVbOMdDIhZnOrYvAVAQwiTDMqM1Ofk6QUPMfXPXx8v4vzLO+6P0d/vmAywCcSzzLdsIg3iKc3LZ3zPnGEleU88TnxmEEXJH7kuuLyG+eSwwLPjBiZ9DxxhFgsdbHSxaxsqMRTxLG8qlG+kHU5z3mLs1qts/Y9+QtDBW1lmeu0hpHEIpYgQYSCOiqowkKcdo0UE2k6T3j4hxy/RC6FXBUwciygBhWy4wf/g9+9NYuTE25SKAH0vNj2xwgQ3AVaDdv+Prbt1gngfwautI6/1gRmPklvdLTYETCwDVxcdzRlD7jcAaJPumzIjuSnJRSLwPsZfVMOCN8CfWtu39rn";
const char strIcon10[] PROGMEM = "OH0AMtSr1A1wcAiMlih73ePdvd19+7em3b8fNxtyj+cYlJUAAAAJcEhZcwAALiMAAC4jAXilP3YAAAAHdElNRQfkBBATEy8wxmOJAAAANlBMVEUAAAD3Ozv3Ozv3Ozv3Ozv3Ozv3Ozv3Ozv3Ozv3Ozv3Ozv3Ozv3Ozv3Ozv3Ozv3Ozv3Ozv///8PkULZAAAAEHRSTlMAECAwQFBgcICPn6+/z9/vIxqCigAAAAFiS0dEEeK1PboAAAI8SURBVEjHzVbJlsMgDDOBLKzW/3/tHGKCoaSdZi7Dpa9JELItCxP993UUsDfP93sAQHy8f8G57FMAJwDHXwH2xzHwCbA8Blj/SIDIhhTd58/CdlcGn44b/lvocpWnn+0AgBn6koHGLAFlojYTzhTyBNwUIF1xzvNkshQR4YacbXplQ0QuacVZxrU6CjatRGQY8PKkyBlJS26DWprCAWQiogCUFoGV363vohmFTX1uKyTXSK669vs7Cix/uRJO0q+lBWUBgPOcghfusdaBzxosaJXdALBNspn7fnTCfa+MZeMGQEfgqJ6+AkBsQpFcOdngAJgzFZcyMoAzDgB8ZqQ4pbuDiIycXIGiShWAJDoGUpVU9cYgSRsAlAocAH9V4riqWpZatvQe4ACwUhEAR0TkSrO2VwAeACKAxdQcqs7af8kgAUXMSIlrTcG8A4jN/1fmjY7bXoyvSVyGMgqNG1PP56NLeRMhyeO6BksCsCrqVBrgqrpe";
const char strIcon11[] PROGMEM = "AbAdnVooly6k0oV7yihOTC20Zor1U9bG1EoJWPGV3F5cVsS1rFa42y5fRUooeozakbBIJHa0NN77ezk1b7mgdz6r1SyNvPSjC8twpx0tmNyuhOCkF706bVLrWmbpxXF00TaVuvS1GhR9yfc3pGGdFwckM75Wsphc0SZ1kH64gMOVwhbP2BLOfxxMlk4T+YuhIA89dMyy+GZJBs3A6NejmmRQtaC5HRPeEOi8oUDJ5vN0+0o4fjWvrq/HbV+FQHth/zoze/qn6wddqDWV7r2fTQAAAABJRU5ErkJggg==";
const char *const arrIconBase64[] PROGMEM = {strIcon0, strIcon1, strIcon2, strIcon3, strIcon4, strIcon5, strIcon6, strIcon7, strIcon8, strIcon9, strIcon10, strIcon11};

/*************************************************\
 *               Variables
\*************************************************/

// detector states
enum MotionStates { MOTION_OFF, MOTION_ON };
enum MotionForce  { MOTION_FORCE_NONE, MOTION_FORCE_OFF, MOTION_FORCE_ON };

// variables
bool    motion_updated   = false;              // data has been updated, needs JSON update
bool    motion_enabled   = true;               // is motion detection enabled
bool    motion_detected  = false;              // is motion currently detected
bool    motion_active    = false;              // is relay currently active
bool    motion_autorearm = false;              // tempo automatically rearmed if motion detected
uint8_t motion_forced = MOTION_FORCE_NONE;     // relay state forced
unsigned long motion_time_start  = 0;          // when tempo started
unsigned long motion_time_update = 0;          // when JSON was last updated
String   str_motion_json;

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
  motion_updated = true;
}

// check if motion is detected
bool MotionDetected ()
{
  bool    last_state;
  uint8_t actual_status, detection_status;

  // save last state
  last_state = motion_detected;

  // check if motion is detected according to detection level
  actual_status    = SwitchGetVirtual (MOTION_BUTTON);
  detection_status = MotionGetDetectionLevel ();
  motion_detected  = (actual_status == detection_status);

  // if change, ask for JSON update
  if (last_state != motion_detected) motion_updated = true;

  return motion_detected;
}

// get relay state
bool MotionRelayActive ()
{
  uint8_t relay_condition;

  // read relay state
  relay_condition = bitRead (power, 0);
  motion_active   = (relay_condition == 1);

  return motion_active;
}

// set relay state
void MotionSetRelay (bool new_state)
{
  // set relay state
  if (new_state == false) ExecuteCommandPower (1, POWER_OFF, SRC_MAX);
  else ExecuteCommandPower (1, POWER_ON, SRC_MAX);
  motion_updated = true;
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
  motion_updated = true;
}

// get if tempo should be rearmed on detection
bool MotionGetAutoRearm ()
{
  motion_autorearm = (Settings.energy_current_calibration == 1);

  return motion_autorearm;
}
 
// set if tempo should be rearmed on detection
void MotionSetAutoRearm (const char* str_string)
{
  String str_command = str_string;

  // check if state in ON
  if (str_command.equals ("1") || str_command.equalsIgnoreCase (D_JSON_MOTION_ON)) Settings.energy_current_calibration = 1;
  else if (str_command.equals ("0") || str_command.equalsIgnoreCase (D_JSON_MOTION_OFF)) Settings.energy_current_calibration = 0;
  motion_updated = true;
}

// enable or disable motion detector (POWER_OFF = disable, POWER_ON = enable)
void MotionEnable (uint32_t state)
{
  if (state == POWER_ON) motion_enabled = true;
  else if (state == POWER_OFF) motion_enabled = false;
  motion_updated = true;
}

// enable or disable motion detector
void MotionEnable (const char* str_string)
{
  String str_command = str_string;

  // check if state in ON
  if (str_command.equals ("1") || str_command.equalsIgnoreCase (D_JSON_MOTION_ON)) motion_enabled = true;
  else if (str_command.equals ("0") || str_command.equalsIgnoreCase (D_JSON_MOTION_OFF)) motion_enabled = false;
  motion_updated = true;
}

// toggle forced state (ON or OFF)
void MotionToggle ()
{
  // toggle motion forced state
  if (motion_active == true) motion_forced = MOTION_FORCE_OFF;
  else motion_forced = MOTION_FORCE_ON;
  motion_updated = true;
}

// force state ON or OFF
void MotionForce (const char* str_string)
{
  String str_command = str_string;

  // check if state is forced ON
  if (str_command.equals ("1") || str_command.equalsIgnoreCase (D_JSON_MOTION_ON))
  {
    motion_forced  = MOTION_FORCE_ON;
    motion_updated = true;
  }

  // else if state is forced OFF
  else if (str_command.equals ("0") || str_command.equalsIgnoreCase (D_JSON_MOTION_OFF))
  {
    motion_forced  = MOTION_FORCE_OFF;
    motion_updated = true;
  } 
}

// get motion status in readable format
void MotionGetStatus (String& str_status)
{
  bool relay_on;

  // if relay is OFF
  MotionRelayActive ();
  if (motion_active == false) str_status = D_JSON_MOTION_OFF;

  // if tempo is forced ON
  else if (motion_forced == MOTION_FORCE_ON) str_status = D_JSON_MOTION_FORCED;

  // if motion is currently detected
  else if ((motion_detected == true) && (motion_autorearm == true)) str_status = D_JSON_MOTION;

  // else, temporisation is runnning
  else str_status = D_JSON_MOTION_TEMPO;
}

// get temporisation left in readable format
void MotionGetTempoLeft (String& str_timeleft)
{
  TIME_T   tempo_dst;
  unsigned long time_tempo, time_now, time_left;
  char     str_number[8];

  // initialise
  str_timeleft = "---";

  // if tempo is forced ON
  if (motion_forced == MOTION_FORCE_ON) str_timeleft = D_MOTION_FORCED;

  // else, if temporisation is runnning,
  else if (motion_time_start != 0)
  {
    // if motion detected and auto-rearm is on, no timer
    if ((motion_detected == true) && (motion_autorearm == true)) str_timeleft = D_MOTION_REARM;

    // else, display timer
    else 
    {
      // get current time and current temporisation (convert from s to ms)
      time_now   = millis ();
      time_tempo = 1000 * MotionGetTempo ();
 
      // if temporisation is not over
      if (time_now - motion_time_start < time_tempo)
      {
        // convert to readable format
        time_left = (motion_time_start + time_tempo - time_now) / 1000;
        BreakTime ((uint32_t) time_left, tempo_dst);
        sprintf (str_number, "%02d", tempo_dst.minute);
        str_timeleft = str_number + String (":");
        sprintf (str_number, "%02d", tempo_dst.second);
        str_timeleft += str_number;
      }
    }
  }
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
  if (motion_autorearm == true) str_json += D_JSON_MOTION_ON;
  else str_json += D_JSON_MOTION_OFF;
  str_json += "\",";

  // motion detection status
  MotionDetected ();
  str_json += "\"" + String (D_JSON_MOTION_DETECTED) + "\":\"";
  if (motion_detected == true) str_json += D_JSON_MOTION_ON;
  else str_json += D_JSON_MOTION_OFF;
  str_json += "\",";

  // light status (relay)
  MotionRelayActive ();
  str_json += "\"" + String (D_JSON_MOTION_LIGHT) + "\":\"";
  if (motion_active == true) str_json += D_JSON_MOTION_ON;
  else str_json += D_JSON_MOTION_OFF;
  str_json += "\",";

  // total temporisation
  time_total = MotionGetTempo ();
  str_json += "\"" + String (D_JSON_MOTION_TEMPO) + "\":" + String (time_total) + ",";

  // motion status (off, timer, forced, ...)
  MotionGetStatus (str_text);
  str_json += "\"" + String (D_JSON_MOTION_STATUS) + "\":\"" + str_text + "\",";

  // tempo left before switch off
  MotionGetTempoLeft (str_text);
  str_json += "\"" + String (D_JSON_MOTION_TIMELEFT) + "\":\"" + str_text + "\"}";

  // save latest motion JSON
  str_motion_json = str_json;

  // generate MQTT message according to append mode
  if (append == true) str_mqtt = mqtt_data + String (",") + str_motion_json;
  else str_mqtt = "{" + str_motion_json + "}";

  // place JSON back to MQTT data and publish it if not in append mode
  snprintf_P (mqtt_data, sizeof(mqtt_data), str_mqtt.c_str ());
  if (append == false) MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));

  // reset need for update
  motion_updated = false;
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
    case CMND_MOTION_TEMPO:  // set detector tempo
      MotionSetTempo (XdrvMailbox.payload);
      break;
    case CMND_MOTION_REARM:  // set auto-rearm flag
      MotionSetAutoRearm (XdrvMailbox.data);
      break;
    case CMND_MOTION_LEVEL:  // set detector level (high or low)
      MotionSetDetectionLevel (XdrvMailbox.payload);
      break;
    case CMND_MOTION_ENABLE:  // enable or disable detector
      MotionEnable (XdrvMailbox.data);
      break;
    case CMND_MOTION_FORCE:  // force detector state 
      MotionForce (XdrvMailbox.data);
      break;
    default:
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
  bool mustbe_active;
  unsigned long time_now, time_tempo;

  // relay must be active by default
  mustbe_active = true;

  // update current status of motion detection and relay
  MotionRelayActive ();
  MotionDetected ();
  MotionGetAutoRearm ();

  // get current time and tempo (convert mn in ms)
  time_now   = millis ();
  time_tempo = 1000 * MotionGetTempo (); 

  // if relay is forced ON : reset timer and keep ON
  if (motion_forced == MOTION_FORCE_ON)
  {
    motion_time_start = 0;
    mustbe_active = true;
  }

  // else, if relay is forced OFF : reset timer and switch OFF
  else if (motion_forced == MOTION_FORCE_OFF)
  {
    motion_time_start = 0;
    mustbe_active = false;
    motion_forced = MOTION_FORCE_NONE;
  }

  // else, if temporisation is reached : switch OFF
  else if ((motion_time_start != 0) && (time_now - motion_time_start >= time_tempo))
  {
    motion_time_start = 0;
    mustbe_active = false;
  }

  // else, if motion enabled and detected : update timer and keep ON
  else if ((motion_enabled == true) && (motion_detected == true))
  {
    // update start time on first detection
    if (motion_time_start == 0) motion_time_start = time_now;

    // or if auto-rearm is set
    else if (motion_autorearm == true) motion_time_start = time_now;
  }

  // else, if tempo not started : relay should be OFF
  else if (motion_time_start == 0)
  {
    mustbe_active = false;
  }

  // check if delay has been reached for JSON update
  if (motion_time_start == 0) motion_time_update = 0;
  else
  {
    // handle timer start
    if (motion_time_update == 0) motion_time_update = time_now;
    
    // if delay is reached, ask for JSON update
    if (time_now - motion_time_update > MOTION_JSON_UPDATE)
    {
      motion_time_update = time_now;
      motion_updated = true;
    }
  }

  // if relay needs to change
  if (mustbe_active != motion_active) MotionSetRelay (mustbe_active);

  // if some important data have been updated, publish JSON
  if (motion_updated == true) MotionShowJSON (false);
}

// update graph data
void MotionEverySecond ()
{
  // check if motion is enabled
  if (motion_graph_enabled == false) motion_graph_enabled = motion_enabled;

  // check if motion is detected
  if (motion_graph_detected == false) motion_graph_detected = motion_detected;

  // check if relay is active
  if (motion_graph_active == false) motion_graph_active = motion_active;

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

// detector main page switch button
void MotionWebMainButton ()
{
  String str_state;

  if (motion_enabled == false) str_state = D_MOTION_ENABLE;
  else str_state = D_MOTION_DISABLE;
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s %s</button></form></p>"), D_PAGE_MOTION_TOGGLE, str_state.c_str (), D_MOTION_MOTION);

  // Motion control page button
  WSContentSend_P (PSTR ("<p><form action='%s' method='get'><button>%s</button></form></p>\n"), D_PAGE_MOTION_GRAPH, D_MOTION_GRAPH);
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

  // get tempo timeleft
  MotionGetTempoLeft (str_time);

  // determine motion detector state
  if (motion_enabled == false) str_motion = D_MOTION_DISABLED;
  else if ( motion_detected == true) str_motion = D_MOTION_ON;
  else str_motion = D_MOTION_OFF;

  // display result
  WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_MOTION_MOTION, str_motion.c_str ());
  WSContentSend_PD (PSTR ("{s}%s{m}%s{e}"), D_MOTION_TEMPO, str_time.c_str ());
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
  if (WebServer->hasArg ("save"))
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

// Display motion icon
void MotionWebDisplayIcon (uint8_t height)
{
  uint8_t nbrItem, index;

  // display img according to height
  WSContentSend_P ("<img height=%d src='data:image/png;base64,", height);

  // loop to display base64 segments
  nbrItem = sizeof (arrIconBase64) / sizeof (char*);
  for (index=0; index<nbrItem; index++) { WSContentSend_P (arrIconBase64[index]); }

  WSContentSend_P ("'/>");
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

// Motion graph web page
void MotionWebPageGraph ()
{
  String str_time;

  // beginning of form without authentification with 5 seconds auto refresh
  WSContentStart_P (D_MOTION, false);
  WSContentSend_P (PSTR ("</script>\n"));
  WSContentSend_P (PSTR ("<meta http-equiv='refresh' content='%d;URL=/%s' />\n"), MOTION_WEB_UPDATE, D_PAGE_MOTION_GRAPH);
  
  // page style
  WSContentSend_P (PSTR ("<style>\n"));

  WSContentSend_P (PSTR ("body {color:white;background-color:#303030;font-family:Arial, Helvetica, sans-serif;}\n"));
  WSContentSend_P (PSTR ("div {width:100%%;margin:auto;padding:3px 0px;text-align:center;vertical-align:middle;}\n"));

  WSContentSend_P (PSTR (".title {font-size:5vh;}\n"));
  WSContentSend_P (PSTR (".info {margin-top:10px; height:60px;}\n"));
  WSContentSend_P (PSTR (".status {color:yellow;}\n"));
  WSContentSend_P (PSTR (".time {font-size:20px;}\n"));
  WSContentSend_P (PSTR (".bold {font-weight:bold;}\n"));

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

  // room name
  WSContentSend_P (PSTR ("<div class='title bold'>%s</div>\n"), SettingsText(SET_FRIENDLYNAME1));

  // if motion active, display motion
  if ((motion_enabled == true) && (motion_detected == true))
  {
    WSContentSend_P (PSTR ("<div class='info'>\n"));
    MotionWebDisplayIcon (50);
    WSContentSend_P (PSTR ("</div>\n"));
  }
  else
  {
    // display tempo timeleft
    MotionGetTempoLeft (str_time);
    WSContentSend_P (PSTR ("<div class='info title status'>%s</div>\n"), str_time.c_str ());
  } 

  // display graph
  WSContentSend_P (PSTR ("<div class='graph'>\n"));
  MotionWebDisplayGraph ();
  WSContentSend_P (PSTR ("</div>\n"));

  // end of page
  WSContentStop ();
}

// JSON public page
void MotionPageJson ()
{
  WSContentBegin(200, CT_HTML);
  WSContentSend_P (PSTR ("{%s}\n"), str_motion_json.c_str ());
  WSContentEnd();
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
      WebServer->on ("/" D_PAGE_MOTION_CONFIG, MotionWebPageConfigure);
      WebServer->on ("/" D_PAGE_MOTION_TOGGLE, MotionWebPageToggle);
      WebServer->on ("/" D_PAGE_MOTION_GRAPH, MotionWebPageGraph);
      WebServer->on ("/" D_PAGE_MOTION_JSON, MotionPageJson);
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
