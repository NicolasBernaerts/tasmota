#ifndef FTP_COMMON_H
#define FTP_COMMON_H

#include <stdint.h>
#include <FS.h>
#include <WiFiClient.h>
#include <WString.h>

#ifdef ESP8266
#include <PolledTimeout.h>
using esp8266::polledTimeout::oneShotMs; // import the type to the local namespace
#define BUFFERSIZE TCP_MSS
#define PRINTu32 "lu"
#elif defined ESP32
#include "esp32compat/PolledTimeout.h"
using esp32::polledTimeout::oneShotMs;
#define BUFFERSIZE CONFIG_TCP_MSS
#define PRINTu32 "u"
#endif

#define FTP_SERVER_VERSION "0.9.7-20200529"

#define FTP_CTRL_PORT 21         // Command port on which server is listening
#define FTP_DATA_PORT_PASV 50009 // Data port in passive mode
#define FTP_TIME_OUT 5           // Disconnect client after 5 minutes of inactivity
#define FTP_CMD_SIZE 127         // allow max. 127 chars in a received command

// Use ESP8266 Core Debug functionality
#ifdef DEBUG_ESP_PORT
#define FTP_DEBUG_MSG(fmt, ...)                                          \
    do                                                                   \
    {                                                                    \
        DEBUG_ESP_PORT.printf_P(PSTR("[FTP] " fmt "\n"), ##__VA_ARGS__); \
        yield();                                                         \
    } while (0)
#else
#define FTP_DEBUG_MSG(...)
#endif

#define FTP_CMD(CMD) (FTP_CMD_LE_##CMD) // make command
#define FTP_CMD_LE_USER 0x52455355      // "USER" as uint32_t (little endian)
#define FTP_CMD_BE_USER 0x55534552      // "USER" as uint32_t (big endian)
#define FTP_CMD_LE_PASS 0x53534150      // "PASS" as uint32_t (little endian)
#define FTP_CMD_BE_PASS 0x50415353      // "PASS" as uint32_t (big endian)
#define FTP_CMD_LE_QUIT 0x54495551      // "QUIT" as uint32_t (little endian)
#define FTP_CMD_BE_QUIT 0x51554954      // "QUIT" as uint32_t (big endian)
#define FTP_CMD_LE_CDUP 0x50554443      // "CDUP" as uint32_t (little endian)
#define FTP_CMD_BE_CDUP 0x43445550      // "CDUP" as uint32_t (big endian)
#define FTP_CMD_LE_CWD 0x00445743       // "CWD"  as uint32_t (little endian)
#define FTP_CMD_BE_CWD 0x43574400       // "CWD"  as uint32_t (big endian)
#define FTP_CMD_LE_PWD 0x00445750       // "PWD"  as uint32_t (little endian)
#define FTP_CMD_BE_PWD 0x50574400       // "PWD"  as uint32_t (big endian)
#define FTP_CMD_LE_MODE 0x45444f4d      // "MODE" as uint32_t (little endian)
#define FTP_CMD_BE_MODE 0x4d4f4445      // "MODE" as uint32_t (big endian)
#define FTP_CMD_LE_PASV 0x56534150      // "PASV" as uint32_t (little endian)
#define FTP_CMD_BE_PASV 0x50415356      // "PASV" as uint32_t (big endian)
#define FTP_CMD_LE_PORT 0x54524f50      // "PORT" as uint32_t (little endian)
#define FTP_CMD_BE_PORT 0x504f5254      // "PORT" as uint32_t (big endian)
#define FTP_CMD_LE_STRU 0x55525453      // "STRU" as uint32_t (little endian)
#define FTP_CMD_BE_STRU 0x53545255      // "STRU" as uint32_t (big endian)
#define FTP_CMD_LE_TYPE 0x45505954      // "TYPE" as uint32_t (little endian)
#define FTP_CMD_BE_TYPE 0x54595045      // "TYPE" as uint32_t (big endian)
#define FTP_CMD_LE_ABOR 0x524f4241      // "ABOR" as uint32_t (little endian)
#define FTP_CMD_BE_ABOR 0x41424f52      // "ABOR" as uint32_t (big endian)
#define FTP_CMD_LE_DELE 0x454c4544      // "DELE" as uint32_t (little endian)
#define FTP_CMD_BE_DELE 0x44454c45      // "DELE" as uint32_t (big endian)
#define FTP_CMD_LE_LIST 0x5453494c      // "LIST" as uint32_t (little endian)
#define FTP_CMD_BE_LIST 0x4c495354      // "LIST" as uint32_t (big endian)
#define FTP_CMD_LE_MLSD 0x44534c4d      // "MLSD" as uint32_t (little endian)
#define FTP_CMD_BE_MLSD 0x4d4c5344      // "MLSD" as uint32_t (big endian)
#define FTP_CMD_LE_NLST 0x54534c4e      // "NLST" as uint32_t (little endian)
#define FTP_CMD_BE_NLST 0x4e4c5354      // "NLST" as uint32_t (big endian)
#define FTP_CMD_LE_NOOP 0x504f4f4e      // "NOOP" as uint32_t (little endian)
#define FTP_CMD_BE_NOOP 0x4e4f4f50      // "NOOP" as uint32_t (big endian)
#define FTP_CMD_LE_RETR 0x52544552      // "RETR" as uint32_t (little endian)
#define FTP_CMD_BE_RETR 0x52455452      // "RETR" as uint32_t (big endian)
#define FTP_CMD_LE_STOR 0x524f5453      // "STOR" as uint32_t (little endian)
#define FTP_CMD_BE_STOR 0x53544f52      // "STOR" as uint32_t (big endian)
#define FTP_CMD_LE_MKD 0x00444b4d       // "MKD"  as uint32_t (little endian)
#define FTP_CMD_BE_MKD 0x4d4b4400       // "MKD"  as uint32_t (big endian)
#define FTP_CMD_LE_RMD 0x00444d52       // "RMD"  as uint32_t (little endian)
#define FTP_CMD_BE_RMD 0x524d4400       // "RMD" as uint32_t (big endian)
#define FTP_CMD_LE_RNFR 0x52464e52      // "RNFR" as uint32_t (little endian)
#define FTP_CMD_BE_RNFR 0x524e4652      // "RNFR" as uint32_t (big endian)
#define FTP_CMD_LE_RNTO 0x4f544e52      // "RNTO" as uint32_t (little endian)
#define FTP_CMD_BE_RNTO 0x524e544f      // "RNTO" as uint32_t (big endian)
#define FTP_CMD_LE_FEAT 0x54414546      // "FEAT" as uint32_t (little endian)
#define FTP_CMD_BE_FEAT 0x46454154      // "FEAT" as uint32_t (big endian)
#define FTP_CMD_LE_MDTM 0x4d54444d      // "MDTM" as uint32_t (little endian)
#define FTP_CMD_BE_MDTM 0x4d44544d      // "MDTM" as uint32_t (big endian)
#define FTP_CMD_LE_SIZE 0x455a4953      // "SIZE" as uint32_t (little endian)
#define FTP_CMD_BE_SIZE 0x53495a45      // "SIZE" as uint32_t (big endian)
#define FTP_CMD_LE_SITE 0x45544953      // "SITE" as uint32_t (little endian)
#define FTP_CMD_BE_SITE 0x53495445      // "SITE" as uint32_t (big endian)
#define FTP_CMD_LE_SYST 0x54535953      // "SYST" as uint32_t (little endian)
#define FTP_CMD_BE_SYST 0x53595354      // "SYST" as uint32_t (big endian)

class FTPCommon
{
public:
    // contruct an instance of the FTP Server or Client using a
    // given FS object, e.g. SPIFFS or LittleFS
    FTPCommon(FS &_FSImplementation);
    virtual ~FTPCommon();

    // stops the FTP Server or Client, i.e. stops control and data connections
    virtual void stop();

    // set disconnect timeout in millisecords
    void setTimeout(uint32_t timeoutMs = FTP_TIME_OUT * 60 * 1000);

    // needs to be called frequently (e.g. in loop() )
    // to process ftp requests
    virtual void handleFTP() = 0;

protected:
    WiFiClient control;
    WiFiClient data;

    File file;
    FS &THEFS;

    IPAddress dataIP;   // IP address for PORT (active) mode
    uint16_t dataPort = // holds our PASV port number or the port number provided by PORT
        FTP_DATA_PORT_PASV;
    virtual int8_t dataConnect(); // connects to dataIP:dataPort, returns -1: no data connection possible, +1: data connection established
    bool parseDataIpPort(const char *p);

    uint32_t sTimeOutMs; // disconnect timeout
    oneShotMs aTimeout;  // timeout from esp8266 core library

    bool doFiletoNetwork();
    bool doNetworkToFile();
    virtual void closeTransfer();

    uint16_t allocateBuffer(uint16_t desiredBytes = BUFFERSIZE); // allocate buffer for transfer
    void freeBuffer();
    uint8_t *fileBuffer = NULL; // pointer to buffer for file transfer (by allocateBuffer)
    uint16_t fileBufferSize;    // size of buffer

    uint32_t millisBeginTrans; // store time of beginning of a transaction
    uint32_t bytesTransfered;  // bytes transfered
};

#endif // FTP_COMMON_H
