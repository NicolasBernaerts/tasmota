/*
  xdrv_50_filesystem_cfg_csv.ino - Extensions for UFS driver
  This exension provides few functions to ease the use of :
   - .cfg configuration files
   - .csv database files

  Copyright (C) 2019-2021  Nicolas Bernaerts

  Version history :
    01/09/2021 - v1.0 - Creation
    29/09/2021 - v1.1 - Add .cfg files management
    15/10/2021 - v1.2 - Add reverse CSV line navigation
    01/04/2022 - v1.3 - Add software watchdog to avoid locked loop
    27/07/2022 - v1.4 - Use String to report strings
    31/08/2022 - v1.5 - Handle empty lines in CSV
    29/09/2022 - v1.6 - Rework of CSV files handling
    09/11/2022 - v1.7 - Add long long handling in config file
                        Remove filesystem cleanup as it can create deadlock

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

#ifdef USE_UFILESYS

// configuration file constant
#define UFS_CFG_LINE_LENGTH             128                 // maximum size of a configuration line
#define UFS_CFG_VALUE_MAX               22                  // maximum size of a numerical value (int, long, long long or float)

// action when reading a CSV line
enum UfsCsvAccessType { UFS_CSV_ACCESS_READ, UFS_CSV_ACCESS_WRITE, UFS_CSV_ACCESS_MAX };
enum UfsCsvLineAction { UFS_CSV_NONE, UFS_CSV_NEXT, UFS_CSV_PREVIOUS };

/*********************************************\
 *            Configuration files
\*********************************************/

// read key value in configuration file (key may be separated with = or ;)
String UfsCfgLoadKey (const char* pstr_filename, const char* pstr_key) 
{
  bool   finished = false;
  bool   found = false;
  int    length;
  char   str_line[UFS_CFG_LINE_LENGTH];
  char   *pstr_data;
  String str_value;
  File   file;

  // validate parameters
  if ((pstr_filename == nullptr) || (pstr_key == nullptr)) return str_value;
  
  // if file exists
  if (ffsp->exists (pstr_filename))
  {
    // open file in read only mode in littlefs filesystem
    file = ffsp->open (pstr_filename, "r");

    // loop to read lines
    do
    {
      length = file.readBytesUntil ('\n', str_line, sizeof (str_line));
      finished = (length == 0);
      if (!finished)
      {
        // set end of string
        str_line[length] = 0;

        // search for '=' separator
        pstr_data = strchr (str_line, '=');

        // if not found, search for ';' separator
        if (pstr_data == nullptr) pstr_data = strchr (str_line, ';');

        // if separator is found, compare key
        if (pstr_data != nullptr)
        {
          // separate key and value
          *pstr_data = 0;
          pstr_data++;

          // check current key
          found = (strcmp (str_line, pstr_key) == 0);

          // if found, save value
          if (found) str_value = pstr_data;
        }
      }
    } while (!finished && !found);

    // close file
    file.close ();
  }

  return str_value;
}

// read integer key value in configuration file
int UfsCfgLoadKeyInt (const char* pstr_filename, const char* pstr_key, const int default_value) 
{
  int    result = default_value;
  String str_result;
  
  // open file in read only mode in littlefs filesystem
  str_result = UfsCfgLoadKey (pstr_filename, pstr_key);
  if (str_result.length () > 0) result = (int)str_result.toInt ();

  return result;
}

// read long key value in configuration file
long UfsCfgLoadKeyLong (const char* pstr_filename, const char* pstr_key, const long default_value) 
{
  long   result = default_value;
  String str_result;
  
  // open file in read only mode in littlefs filesystem
  str_result = UfsCfgLoadKey (pstr_filename, pstr_key);
  if (str_result.length () > 0) result = str_result.toInt ();

  return result;
}

// read long key value in configuration file
long long UfsCfgLoadKeyLongLong (const char* pstr_filename, const char* pstr_key, const long long default_value) 
{
  long long result = default_value;
  String    str_result;
  
  // open file in read only mode in littlefs filesystem
  str_result = UfsCfgLoadKey (pstr_filename, pstr_key);
  if (str_result.length () > 0) result = atoll (str_result.c_str ());

  return result;
}

// read float key value in configuration file
float UfsCfgLoadKeyFloat (const char* pstr_filename, const char* pstr_key, const float default_value) 
{
  float  result = default_value;
  String str_result;

  // open file in read only mode in littlefs filesystem
  str_result = UfsCfgLoadKey (pstr_filename, pstr_key);
  if (str_result.length () > 0) result = str_result.toFloat ();

  return result;
}

