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

// partition constant
#define UFS_PARTITION_MIN_KB            25                  // minimum free space in USF partition (kb)

// configuration file constant
#define UFS_CFG_LINE_LENGTH             64                  // maximum size of a configuration line
#define UFS_CFG_VALUE_MAX               16                  // maximum size of a numerical value (int or float)

// CSV file constant
#define UFS_CSV_LINE_LENGTH             128                 // maximum line length in CSV files
#define UFS_CSV_COLUMN_MAX              32                  // maximum number of columns in CSV files

// action when reading a CSV line
enum UfsCsvAccessType { UFS_CSV_ACCESS_READ, UFS_CSV_ACCESS_WRITE, UFS_CSV_ACCESS_MAX };
enum UfsCsvLineAction { UFS_CSV_NONE, UFS_CSV_NEXT, UFS_CSV_PREVIOUS };

// CSV file management structure
static struct {
  File  file[UFS_CSV_ACCESS_MAX];                           // file object
  bool  is_open[UFS_CSV_ACCESS_MAX] = { false, false };     // flag if file is opened for reading
  bool  has_header[UFS_CSV_ACCESS_MAX] = { false, false };  // flag if file is opened for reading
  int   nb_column = 0;                                      // number of columns in last line
  char  str_line[UFS_CSV_LINE_LENGTH];                      // last read line
  char* pstr_value[UFS_CSV_COLUMN_MAX];                     // array of values in last line
} ufs_csv;

/*********************************************\
 *            Configuration files
\*********************************************/

// read key value in configuration file
bool UfsCfgLoadKey (const char* pstr_filename, const char* pstr_key, char* pstr_value, int size) 
{
  bool finished = false;
  bool found = false;
  int  length;
  char str_line[UFS_CFG_LINE_LENGTH];
  char* pstr_data;
  File file;

  // init
  strcpy (pstr_value, "");

  // if file exists
  if (ffsp->exists (pstr_filename))
  {
    // open file in read only mode in littlefs filesystem
    file = ffsp->open (pstr_filename, "r");

    // loop to read lines
    do
    {
      length = file.readBytesUntil ('\n', str_line, sizeof (str_line));
      if (length > 0)
      {
        // split resulting string
        str_line[length] = 0;
        pstr_data = strchr (str_line, '=');
        if (pstr_data != nullptr)
        {
          // separate key and value
          *pstr_data = 0;
          pstr_data++;

          // check current key
          found = (strcmp (str_line, pstr_key) == 0);
          if (found) strlcpy (pstr_value, pstr_data, size);
        }
      }
      else finished = true;
    } while (!finished && !found);

    // close file
    file.close ();
  }


  return found;
}

// read integer key value in configuration file
int UfsCfgLoadKeyInt (const char* pstr_filename, const char* pstr_key, const int default_value = INT_MAX) 
{
  bool found;
  char str_value[UFS_CFG_VALUE_MAX];
  int  result = INT_MAX;

  // open file in read only mode in littlefs filesystem
  found = UfsCfgLoadKey (pstr_filename, pstr_key, str_value, sizeof (str_value));
  if (found) result = atoi (str_value);
  else result = default_value;

  return result;
}

// read float key value in configuration file
float UfsCfgLoadKeyFloat (const char* pstr_filename, const char* pstr_key, const float default_value = NAN) 
{
  bool  found;
  char  str_value[UFS_CFG_VALUE_MAX];
  float result = NAN;

  // open file in read only mode in littlefs filesystem
  found = UfsCfgLoadKey (pstr_filename, pstr_key, str_value, sizeof (str_value));
  if (found) result = atof (str_value);
  else result = default_value;

  return result;
}

// save key value in configuration file
void UfsCfgSaveKey (const char* pstr_filename, const char* pstr_key, const char* pstr_value, bool create = true) 
{
  char str_line[UFS_CFG_LINE_LENGTH];
  File file;
  
  // retrieve saved settings from config.ini in littlefs filesystem
  if (create) file = ffsp->open (pstr_filename, "w");
  else file = ffsp->open (pstr_filename, "a");

  // write key=value
  sprintf (str_line, "%s=%s\n", pstr_key, pstr_value);
  file.print (str_line);

  // close file
  file.close ();
}

