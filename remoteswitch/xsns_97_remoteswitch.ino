/*
  xsns_97_remoteswitch.ino - Remote switch support for Sonoff
  Allow operation thru push buttons and motion detectors
  
  Copyright (C) 2019  Nicolas Bernaerts

    08/07/2019 - v1.0 - Creation

  Settings are stored using some unused display parameters :
   - Settings.display_model     = Push button enabled
   - Settings.display_mode      = Motion detector enabled
   - Settings.display_refresh   = Debounce duration (sec)
   - Settings.display_size      = Switch duration (sec)
    
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

#ifdef USE_REMOTESWITCH

/*********************************************************************************************\
 * Universal Remote switch
\*********************************************************************************************/

#define XSNS_97                               97

#define D_PAGE_REMOTESWITCH                   "switch"
#define D_CMND_REMOTESWITCH_BUTTON            "button"
#define D_CMND_REMOTESWITCH_MOTION            "motion"
#define D_CMND_REMOTESWITCH_DEBOUNCE          "debounce"
#define D_CMND_REMOTESWITCH_DURATION          "duration"
#define D_CMND_REMOTESWITCH_SLOT0             "s0"
#define D_CMND_REMOTESWITCH_SLOT0_START_HOUR  "s0srthr"
#define D_CMND_REMOTESWITCH_SLOT0_START_MIN   "s0srtmn"
#define D_CMND_REMOTESWITCH_SLOT0_STOP_HOUR   "s0stphr"
#define D_CMND_REMOTESWITCH_SLOT0_STOP_MIN    "s0stpmn"
#define D_CMND_REMOTESWITCH_SLOT1             "s1"
#define D_CMND_REMOTESWITCH_SLOT1_START_HOUR  "s1srthr"
#define D_CMND_REMOTESWITCH_SLOT1_START_MIN   "s1srtmn"
#define D_CMND_REMOTESWITCH_SLOT1_STOP_HOUR   "s1stphr"
#define D_CMND_REMOTESWITCH_SLOT1_STOP_MIN    "s1stpmn"

#define D_JSON_REMOTESWITCH                   "RemoteSwitch"
#define D_JSON_REMOTESWITCH_ENABLED           "Enabled"
#define D_JSON_REMOTESWITCH_STATE             "State"
#define D_JSON_REMOTESWITCH_RELAY             "Relay"
#define D_JSON_REMOTESWITCH_BUTTON            "PushButton"
#define D_JSON_REMOTESWITCH_MOTION            "MotionDetector"
#define D_JSON_REMOTESWITCH_DEBOUNCE          "Debounce"
#define D_JSON_REMOTESWITCH_DURATION          "Duration"
#define D_JSON_REMOTESWITCH_SLOT              "Slot"


#define REMOTESWITCH_LABEL_BUFFER_SIZE        16
#define REMOTESWITCH_MESSAGE_BUFFER_SIZE      64

#define REMOTESWITCH_DEFAULT_DURATION         30         // 30 seconds
#define REMOTESWITCH_DEFAULT_DEBOUNCE         2          // 2 seconds