// save key value in configuration file
void UfsCfgSaveKey (const char* pstr_filename, const char* pstr_key, const char* pstr_value, bool create) 
{
  char str_line[UFS_CFG_LINE_LENGTH];
  File file;
  
  // validate parameters
  if ((pstr_filename == nullptr) || (pstr_key == nullptr) || (pstr_value == nullptr)) return;
  
  // open file in creation or append mode
  if (create) file = ffsp->open (pstr_filename, "w");
    else file = ffsp->open (pstr_filename, "a");

  // write key=value
  sprintf (str_line, "%s=%s\n", pstr_key, pstr_value);
  file.print (str_line);

  // close file
  file.close ();
}

// save int as a key in configuration file
void UfsCfgSaveKeyInt (const char* pstr_filename, const char* pstr_key, const int value, bool create) 
{
  char str_value[UFS_CFG_VALUE_MAX];
  
  // convert value to string and save key = value
  itoa (value, str_value, 10);
  UfsCfgSaveKey (pstr_filename, pstr_key, str_value, create);
}

// save long as a key in configuration file
void UfsCfgSaveKeyLong (const char* pstr_filename, const char* pstr_key, const long value, bool create) 
{
  char str_value[UFS_CFG_VALUE_MAX];
  
  // convert value to string and save key = value
  ltoa (value, str_value, 10);
  UfsCfgSaveKey (pstr_filename, pstr_key, str_value, create);
}

// save long long as a key in configuration file
void UfsCfgSaveKeyLongLong (const char* pstr_filename, const char* pstr_key, const long long value, bool create) 
{
  lldiv_t result;
  char    str_value[12];
  char    str_result[UFS_CFG_VALUE_MAX];

  // if needed convert upper digits
  result = lldiv (value, 10000000000000000LL);
  if (result.quot != 0) ltoa ((long)result.quot, str_value, 10);
    else strcpy (str_result, "");

  // convert middle digits
  result = lldiv (result.rem, 100000000LL);
  if (result.quot != 0)
  {
    if (strlen (str_result) == 0) ltoa ((long)result.quot, str_value, 10);
      else sprintf_P (str_value, PSTR ("%08d"), (long)result.quot);
    strlcat (str_result, str_value, sizeof (str_result));
  }

  // convert lower digits
  if (strlen (str_result) == 0) ltoa ((long)result.rem, str_value, 10);
    else sprintf_P (str_value, PSTR ("%08d"), (long)result.rem);
  strlcat (str_result, str_value, sizeof(str_result));

  // save key = value
  UfsCfgSaveKey (pstr_filename, pstr_key, str_result, create);
}

// save float as a key in configuration file
void UfsCfgSaveKeyFloat (const char* pstr_filename, const char* pstr_key, const float value, bool create) 
{
  char str_value[UFS_CFG_VALUE_MAX];
  
  // convert value to string and save key = value
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%03_f"), &value);
  UfsCfgSaveKey (pstr_filename, pstr_key, str_value, create);
}

/*********************************************\
 *                 CSV files
\*********************************************/

bool UfsSeekToStart (File &file)
{
  bool done;

  // set to start of file
  done = file.seek (0);

  return done;
}

bool UfsSeekToEnd (File &file)
{
  bool done;

  // get file size and seek to last byte
  done = file.seek (file.size () - 1);

  return done;
}

// extract specific column from given line (first column is 1)
// return size of column string
size_t UfsExtractCsvColumn (const char* pstr_content, const char separator, int column, char* pstr_value, const size_t size_value, const bool till_end_of_line)
{
  bool  search = true;
  int   index  = 0;
  char *pstr_line  = (char*)pstr_content;
  char *pstr_start = pstr_line;
  char *pstr_stop  = nullptr;

  // check parameter
  if ((pstr_content == nullptr) || (pstr_value == nullptr)) return 0;

  // loop to find column
  strcpy (pstr_value, "");
  while (search)
  {
    // if seperator found
    if (*pstr_line == separator)
    {
      // increase to next column and check if target column is reached
      index ++;
      if (index == column) 
      {
        // if value should be all next column
        if (till_end_of_line) strcpy (pstr_value, pstr_start);

        // else value is column content
        else
        {
          strncpy (pstr_value, pstr_start, pstr_line - pstr_start);
          pstr_value[pstr_line - pstr_start] = 0;
        }
      }

      // update start caracter to next column
      else pstr_start = pstr_line + 1;
    }

    // else if end of line reached, fill content is target column was reached
    else if ((*pstr_line == 0) && (index == column - 1)) strcpy (pstr_value, pstr_start);

    // check if loop should be stopped
    if ((index == column) || (*pstr_line == 0)) search = false;

    pstr_line++;
  }

  return strlen (pstr_value);
}