// save key value in configuration file
void UfsCfgSaveKeyInt (const char* pstr_filename, const char* pstr_key, const int value, bool create = true) 
{
  char str_value[UFS_CFG_VALUE_MAX];
  
  // convert value to string
  itoa (value, str_value, 10);

  // save key and value
  UfsCfgSaveKey (pstr_filename, pstr_key, str_value, create);
}

// save key value in configuration file
void UfsCfgSaveKeyFloat (const char* pstr_filename, const char* pstr_key, const float value, bool create = true) 
{
  char str_value[UFS_CFG_VALUE_MAX];
  
  // convert float to string
  ext_snprintf_P (str_value, sizeof (str_value), PSTR ("%03_f"), &value);

  // save key and value
  UfsCfgSaveKey (pstr_filename, pstr_key, str_value, create);
}

/*********************************************\
 *                 CSV files
\*********************************************/

bool UfsCsvSeekToStart ()
{
  // if file is opened
  if (ufs_csv.is_open[UFS_CSV_ACCESS_READ])
  {
    // set to start of file
    ufs_csv.file[UFS_CSV_ACCESS_READ].seek (0);

    // if asked, skip header
    if (ufs_csv.has_header[UFS_CSV_ACCESS_READ]) UfsCsvNextLine ();

    // read first data lined
    UfsCsvNextLine ();
  }

  return ufs_csv.is_open[UFS_CSV_ACCESS_READ];
}

int UfsCsvSeekToEnd ()
{
  int      index = -1;
  uint32_t pos_end;

  // if file is opened
  if (ufs_csv.is_open[UFS_CSV_ACCESS_READ])
  {
    // set to start of file
    pos_end = ufs_csv.file[UFS_CSV_ACCESS_READ].size () - 1;
    ufs_csv.file[UFS_CSV_ACCESS_READ].seek (pos_end);

    // read last line
    index = UfsCsvPreviousLine ();
  }

  return index;
}

// read next line and return number of columns in the line
int UfsCsvNextLine ()
{
  int      index;
  size_t   length;
  uint32_t pos_start, pos_delta;
  char    *pstr_token;

  // init
  ufs_csv.nb_column = 0;

  // read next line
  pos_start = ufs_csv.file[UFS_CSV_ACCESS_READ].position ();
  length = ufs_csv.file[UFS_CSV_ACCESS_READ].readBytes (ufs_csv.str_line, sizeof (ufs_csv.str_line) - 1);
  ufs_csv.str_line[length] = 0;

  // align string and file position on end of line
  pstr_token = strchr (ufs_csv.str_line, '\n');
  if (pstr_token != nullptr)
  {
    pos_delta = pstr_token - ufs_csv.str_line;
    ufs_csv.file[UFS_CSV_ACCESS_READ].seek (pos_start + pos_delta + 1);
    *pstr_token = 0;
  } 

  // loop to populate array of values
  index = 0;
  pstr_token = strtok (ufs_csv.str_line, ";");
  while (pstr_token != nullptr)
  {
    if (index < UFS_CSV_COLUMN_MAX) ufs_csv.pstr_value[index++] = pstr_token;
    pstr_token = strtok (nullptr, ";");
  }
  ufs_csv.nb_column = index;

  return index;
}