// push button icon coded in base64
const char strButton0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAADEAAAAgCAMAAACMyGAQAAAdoXpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjarZvnlSQ5doX/wwqaAC3MgTyHHtB8fhcRWS2qZ2aX3K7dzurMyAjgiSsAjNn/89/H/Bd/SgzRxFRqbjlb/sQWm+/8Uu3z53l1Nt6/75/wvvLvX943/n3fet4KuvL5Z97v9Z33048vlPi+P35935T53qe+N/o8MXyNwFk97b2uvjcK/nnfvf827f1ejz9N5/3/3PcW1r03/f3fsRCMlXgzeON34H3+vk8JjCC00Hkt9+/q9Y7j98C/ekih/Dl25uvX34LXy59jZ/t7Rfg1FMbm94L8W4ze9136c+xuhH4ekfv86n/94ATy8vOfn2J3zqrn7Gd2PWYilc07Kfve4v7Ghdwkhvu1zE/h/4nfy/1p/FSmOMnYIpuDn2lcc55oHxfdct0dt+/rdJMhRr994dX76cN9r4bim583KVE/7vhCepYJlVxNshZ423+Nxd3ntvu86SpPXo4rveNmjm98+zF/evP/8vN1o3NUus4pmKTePQn2KkCGoczpb64iIe68MU03vvfHfKX1xx8lNpDBdMNcmWC347nFSO5HbYWb58B1yUZjn9ZwZb03IEQ8OzEYKjo6m11ILjtbvC/OEcdKfjoj9yH6QQZcSn45c8hNCJnk0A08m+8Ud6/1yT9vAy0kIoVM21Q1EMmKMVE/JVZq";
const char strButton1[] PROGMEM = "qKeQokkp5VRSTS31HHLMKedcsjCql1BiSSWXUmpppddQY00111JrbbU33wIQllpuxbTaWuudh3Zu3fl254rehx9hxJFGHmXU0UaflM+MM808y6yzzb78Cov2X3kVs+pqq2+3KaUdd9p5l1132/1QayeceNLJp5x62ulfWXuz+mvW3G+Z+/usuTdryli815UfWePtUj63cIKTpJyRMR8dGS/KAAXtlTNbXYxemVPObPM0RfJkzSUlZzlljAzG7Xw67it3PzL3t3kzKf5befN/lTmj1P0nMmeUujdz3/P2h6ytfhkl3ASpCxVTGw7Atu2u3dd+Sj1U/f1d9PTvvZq/ueDEscKYvnU/Dx3T04wtp9FT86u51cZcxbUMObfA1LrjneAIUF5r5FLnASZWSEWvrs+wB63OBAWBu+sJLh67crOp2pCbrjPNCWRy32mUGk+MLZS8h5/wnS7wK+nzVhb6wO1JxltLLu22tm/Htl1bJ6cm5p0PsZ9jjtFiqrOP0/pmzCXZUsGOZVuHVLioLYrFpeWcr6SRgSoOfkQf/y5GfiSqaPlcxhhnUJSZv9aebufoePRw1ee5Vs0+mVqiq5O0l9rjoo4YfUtz9r7HiPnYvk4afFKY7/GLasp19TAoO4JbuG3NMweju4S1RwkpL0uOeNnxNE9iCtEi8j0Np3IikIXbTyRVsxE0a2ukTUPnOgNCy/Vqa9nRTesawTyOAe0+SM+y9PxKfMctopyp9lN3";
const char strButton2[] PROGMEM = "iLMOanrZtE4g2nXsZc5eqYTV56m9z5Lm8r2skBchoh8piTFSJ8YWccI0dkHY7RqWV9Psk2mIkk8zw+fNxCcoQv867hMT4O1g/hlg3BTqdo3OZQ67r7nrifPYsGlIujBQUPHk2g1BhdWp0+rQURQK48mjHaqBbPmx2ubevudYfSTMKkefIn0VjufGjGwSnW6Ajz5ismt3yjLNISpCi1S3OzOmAlIa8PWmD13h9uVo7qBUAjxqLMGBOSUjRkdb1Z4xQLLte6P/HcjEt9uE0kptpMklzwROWAM4aszVA3jBu0wcenGk0iymPxpd47gc2P16BdnqYVATMCKaeeV5nA+MO0fP/yJg4gFhy/CoQOMBS4pMf2jtONyoZbjSUpmzxUjSQqaaglvE3rq3dPdpJXamHIApICp6Y9cKZfazU2hMiwKiv6ilRnGuDJ6P4ndVvVaEqF9xlO4pWf7OFS6vqXhqp5hYnQoaiGitAP7FI8pOrSX3CQfAAzHR3ZT53ID3CaHsgL4s4PuibfQhrNDMAh8XGM9sCjyUOkGzQ7Xokx0EwdpGx1hAB8ZCtp0dQOPVa+6AdO987+TZzJlL0DyA6wSmgx4brdgtON9S2ACRtFybp6VEtXlan763dU44Q+iuL9k+zBoHpIm1TG6fQK25xtolqIMEbDMlvZfaSCvQEYdCWpVYiisyRFdaALqWoV9am9Rp5370YQd8QjgZAkUxje3tZpLFHep8nTO2C0dt2Veg";
const char strButton3[] PROGMEM = "f0ucRHav6s0+YDRdpqdTj0MCeoLHUCq/tHEeBC8j6LUmypv4rq1RnZGBkpnGPtNskKCTMuBOlEAwqUdAmYstLRInNRQXLQG+FRSM52d54kVpzwSgPGagGcEC0FUgiVUOdcHdYFYKknom2NfpxYwQX5PGl9hRK4+6QqedAQQQDCQxkKI4GUqfaL8D3O8Klm3m6LjsFEClQAanQK7kcw4QsOC3OsRFPIEu6QsKcpXikxJEsuCYnHwSOIBnkW6kMR3SYKtuJ+EG5gvJpdKz67Qz2L8Zdlsmxz0gQcJUiE8GOVeaPQ/AHQWKUhgdpTInnFRBgw3ohLoWpuFwae36GD6LhlaioEbudb4kVXCZ9zdJ99Ynd1b9wAkL5OgeGIH+1HTosbptLiBsNyPQfA1siwwiEr2D4hOjAx7ugH99LFfiBtpW4AaZyFaAkChbJBTxG8GRK8PAaKoWxaitqMtbil1iiYaUQ80LN7YpxK0MFWm9CO5jLkZmehAJ9XWqafQH1Z95M95aBp46hUQL+AoGofFw0qcNeavICCATJgvNqVjSAdqrVJmpk84rZD8lblQPIE2YuApESCHMJmpe7SACMqISaQGQe75VEhB+SJQwGqFV4gi9AvTAP7iorFTR0942jMvBMA4BhsyJVyYegF3MfkfyDiynShlCneYUktgWRBf2lvZIeLxxQAicgSZHTxfAo1GHcIZwFlkDrgPmnYdPUDNpauEOBpQC1SAalA3lMmuy";
const char strButton4[] PROGMEM = "TKs3+IE0jiulbAXVz4mAT9e/FX9anBT2eKQhu02JNlvHnoRMBdFin3NUXnk2gaooOqqG0DUYhHrv3kobDyS3+BsODdtkSYalDqXV11nIiBwqZng3UGBNoM4DhoiJMkB/qlgee4BDDRyJbaDwkH8bzCYJqDxkZAAkSXcnmrxRYGabDwPKFFWhMNFEg1ihanhqSIMAFEkKiKNGuj8Bw2sibsgKFsF9+gMdjv1eGIizYWLUGlUG2UGv6roV0JnUy2bywJYZG5ZDhlMD2PFJ75J4ev8wYaBkUkIN85IdMAF2tz1WF+cjXJBkJQzYNwCEZri4AyQhAo+HKofpUSidehgI/oWeyRNBiN8/mJOBZAGbxWuAVDp9jE1X7GlgoyVpM+w+LuTebEbPeTdoHdSfj7H2hDbn1WK7arS6V6ODUFYCi574beD7Xd78CduBAZlEVirJgtQuZtgURgPJaEp/SWD6MBgrESswZTlq89nQ0sOQgcLU5IdQLXUjphB6zdnthBNwJPF4OAa2ioOvc4NDhG6RAlcUHiLToM8BXTje6ZaULB+TdFTnPnaCqEGOQTwNg+GiJj4twCwnR8QdpY9WOQHlT3eSHUQEj86kKRCfhfIExlCmFDwI9TwboTuBMLB1aS2GzkcsM0ye38o2QM3y0PN9j/oQ9w+07Z5wGDYNwVRCR1PiJk+hv+LY6CKIEGGHVcrYCCyiM/hQTNEReyFaChNmKofHiZOThfMjEC+iAJ1p";
const char strButton5[] PROGMEM = "IxqHEgVVEaSRuyCDYEWC3RcjwEsTbQiYGZ6mzwHpmDMCHikNNdNi4C9zERGRG0/JcCloOqHukGI1mKOE5HCYK9yjsM0H3BJd3AsEIKOKSPBYPg+2EBeRL5gsdKTWh67cdhSzPByRmwCJFC03qRNcim2QVJXQSFtuEXE8nKOdd0YkNUw9XbORMij7s6yfpsHLG0KEgFGGDrPvghY1vaoXUYswsTR52z41pgCPRJzzonA3LIoaUTEQMwPi4AxdU2jEtZDQDJIq0G7DrDNOO5udgnLGS7/AbVwKn8ACuHsP45NyE+lZCtEhamhBXypDJaexVn+LEwUwGxVX1AoQHDOqFStXSRa61MEWKL21zTjgWRk2ugA3IPHoqlnRHQHsQ/vvs0S6kIu0H2wGtGZxTII9ZKepB0+1GVfktrFqyD+M8akrBxpkIuJRf2gLSnDJG1TuiEumsJDhUMABVeAzmWp8ocA/0xlJdJuYE31JQROcMCcfURQEDkXtOxZtwjN+X5KD4IuduMIoMxU7+sgH6zPeGZ0lhAX3gH9HUKc0YERPuqXVxeuJz96CzgPCeu5PjtDEoNAcBtO4EwEbatCmH8zXFtqMStNvig5UOuAh7bPE1Ugt2ungnAeyhzYIRLYb+JKYYM6eHm/0KaCPhKXAL6SEWM706ng64IJbvfpjYRgQ/VjW9KxEyEcLg5frARWkJVI0tRQ1cU94a+rbc/0Qqxxk9sZ3V67A2GjCWtHQjbSI";
const char strButton6[] PROGMEM = "fDb9QRaeRYz7KGTYfdR9Ds//6TmM6T5JpF2Xex5lzcEF7SZfWh6GR0DjCSVNMCuoLNh55ExnKkugMl52rFQbEeVBcV2j4bDrQmM+QXiCAD9Ny2tMWWPiV5QBsS3SF2h1bE7xqHguAL8zYmoFw6VUtm4HoH3ezXcCertjLw5Tep5BwW7saXyfwfC/noJdR4BCUlMGNEjVH2ZKKOEdALv//pZ8CSYNrGlilKt0lCrzxPDXB+KxF8TqwG3qaV41RFuhhfbNUNr1efu+iXDat0VO2oD798GiJ2O8F8rcfGUJK4gu/zYB8zXcIkWEVwP4KfJpwxM37IKyTq4gpnOfgLefE4MXP4/UMoF55yDzVirOC56i7/B5KA8gBiRq6DgUPKQJ1KMi3a0s9CTTW4xH8ToBMcrjqVrGhx5dE+CnejCLC5gMaISlhZWI7PJRWxpqdjQlg2qQU3+YUsBGvjDHQL5nNChEbhiYoFYfGMa0WqayCQ3QInKrNbkd9S7jdDPs9qwNrobyb1qhQuogChMKIYpwRQPygTC3VBOjxk9Y2orZbAQWFV/XGWPYoLUkEM1gwRCBcm3JRuS/Aya0VnI3kqAyXJqrs+SOogabLEakA8aTBGhxKIoaKBKl31KSMFKYKCiH18TKexQjIxZz24NvwlRpTOGGGS8hfJM3pvWyFUe6YxJcvH0ed0GJOfLE7bTigq+FI4nUBKoCdMUE5oVVrDQWfAhnQfllj6DYLFUV0q8D";
const char strButton7[] PROGMEM = "ikSy3RLGxtCeYBwDRB851SMBcrJQS+ndJIhIUgGAoxa5oOwE9/Ql2sPNHW0fIguylq5mxJCTWugE5UFm59NXJAelWDB2GiGRR41tgzI/E7Z7Op0uwbzSBTPxyEPqSKEslwhInqOW2zTe7fYBIKKJtjdd6466F0O4JaFb0VBUFr1ICcEA1BMKAm0J4gUekQBfrKUN4prltQhWjQhQv6G0Wu0BQYZhhaDgptS1o6vrYJCFrvBUKfqMqZa1m0UY+UYKBZjOZA170lQrogeQtYLSO+x6rFYoKKRMyWAQ5FGoph/XO9l7CACQ3gZOlH70uG1KoDGBgdqB2Aqq/r3aY6huZB4icJrEYkBJdmdZrcsdAybzldpFHoKP91kICRxGgyHgoUPVtXOvfEcqgVzuzcN7c8PdS/DvkGTzmMMFOKVnvLQT6NrUQVclMFzmH2iGjPLA9M6DFDbori7zaEfiSQAlvVAWToEgokgPOqpJlFNNiN9Sbk3ToLLXCEA0BY0S8WtatQMwXG8PHSdqGd4HZQ4wROvQ34ik+xltpmrrt9poSPl9oAfonNEAiBaUw9IQo4ShYlzVwwYY8qE1giDldNLqFGOGcbCaiHdgDQDqqHmhT/DddAoLzvTahE8R/RrxkjOGRHdrs4iHQHAw4ozPrgRSTt7a3hF7SUAEPsHO+Ppwjb4/lGx1CQzIWnS+q6c03fR0vNwcH615mxdVM7VBcQTwdAViAd8/xtM9/ceTPDVF";
const char strButton8[] PROGMEM = "BlEfWX4gRkZR70a2nM/tZ0AwqJ/R9zZiIQ2RtXwPS6eNMJmURHP96ORLSZfxbjfTaknaya9bJJe11M7J/HU//2M74yvtTnAmYfUGLESwfK1g/fnVwaAZci+17LtWlbWpiyrF4iO9aQCHYkNyCm4JVtK6AO5lbWBbljJ4B7DP2ZG4dWvl3mPOKBwUdosIdZq5wiu2LqMd+5NEk6iy/vRNuBKjRlj9YHQBVsQFlX8E7lViIraneToAcddVpinCY32Qf/0AmxlRS1hPUSZF6NZT+xRrf/QThnJIlPGFuzQmhWkbFlZyR8lYJ99tWS258o3m501HBPQYMqE+V+jGwVfUKhtFcgwz12YIdGY35bjG9TVxCswwWihUbvwuCdOpWovfd9HC7tyoUWhkYhSmllirFoMaRq36ju7BlhzrAWlacYUKskgcl1StxoyFo/zqgni0/VpApwyQejQkmaFUuEUR52J8MUfYIlu0LwKdLnAs4UGOl40/uF+6B8kGisPEGBoJ7mnyt7cy+KTViK5l37RkndG4KCHsNiiUF+3FfQv4MlpXVrCDiAjy4ipulrxIdMTrXADd5mRYwrd3MOwTOJkV02Ln5WamG8ma1Ct41gTYV7sqCzpygLerS2dCGEgumFraFbhEOEBoMEENOteDnIsMC+7H0wAMiK4xthR0DzT2BFiH9IOWmabWYeAq32qZmHn4jjLJaQtLO64CHDFxad3LaQkIrL9+CEURLZ4BKVcm";
const char strButton9[] PROGMEM = "lK19X20iYqIxZU5Lgwm0pjywH6Jff0oyrSZJuiXHlgISD6xMTYsCggMgOOGcM9EoyYNzuY0tSlnMTfstDHTI9WIhtjZpMfNH2/9atPeWosmLeHQt+FoSc0+BaPlMEhQVuSmLAjBq4YaxeUrQIKUCo7hr5qNqzXwSM6F8xg+i4oH4oQW+TgbmAhblOUjy3lpao/C0MbKDLISWzmAAHHS9Dx3al5GPawtxkzHKIyCxeQ6+SKuaaGNm7iXv7K5WWlX6CFeAdKU1ACJa1WWXp1ZXeJT2h+9ucQPkCglcMLWjDDo1HvH0DEU2l5ub3kbqpFgPdxtPK93iGk/V6Qe6ZR1BHopjh1z1zJnkJz1Vnwoq7q6nYUU7YxXQ+RQJAHwQuiijY+5lnpMlTdxXe0k2UqxJK3tOWzI6f4HwkjNYYZmpnVkkbQ4FkA1OS76ubjBSHhUKhCerjkfsWuIjmXcMK7xWSK1QEWfNbM/Immv58vvWESNKZOEqwn2n2XMtlswY+NYdnxW0DrfLSQh50aVamjb46w2j7zrqglqENzB/zGa+slAH8f78CkUk5cU2Q861N3FXdFT/WxvYYFWB1XGTsvfaM3N3OxgFDZ1L0uQdgUToaiwGjCUx6c6WiIDS2uTbopBVPzLn4sHnVrRoALS0cFNH0EYGLBzuflx87Hp53Lv3t5OTX0gs6MT1Rew8tnvqjMrEH00AdGT7WMGIGcr+rvMh/egI8ryLdi4UpboGDKVV";
const char strButtonA[] PROGMEM = "L5pFJAveFqk0IEuE3jNZdqhBqhtJFLUQg68xWDdLFkq6C5h+yUKo08JdS89WO19ZmJjvvlglOsAAjJZpBRT1ypvKqc1QOjznlKgatCGX9PoNsII3aBLE/SRDiCodPUnTC7Cd/AlBAeC18+/R2cLllp59UwHZlFRDkQRt63QXtk5rpHugQsKtFG3Aa//dCnVTFOZOYOSufrtJkVJn9AmjDFtAOrHHB5oGDIBgFDDYO+gPxDbwikn00qLaVeapvRmaCV8sew1zU46yTKkBfICd9jjg5gF9ODQaTYeVs2U1VDjal+5aW4KZJxQd98g6QFS4MXCMRz3zLklPZB2PWg4/WQYQPQillp8RKfB5dlYapxFKrT57QzzBpazl57aJDzCa7LjHl8A5Ute0bVooIQAXLKMWIvWPicBr4kpwyR3IXkbLPI4BDilIDATmkgqiEzUjaWVtkQWM7AZsmwtOxwwoWJ1VASsRLgSL2DEiz/AgaZ0gAPC0jBSwCAFlNr0M1CWN+pAGWm3r6AKRn5pWRsW4YtEwxqNgdr7785hZFEzfkiEMUeepeo3FA8QMwsaoXSkcDeRLuBLsgvMZ2lbMIRrUA2Gv7aBl5aJpghy1J5r0HO91XMRmnzKTtXjcqdWWKajpe9SpQ2dN2sGgELXj+JfI968Cn/kn5Btw4ot8UseTgc0sn+hgW2roczzEfDsvol01nWSiLRvMlbQrhcRjiktbxlocAgF7g2OQSdc/bpyM";
const char strButtonB[] PROGMEM = "+RwYmHtDsrl81l5jRpWgoqVDcX7COZ1kuKIYSfIcBEvP8mvrvRiJokf44iPXT6cg4GWKB++r3X1wVShZ40HRE7PznkjQOsr1ufg1OiYjW4dE0mr+rvihJmPW/kF0tbqvfNHt4BnV0qIlcZniyNjhKkuAYJfb1fneEeHGSFx2BvYyMd0bsUQz6vQV/ZaAEeCFssDxNrs8TgRuJ1YFERGwNwgrf7qW77meXwm2zrIN7Vbh3edUaHgWqq7g37DOkDKkA4/ndLzuk41EGIAz2l3iSV1HFChWJAI1pD0pfCo1nEEFzAl6X8dVPKgjLScyF85Ef4/DYls9KhH0L27X/dgHr+XEfaI2H7xH0Dn1afCAro+RhMD7OgOKDoDskceyd2M3PDSKpmglY9wDoqTuLrjuuy7l5fYsIIDUmHj983x4F1xdSMmTfpvQhU7nBcO6p8n6OyYb0EY9P4V3jUv9HFPphAqn0BN9O6fjr2O6f3ZkVFxcGk4RauiEpxYkgP26SV4hUBEixn9qZXHiwbWXgnBaIrIak+kr6twgOL3SkiS3XsJ2Qr8XesasA8ikgYGzznCzjrP53NQvYBgsElX5qJGs/2xBBwYgGuQaJel16+42AL359syCAZ3fYHjAl44QMG8KdVwVFYQpZleBSmFYWs8bbzfP+Bu2fEMWAOTBlosskn4/wOWBlrvw3W/XfccWnY7/oEv5oEvPUJMJ8ihi5K3TaqvWj3lvtX47I9mgDB0W";
const char strButtonC[] PROGMEM = "sQFe6pIM1AIBSzkYgk9N44Pde5CP+stqM1x41MqaqllBPJIStui8UxMO5CMv3h58nNXo4FJQkKxsXsN/ZHRyHA6OQ0CDZhouqeIe2h3dF4GYjlyZfaYLmBTzoElXhrBsoBDuK2lJ1D3r6M+Dpeni1XS3JJ/9pHCt/jxdos7E99DrWM8+jY53PItwfr8w+qnn+txaj3q2Gdxddpj+AX+n/uuDQsOx9/Ac7Fi7PWt6Wnm5J2aJAyUDdDhkf8Or5Ksvp47GoC+HYYKYHj8xZO1uH71P0AO2jsX0glBs92xOju/JnDvbTrlnaqDqVMEkawmFuSDjLf8wUsQUoZAwgyVjOgHZk6ZFGINEoGePEoFOdCJupNblsqY3t0Sy/6eFn6+jG/LyFBAhPuBa0v4nZbSr0TIDMKOZDEIwAe1Sq1DbalfuLFRvx5gummTBQ+C8FqZKcXOCZIxV//WGdvwwlwnBVje2KhX6Uvesm8LrU2txTMKSsFGsW0ksQcUhOTvlOn3Sia4mw2Po1uR2RPIVr71Q2oSQ13LXA0DmpH1G+QLlDYH9nEYA3QWmzODzuXkv0FEuKInhrqZDxbO2uwJ+T071fZPM/Yd7KnndG9dnzQhy6c+NghZAtaI1PitaZ+GadMCQkEFcTudDqE8dpvmMtD7s/Bmr+Qz2eaha4T72PlSHuHgoDoGbOhRmes+DaO3yHclnIObXkVyWGutpirI9cdgqYK2uVZ6OYq7FR80d20se";
const char strButtonD[] PROGMEM = "fYlhKgLmH8fz9Wj9J0RPDLRVOF4qiuOJAkxL9kA7XHnQ6To+2c/aeRlaVezn90n8eQ7mLyaxGH49DQb27ecb05ctrPL9Eeb7M/SLCiU/Cfe/JdyhjsuaLurorJQL/YcdMsrA1MJ8TmUheIAA0AsZC2ZzF0TEmZinELTdt9HGSEAqjE4LSgqj1+kRRx39FNjptficqjROyr5q7WM6dN6kpULXmjLcVzwOIdZGqyadJWASDukHmdGjtNbdy886lKpTZmizOdFxFcwMGVTUoak+m0AAXTf8QOCv9EWDzvyDsf/99a55kShZEPssylZBthOL/OiakO4yPHbh2ZkBNj/NGmEaPILgrASltPx6O/Pc71+/nTrFYf1+v5s5n7HVn2v5l3s9d/r7+zxn/f4TozL3dv+BUZlfY/9/H5X5muT/Z1S0oAFjvf7DJyyxuF0HNR6lCujqJKfKNNAqIT9HO59dxm+e6ilI+4cP7it35anW/C9soBG9xhB5VAAAAMBQTFRFAAAAKEeBKUiDMEh+NUyDQFKERlSBTVqIR1yJU2COTmKPUWKKVmaPWmuTYW6RaHWZbniXcXiRbnugdX+deoSjdIajeoiggIegfoinhI6thY+pjpKnjpawlJitj5uulpqpmZ6znKC1m6K9maW4n6W0oaW7paq/qau7o628pq21rLXDtbfHure7tLm7ubjDtLrDs7zKv77IucDIv8PGv8TUv8bPycbLw8jKv8nXxczU";
const char strButtonE[] PROGMEM = "z8zRyc7R1M/O1dLW0NXYz9XeO6BpIQAAAAF0Uk5TAEDm2GYAAAABYktHRACIBR1IAAAACXBIWXMAAA7DAAAOwwHHb6hkAAAAB3RJTUUH4wcICR46NgzqkQAAActJREFUOMvNlNl6gjAQRk0CLmiVNihu6BcVpYiiuFWp+v5v1SxAWUJ704v+FyEkc5KZySSVyr+Qb1sWcR/SuYfv54fuBI+c3c4l5jAsAhtdQe1rdggv4+7emOfsw2kNAgBq69TYFqcXsCYZYPuqAC6VJGMH/BSLnYRH1vdU5Tatg1ionyx65J+eOTaxy3rmR+JutIHa7jCNJUmxhrQN+nEE8Qb4Eoa3W3iXpXLBojC5mwedbwAhREqzJVNb9yPrlU07n7pYH0IehBBMCTF3ac5WM+r+gBJeXVhCgUgFAQ1tS60PLBCvikB6ScEhxNuESRObPEHNIHcL0JbuzX5Vmtb3maAoAbMEBIqmNZua1tC0KHSdFsKDRe7M5UTnfOC6pLNMSJxdCaG+RNKPsf15xI51z0+wQKSTZPDaMEeiSu7m8TcCiQJ/XEVFTsVv0SsVG0a3axhGz85crklUb0Wifb5x8ekgubwJnycAUBpNrhZb08Gjhbt2LHOQXK4cIYoC8APUxC5LQmw3fZWLRBQ2epM/Ll7eK3VocZFlpYzI1hWor3ymoPQB86oAZMoQqUKdnwiAMpUbHcuzhLjrCCKYF0IKLn9YA6lOf/ZwfwFCkDB2e5fl";
const char strButtonF[] PROGMEM = "1QAAAABJRU5ErkJggg==";
const char *const arrButtonBase64[] PROGMEM = {strButton0, strButton1, strButton2, strButton3, strButton4, strButton5, strButton6, strButton7, strButton8, strButton9, strButtonA, strButtonB, strButtonC, strButtonD, strButtonE, strButtonF};

