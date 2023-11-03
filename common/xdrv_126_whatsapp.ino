/*
  xsns_126_whatsapp.ino - Send whatsapp message from Tasmota
    Use free API from callmebot.com

  Copyright (C) 2023  Nicolas Bernaerts
    15/09/2023 - v1.0 - Creation

  CallMeBot parameters are stored using Settings :
    * SET_WHATSAPP_NUMBER             // target phone number (in +xx xxxxxx format)
    * SET_WHATSAPP_KEY                // callmebot API key
  
  Use whats_help command to list available commands

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

#ifdef USE_WHATSAPP

#define XDRV_126                      126

// commands
#define D_CMND_WHATSAPP_HELP          "help"
#define D_CMND_WHATSAPP_NUMBER        "number"
#define D_CMND_WHATSAPP_KEY           "key"
#define D_CMND_WHATSAPP_SEND          "send"

// remote sensor commands
const char kWhatsappCommands[] PROGMEM = "whats_" "|" D_CMND_WHATSAPP_HELP "|" D_CMND_WHATSAPP_NUMBER "|" D_CMND_WHATSAPP_KEY "|" D_CMND_WHATSAPP_SEND;
void (* const WhatsappCommand[])(void) PROGMEM = { &CmndWhatsappHelp, &CmndWhatsappNumber, &CmndWhatsappKey, &CmndWhatsappSend };

/***********************************************\
 *                  Commands
\***********************************************/

// whatsapp help
void CmndWhatsappHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: Whatsapp commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - whats_number <+num> = target phone number (+xx .... format)"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - whats_key <key>     = CallMeBot API key"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - whats_send <msg>    = send a whatsapp message"));

  ResponseCmndDone();
}

void CmndWhatsappNumber ()
{
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.data[0] == '+')) SettingsUpdateText (SET_WHATSAPP_NUMBER, XdrvMailbox.data);
  ResponseCmndChar (SettingsText (SET_WHATSAPP_NUMBER));
}

void CmndWhatsappKey ()
{
  if (XdrvMailbox.data_len > 0) SettingsUpdateText (SET_WHATSAPP_KEY, XdrvMailbox.data);
  ResponseCmndChar (SettingsText (SET_WHATSAPP_KEY));
}

void CmndWhatsappSend ()
{
  bool   result;

  // check if all data are ok
  result = ((XdrvMailbox.data_len > 0) && (strlen (SettingsText (SET_WHATSAPP_NUMBER)) > 0) && (strlen (SettingsText (SET_WHATSAPP_KEY)) > 0));

  // send message
  if (result) result = WhatsappSendMessage (SettingsText(SET_WHATSAPP_NUMBER), SettingsText(SET_WHATSAPP_KEY), XdrvMailbox.data);

  // answer
  if (result) ResponseCmndDone (); else ResponseCmndFailed ();
}

/**************************************************\
 *                  Functions
\**************************************************/

// get current week label dd/mm - dd/mm (0 is current week)
bool WhatsappSendMessage (const char* pstr_number, const char* pstr_key, const char* pstr_message)
{
  bool result;
  int  http_code;
  String str_url;
  HTTPClient http_client;

  // check parameters
  if (pstr_number == nullptr) return false;
  if (pstr_key == nullptr) return false;
  if (pstr_message == nullptr) return false;
  if (strlen (pstr_number) == 0) return false;
  if (strlen (pstr_key) == 0) return false;
  if (strlen (pstr_message) == 0) return false;
  
  // log
  AddLog (LOG_LEVEL_DEBUG_MORE, PSTR ("WAP: sendMessage"));

  // Data to send with HTTP POST
  str_url = "https://api.callmebot.com/whatsapp.php?phone=";
  str_url += pstr_number;
  str_url += "&apikey=";
  str_url += pstr_key;
  str_url += "&text=";
  str_url += UrlEncode (pstr_message);    

  // prepare URL call
  http_client.begin (str_url);
  http_client.addHeader ("Content-Type", "application/x-www-form-urlencoded");
  
  // Send HTTP POST request
  http_code = http_client.POST (str_url.c_str ());
  result = (http_code == 200);

  return result;
}

// get current week label dd/mm - dd/mm (0 is current week)
bool WhatsappSendMessage (const char* pstr_message)
{
  bool result;

  result = WhatsappSendMessage (SettingsText(SET_WHATSAPP_NUMBER), SettingsText(SET_WHATSAPP_KEY), pstr_message);

  return result;
}

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv126 (uint32_t function)
{
  bool result = false;

  switch (function) {

    case FUNC_COMMAND:
      result = DecodeCommand (kWhatsappCommands, WhatsappCommand);
      break;
  }
  return result;
}

#endif      // USE_WHATSAPP
