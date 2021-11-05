/** \mainpage FTPClient library
 *
 * MIT license
 * written by Daniel Plasa:
 * Inspired by https://github.com/danbicks and his code posted in https://github.com/esp8266/Arduino/issues/1183#issuecomment-634556135
 *
 * The Client supports two ways of getting/putting files:
 *    a)  blocking, returns only after transfer complete (or error)
 *    b)  non-blocking, returns immedeate. call check() for status of process
 * 
 *    When using non-blocking mode, be sure to call update() frequently, e.g. in loop().
 */

#ifndef FTP_CLIENT_H
#define FTP_CLIENT_H

/*******************************************************************************
 **                                                                            **
 **                       DEFINITIONS FOR FTP SERVER/CLIENT                    **
 **                                                                            **
 *******************************************************************************/
#include <FS.h>
#include "FTPCommon.h"

class FTPClient : public FTPCommon
{
public:
	struct ServerInfo
	{
		ServerInfo(const String &_l, const String &_pw, const String &_sn, uint16_t _p = 21, bool v = false) : login(_l), password(_pw), servername(_sn), port(_p), validateCA(v) {}
		ServerInfo() = default;
		String login;
		String password;
		String servername;
		uint16_t port;
		bool authTLS = false;
		bool validateCA = false;
	};

	typedef enum
	{
		OK,
		PROGRESS,
		ERROR,
	} TransferResult;

	static constexpr int16_t errorLocalFile = -1;
	static constexpr int16_t errorAlreadyInProgress = -2;
	static constexpr int16_t errorConnectionFailed = -3;
	static constexpr int16_t errorServerResponse = -4;
	static constexpr int16_t errorDataConnectionFailed = -5;
	static constexpr int16_t errorUninitialized = -6;
	static constexpr int16_t errorTimeout = -7;
	static constexpr int16_t errorMemory = -8;

	typedef struct
	{
		TransferResult result;
		int16_t code;
		String desc;
	} Status;

	typedef enum
	{
		FTP_PUT = 1 | 0x80,
		FTP_GET = 2 | 0x80,
		FTP_PUT_NONBLOCKING = FTP_PUT & 0x7f,
		FTP_GET_NONBLOCKING = FTP_GET & 0x7f,
	} TransferType;

	// contruct an instance of the FTP Client using a
	// given FS object, e.g. SPIFFS or LittleFS
	FTPClient(FS &_FSImplementation);

	// initialize FTP Client with the ftp server's credentials
	void begin(const ServerInfo &server);

	// transfer a file (nonblocking via handleFTP() )
	const Status &transfer(const String &localFileName, const String &remoteFileName, TransferType direction = FTP_GET);

	// check status
	const Status &check();

	// call freqently (e.g. in loop()), when using non-blocking mode
	void handleFTP();

protected:
	typedef enum
	{
		cConnect = 0,
		cGreet,
		cUser,
		cPassword,
		cPassive,
		cData,
		cTransfer,
		cFinish,
		cQuit,
		cIdle,
		cTimeout,
		cError
	} internalState;
	internalState ftpState = cIdle;
	Status _serverStatus;
	uint32_t waitUntil = 0;
	const ServerInfo *_server = nullptr;

	String _remoteFileName;
	TransferType _direction;

	int8_t controlConnect(); // connects to ServerInfo, returns -1: no connection possible, +1: connection established

	bool waitFor(const int16_t respCode, const __FlashStringHelper *errorString = nullptr, uint32_t timeOut = 10000);
};

#endif // FTP_CLIENT_H
