/*
  xdrv_40_telegram_extension.ino - Telegram message extension

  Copyright (C) 2021  Nicolas Bernaerts 
    12/07/2021 - v1.0 - Creation
    08/03/2022 - v1.1 - handle chatid as string to allow very big ids
                   
  This extends Telegram driver to allow :
   - markdown syntax
   - HTML syntax
   - conversion from \n to telegram LF
   - disabling of web link preview
   - disabling of message notification
   - message update
   - message reply

  Make sure to run this command in cosole to enable telegram messages :
    SetOption132 1
    
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

#ifdef USE_TELEGRAM
#ifdef USE_TELEGRAM_EXTENSION

/*************************************************\
 *               Variables
\*************************************************/

// syntax available
enum TelegramExtensionSyntax { TELEGRAM_SYNTAX_PLAIN, TELEGRAM_SYNTAX_MARKDOWN, TELEGRAM_SYNTAX_HTML, TELEGRAM_SYNTAX_MAX };
enum TelegramExtensionCommand { TELEGRAM_COMMAND_SEND, TELEGRAM_COMMAND_UPDATE, TELEGRAM_COMMAND_MAX };

/**************************************************\
 *                  Functions
\**************************************************/

// initialisation
void TelegramExtensionInit ()
{
  // force telegram send flag if chat id is set
  if (strlen (SettingsText (SET_TELEGRAM_CHATID)) > 0) Settings->sbflag1.telegram_send_enable = 1;
}

// generate base of telegram message command
String TelegramGenerateCommand (uint8_t command_type, const char* pstr_chatid, uint8_t syntax, bool page_preview)
{
  String str_command;

  // if chat id not given, get default one
  if (pstr_chatid == nullptr) pstr_chatid = SettingsText (SET_TELEGRAM_CHATID);
  AddLog (LOG_LEVEL_DEBUG, PSTR ("SMK: Telegram chatid : %s"), SettingsText (SET_TELEGRAM_CHATID));

  // generate string command with token
  str_command = "bot";
  str_command += SettingsText(SET_TELEGRAM_TOKEN);
  str_command += "/";

  // set command type
  if (command_type == TELEGRAM_COMMAND_SEND) str_command += "sendMessage";
  else if (syntax == TELEGRAM_COMMAND_UPDATE) str_command += "editMessageText";

  // add chat id
  str_command += "?chat_id=";
  str_command += pstr_chatid;

  // handle syntax
  if (syntax == TELEGRAM_SYNTAX_MARKDOWN) str_command += "&parse_mode=MarkdownV2";
  else if (syntax == TELEGRAM_SYNTAX_HTML) str_command += "&parse_mode=HTML";

  // handle page preview
  if (page_preview == false) str_command += "&disable_web_page_preview=True";

  return str_command;
}

// generate base of telegram message command
long TelegramGetMessageId (const char* str_response)
{
  long message_id = 0;

  // retrieve message id
  JsonParser       json_parser ((char*)str_response);
  JsonParserObject json_root = json_parser.getRootObject ();
  if (json_root) message_id = json_root["result"].getObject ()["message_id"].getUInt ();

  return message_id;
}

// replace special caracters
void TelegramCleanupMessage (String &str_message)
{
  str_message.replace ("\n", "%0A");
  str_message.replace (".",  "\\.");
  str_message.replace ("(",  "\\(");
  str_message.replace (")",  "\\)");
  str_message.replace ("!",  "\\!");
}

// send a message
long TelegramSendMessage (const char* pstr_message, const char* pstr_chatid = nullptr, uint8_t syntax = TELEGRAM_SYNTAX_MARKDOWN, bool page_preview = true, bool notification = true)
{
  long   message_id = 0;
  String str_message, str_command, str_response;

  // check parameter
  if (pstr_message == nullptr) return 0;

  // prepare message (replace special caracters)
  str_message = pstr_message;
  TelegramCleanupMessage (str_message);

  // generate base of message command
  str_command = TelegramGenerateCommand (TELEGRAM_COMMAND_SEND, pstr_chatid, syntax, page_preview);
  if (notification == false) str_command += "&disable_notification=True";
  str_command += "&text=" + UrlEncode (str_message);
  
  // send message
  AddLog (LOG_LEVEL_DEBUG, PSTR("TGM: Send - %s"), str_command.c_str ());
  str_response = TelegramConnectToTelegram (str_command);

  // retreive message id
  AddLog (LOG_LEVEL_DEBUG, PSTR("TGM: Response - %s"), str_response.c_str ());
  message_id = TelegramGetMessageId (str_response.c_str ());

  return message_id;
}

// update a sent message
long TelegramUpdateMessage (const char* pstr_message, long message_id, uint8_t syntax = TELEGRAM_SYNTAX_MARKDOWN, bool page_preview = true)
{
  String str_message, str_command, str_response;

  // check parameter
  if (pstr_message == nullptr) return 0;

  // prepare message (replace special caracters)
  str_message = pstr_message;
  TelegramCleanupMessage (str_message);

  // generate base of message command and add original message id
  str_command = TelegramGenerateCommand (TELEGRAM_COMMAND_UPDATE, 0, syntax, page_preview);
  str_command += "&message_id=" + String (message_id);
  str_command += "&text=" + UrlEncode (str_message);
  
  // send message
  AddLog (LOG_LEVEL_DEBUG, PSTR("TGM: Update - %s"), str_command.c_str ());
  str_response = TelegramConnectToTelegram (str_command);
  AddLog (LOG_LEVEL_DEBUG, PSTR("TGM: Response - %s"), str_response.c_str ());

  return message_id;
}

// reply to a message
long TelegramReplyMessage (const char* pstr_message, long message_id, const char* pstr_chatid = nullptr, uint8_t syntax = TELEGRAM_SYNTAX_MARKDOWN, bool page_preview = true, bool notification = true)
{
  long   reply_id = 0;
  String str_message, str_command, str_response;

  // check parameter
  if (pstr_message == nullptr) return 0;

  // prepare message (replace special caracters)
  str_message = pstr_message;
  TelegramCleanupMessage (str_message);

  // generate base of message command
  str_command = TelegramGenerateCommand (TELEGRAM_COMMAND_SEND, pstr_chatid, syntax, page_preview);
  if (notification == false) str_command += "&disable_notification=True";
  str_command += "&reply_to_message_id=" + String (message_id);
  str_command += "&text=" + UrlEncode (str_message);
  
  // send message
  AddLog(LOG_LEVEL_DEBUG, PSTR("TGM: Reply - %s"), str_command.c_str ());
  str_response = TelegramConnectToTelegram (str_command);

  // retreive message id
  AddLog(LOG_LEVEL_DEBUG, PSTR("TGM: Response - %s"), str_response.c_str ());
  reply_id = TelegramGetMessageId (str_response.c_str ());

  return reply_id;
}

#endif     // USE_TELEGRAM_EXTENSION
#endif     // USE_TELEGRAM