// motion icon coded in base64
const char strMotion0[] PROGMEM = "iVBORw0KGgoAAAANSUhEUgAAADIAAAAgCAMAAABn/9sTAAAe0XpUWHRSYXcgcHJvZmlsZSB0eXBlIGV4aWYAAHjatZppkhw5coX/4xQ6AuDYj4PVTDfQ8fU9ZDbJ5nTPaGRSs8mqysqMQPjyFgfc+a//vO4/+K9a8y7l2kovxfNf6qnb4JvmP/99vgaf3r/vv2jf34U/v+7+eN0bL0W98/NjOd/3D17PPz9Q0/f1+efXXV3f67Tvhb6/+OOCUXfW3b7va+nHyt7r4fuz69/PjfTL43z/rvMu4cP3or//nCrB2JkXozk7kdf5990lsoLY4+Brff820yuB7+P7N8T217FzP779LXgj/nXs/Pi+I/45FM6X7xvKbzH6vh7yX8fuRejXFYU/vrU//yIP8vLrf7/E7t7d7j2fpxupEKnivg/lv5d43/FGLpLi+1jhT+Vv5vv6/nT+NB5xkbFNNid/lgs9GNG+IYUdRrjhvK8rLJaY7Fjlq9my+F5rsVq39ZKS9Cdcq6Rnu9jI1SJrkZftx1rCu29/91uhcecdeKcFLhb4xD/8cX/14v/mz48L3avSDUHBHPHFin9NBcgylDn9y7tISLjfmOYX3/fH/Ujrz/+U2EgG8wtz4wGHn59LzBx+1lZ8eY68L/vk/Kc1Qt3fCxAi7p1ZDBWdgi8h5lCCr2Y1BOLYyM9g5RaTTTIQcrYd3CU3MRaSQzdwbz5Tw3uvZfu8DLSQiBwLbdPUQCQr";
const char strMotion1[] PROGMEM = "pUz91NSooZFjTi7nXHLNLfc8Siyp5FJKLcKoUWNNNddSa22119FiSy230mprrbfRrUcgLPfSq+ut9z4GNx1cevDpwTvGmDbjTDPPMutss8+xKJ+VVl5l1dVWX2Pbjpv232VXt9vue5xwKKWTTj7l1NNOP+NSazfedPMtt952+x0/svbN6p+zFn7L3D/PWvhmTRlL7331Z9Z4udY/LhEEJ1k5I2OWAhmvygAFbcqZbyElU+aUM9+NpshG1kJWcnZQxshgOsHyDT9y9zNz/zRvLqd/K2/2d5lzSt3/ReacUvfN3D/m7S+ytsdjlE83qgsVUx8vwHZCGYUblzrX3M12iTbCKT0UgrhpHas+xts35V1tDZurldLsltBK9hNC6MEVEqpIlr4OkVzEeNza5wSlTspnl7tiqHP428Mto5e97zp7z7oGiQhtFZDNu7jLNM9zw1Y95LHHPjPzwfE+2GcPkTDPWU4gTTG2m8LZlAdAev3pnavHQWWv2+tJt/dTKnQ3a5jcYd20ah0LPuuWiDMFMvjtydGTCes7rGppUSsxkdTotu6T+thtrcIPSnsfLZxO0Vg9WtBtlMjgTXX6A+Rn/o+CGn3nP1/d9xsbQ5JkDN95cBX56pHfddRKnieEbTfxiGf3K3zsoE/1J/IUBICCciv2O/ek2tt6b6EeEk9xG9C3wH/LnYfv70rvIruPGTpqaOU91uIpYqGO1o088hzlhrzbadQMby7pnsXy4zn7";
const char strMotion2[] PROGMEM = "nnI7zN9ruLWGTq3FuemmPjJx9ntQXMNl6G0kcunLjZ12873F7Q95PaDrTG1tOmBxGYKXjbs2miB9vpEcGjMSUXfrpAEtt7EOqESSVhyrpBZvO0Z6yj6l8dYKXCfKgLVne/XJc4TdeejUqCNuOTtKJ68KyO+yyNMoe+WTQWpVVrqARzUuQaJBokWM26XJ+po987mgrnaFzJexfW1EZcekN1hqpbaRAg2x0Xo2OiJvHBDHuB9PuQnSXHfmvGMp22J2rH1RtOqRYnxHUC1XdNwN3VP2rM5qjnNtG0eVlPskYIUgEQ7AibdVUu4o8HIGoW+gR812BuqAlQAVHoTw7UoVEnk6bhXAgGrv/JJFDRZY1yJpaV2n2BtNQeXvNYtW3NEqCCBKL/a2d63n3rwuEDR3rCgREO+Onuh/WLC0S+02J2FJs9g8cxPBlLnjyoiSTTgK9X4g3SY49kKIQal5LliowsH6aSlaPI3jgMBEsIjbfJW97+1krFCHfm9hy1518F6e+8RxZzxt8zeyYhpizVnLjDe6uy6A4ueMFaSZFA810rbk1AQsO8hIwG7hV+Nsy7yNGgI1yUObEAifpMGKK0U40Vc3wHvCFLWSx7AW1bga1aC1ekpjNrOdLxflWoM+MCBqq4rb4WEcAR85z+sLV3oPp6faPs9MdHVtfgjpqgNoa9CNYuyquDn65hkW3EeHuiuA5Xko2iwOeoKcahILDqgAME0enZDXnq2Shokqvayx";
const char strMotion3[] PROGMEM = "6qadnkbiEgnUyG7yRzHxNdEf1FImAZTdMUhvteyBTR8pNzqZtQAC4FfqJBEhRMXYbAK2HPKE/R/s+Z9f86bb9wl3A05DiSmDRy00Ijp2US2T9dDj587KtZwITRRU+6bWDoi+IAOKTOCdqGbqcee+KQijmA6PeTOJzdDivDNVVTa47yqMCChP4ONEyhB0OXBASCzepvGY/CdlDZeQ55NVWVrOTnflSr2Tp7bNTZCQddcIDfb5WPb0zGVLnEdc1UCPg+7l6oN40hxCht5TACJLNhIhhe4e0O2GJXiYR1DrjvA794K5YNp4Vp+32LwLiCuPnsRVcwMSfHtlBlt1BIHuXO29EIDofSEiSmtNXRRxNkOqNFa5fmVk96hAeMvT6Osq+sv81m+386i0CTKlVu4BE4CsFBAPskOnccBn1gOcx2ukBFty6JIAHIB0wB5ctW5ojqvQsgNTSV+VQB64YeJpqKPd2+JhQ9s8rw0KgHLOQV2wemjIg7oQEmmCxS7f6CdCC8wT6PD/iXmlDTwAO5CvpQQ+htMoyJmHEbySO/TNU1xymRoicUYXdlMv08pUOz1nFaXAykpdIBFAdhS9w8+zxH0Gqi8CM3ei1igQBJis/YZpaZ9wAIsFaPtGugZdBcQPnxP6E/KRmuQZUTnIEDI/FQeYqJw2aIOK+LHuEkm501BtMBjI9FkBBHiv2v3EGwbRnZlQQJAkLiMwTSRQF9VEr3GHEt2g/meQhNqshVrY";
const char strMotion4[] PROGMEM = "0NiBXwgX5hFEoKNArh15Q4otn8UfBGD3G+A19QAaOThgi2XT0CSTLFTkZMn80/NOB8jNsv4AdaIJIZicQ0Vvk75OMOEpyBJMmcHxTFFSD3xBfC2ehlhfgS2eCZkByO6EyBKZAI4nA2zUUUR5Jf95cumw42hJgsuTAXi0YwfRTwGEUC6HtV6Il+6aKJuBckb2tk3ZwIgkGQrjLRmXWwC2CQyiyymejU9HUoE3UIQJPdCQkp6UieC5TrB4H3434AS0oGAEi9ILWsjRi+MCwHwCMmwLU8jLacyO7ha3cDWueyPFXivXMVQCcDIznga5JJ+ADvMOpixyEWAPZUyWAYlDdB8JBnIbM/SGqgeqeORZQUXggYrhva34K1mwd3GA6YfQUMgRqQOn1DiznvTo72qIWkKBfkBZ7BGGRHvytD3BQ7ZiL5qVgRqB5IR9QnABeDIvKZhE84ZcpKTkTsAKo7UR2Kgag59bQ5xmbg77cAGH30DXVCAW94KFuNQZMpGqGJFmRuhAkfZBRii4EhxsFnK+5KFGATVPqyHCIkVOhfa5B9SFCM9jLPRE/fVl798veFnvRhMhl06h8nG0YhaH4ifjk8zRm3gUAg69eDL0BBlFC9mRtt3mPSCgLslaDEUMvGE/EB8oOwoy0CyDQGx+7lLbiJQmyzChYtyCqLnhDsiudA6caWfxbJGQH63cS41nB6DtCItNmc9HIhmhBy4POISyGVK9XIMWgqywueDNQpKh";
const char strMotion5[] PROGMEM = "TdE5dJXyU45xIegTewp0LkQy5bxERRGxirzmsygeQPLDwKWlPxwIDZAQA2LUueEmeg2MJg8AO8RO46Ne2wTlAHsyg0KoLEbiX/YJuN6vqydLGUgMxCioFkpy9AWS2yvMvyAGhawWmlJuV3ZJ6aB7cAin+0njXVUGDx+oYDlNZ9wqnTsMNAPdu4Sg7XonghO6McFLhKEwlPiwQxuhdOA7PC/3OryXVocCHWrnbD0qYb5UKzxUtPwXtKKOgSgGbmBDv8IQXoUU+Ae+VVNy4UsDOZgQNZjDnBL9mmgFlHtCDHBHoL8j+mhNSUwgzVPPFOTNfJTnws6Uk5+UczegDPFYgfJlAfy0iHstC/8huHzlAO0tGnbAoZDwFHhJHKP0KMlC0i07L3FAaC528RYPUqI1xhkgCno8QGEng8qwGvA5Q0N9YGQDzOSPv5kPUGOApsOP0zUYJDogoElpAuoJqH5uUVoKqsJ7QO2S0zwh0D0/yE4DN7L5Wsi1OjahA75Jz8UdqpyLFgnZFEqXWPEBJN1CNApOz5ZZ6wQRbnw6eNaMhUDQVu6E3Kprw8H8Ht7mbvgvCCtJxOH1ZW6X2D1SqTgaQNOfgDG6GlhNgj08ZsITF78CekTIivzfKvDqcStXUxFwBOnNFWFob8hctA3hqdSQcDVfuL+ZJiPHLq7lXtRj/UxBAEbIRcZN6IQ4uesgaihJHDBPkWexDVlXlQOeFmwBfhA/IG2BGA6M3nCTQ+Q+";
const char strMotion6[] PROGMEM = "Nt7xysFStFeFNpVhxNsUbkNSFwhCWqzAhQ4XT+D/2hldiFH9YPe2YnwjlcD7K6WkJ6Kg46I62shzoDUOUg7xWhaVfRHe9AgVbAJX9D1AGmmGrRkJvQ+saaECIGM5NF7TkOD6+4bLLIo7OJx+JukIF2B+ysLL96Io1eTRvNoT3cMjloLgRqX6pAHSQnnv6kHFgrsd2SFRzBe5YNPYBMjHRUecPmoZa49m6EtjRCrcH5zVTAssQx42emNCmLV1AqEVBQvmyaY6pG7ZzQwmn5UGiqCfAfMRKwzp4sbcB1wfMuNo5Yb3ps88UgdzTFQXrUwP4E+wGZBNEGj3BGZ+3GjsN2HUxPHEA61JykAckDkp0/zsHcKaYLJoGrICTnQDV/YbTMWB4d5T68GTKRpP/+CQPIDWNAFIop43x8pDsxGkGUGBV3ah0GbbwQgcrgmRCaAgUJv/IB0EdY7HKwMFBeIvlL9ENI/uvsMpWP1gywFuQBZ2kUauGA2MOtIN80bBoq9FG9wqRLqW3j0+4y0L78JmpYv0xGBdtJ1BJJ7uTo2ogF1gXtbDmoCCp1hrkwIe4jzveMME67ysLpgdDm4aTzWyWrl64ODw68RlM/a+yh9gcRoWGeythIeHogkSbThVUmrfmRyYnOBe+o6rcEswBQ/CP0vADLPcq9Hv1XMAGbSFOkaDVgO5RobigTwDRja84nkOWvA2ZEXi7UU4AGwgMzVeOEK8LAgdcWCFNIbtvVZt";
const char strMotion7[] PROGMEM = "6IQNH29WdA33glmdfi1KLaC7iRPqDmXmeV7ZWsQMAhP5pPeAbwl/TdjaQiFRxwS4DQeaErU3AkVOyJi3JFt+jtFX5FDN5/tlPShBKhUnirzSJLbZ0YQc+qxrkTXEu94B+YGwADxwT5xxOVsIDHobflnzUhoRNJC8nKcTvwLlIGmxEQCDS1iffQ1ZzHVzHq1SCdhYGn7LQAofg8a6CPkNrVKAQZLZNhg2sF9v3GgIdo0UdMNZNOPqmFrNwPZLAs+ewZ+o7bIGYm6eY6GKM/JB97v0Dl1BDItLuErgr5fDkrwGtagdn5GiNBIiCfvL36KpMsQwqfWE6YD0qJMP5SHjaTjXNUmKm5IkCmie1TcaSoKcUpEH1oAWKDDdGvyTpDNpuQLl46AMLUeXXXz/xU13REMRoFUpJAgbnqYYaAtAGlPVQAopIdhF3kKVTU8SeN4c8fpWXdEOHrUK8KCUsA7Da13YPNQRMSoLgziWQjMJepLn4lIL7z81n6S2LrKt4SBBcsj+OcatOTMqXWMEQBQDiVPA4xBy4+ooAnAbt7JlziDqKmYntHCFG2r2BSTIS4eowRfd/SYA4Un9pYvi+OTeJG4l/XeGVADZOhF6+IAasuOSADFXYKEGDMmxLjR5Dhq9Hepr5iumZVmSUC9vuOtMx9DYFiXZ4lO1FJV4+M3F746mEQ1eSACA8PI49ig093sHxAEPpZ4+z4Ejwxd6EWdV3XfwLhVF+qg3Gr08y96L";
const char strMotion8[] PROGMEM = "nC43qaBDQqBBjyhnIgQRT8j/AGHUN/5PM7ZrYRJTaA7XkRoqEn6CQbx2SOKz9BA5K53Z61bkYWyiiIbAAl+Jec3svCN7kCb1njRFmogUNHofthDiwu9KkxVMgDz2G5jK1OgsAEvtIMfULRCVbkhWAnWoO4D5M13FTBRpvIMweGUFK0E/CJSkPGZcskb12KYkOQZKWXdnBuoW2JKVBmw8nZ62F3NwwwooCky0uzagT8p4ariibjN8z0IES1TW7YAWYk9rZE3ckb0RuURb4YhUy6ckCUmeob9tBDoXOth02ZI7BcJoLPR0cajxQPLBXg0mBoBD2D9zs4i6ppgrxQJyfqZmIse4F+o4AbnUrLajAorJaTtKOqPJkPAWXFMZ6lZ5evBSewmTegJsyBpQXQrAa7B+9ajH1ek3Oi+5yOIQMVe089BwAKFNZSgxccn+0Whih4SMRKlH+ki7f14bOlcSC+SRhpRduMJHuUggolGy4NySviFikmyYFWnz3rPglJwJOcsbLy5usgjlKo4HoxUMk9rWmzBSJuDhTNpjA2cp0xWUMjAG2K7pzePia6WPUdaupUxNkV/W3FTQABmHW96UWIdGAKUbUIVPAn9ok9DzNq5B58xHCZQzRYWD1EYMscDA0N8YlFAM1AGkiErwS6AGveOqEwZiSRZNyakM4dP41vaw2prrLcUCJaESgSLsHVK5yxOKG27WLVHwaSSMJD/0jTKsFOmmOEbV3H9Jz3j3";
const char strMotion9[] PROGMEM = "rZmL++JKaOQrP+Wb2rzT/vhc7fIsT15zWpqOo8v9piwoTET58SilCvdvqBP366V8cjcQC0+VPYK0PiPH96nytM/ImbZ136A3gE8SIwN6guu7o9XkJ0cDCzSgHB35BwNqddgLMqkdhBs9OI+j3xKtdN5GnAvWW9PQXpPRpf5oALLGE1T3G4gJyU6KyCLtYFfJQGkh8jlF9XLeOFk5dxO6ooejkwYbiBpsRgB6Efqm0ULRsiG3LM9d20bAQ1foaXw7TY4CRZDKBGPV3szQvRks/YY1kB0LmtvdakVmPGtNvqxe1HfBoIG4PJdDfDcvMvWUM0QGLTqCiG0HZmBwIkxrwA/ab6ZyEHgDBolaRpOlJGoHg4UrvZodS5AObU/ApA7YoVhpNEoRcU15HuURqaxJICofDYG2q7T9Xb5q64klb/SJit37gkhBGlSHw+UVvFHUqJkyQWjiP5HiOkwAEWDzEbgUG7wAhr0Bc9O0qmrj7v1N0pA0Azj46CcCXuCefvV4d2rHBHbE0/aBsKPSlaPTvDZCskz2VU61E3Wd7j9gkLd5g7SgU7Qdgh3UNlMRuV5tSnWD9XDgcv06cKH9ZvE+MVzafnELRQnT1Q6c5kEPeGmtEbuJS1FvWfNNe8NgLC1wKfombBPJ0CDao01sOy7m9DaZ+U17khNmWgN9YlhO4hOs0x5Lm5Jee3ABR0lfgnsqCxzqpc2xCC5pElPk5OaTKJGmB43FeWGq0P2UeqSZ";
const char strMotionA[] PROGMEM = "KakWJDbp1S7I5ElOwP1rs8BcEejL5ExpqFsWsmYDBEhkWuFoAzASNEmABEvcdyQDuwPaQHQEAJbTAQpH8Lu4MMFsXZINjuHNhAzRVL67QICS3JMeDs0H+fee6HWvAlyLII7r6A4YlKtkJQ2sGV5QhDXB0UM10LfK3rRPtzIGTIc2UM1SSDR3PI0glDTcayfZd20K0obNoxkkTYl6fGIc/vikeVxgd9RIrW01n0jZbIOugfTjL4vOqVEa87P9CJeAruQ5nFeY/o1Vl332ZlAtmpMhXbnLmpqPCQm3ozyoy/OZhFBCOn03aLwSeE5kgZQMH9JGaoao3zwpQxSCiUEB0hlGPZjTyBuN1KM29xEyctvylljsDqyPiMoIXpM8rDdeUaftANhQ9Ux9aWcsa4NT5hhV2bWjQJqD9jxlaTTqqTrcVnVEpG9tmUQtE5cCXmhjFEMeoLKp3bl53cDRNoqvA4raz2yy7ixp4dKKJpegRn4nIKhAbZgnLqljRBKQIV1Z+Ip/c32je2IsC50PCmu/j2a50vZpobN05EM7lwOBjckAO6VQOnSPLZwgOsvpSAH3FfF+S+7quBMei/iukkVwtCHuVLaHqM0SP1A2tKnytiigOC6FtplOGzvIgm1BJz4M9TJDwGWuAIabRxGXpol89XQDTuVVBjA6uCPfzyxgoQOdcBJLFZVftAOcKqvXkEFoGZ1wQXk+cauthIg6Bq6Ikc7MnCpxzwfAmuBkLjcw";
const char strMotionB[] PROGMEM = "kp5NHAAFyhDcR2piNztWrNP+HlLQqT5NqwoCbz/Bgo+HNXG1kK0DQ5BukCx6TB6BYkryCSl6vaotvjT3Tjqe4+UW6NhWMGXyZVxIGzraF3V4gUbx5/im4CTXtHOlfelD1LXr/baSWhvayBpJeyfUvDY4tXFGhV1trBFscAPT9JSHjNH4HsVh3VF79Jr0C/9taR/GKCC0kpYh04fMpuE3ARxuD52oPXaBSJ1JwcBqSoBewicib8M7QwTiVp2GyxTyNg1hUWpLZi0iPzWedRuNC6Arq1EnOZb5XUhM0uYdeis0jWhAAM1S1tvqC9raqhrlavcnShvOjTnO9IT9vjf+8+sBC0IPuyWiJYea0IPUPrBMPFg5DpaYOEhd9oZOwtpD2VADYg8TOb7UF+SVMTXKOhUdjxkX70DAm8zr/A91HJ0Gwjm87V+4iwrJqi/IK5omCNSyzjKlIxjyUAraLSPYU8+kldwgMEyK1VErRQfJNGiMvhLbKIh6TAhNEWoNzClEnW2q2iN7Q1HwYWhgV3UMqMKNLm0ruwQzHURK2LaoTbios1MI9WZAdNFIPSP0K41Kt1GeoDAPToPEllYq1gNeJCEMm9rhSLhCs2j0BdGnJiHdf4yTJ2T1p6Nrv36tuCMdxqk3qUZT6wUPNJEztELEEHntXcGWO5M3cJEixeBwWcPCw+QyHtJP2oIesnsgFaI5zEG7JVItcy5NLasmtNd5PvCxHNM+G1amyKwF2sre";
const char strMotionC[] PROGMEM = "7lN0iGiCRxdEIEQnRHJCTaKHDukm1POzfDjw754rzAtwuCZFvqssM0qqQHuakcHHHfGJsekgjzoQs1/6oWGtTSMroFAgslPD1M7bnQy0L1HqHvEH18K6Q9XZChoVOMrA25I7z0hs9LsNlAPdE7XPPUlr1gZDd7C7v5rhAW5NEyfJtKItm5oQaXNpM11OC37EvJVnBLldk57WWaecEiVjjk9q8HPr20LFiqCA7UnVo8Nt0N9gbSi47F/+YfS3OcgjHDQbDzFpLayoDu+sfVLQeQxqmi492JiGJ93aGUufeTswKxu/aEcqsa6sGZBOtnfugXqVy6bePXWq5tX8CJiZrWg4JStBeGWPGm6PDlg9o0UMK5W1RZ0FwoAShtoQESwJY6GTcpMa0c4DisRzf+RvKtSO5uU8K6IOhsNOQGUQRh9VR+V0FEETZ4fcMkH6FOjDA1PVq32LC1wH5Y9r4cHlgVVbGvQX5J2vWbsTOJCAuRoREQEtjuVbXDq4xzV2WlnnMdvUaVYduEE/nmRpTDhBWwyeq1Dw7bOnr/l7bRCkTnI8pb/lMXQ0Z/dZtdFuuG8dX0gawePikafgTTgSNZ3iWzoEhlCjBeB+mq6iu3TYuGg3NVQpIOB6YYI/KsT8L+cx/+ar+xdv0NbMnHaFaIBw1gwIG9g0sla3Lh0+JuH4NZp4oOu13jzQ0rRbkkDv4c2mMB5KJCs9eFlDhG7NvnN4iN61Sc97ALZnve6b12mS";
const char strMotionD[] PROGMEM = "PSQxUSb35vC26EjBxaTRmkGENb3OOEFeiv6TqkObHqm7Sp8KlD8fKv1zCM/yZ/K3qzY6106aLb33+MYHNnL9Xvvo3UJdXh2r5lf391/BfFc6/7x83vg9kOd1NMA+57lSf2vwb4x/l/ssQgbi5xoC2pFQls8HcCLvSYjAe34kPFCizRdeDnRLhAK7izrSFr32aIu2FG/QCWGMBeqW1r51rPhXK/hGIZU3tKvZecmkmuH3JopppMDrJKZmLMhZSTB6bFjWSXFEWcahgmja19U5zxBuqxr9OBQ2MlBhAEwQjCG+4+Ab86sJNKpIQjth+xbQqJnzYbWd7tCcCOP1yESnoWETbQdRbEOmpuigOI7v7WTs74Gk8KG8CMg9n1X0d+g8shwyJHK3TowrfCPSZu0jnaCupLmbRfTPa2gsLcF6Z2N5sPpsG3bVVLjryklMx9X35lmxyC+tb5bSUSEpfQYLrAW6xKMeSUaCNDBmUz4Ve7zfqYzeAsGGuTQotoUR1W4EOI1aRSMO1ICHtAfiHWLT+R9gV2MR45eCOA9/Vg8CQITOtNsqnYtQG+nvyf1ffXX/3geC1NibAmJXPod5tPl/ynG4jfiOXlQ5PvQ0skA7cLgq07lY4lSRlqXohARByPQI2nzOpsPUSaeMNCzNDsBZ4XNunV6DiTSLC6XAqZr0lgepOvGPRQc7hBzgfkI3QoUz6oRs6aVtNxrFI48CjqsqnvvUfss57zgDMrRqQ1m7";
const char strMotionE[] PROGMEM = "ziARV/lMl+dVqxfkws7kkO5HzdAz0WfuKGOm0fcSNGLIsEfw5dVW3oCGEHlGy4DGCHCd8YCZWtZplhrd1daT9ovmZwNi6eyVRkZIXi+zpA3eXT6HfTTNxXEumbw5PxK6tyeu3T+H9P/51/+fCwmc3X8DtHOW3sK9YpsAAADAUExURQAAACpIgyhKfjFIfilLgC1OgjRPfzdOhD1PgDlQhzhSgzxWh0JXg0BZikdciEtgjE9giFBkkVNkjFdokFxpjFprk2FukWRxlWlykWJ3mWl3mm14l3J9nXOEoHmDooKJo32MpYGNn4mQqoWUrImVqI+arY2btJicsZSgs5agraGlu5unu6CotqSqs6qttqutvaayxqmzwbG3v7K8yra9xrrBybrC0cXEz8HGyMDHz8LM2sbM1crN0MzT3M/U19Pa4hD9TgQAAAABdFJOUwBA5thmAAAAAWJLR0QAiAUdSAAAAAlwSFlzAAAOwwAADsMBx2+oZAAAAAd0SU1FB+MHCAkfE22lQ7wAAAIhSURBVDjLlVSPd5pADD6THO6dDJyu2vkL2qqlne0GLWqHdv//f9XkDrTsrY+SJ/EC95Hky3co1cL+nlY7Yz6F8L2XahnC4H87il9P9RtD8svVkrwj/10n9Q0LQAiOZXAfsdsbunGhgTX7iL7UEBsEIKzS+zhjvwJto4R67FOinzXImBgBWEZ3HUwFSbcSDSAWryf1uibEdoKoKYXsr1C88pC7";
const char strMotionF[] PROGMEM = "zImKOmRDAEjjKtxr2iqVoWai9yj1LSGsHj5PrF1oRrzLbIt51ShA6Epe257Ya4+bkB9nGdUIlIw94JYeO9L9GEv61AhkO8oFtD5DVjBkHwgLj2AhnRISMbcm8EGAAPrM4hItRLJk4FlCXNmpBgiUepAsFnMSwAwmdoZ846DhKGmdFkZEhsO+ABCZZG82j60a+1L6QeMfGQ9w9heNua1YG37NqmP7CadcJY/zGz/INR1FEsYNaWyH3XeEKZtY9upCRRoJtajmOwp9l27gGSJP8bkL26rsmHvn/kWmyWI+F0XmqFn4Ow2ut4F9wWZxnhqPBfBrTQyxcPcDy4FnWph7Z55jePvvAUoRKwLjoKg/sfye9VVZHlx/cF4jcL3g06c/CgOmi0VJNGzem4ZBGIYBTxDyOePgdxOi6FKpE+zx+eDKwiZIhkAOIoKLhIL7JkwfUMrhi0k/GF76jc1skltriVM7o9eqlcmhMe0gd/LB2LbDBITddgi1m162TPKRvQGm8iTYoBPwpAAAAABJRU5ErkJggg==";
const char *const arrMotionBase64[] PROGMEM = {strMotion0, strMotion1, strMotion2, strMotion3, strMotion4, strMotion5, strMotion6, strMotion7, strMotion8, strMotion9, strMotionA, strMotionB, strMotionC, strMotionD, strMotionE, strMotionF};

