#include "FTPCommon.h"

FTPCommon::FTPCommon(FS &_FSImplementation) : THEFS(_FSImplementation), sTimeOutMs(FTP_TIME_OUT * 60 * 1000), aTimeout(FTP_TIME_OUT * 60 * 1000)
{
}

FTPCommon::~FTPCommon()
{
    stop();
}

void FTPCommon::stop()
{
    control.stop();
    data.stop();
    file.close();
    freeBuffer();
}

void FTPCommon::setTimeout(uint32_t timeoutMs)
{
    sTimeOutMs = timeoutMs;
}

//
// allocate a big buffer for file transfers
//
uint16_t FTPCommon::allocateBuffer(uint16_t desiredBytes)
{
#if (defined ESP8266)
    uint16_t maxBlock = ESP.getMaxFreeBlockSize() / 2;

    if (desiredBytes > maxBlock)
        desiredBytes = maxBlock;
#endif
    while (fileBuffer == NULL && desiredBytes > 0)
    {
        fileBuffer = (uint8_t *)malloc(desiredBytes);
        if (NULL == fileBuffer)
        {
            FTP_DEBUG_MSG("Cannot allocate buffer for file transfer, re-trying");
            // try with less bytes
            desiredBytes--;
        }
        else
        {
            fileBufferSize = desiredBytes;
        }
    }
    return fileBufferSize;
}

void FTPCommon::freeBuffer()
{
    free(fileBuffer);
    fileBuffer = NULL;
}

int8_t FTPCommon::dataConnect()
{
    // open our own data connection
    data.stop();
    FTP_DEBUG_MSG("Open data connection to %s:%u", dataIP.toString().c_str(), dataPort);
    data.connect(dataIP, dataPort);
    return data.connected() ? 1 : -1;
}

bool FTPCommon::parseDataIpPort(const char *p)
{
    // parse IP and data port of "ip,ip,ip,ip,port,port"
    uint8_t parsecount = 0;
    uint8_t tmp[6];
    while (parsecount < sizeof(tmp))
    {
        tmp[parsecount++] = atoi(p);
        p = strchr(p, ',');
        if (NULL == p || *(++p) == '\0')
            break;
    }
    if (parsecount >= sizeof(tmp))
    {
        // copy first 4 bytes = IP
        for (uint8_t i = 0; i < 4; ++i)
            dataIP[i] = tmp[i];
        // data port is 5,6
        dataPort = tmp[4] * 256 + tmp[5];
        return true;
    }
    return false;
}

bool FTPCommon::doFiletoNetwork()
{
    // data connection lost or no more bytes to transfer?
    if (!data.connected() || (bytesTransfered >= file.size()))
    {
        return false;
    }

    // how many bytes to transfer left?
    uint32_t nb = (file.size() - bytesTransfered);
    if (nb > fileBufferSize)
        nb = fileBufferSize;

    // transfer the file
    FTP_DEBUG_MSG("Transfer %d bytes fs->net", nb);
    nb = file.readBytes((char *)fileBuffer, nb);
    if (nb > 0)
    {
        data.write(fileBuffer, nb);
        bytesTransfered += nb;
    }

    return (nb > 0);
}

bool FTPCommon::doNetworkToFile()
{
    // Avoid blocking by never reading more bytes than are available
    int16_t navail = data.available();

    if (navail > 0)
    {
        if (navail > fileBufferSize)
            navail = fileBufferSize;
        FTP_DEBUG_MSG("Transfer %d bytes net->FS", navail);
        navail = data.read(fileBuffer, navail);
        file.write(fileBuffer, navail);
        bytesTransfered += navail;
    }

    if (!data.connected() && (navail <= 0))
    {
        // connection closed or no more bytes to read
        return false;
    }
    else
    {
        // inidcate, we need to be called again
        return true;
    }
}

void FTPCommon::closeTransfer()
{
    data.stop();
    file.close();
    freeBuffer();
}