// read previous line and return true if line exists
int UfsCsvPreviousLine ()
{
  int      index = -1;
  uint8_t  current_car;
  uint32_t pos_search;
  uint32_t pos_start = UINT32_MAX;
  uint32_t pos_stop  = UINT32_MAX;
  size_t   length;
  char    *pstr_token;
  char     str_buffer[UFS_CSV_LINE_LENGTH];

  // init
  ufs_csv.nb_column = 0;
  strcpy (str_buffer, "");
  current_car = '\n';

  // get current file position
  pos_search = ufs_csv.file[UFS_CSV_ACCESS_READ].position ();

  // loop to avoid empty lines
  while ((current_car == '\n') && (pos_search > 0))
  {
    current_car = (uint8_t)ufs_csv.file[UFS_CSV_ACCESS_READ].read ();

    if (current_car != '\n') pos_stop = pos_search;
    if (pos_search > 0) pos_search--;
    ufs_csv.file[UFS_CSV_ACCESS_READ].seek (pos_search);
  }

  // loop to read previous line
  while ((current_car != '\n') && (pos_search > 0))
  {
    current_car = ufs_csv.file[UFS_CSV_ACCESS_READ].read ();

    if (current_car != '\n')
    {
      if (pos_search > 0) pos_search--;
      ufs_csv.file[UFS_CSV_ACCESS_READ].seek (pos_search);
    }
    pos_start = pos_search;
  }

  // get complete line
  if (pos_stop > pos_start)
  {
    // read current line
    length = ufs_csv.file[UFS_CSV_ACCESS_READ].readBytes (ufs_csv.str_line, pos_stop - pos_start + 1);
    ufs_csv.str_line[length] = 0;

    // position to caracter before line
    if (pos_start > 0) pos_start--;
    ufs_csv.file[UFS_CSV_ACCESS_READ].seek (pos_start);

    // if line is the header and it should be skipped
    if (ufs_csv.has_header[UFS_CSV_ACCESS_READ] && (pos_start == 0)) return -1;

    // loop to populate array of values
    index = 0;
    pstr_token = strtok (ufs_csv.str_line, ";");
    while (pstr_token != nullptr)
    {
      if (index < UFS_CSV_COLUMN_MAX) ufs_csv.pstr_value[index++] = pstr_token;
      pstr_token = strtok (nullptr, ";");
    }
    ufs_csv.nb_column = index;
  }

  return index;
}

bool UfsCsvOpen (const char* pstr_filename, bool has_header)
{
  bool result = false;

  // close file if already opened
  UfsCsvClose (UFS_CSV_ACCESS_READ);
  
  // check parameter
  if (pstr_filename != nullptr) result = ffsp->exists (pstr_filename);

  // if file exists
  if (result)
  {
    // open file in read mode
    ufs_csv.is_open[UFS_CSV_ACCESS_READ]    = true;
    ufs_csv.has_header[UFS_CSV_ACCESS_READ] = has_header;
    ufs_csv.file[UFS_CSV_ACCESS_READ]       = ffsp->open (pstr_filename, "r");

    // if asked, skip header
    if (has_header) UfsCsvNextLine ();

    // read first data lined
    UfsCsvNextLine ();
  }

  return ufs_csv.is_open[UFS_CSV_ACCESS_READ];
}

void UfsCsvClose (int access_type)
{
  if (ufs_csv.is_open[access_type])
  {
    // close current file
    ufs_csv.file[access_type].close ();

    // init file caracteristics
    ufs_csv.is_open[access_type] = false;
    if (access_type == UFS_CSV_ACCESS_READ) ufs_csv.nb_column = 0;
  }
}

// read value from a column in current line
//  column : column where to read data
//  action : can change current line before or after reading
int UfsCsvGetColumnInt (const int column, int action = UFS_CSV_NONE)
{
  int result = INT_MAX;

  if (ufs_csv.is_open[UFS_CSV_ACCESS_READ])
  {
    // if needed, load previous or next line
    if (action == UFS_CSV_PREVIOUS) UfsCsvPreviousLine ();
    else if (action == UFS_CSV_NEXT) UfsCsvNextLine ();

    // if ok, extract data
    if (column < ufs_csv.nb_column) result = atoi (ufs_csv.pstr_value[column]);
  }

  return result;
}

// read value from a column in current line
long UfsCsvGetColumnLong (const int column, int action = UFS_CSV_NONE)
{
  long result = LONG_MAX;

  if (ufs_csv.is_open[UFS_CSV_ACCESS_READ])
  {
    // if needed, load previous or next line
    if (action == UFS_CSV_PREVIOUS) UfsCsvPreviousLine ();
    else if (action == UFS_CSV_NEXT) UfsCsvNextLine ();

    // if ok, extract data
    if (column < ufs_csv.nb_column) result = atol (ufs_csv.pstr_value[column]);
  }

  return result;
}