// remote switch enumerations
enum RemoteSwitchSlot { SLOT_START_HOUR, SLOT_START_MINUTE, SLOT_STOP_HOUR, SLOT_STOP_MINUTE };

// remote switch commands
enum RemoteSwitchCommands { CMND_REMOTESWITCH_BUTTON, CMND_REMOTESWITCH_MOTION, CMND_REMOTESWITCH_DEBOUNCE, CMND_REMOTESWITCH_DURATION, CMND_REMOTESWITCH_SLOT0, CMND_REMOTESWITCH_SLOT0_START_HOUR, CMND_REMOTESWITCH_SLOT0_START_MIN, CMND_REMOTESWITCH_SLOT0_STOP_HOUR, CMND_REMOTESWITCH_SLOT0_STOP_MIN, CMND_REMOTESWITCH_SLOT1, CMND_REMOTESWITCH_SLOT1_START_HOUR, CMND_REMOTESWITCH_SLOT1_START_MIN, CMND_REMOTESWITCH_SLOT1_STOP_HOUR, CMND_REMOTESWITCH_SLOT1_STOP_MIN };
const char kRemoteSwitchCommands[] PROGMEM = D_CMND_REMOTESWITCH_BUTTON "|" D_CMND_REMOTESWITCH_MOTION "|" D_CMND_REMOTESWITCH_DEBOUNCE "|" D_CMND_REMOTESWITCH_DURATION "|" D_CMND_REMOTESWITCH_SLOT0 "|" D_CMND_REMOTESWITCH_SLOT0_START_HOUR "|" D_CMND_REMOTESWITCH_SLOT0_START_MIN "|" D_CMND_REMOTESWITCH_SLOT0_STOP_HOUR "|" D_CMND_REMOTESWITCH_SLOT0_STOP_MIN "|" D_CMND_REMOTESWITCH_SLOT1 "|" D_CMND_REMOTESWITCH_SLOT1_START_HOUR "|" D_CMND_REMOTESWITCH_SLOT1_START_MIN "|" D_CMND_REMOTESWITCH_SLOT1_STOP_HOUR "|" D_CMND_REMOTESWITCH_SLOT1_STOP_MIN;