// read next line, skipping empty lines, and return number of caracters in the line
size_t UfsReadNextLine (File &file, char* pstr_line, const size_t size_line)
{
  size_t   index;
  size_t   length = 0;
  uint32_t pos_start;
  char    *pstr_start;
  char    *pstr_token;

  // check parameter & init
  if (pstr_line == nullptr) return 0;
  strcpy (pstr_line, "");

/*
  String  str_line;

  // read lines until non empty line
  while (file.available() && (str_line.length () == 0)) str_line = file.readStringUntil ('\n');
  if (str_line.length () > 0) strlcpy (pstr_line, str_line.c_str (), size_line);
*/
  // read next line
  pos_start = file.position ();
  length = file.readBytes (pstr_line, size_line - 1);
  pstr_line[length] = 0;

  // loop to skip empty lines
  pstr_start = pstr_line;
  while ((*pstr_start == '\n') && (*pstr_start != 0)) pstr_start++;

  // if next end of line found
  pstr_token = strchr (pstr_start, '\n');
  if (pstr_token != nullptr)
  {
    // set end of string
    *pstr_token = 0;

    // seek file to beginning of next line
    file.seek (pstr_token - pstr_line + pos_start + 1);
  } 

  // if needed, remove \n from empty lines
  length = strlen (pstr_start);
  if (pstr_start > pstr_line) for (index = 0; index <= length; index ++) pstr_line[index] = pstr_start[index];

  // give control back to system to avoid watchdog
  yield ();

  return length;


//  return str_line.length ();
}

// read previous line and return number of caracters in the line
size_t UfsReadPreviousLine (File &file, char* pstr_line, const size_t size_line)
{
  uint8_t  current_car;
  uint32_t pos_search;
  uint32_t pos_start = 0;
  uint32_t pos_stop  = 0;
  size_t   length = 0;

  // check parameter
  if (pstr_line == nullptr) return 0;

  // init
  pstr_line[0] = 0;

  // if file exists
  if (file.available ())
  {
    // loop to avoid empty lines
    pos_search = file.position ();
    if (pos_search > 0) do
    {
      pos_search--;
      file.seek (pos_search);
      current_car = (uint8_t)file.read ();
      if (current_car != '\n') pos_stop = pos_search + 1;
    }
    while ((current_car == '\n') && (pos_search > 0));

    // loop to read previous line
    pos_search++;
    if (pos_search > 0) do
    {
      pos_search--;
      file.seek (pos_search);
      current_car = (uint8_t)file.read ();
      if (current_car != '\n') pos_start = pos_search;
    }
    while ((current_car != '\n') && (pos_search > 0));

    // get complete line
    if (pos_stop > pos_start)
    {
      if (current_car == '\n') pos_search++;
      file.seek (pos_search);
      length = file.readBytes (pstr_line, pos_stop - pos_start);
      pstr_line[length] = 0;
      file.seek (pos_search);
    }
  }

  // give control back to system to avoid watchdog
  yield ();

  return length;
}

// rotate files according to file naming convention
//   file-2.csv -> file-3.csv
//   file-1.csv -> file-2.csv
void UfsFileRotate (const char* pstr_filename, const int index_min, const int index_max) 
{
  int  index;
  char str_original[UFS_FILENAME_SIZE];
  char str_target[UFS_FILENAME_SIZE];

  // check parameter
  if (pstr_filename == nullptr) return;

  // rotate previous daily files
  for (index = index_max; index > index_min; index--)
  {
    // generate file names
    sprintf (str_original, pstr_filename, index - 1);
    sprintf (str_target, pstr_filename, index);

    // if target exists, remove it
    if (ffsp->exists (str_target))
    {
      ffsp->remove (str_target);
      AddLog (LOG_LEVEL_INFO, PSTR ("UFS: deleted %s"), str_target);
    }

    // if file exists, rotate it
    if (ffsp->exists (str_original))
    {
      ffsp->rename (str_original, str_target);
      AddLog (LOG_LEVEL_INFO, PSTR ("UFS: renamed %s to %s"), str_original, str_target);
    }

    // give control back to system to avoid watchdog
    yield ();
  }
}

uint32_t UfsGetFileSizeKb (const char* pstr_filename)
{
  uint32_t file_size = 0;
  File     file;

  // check parameter
  if (pstr_filename == nullptr) return file_size;

  // if file exists
  if (ffsp->exists (pstr_filename))
  {
    // open file in read only and get its size
    file = ffsp->open (pstr_filename, "r");
    file_size = (uint32_t)file.size () / 1024;
    file.close ();
  }

  return file_size;
}

#endif    // USE_UFILESYS