// read value from a column in current line
float UfsCsvGetColumnFloat (const int column, int action = UFS_CSV_NONE)
{
  float result = NAN;

  if (ufs_csv.is_open[UFS_CSV_ACCESS_READ])
  {
    // if needed, load previous or next line
    if (action == UFS_CSV_PREVIOUS) UfsCsvPreviousLine ();
    else if (action == UFS_CSV_NEXT) UfsCsvNextLine ();

    // if ok, extract data
    if (column < ufs_csv.nb_column) result = atof (ufs_csv.pstr_value[column]);
  }

  return result;
}

// add a line to a CSV file
void UfsCsvAppend (const char* pstr_filename, const char* pstr_line, bool keep_open = false) 
{
  bool exists;
  int  position;

  // check parameters
  if ((pstr_filename == nullptr) || (pstr_line == nullptr)) return;
  if ((strlen (pstr_filename) == 0) || (strlen (pstr_line) == 0)) return;

  // if file is not already opened
  if (!ufs_csv.is_open[UFS_CSV_ACCESS_WRITE])
  {
    // check if file already exists and open in append mode
    ufs_csv.file[UFS_CSV_ACCESS_WRITE] = ffsp->open (pstr_filename, "a");
    ufs_csv.is_open[UFS_CSV_ACCESS_WRITE] = true;
  }

  // append current line
  if (ufs_csv.is_open[UFS_CSV_ACCESS_WRITE])
  {
    // write the string
    ufs_csv.file[UFS_CSV_ACCESS_WRITE].print (pstr_line);

    // append '\n' if needed
    position = strlen (pstr_line) - 1;
    if (pstr_line[position] != '\n') ufs_csv.file[UFS_CSV_ACCESS_WRITE].print ("\n");
  }

  // if needed, close file
  if (ufs_csv.is_open[UFS_CSV_ACCESS_WRITE] && !keep_open)
  {
    ufs_csv.file[UFS_CSV_ACCESS_WRITE].close ();
    ufs_csv.is_open[UFS_CSV_ACCESS_WRITE] = false;
  }
}

// rotate files according to file naming convention
//  file-2.csv -> file-3.csv
//  file-1.csv -> file-2.csv
void UfsCsvFileRotate (const char* pstr_filename, const int index_min, const int index_max) 
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

uint32_t UfsCsvGetFileSizeKb (const char* pstr_filename)
{
  uint32_t file_size = 0;
  File     file;

  // check parameter
  if (pstr_filename == nullptr) return 0;

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

// cleanup filesystem from oldest CSV files according to free size left (in Kb)
void UfsCsvCleanupFileSystem (uint32_t size_minimum) 
{
  bool     is_candidate;
  bool     removal_done = true;
  uint32_t size_available;
  time_t   file_time;
  char     str_filename[UFS_FILENAME_SIZE];
  File     root_dir; 

  // open root directory
  root_dir = dfsp->open ("/", UFS_FILE_READ);

  // loop till minimum space is available
  size_available = UfsInfo (1, 0);
  while (removal_done && (size_available < size_minimum))
  {
    // init
    removal_done = false;
    strcpy (str_filename, "");
    file_time = time (NULL);

    // loop thru filesystem to get oldest CSV file
    while (true) 
    {
      // read next file
      File current_file = root_dir.openNextFile();
      if (!current_file) break;

      // check if file is candidate for removal
      is_candidate  = (strstr (current_file.name (), ".csv") != nullptr);
      is_candidate |= (strstr (current_file.name (), ".CSV") != nullptr);
      is_candidate &= (current_file.getLastWrite () < file_time);

      // if candidate, save file name
      if (is_candidate)
      {
        strcpy (str_filename, "/");
        strlcat (str_filename, current_file.name (), sizeof (str_filename));
        file_time = current_file.getLastWrite ();
      }

      // close file
      current_file.close ();
    }

    // remove oldest CSV file
    if (strlen (str_filename) > 0) removal_done = ffsp->remove (str_filename);
    if (removal_done) AddLog (LOG_LEVEL_INFO, PSTR ("UFS: Purged %s"), str_filename);

    // read new space left
    size_available = UfsInfo (1, 0);

    // give control back to system to avoid watchdog
    yield ();
  }

  // close directory
  root_dir.close ();
}

#endif    // USE_UFILESYS