// time slot structure
struct timeslot {
  uint8_t  number;
  uint8_t  start_hour;
  uint8_t  start_minute;
  uint8_t  stop_hour;
  uint8_t  stop_minute;
};

// variables
ulong start_switch   = 0;            // time of latest switch command
ulong start_debounce = 0;            // time of latest debounced command
bool  motion_used = false;

/*********************************************************************************************/

// save motion time slot (format is HH:HH-HH:MM)
void RemoteSwitchMotionSetSlot (uint8_t slot_number, char* strSlot)
{
  uint8_t index = 0;
  char* token;
  char* arr_token[4];
  uint32_t time_setting;
  timeslot time_slot;
  
  // split string into array of values
  token = strtok (strSlot, ":");
  if (token != NULL)
  {
    arr_token[index++] = token;
    token = strtok (NULL, "-");
  }
  if (token != NULL)
  {
    arr_token[index++] = token;
    token = strtok (NULL, ":");
  }
  if (token != NULL) 
  {
    arr_token[index++] = token;
    token = strtok (NULL, ":");
  }
  if (token != NULL) arr_token[index++] = token;

  // convert strings to time slot
  time_slot.start_hour   = atoi (arr_token[0]);
  time_slot.start_minute = atoi (arr_token[1]);
  time_slot.stop_hour    = atoi (arr_token[2]);
  time_slot.stop_minute  = atoi (arr_token[3]);
  
  // calculate value to save according to time slot
  time_setting = time_slot.start_hour * 1000000;
  time_setting += time_slot.start_minute * 10000;
  time_setting += time_slot.stop_hour * 100;
  time_setting += time_slot.stop_minute;

  // write time slot in timer setting
  Settings.timer[slot_number].data = time_setting;
}

// get motion time slot data
struct timeslot RemoteSwitchMotionGetSlot (uint8_t slot_number)
{
  div_t    div_result;
  uint32_t slot_value = 0;
  timeslot slot_result;

  // set time slot number
  slot_result.number = slot_number;

  // read time slot raw data
  if (slot_number < 2) slot_value = Settings.timer[slot_number].data;

  // read start hour
  div_result = div (slot_value, 1000000);
  slot_result.start_hour = div_result.quot;

  // read start minute
  div_result = div (slot_value, 1000000);
  div_result = div (div_result.rem, 10000);
  slot_result.start_minute = div_result.quot;

  // read stop hour
  div_result = div (slot_value, 10000);
  div_result = div (div_result.rem, 100);
  slot_result.stop_hour = div_result.quot;

  // read start minute
  div_result = div (slot_value, 100);
  slot_result.stop_minute = div_result.rem;

  return slot_result;
}

// update motion detector usage according to current time slots
bool RemoteSwitchMotionUpdateUsage ()
{
  uint8_t  index;
  uint8_t  current_hour, current_minute;
  timeslot current_slot;

  // get current time
  current_hour = RtcTime.hour;
  current_minute = RtcTime.minute;

  // loop thru both time slots
  for (index = 0; index < 2; index ++)
  {
    // get current slot
    current_slot = RemoteSwitchMotionGetSlot (index);

    // update motion collect state
    if ((current_hour == current_slot.start_hour) && (current_minute == current_slot.start_minute)) motion_used = false;
    else if ((current_hour == current_slot.stop_hour) && (current_minute == current_slot.stop_minute)) motion_used = true;
  }
  
  return motion_used;
}

// get push button enable status
bool RemoteSwitchButtonIsEnabled ()
{
  return (Settings.display_model == 1);
}

// set push button status
void RemoteSwitchButtonEnable (uint8_t status)
{
  if (status > 1) status = 1;
  Settings.display_model = status;
}

// get motion detection enable status
bool RemoteSwitchMotionIsEnabled ()
{
  return (Settings.display_mode == 1);
}

// set motion detection status
void RemoteSwitchMotionEnable (uint8_t status)
{
  if (status > 1) status = 1;
  Settings.display_mode = status;
}

// get remote switch debounce delay (sec)
uint8_t RemoteSwitchGetDebounce ()
{
  return Settings.display_refresh;
}

// set remote switch debounce delay (sec)
void RemoteSwitchSetDebounce (uint8_t debounce)
{
  Settings.display_refresh = debounce;
}

// get remote switch standard duration (sec)
uint8_t RemoteSwitchGetDuration ()
{
  return Settings.display_size;
}

// set remote switch standard duration
void RemoteSwitchSetDuration (uint8_t duration)
{
  Settings.display_size = duration;
}

// Show JSON status (for MQTT)
void RemoteSwitchShowJSON (bool append)
{
  bool     button_enabled, button_state;
  bool     motion_enabled, motion_state;
  uint8_t  relay_state, relay_duration;
  uint8_t  debounce, duration;
  timeslot slot_0, slot_1;

  // collect data
  button_enabled = RemoteSwitchButtonIsEnabled ();
  button_state = 1;
  motion_enabled = RemoteSwitchMotionIsEnabled ();
  motion_state = 0;
  debounce = RemoteSwitchGetDebounce ();
  duration = RemoteSwitchGetDuration ();
  slot_0 = RemoteSwitchMotionGetSlot (0);
  slot_1 = RemoteSwitchMotionGetSlot (1);
  
  // read relay state
  relay_state = bitRead (power, 0);

  // start message  -->  {  or  ,
  if (append == false) snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("{"));
  else snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s,"), mqtt_data);

  // "RemoteSwitch":{"Debounce":2,"Duration":35,
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_REMOTESWITCH "\":{\"" D_JSON_REMOTESWITCH_DEBOUNCE "\":%d,\"" D_JSON_REMOTESWITCH_DURATION "\":%d,"), mqtt_data, debounce, duration);
  
  // "Relay":{"State":1,"Duration":35},
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_REMOTESWITCH_RELAY "\":{\"" D_JSON_REMOTESWITCH_STATE "\":%d,\"" D_JSON_REMOTESWITCH_DURATION "\":%d},"), mqtt_data, relay_state, relay_duration);

  // "PushButton":{"Enabled":1,"State":0},
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_REMOTESWITCH_BUTTON "\":{\"" D_JSON_REMOTESWITCH_ENABLED "\":%d,\"" D_JSON_REMOTESWITCH_STATE "\":%d},"), mqtt_data, button_enabled, button_state);

  // "MotionDetector":{"Enabled":1,"State":1,"Slot1":"01:00-12:00","Slot2":"00:00-00:00"}}
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_REMOTESWITCH_MOTION "\":{\"" D_JSON_REMOTESWITCH_ENABLED "\":%d,\"" D_JSON_REMOTESWITCH_STATE "\":%d}}"), mqtt_data, motion_enabled, motion_state);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_REMOTESWITCH_SLOT "1\":\"%2d:%2d-%2d:%2d\","), mqtt_data, slot_0.start_hour, slot_0.start_minute, slot_0.stop_hour, slot_0.stop_minute);
  snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_JSON_REMOTESWITCH_SLOT "2\":\"%2d:%2d-%2d:%2d\"},"), mqtt_data, slot_1.start_hour, slot_1.start_minute, slot_1.stop_hour, slot_1.stop_minute);

  // if not in append mode, publish message 
  if (append == false)
  { 
    // end of message   ->  }
    snprintf_P (mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);

    // publish full state
    MqttPublishPrefixTopic_P (TELE, PSTR(D_RSLT_SENSOR));
  }
}

// Handle remote switch MQTT commands
bool RemoteSwitchCommand ()
{
  bool serviced = true;
  timeslot time_slot;
  int  command_code;
  char command [CMDSZ];

  // check MQTT command
  command_code = GetCommandCode (command, sizeof(command), XdrvMailbox.topic, kRemoteSwitchCommands);

  // handle command
  switch (command_code)
  {
    case CMND_REMOTESWITCH_BUTTON:       // enable/disable push button
      RemoteSwitchButtonEnable(XdrvMailbox.payload);
      break;
    case CMND_REMOTESWITCH_MOTION:     // enable/disable motion detector
      RemoteSwitchMotionEnable(XdrvMailbox.payload);
      break;
    case CMND_REMOTESWITCH_DEBOUNCE:  // set debounce delay
      RemoteSwitchSetDebounce (XdrvMailbox.payload);
      break;
    case CMND_REMOTESWITCH_DURATION:  // set switch minimum duration
      RemoteSwitchSetDuration (XdrvMailbox.payload);
      break;
    case CMND_REMOTESWITCH_SLOT0:     // set motion detector first disable slot
      RemoteSwitchMotionSetSlot (0, XdrvMailbox.data);
      break;
    case CMND_REMOTESWITCH_SLOT1:     // set motion detector second disable slot
      RemoteSwitchMotionSetSlot (1, XdrvMailbox.data);
      break;
    default:
      serviced = false;
  }

  // send MQTT status
  if (serviced == true) RemoteSwitchShowJSON (false);
  
  return serviced;
}

// update pilot wire relay states according to current status
void RemoteSwitchEvery250MSecond ()
{
}

#ifdef USE_WEBSERVER

// Pilot Wire icon
void RemoteSwitchWebDisplayIcon (uint8_t height)
{
  uint8_t nbrItem, index;

  // display img according to height
  WSContentSend_P ("<img height=%d src='data:image/png;base64,", height);

  // loop to display base64 segments
  nbrItem = sizeof (arrMotionBase64) / sizeof (char*);
  for (index=0; index<nbrItem; index++) { WSContentSend_P (arrMotionBase64[index]); }

  WSContentSend_P ("'/>");
}

// remote switch configuration button
void RemoteSwitchWebConfigButton ()
{
  // beginning
  WSContentSend_P (PSTR ("<table style='width:100%%;'><tr>"));

  // vmc icon
  WSContentSend_P (PSTR ("<td align='center'>"));
  RemoteSwitchWebDisplayIcon (32);
  WSContentSend_P (PSTR ("</td>"));

  // button
  WSContentSend_P (PSTR ("<td><form action='%s' method='get'><button>%s</button></form></td>"), D_PAGE_REMOTESWITCH, D_REMOTESWITCH_CONFIGURE);

  // end
  WSContentSend_P (PSTR ("</tr></table>"));
}

// remote switch web configuration page
void RemoteSwitchWebPage ()
{
  bool     updated = false;
  bool     button_enabled, motion_enabled;
  uint8_t  duration, debounce;
  timeslot slot_0, slot_1;
  char     argument[REMOTESWITCH_LABEL_BUFFER_SIZE];
  char     slot[REMOTESWITCH_LABEL_BUFFER_SIZE];

  // if access not allowed, close
  if (!HttpCheckPriviledgedAccess()) return;

  // get remote switch button enabled according to 'button' parameter
  if (WebServer->hasArg(D_CMND_REMOTESWITCH_BUTTON))
  {
    WebGetArg (D_CMND_REMOTESWITCH_BUTTON, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    RemoteSwitchButtonEnable ((uint8_t) atoi (argument)); 
    updated = true;
  }

  // get remote switch motion detector enabled according to 'motion' parameter
  if (WebServer->hasArg(D_CMND_REMOTESWITCH_MOTION))
  {
    WebGetArg (D_CMND_REMOTESWITCH_MOTION, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    RemoteSwitchMotionEnable ((uint8_t) atoi (argument)); 
    updated = true;
  }

  // get remote switch debounced delay according to 'debounce' parameter
  if (WebServer->hasArg(D_CMND_REMOTESWITCH_DEBOUNCE))
  {
    WebGetArg (D_CMND_REMOTESWITCH_DEBOUNCE, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    RemoteSwitchSetDebounce ((uint8_t) atoi (argument)); 
    updated = true;
  }

  // get remote switch duration according to 'duration' parameter
  if (WebServer->hasArg(D_CMND_REMOTESWITCH_DURATION))
  {
    WebGetArg (D_CMND_REMOTESWITCH_DURATION, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    RemoteSwitchSetDuration ((uint8_t) atoi (argument)); 
    updated = true;
  }

  // get remote switch first motion detector slot according to 'slot0' parameters
  if (WebServer->hasArg(D_CMND_REMOTESWITCH_SLOT0_START_HOUR))
  {
    WebGetArg (D_CMND_REMOTESWITCH_SLOT0_START_HOUR, slot, REMOTESWITCH_LABEL_BUFFER_SIZE);
    WebGetArg (D_CMND_REMOTESWITCH_SLOT0_START_MIN, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    strcat (slot, ":");
    strcat (slot, argument);
    WebGetArg (D_CMND_REMOTESWITCH_SLOT0_STOP_HOUR, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    strcat (slot, "-");
    strcat (slot, argument);
    WebGetArg (D_CMND_REMOTESWITCH_SLOT0_STOP_MIN, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    strcat (slot, ":");
    strcat (slot, argument);
    RemoteSwitchMotionSetSlot (0, slot); 
    updated = true;
  }

  // get remote switch second motion detector slot according to 'slot1' parameters
  if (WebServer->hasArg(D_CMND_REMOTESWITCH_SLOT1_START_HOUR))
  {
    WebGetArg (D_CMND_REMOTESWITCH_SLOT1_START_HOUR, slot, REMOTESWITCH_LABEL_BUFFER_SIZE);
    WebGetArg (D_CMND_REMOTESWITCH_SLOT1_START_MIN, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    strcat (slot, ":");
    strcat (slot, argument);
    WebGetArg (D_CMND_REMOTESWITCH_SLOT1_STOP_HOUR, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    strcat (slot, "-");
    strcat (slot, argument);
    WebGetArg (D_CMND_REMOTESWITCH_SLOT1_STOP_MIN, argument, REMOTESWITCH_LABEL_BUFFER_SIZE);
    strcat (slot, ":");
    strcat (slot, argument);
    RemoteSwitchMotionSetSlot (1, slot); 
    updated = true;
  }

  // if parameters updated, back to main page
  if (updated == true)
  {
    WebServer->sendHeader ("Location", "/", true);
    WebServer->send ( 302, "text/plain", "");
  }
  
  // read data
  slot_0 = RemoteSwitchMotionGetSlot (0);
  slot_1 = RemoteSwitchMotionGetSlot (1);
  button_enabled = RemoteSwitchButtonIsEnabled ();
  motion_enabled = RemoteSwitchMotionIsEnabled ();
  duration = RemoteSwitchGetDuration ();
  debounce = RemoteSwitchGetDebounce ();
  
  // beginning of form
  WSContentStart_P (D_REMOTESWITCH_CONFIGURE);
  WSContentSendStyle ();

  // form
  WSContentSend_P (PSTR ("<fieldset><legend><b>&nbsp;%s&nbsp;</b></legend><form method='get' action='%s'>"), D_REMOTESWITCH_PARAMETERS, D_PAGE_REMOTESWITCH);

  // duration
  WSContentSend_P (PSTR ("<p><b>%s</b><br/><input type='number' name='%s' min='0' step='1' value='%d'></p>"), D_REMOTESWITCH_DURATION, D_CMND_REMOTESWITCH_DURATION, duration);

  // debounce
  WSContentSend_P (PSTR ("<p><b>%s</b><br/><input type='number' name='%s' min='0' step='1' value='%d'></p>"), D_REMOTESWITCH_DEBOUNCE, D_CMND_REMOTESWITCH_DEBOUNCE, debounce);

  // push button input
  WSContentSend_P (PSTR ("<p><b>%s</b><br/>"), D_REMOTESWITCH_BUTTON);
  if (button_enabled == true) strcpy (argument, "checked");
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<input type='checkbox' name='%s' value='1' %s>%s</p>"), D_CMND_REMOTESWITCH_BUTTON, argument, D_REMOTESWITCH_ENABLE);

  // motion detector input
  WSContentSend_P (PSTR ("<p><b>%s</b><br/>"), D_REMOTESWITCH_MOTION);
  if (motion_enabled == true) strcpy (argument, "checked");
  else strcpy (argument, "");
  WSContentSend_P (PSTR ("<input type='checkbox' name='%s' value='1' %s>%s<br />"), D_CMND_REMOTESWITCH_MOTION, argument, D_REMOTESWITCH_ENABLE);

  WSContentSend_P (PSTR ("%s "), D_REMOTESWITCH_FROM);
  WSContentSend_P (PSTR ("<input type='number' name='%s' style='width:10%;' min='0' max='23' step='1' value='%d'> h "), D_CMND_REMOTESWITCH_SLOT0_START_HOUR, slot_0.start_hour);
  WSContentSend_P (PSTR ("<input type='number' name='%s' style='width:10%;' min='0' max='59' step='1' value='%d'>"), D_CMND_REMOTESWITCH_SLOT0_START_MIN, slot_0.start_minute);
  WSContentSend_P (PSTR (" %s "), D_REMOTESWITCH_UNTIL);
  WSContentSend_P (PSTR ("<input type='number' name='%s' style='width:10%;' min='0' max='23' step='1' value='%d'> h "), D_CMND_REMOTESWITCH_SLOT0_STOP_HOUR, slot_0.stop_hour);
  WSContentSend_P (PSTR ("<input type='number' name='%s' style='width:10%;' min='0' max='59' step='1' value='%d'><br />"), D_CMND_REMOTESWITCH_SLOT0_STOP_MIN, slot_0.stop_minute);

  WSContentSend_P (PSTR ("%s "), D_REMOTESWITCH_FROM);
  WSContentSend_P (PSTR ("<input type='number' style='width:10%;' name='%s' min='0' max='23' step='1' value='%d'> h "), D_CMND_REMOTESWITCH_SLOT1_START_HOUR, slot_1.start_hour);
  WSContentSend_P (PSTR ("<input type='number' name='%s' style='width:10%;' min='0' max='59' step='1' value='%d'>"), D_CMND_REMOTESWITCH_SLOT1_START_MIN, slot_1.start_minute);
  WSContentSend_P (PSTR (" %s "), D_REMOTESWITCH_UNTIL);
  WSContentSend_P (PSTR ("<input type='number' name='%s' style='width:10%;' min='0' max='23' step='1' value='%d'> h "), D_CMND_REMOTESWITCH_SLOT1_STOP_HOUR, slot_1.stop_hour);
  WSContentSend_P (PSTR ("<input type='number' name='%s' style='width:10%;' min='0' max='59' step='1' value='%d'>"), D_CMND_REMOTESWITCH_SLOT1_STOP_MIN, slot_1.stop_minute);
  
  WSContentSend_P (PSTR ("</p>"));

  // end of form
  WSContentSend_P(HTTP_FORM_END);
  WSContentSpaceButton(BUTTON_CONFIGURATION);
  WSContentStop();
}

// append pilot wire state to main page
bool RemoteSwitchWebState ()
{
  float   corrected_temperature;
  float   target_temperature;
  uint8_t actual_mode;
  uint8_t actual_state;
  char*   actual_label;
  char    argument[REMOTESWITCH_LABEL_BUFFER_SIZE];

  // add push button state
  snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><th>%s</th><td>%s</td></tr>", mqtt_data, D_REMOTESWITCH_BUTTON, D_REMOTESWITCH_ON);

  // add motion detector state
  snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><th>%s</th><td>%s</td></tr>", mqtt_data, D_REMOTESWITCH_MOTION, D_REMOTESWITCH_OFF);

  // add times
  snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><th>%s</th><td>%d</td></tr>", mqtt_data, D_REMOTESWITCH_TIME_ON, 12);
  snprintf_P (mqtt_data, sizeof(mqtt_data), "%s<tr><th>%s</th><td>%d</td></tr>", mqtt_data, D_REMOTESWITCH_TIME_REMAIN, 12);
}

#endif  // USE_WEBSERVER

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns97 (byte callback_id)
{
  bool result = false;

  // main callback switch
  switch (callback_id)
  {
    case FUNC_COMMAND:
      result = RemoteSwitchCommand ();
      break;
    case FUNC_EVERY_250_MSECOND:
      RemoteSwitchEvery250MSecond ();
      break;
    case FUNC_JSON_APPEND:
      RemoteSwitchShowJSON (true);
      break;

#ifdef USE_WEBSERVER

    case FUNC_WEB_ADD_HANDLER:
      WebServer->on ("/" D_PAGE_REMOTESWITCH, RemoteSwitchWebPage);
      break;
    case FUNC_WEB_APPEND:
      RemoteSwitchWebState ();
      break;
    case FUNC_WEB_ADD_BUTTON:
      RemoteSwitchWebConfigButton ();
      break;

#endif  // USE_WEBSERVER

  }
  return result;
}

#endif // USE_REMOTESWITCH
