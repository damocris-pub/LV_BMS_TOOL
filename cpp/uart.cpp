#include <string.h>
#include <windows.h>
#include <thread>
#include <vector>
#include "uart.h"
#include "dfu_common.h"
#include "printf.h"

static HANDLE gSerial;

/* Old protocol for wifi upgrading */
const uint8_t highBaudRateCmd[] = {
    0x5A, 0xC0, 0x00, 0x00, 0x40, 0xA5,
};

const uint8_t lowBaudRateCmd[] = {
    WIFI_SOP, 0xC4, 0x00, 0x00, 0x3C, WIFI_EOP, 
};

const uint8_t requestUpdateCmd[] = {
    WIFI_SOP, 0xC0, 0x00, 0x00, 0x40, WIFI_EOP, 
};

const uint8_t requestUpdateResponse[] = {
    WIFI_SOP, 0xA0, 0x00, 0x00, 0x01, 0x5F, WIFI_EOP,
};

const uint8_t requestCompleteCmd[] = {
    WIFI_SOP, 0xC3, 0x00, 0x00, 0x3D, WIFI_EOP,
};

const uint8_t requestCompleteReseponse[] = {
    WIFI_SOP, 0xA3, 0x00, 0x00, 0x06, 0x57, WIFI_EOP,
};

static uint8_t wifi_checksum(uint8_t *buffer, int len)
{
    uint8_t sum = 0;
    for (int i=0; i<len; ++i) {
        sum += buffer[i];
    }
    sum = (~sum + 1) & 0xFF;
    return sum;  
}

bool uart_connect(const char *port, uint32_t baud_rate)
{
    gSerial = CreateFileA(port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (gSerial == INVALID_HANDLE_VALUE) {
        return false;
    }
    DCB serial_params = {0};
    serial_params.DCBlength = sizeof(serial_params);
    if (!GetCommState(gSerial, &serial_params)) {
        return false;
    }
    serial_params.BaudRate = baud_rate;
    serial_params.ByteSize = 8;
    serial_params.StopBits = ONESTOPBIT;
    serial_params.Parity = NOPARITY;
    if (!SetCommState(gSerial, &serial_params)) {
        return false;
    }
    COMMTIMEOUTS timeout = {0};
    timeout.ReadIntervalTimeout = 50;   //in ms
    timeout.ReadTotalTimeoutConstant = 500;     //in ms , uart response time-out 
    timeout.ReadTotalTimeoutMultiplier = 10;    //in ms for every byte
    timeout.WriteTotalTimeoutConstant = 500;
    timeout.WriteTotalTimeoutMultiplier = 10;   //in ms for every byte
    SetCommTimeouts(gSerial, &timeout);
    return true;
}

bool uart_disconnect(void)
{
    return CloseHandle(gSerial);
}

//wifi upgrading
bool uart_changeHostBaud(uint32_t baud_rate)
{
    FlushFileBuffers(gSerial);
    DCB serial_params = {0};
    serial_params.DCBlength = sizeof(serial_params);
    if (!GetCommState(gSerial, &serial_params)) {
        return false;
    }
    serial_params.BaudRate = baud_rate;
    if (!SetCommState(gSerial, &serial_params)) {
        return false;
    }
    PurgeComm(gSerial, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return true;
}

bool uart_requestSlaveBaud(bool to_high)
{
    DWORD bytesWritten;
    if (to_high) {
        if (!WriteFile(gSerial, highBaudRateCmd, sizeof(highBaudRateCmd), &bytesWritten, NULL)) {
            printf_("cannot send out request update command to UART\n");
            return false;
        }
    } else {
        if (!WriteFile(gSerial, lowBaudRateCmd, sizeof(lowBaudRateCmd), &bytesWritten, NULL)) {
            printf_("cannot send out request update command to UART\n");
            return false;
        }
    }
    if (bytesWritten != sizeof(highBaudRateCmd)) {
        printf_("couldn't send enough bytes to UART\n");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return true;
}

bool uart_requestUpgrade(void)
{
    char buffer[8];
    DWORD bytesWritten, bytesRead;
    if (!WriteFile(gSerial, requestUpdateCmd, sizeof(requestUpdateCmd), &bytesWritten, NULL)) {
        printf_("cannot send out request update command to UART\n");
        return false;
    }
    if (bytesWritten != sizeof(requestUpdateResponse)) {
        printf_("couldn't send enough bytes to UART\n");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(requestUpdateResponse), &bytesRead, NULL)) {
        printf_("cannot read request update response from UART\n");
        return false;
    }
    if (bytesRead != sizeof(requestUpdateResponse)) {
        printf_("don't receive enough bytes from UART\n");
        return false;
    }
    if (memcmp(buffer, requestUpdateResponse, sizeof(requestUpdateResponse)) != 0) {
        printf_("received response for request update command is not expected\n");
        return false;
    }
    return true;
}

bool uart_sendFrameCount(uint16_t cnt)
{
    //every frame has 512 bytes
    uint8_t buffer[8];
    DWORD bytesWritten, bytesRead;
    buffer[0] = WIFI_SOP;
    buffer[1] = WIFI_UPGRADE_LENGTH;
    buffer[2] = cnt >> 8;   //msb
    buffer[3] = cnt & 0xFF;   //lsb
    buffer[4] = wifi_checksum(buffer + 1, 3);
    buffer[5] = WIFI_EOP;
    if (!WriteFile(gSerial, buffer, 6, &bytesWritten, NULL)) {
        printf_("cannot send out request update command to UART\n");
        return false;
    }
    if (bytesWritten != 6) {
        printf_("couldn't send enough bytes to UART\n");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, 7, &bytesRead, NULL)) {
        printf_("cannot read request update response from UART\n");
        return false;
    }
    if (bytesRead != 7) {
        printf_("couldn't receive enough bytes from UART\n");
        return false;
    }
    if (buffer[0] != WIFI_SOP && buffer[6] != WIFI_EOP) {
        printf_("response packet SOF and EOF error\n");
        return false;
    }
    if (wifi_checksum(buffer + 1, 5) != 0x00) {
        printf_("reponse packet checksum error\n");
        return false;
    }
    return true;
}

//len <= 0x200
int uart_sendFrameData(uint16_t seq, uint16_t len, uint8_t *dat)
{
    uint8_t buffer[0x206];
    DWORD bytesWritten, bytesRead;
    buffer[0] = WIFI_SOP;
    buffer[1] = WIFI_UPGRADE_DATA;
    buffer[2] = seq >> 8;   //msb
    buffer[3] = seq & 0xFF; //lsb
    memcpy(buffer+4, dat, len);
    if (len < 0x200) {
        memset(buffer+4+len, 0x00, 0x200-len);
    }
    buffer[0x204] = wifi_checksum(buffer + 4, 0x200);
    buffer[0x205] = WIFI_EOP;
    if (!WriteFile(gSerial, buffer, 0x206, &bytesWritten, NULL)) {
        printf_("cannot send out request update command to UART\n");
        return -1;
    }
    if (bytesWritten != 0x206) {
        printf_("couldn't send enough bytes to UART\n");
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, 7, &bytesRead, NULL)) {
        printf_("cannot read request update response from UART\n");
        return -1;
    }
    if (bytesRead != 7) {
        printf_("couldn't receive enough bytes from UART\n");
        return -1;
    }
    if (buffer[0] != WIFI_SOP && buffer[6] != WIFI_EOP) {
        printf_("response packet SOF and EOF error\n");
        return -1;
    }
    if (wifi_checksum(buffer + 1, 5) != 0x00) {
        printf_("reponse packet checksum error\n");
        return -1;
    }
    if (buffer[4] != 0x03) {
        return 0;   //need host repeating 
    }
    return 1;   //successful 
}

bool uart_requestComplete(void)
{
    uint8_t buffer[8];
    DWORD bytesWritten, bytesRead;
    if (!WriteFile(gSerial, requestCompleteCmd, sizeof(requestCompleteCmd), &bytesWritten, NULL)) {
        printf_("cannot send out request update command to UART\n");
        return false;
    }
    if (bytesWritten != sizeof(requestCompleteCmd)) {
        printf_("couldn't send enough bytes to UART\n");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(requestCompleteReseponse), &bytesRead, NULL)) {
        printf_("cannot read request update response from UART\n");
        return false;
    }
    if (bytesRead != sizeof(requestCompleteReseponse)) {
        printf_("don't receive enough bytes from UART\n");
        return false;
    }
    if (memcmp(buffer, requestCompleteReseponse, sizeof(requestCompleteReseponse)) != 0) {
        printf_("received response for request complete command is not expected\n");
        return false;
    }
    return true;
}


//RS-485 upgrading
int uart_prepareCmd(uint8_t addr, uint8_t *resp)
{
    uint8_t buffer[16];
    DWORD bytesWritten, bytesRead;
    prepareCmd[CMD_LEN_OFFSET] = addr;
    uint16_t crc = crc16(&prepareCmd[CMD_LEN_OFFSET], 4+1, 0xffff);
    prepareCmd[CMD_CRC_OFFSET(4)] = crc & 0xFF;
    prepareCmd[CMD_CRC_OFFSET(4) + 1] = (crc >> 8) & 0xFF;
    if (!WriteFile(gSerial, prepareCmd, 9, &bytesWritten, NULL)) {
        printf_("cannot send out prepare update command to UART\n");
        return -1;
    }
    if (bytesWritten != 9) {
        printf_("couldn't send enough bytes to UART\n");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(10), &bytesRead, NULL)) {
        printf_("cannot get prepare update response from UART\n");
        return -1;
    }
    if (bytesRead != 10) {
        printf_("don't receive enough bytes from UART\n");
        return -1;
    }
    if (!verifyPrepare(buffer, true)) {
        return -1;
    }
    resp[0] = buffer[6];    //get the cell num
    return 1;
}

int uart_getBootloaderVerCmd(uint8_t addr, uint8_t *resp)
{
    uint8_t buffer[16];
    DWORD bytesWritten, bytesRead;
    getBootloaderVerCmd[CMD_ADR_OFFSET] = addr;
    uint16_t crc = crc16(&getBootloaderVerCmd[CMD_LEN_OFFSET], 4+1, 0xffff);
    getBootloaderVerCmd[CMD_CRC_OFFSET(4)] = crc & 0xFF;
    getBootloaderVerCmd[CMD_CRC_OFFSET(4) + 1] = (crc >> 8) & 0xFF;
    if (!WriteFile(gSerial, getBootloaderVerCmd, 9, &bytesWritten, NULL)) {
        printf_("cannot send out getBootloaderVer command to UART\n");
        return -1;
    }
    if (bytesWritten != 9) {
        printf_("couldn't send enough bytes to UART\n");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(12), &bytesRead, NULL)) {
        printf_("cannot get getBootloaderVer response from UART\n");
        return -1;
    }
    if (bytesRead != 12) {
        printf_("don't receive enough bytes from UART\n");
        return -1;
    }
    if (!verifyGetBootloaderVer(buffer, true)) {
        return -1;
    }
    memcpy(resp, &buffer[RSP_DAT_OFFSET - 1], 5);
    return 5;
}

int uart_getBatterySN(uint8_t addr, uint8_t *resp)
{
    uint8_t buffer[16];
    DWORD bytesWritten, bytesRead;
    getBatterySNCmd[CMD_ADR_OFFSET] = addr;
    uint16_t crc = crc16(&getBatterySNCmd[CMD_LEN_OFFSET], 4+1, 0xffff);
    getBatterySNCmd[CMD_CRC_OFFSET(4)] = crc & 0xFF;
    getBatterySNCmd[CMD_CRC_OFFSET(4) + 1] = (crc >> 8) & 0xFF;
    if (!WriteFile(gSerial, getBatterySNCmd, 9, &bytesWritten, NULL)) {
        printf_("cannot send out getBatterySN command to UART\n");
        return -1;
    }
    if (bytesWritten != 9) {
        printf_("couldn't send enough bytes to UART\n");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(12), &bytesRead, NULL)) {
        printf_("cannot getBatterySN response from UART\n");
        return -1;
    }
    if (bytesRead != 12) {
        printf_("don't receive enough bytes from UART\n");
        return -1;
    }
#if 0
    if (!verifyGetBootloaderVer(buffer, true)) {
        return -1;
    }
    memcpy(resp, &buffer[RSP_DAT_OFFSET - 1], 5);
#endif 
    return 30;
}

int uart_getHardwareInfoCmd(uint8_t addr, uint8_t *resp)
{
    uint8_t buffer[16];
    DWORD bytesWritten, bytesRead;
    getHardwareInfoCmd[CMD_ADR_OFFSET] = addr;
    uint16_t crc = crc16(&getHardwareInfoCmd[CMD_LEN_OFFSET], 4+1, 0xffff);
    getHardwareInfoCmd[CMD_CRC_OFFSET(4)] = crc & 0xFF;
    getHardwareInfoCmd[CMD_CRC_OFFSET(4) + 1] = (crc >> 8) & 0xFF;
    if (!WriteFile(gSerial, getHardwareInfoCmd, 9, &bytesWritten, NULL)) {
        printf_("cannot send out getHardwareInfo command to UART\n");
        return -1;
    }
    if (bytesWritten != 9) {
        printf_("couldn't send enough bytes to UART\n");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(12), &bytesRead, NULL)) {
        printf_("cannot getHardwareInfo response from UART\n");
        return -1;
    }
    if (bytesRead != 12) {
        printf_("don't receive enough bytes from UART\n");
        return -1;
    }
    if (!verifyGetHardwareInfo(buffer, true)) {
        return -1;
    }
    memcpy(resp, &buffer[RSP_DAT_OFFSET-1], 5);
    return 5;
}

int uart_getHardwareTypeCmd(uint8_t addr, uint8_t *resp)
{
    uint8_t buffer[16];
    DWORD bytesWritten, bytesRead;
    getHardwareTypeCmd[CMD_ADR_OFFSET] = addr;
    uint16_t crc = crc16(&getHardwareTypeCmd[CMD_LEN_OFFSET], 4+1, 0xffff);
    getHardwareTypeCmd[CMD_CRC_OFFSET(4)] = crc & 0xFF;
    getHardwareTypeCmd[CMD_CRC_OFFSET(4) + 1] = (crc >> 8) & 0xFF;
    if (!WriteFile(gSerial, getHardwareTypeCmd, 9, &bytesWritten, NULL)) {
        printf_("cannot send out getHardwareType command to UART\n");
        return -1;
    }
    if (bytesWritten != 9) {
        printf_("couldn't send enough bytes to UART\n");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(12), &bytesRead, NULL)) {
        printf_("cannot get getHardwareType response from UART\n");
        return -1;
    }
    if (bytesRead != 12) {
        printf_("don't receive enough bytes from UART\n");
        return -1;
    }
    if (!verifyGetHardwareType(buffer, true)) {
        return -1;
    }
    memcpy(resp, &buffer[RSP_DAT_OFFSET-1], 5);
    return 5;
}

int uart_getApplicationVerCmd(uint8_t addr, uint8_t *resp)
{
    uint8_t buffer[16];
    DWORD bytesWritten, bytesRead;
    getApplicationVerCmd[CMD_ADR_OFFSET] = addr;
    uint16_t crc = crc16(&getApplicationVerCmd[CMD_LEN_OFFSET], 4+1, 0xffff);
    getApplicationVerCmd[CMD_CRC_OFFSET(4)] = crc & 0xFF;
    getApplicationVerCmd[CMD_CRC_OFFSET(4) + 1] = (crc >> 8) & 0xFF;
    if (!WriteFile(gSerial, getApplicationVerCmd, 9, &bytesWritten, NULL)) {
        printf_("cannot send out getApplicationVer command to UART\n");
        return -1;
    }
    if (bytesWritten != 9) {
        printf_("couldn't send enough bytes to UART\n");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(12), &bytesRead, NULL)) {
        printf_("cannot get getApplicationVer response from UART\n");
        return -1;
    }
    if (bytesRead != 12) {
        printf_("don't receive enough bytes from UART\n");
        return -1;
    }
    if (!verifyGetApplicationVer(buffer, true)) {
        return -1;
    }
    memcpy(resp, &buffer[RSP_DAT_OFFSET-1], 5);
    return 5;
}

int uart_getPacketLenCmd(uint8_t addr, uint8_t *resp)
{
    uint8_t buffer[16];
    DWORD bytesWritten, bytesRead;
    getPacketLenCmd[CMD_ADR_OFFSET] = addr;
    uint16_t crc = crc16(&getPacketLenCmd[CMD_LEN_OFFSET], 4+1, 0xffff);
    getPacketLenCmd[CMD_CRC_OFFSET(4)] = crc & 0xFF;
    getPacketLenCmd[CMD_CRC_OFFSET(4) + 1] = (crc >> 8) & 0xFF;
    if (!WriteFile(gSerial, getPacketLenCmd, 9, &bytesWritten, NULL)) {
        printf_("cannot send out getPacketLen command to UART\n");
        return -1;
    }
    if (bytesWritten != 9) {
        printf_("couldn't send enough bytes to UART\n");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(12), &bytesRead, NULL)) {
        printf_("cannot getPacketLen response from UART\n");
        return -1;
    }
    if (bytesRead != 12) {
        printf_("don't receive enough bytes from UART\n");
        return -1;
    }
    if (!verifyGetPacketLen(buffer, true)) {
        return -1;
    }
    memcpy(resp, &buffer[RSP_DAT_OFFSET-1], 4);
    return 4;
}

int uart_setPacketLenCmd(uint8_t addr, uint16_t packetLen)
{
    uint8_t buffer[16];
    if (packetLen != 8 && packetLen != 16 && packetLen != 32 && packetLen != 64 &&
        packetLen != 128 && packetLen != 256 && packetLen != 512) {
        printf_("packetLen should be 8, 16, 32, 64, 128, 256, 512\n");
		return -1;
    }
    DWORD bytesWritten, bytesRead;
    setPacketLenCmd[CMD_ADR_OFFSET] = addr;
    setPacketLenCmd[CMD_DAT_OFFSET] = packetLen & 0xFF;
    setPacketLenCmd[CMD_DAT_OFFSET + 2] = 0x00;
    setPacketLenCmd[CMD_DAT_OFFSET + 3] = 0x00;
    setPacketLenCmd[CMD_DAT_OFFSET + 1] = (packetLen >> 8) & 0xFF;
    uint16_t crc = crc16(&setPacketLenCmd[CMD_LEN_OFFSET], 4+1, 0xffff);
    setPacketLenCmd[CMD_CRC_OFFSET(4)] = crc & 0xFF;
    setPacketLenCmd[CMD_CRC_OFFSET(4) + 1] = (crc >> 8) & 0xFF;
    if (!WriteFile(gSerial, setPacketLenCmd, 11, &bytesWritten, NULL)) {
        printf_("cannot send out setPacketLen command to UART\n");
        return -1;
    }
    if (bytesWritten != 11) {
        printf_("couldn't send enough bytes to UART\n");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(8), &bytesRead, NULL)) {
        printf_("cannot get setPacketLen response from UART\n");
        return -1;
    }
    if (bytesRead != 8) {
        printf_("don't receive enough bytes from UART\n");
        return -1;
    }
    if (!verifySetPacketLen(buffer, true)) {
        return -1;
    }
    return 0;
}

int uart_setApplicationLenCmd(uint8_t addr, uint32_t applicationLen, uint8_t *resp)
{
    uint8_t buffer[16];
    DWORD bytesWritten, bytesRead;
    setApplicationLenCmd[CMD_ADR_OFFSET] = addr;
    setApplicationLenCmd[CMD_DAT_OFFSET] = applicationLen & 0xFF;
    setApplicationLenCmd[CMD_DAT_OFFSET + 1] = (applicationLen >> 8) & 0xFF;
    setApplicationLenCmd[CMD_DAT_OFFSET + 2] = (applicationLen >> 16) & 0xFF;
    setApplicationLenCmd[CMD_DAT_OFFSET + 3] = (applicationLen >> 24) & 0xFF;
    uint16_t crc = crc16(&setApplicationLenCmd[CMD_LEN_OFFSET], 4+1, 0xffff);
    setApplicationLenCmd[CMD_CRC_OFFSET(4)] = crc & 0xFF;
    setApplicationLenCmd[CMD_CRC_OFFSET(4) + 1] = (crc >> 8) & 0xFF;
    if (!WriteFile(gSerial, setApplicationLenCmd, 11, &bytesWritten, NULL)) {
        printf_("cannot send out setApplicationLen command to UART\n");
        return -1;
    }
    if (bytesWritten != 11) {
        printf_("couldn't send enough bytes to UART\n");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(12), &bytesRead, NULL)) {
        printf_("cannot get setApplicationLen response from UART\n");
        return -1;
    }
    if (bytesRead != 12) {
        printf_("don't receive enough bytes from UART\n");
        return -1;
    }
    if (!verifySetApplicationLen(buffer, true)) {
        return -1;
    }
    memcpy(resp, &buffer[RSP_DAT_OFFSET], 4);
    return 4;
}

int uart_setPacketSeqCmd(uint8_t addr, uint16_t packetSeq, uint8_t *resp)
{
    uint8_t buffer[16];
    DWORD bytesWritten, bytesRead;
    setPacketSeqCmd[CMD_ADR_OFFSET] = addr;
    setPacketSeqCmd[CMD_DAT_OFFSET] = packetSeq & 0xFF;
    setPacketSeqCmd[CMD_DAT_OFFSET + 1] = (packetSeq >> 8) & 0xFF;
    uint16_t crc = crc16(&setPacketSeqCmd[CMD_LEN_OFFSET], 4+1, 0xffff);
    setPacketSeqCmd[CMD_CRC_OFFSET(4)] = crc & 0xFF;
    setPacketSeqCmd[CMD_CRC_OFFSET(4) + 1] = (crc >> 8) & 0xFF;
    if (!WriteFile(gSerial, setPacketSeqCmd, 9, &bytesWritten, NULL)) {
        printf_("cannot send out setPacketSeq command to UART\n");
        return -1;
    }
    if (bytesWritten != 9) {
        printf_("couldn't send enough bytes to UART\n");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(12), &bytesRead, NULL)) {
        printf_("cannot get setPacketSeq response from UART\n");
        return -1;
    }
    if (bytesRead != 12) {
        printf_("don't receive enough bytes from UART\n");
        return -1;
    }
    if (!verifySetPacketSeq(buffer, true)) {
        return -1;
    }
    memcpy(resp, &buffer[RSP_DAT_OFFSET], 2);
    return 2;
}

int uart_setPacketAddrCmd(uint8_t addr, uint32_t packetAddr, uint8_t *resp)
{
    uint8_t buffer[16];
    DWORD bytesWritten, bytesRead;
    setPacketAddrCmd[CMD_ADR_OFFSET] = addr;
    setPacketAddrCmd[CMD_DAT_OFFSET] = packetAddr & 0xFF;
    setPacketAddrCmd[CMD_DAT_OFFSET + 1] = (packetAddr >> 8) & 0xFF;
    setPacketAddrCmd[CMD_DAT_OFFSET + 2] = (packetAddr >> 16) & 0xFF;
    setPacketAddrCmd[CMD_DAT_OFFSET + 3] = (packetAddr >> 24) & 0xFF;
    uint16_t crc = crc16(&setPacketAddrCmd[CMD_LEN_OFFSET], 4+1, 0xffff);
    setPacketAddrCmd[CMD_CRC_OFFSET(4)] = crc & 0xFF;
    setPacketAddrCmd[CMD_CRC_OFFSET(4) + 1] = (crc >> 8) & 0xFF;
    if (!WriteFile(gSerial, setPacketAddrCmd, 11, &bytesWritten, NULL)) {
        printf_("cannot send out setPacketAddr command to UART\n");
        return -1;
    }
    if (bytesWritten != 11) {
        printf_("couldn't send enough bytes to UART\n");
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(12), &bytesRead, NULL)) {
        printf_("cannot get setPacketAddr response from UART\n");
        return -1;
    }
    if (bytesRead != 12) {
        printf_("don't receive enough bytes from UART\n");
        return -1;
    }
    if (!verifySetPacketSeq(buffer, true)) {
        return -1;
    }
    memcpy(resp, &buffer[RSP_DAT_OFFSET], 4);
    return 4;
}

int uart_sendPacketData(uint16_t packetLen, uint8_t *data)
{
    uint8_t buffer[512];
    DWORD bytesWritten, bytesRead;
    if (packetLen != 8 && packetLen != 16 && packetLen != 32 && packetLen != 64 &&
        packetLen != 128 && packetLen != 256 && packetLen != 512) {
        printf_("packetLen should be 8, 16, 32, 64, 128, 256, 512\n");
		return -1;
    }
    buffer[CMD_SOP_OFFSET] = DFU_DAT_SOP;
    memcpy(buffer + 1, data, packetLen);
    uint16_t crc = crc16(&buffer[CMD_SOP_OFFSET], packetLen, 0xffff);
    buffer[CMD_CRC_OFFSET(packetLen)] = crc & 0xFF;
    buffer[CMD_CRC_OFFSET(packetLen) + 1] = (crc >> 8) & 0xFF;
    buffer[CMD_EOP_OFFSET(packetLen)] = DFU_CMD_EOP;
    if (!WriteFile(gSerial, buffer, packetLen + 4, &bytesWritten, NULL)) {
        printf_("cannot send out sendPacketData command to UART\n");
        return -1;
    }
    if (bytesWritten != packetLen + 4) {
        printf_("couldn't send enough bytes to UART\n");
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
#if 0
    if (!ReadFile(gSerial, buffer, sizeof(8), &bytesRead, NULL)) {
        printf_("cannot get sendPacketData response from UART\n");
        return -1;
    }
    if (bytesRead != 8) {
        printf_("don't receive enough bytes from UART\n");
        return -1;
    }
    if (!verifySendPacketData(buffer, true)) {
        return -1;
    }
#endif
    return 0;
}

int uart_verifyPacketDataCmd(uint8_t addr, uint16_t packetCrc)
{
    uint8_t buffer[16];
    DWORD bytesWritten, bytesRead;
    verifyPacketDataCmd[CMD_ADR_OFFSET] = addr;
    verifyPacketDataCmd[CMD_DAT_OFFSET] = packetCrc & 0xFF;
    verifyPacketDataCmd[CMD_DAT_OFFSET + 1] = (packetCrc >> 8) & 0xFF;
    uint16_t crc = crc16(&verifyPacketDataCmd[CMD_LEN_OFFSET], 4+1, 0xffff);
    verifyPacketDataCmd[CMD_CRC_OFFSET(4)] = crc & 0xFF;
    verifyPacketDataCmd[CMD_CRC_OFFSET(4) + 1] = (crc >> 8) & 0xFF;
    if (!WriteFile(gSerial, verifyPacketDataCmd, 9, &bytesWritten, NULL)) {
        printf_("cannot send out verifyPacketData command to UART\n");
        return -1;
    }
    if (bytesWritten != 9) {
        printf_("couldn't send enough bytes to UART\n");
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(8), &bytesRead, NULL)) {
        printf_("cannot get verifyPacketData response from UART\n");
        return -1;
    }
    if (bytesRead != 8) {
        printf_("don't receive enough bytes from UART\n");
        return -1;
    }
    if (!verifyPacketData(buffer, true)) {
        return -1;
    }
    return 0;
}

int uart_verifyAllDataCmd(uint8_t addr, uint8_t crcType, uint32_t fileCrc)
{
    uint8_t buffer[16];
    DWORD bytesWritten, bytesRead;
    if (crcType != 0 && crcType != 1) {
        printf_("crc type should be 0 - crc16 or 1 - crc32\n");
		return -1;
    }
    if (crcType == 0) {
        verifyAllDataCrc16Cmd[CMD_ADR_OFFSET] = addr;
        verifyAllDataCrc16Cmd[CMD_DAT_OFFSET + 1] = fileCrc & 0xFF;
        verifyAllDataCrc16Cmd[CMD_DAT_OFFSET + 2] = (fileCrc >> 8) & 0xFF;
        uint16_t crc = crc16(&verifyAllDataCrc16Cmd[CMD_LEN_OFFSET], 4+1, 0xffff);
        verifyAllDataCrc16Cmd[CMD_CRC_OFFSET(4)] = crc & 0xFF;
        verifyAllDataCrc16Cmd[CMD_CRC_OFFSET(4) + 1] = (crc >> 8) & 0xFF;
        if (!WriteFile(gSerial, verifyAllDataCrc16Cmd, 10, &bytesWritten, NULL)) {
            printf_("cannot send out verifyAllDataCrc16 command to UART\n");
            return -1;
        }
        if (bytesWritten != 10) {
            printf_("couldn't send enough bytes to UART\n");
            return -1;
        }
    } else {
        verifyAllDataCrc32Cmd[CMD_ADR_OFFSET] = addr;
        verifyAllDataCrc32Cmd[CMD_DAT_OFFSET + 1] = fileCrc & 0xFF;
        verifyAllDataCrc32Cmd[CMD_DAT_OFFSET + 2] = (fileCrc >> 8) & 0xFF;
        verifyAllDataCrc32Cmd[CMD_DAT_OFFSET + 3] = (fileCrc >> 16) & 0xFF;
        verifyAllDataCrc32Cmd[CMD_DAT_OFFSET + 4] = (fileCrc >> 24) & 0xFF;
        uint16_t crc = crc16(&verifyAllDataCrc32Cmd[CMD_LEN_OFFSET], 6+1, 0xffff);
        verifyAllDataCrc32Cmd[CMD_CRC_OFFSET(6)] = crc & 0xFF;
        verifyAllDataCrc32Cmd[CMD_CRC_OFFSET(6) + 1] = (crc >> 8) & 0xFF;
        if (!WriteFile(gSerial, verifyAllDataCrc32Cmd, 12, &bytesWritten, NULL)) {
            printf_("cannot send out verifyAllDataCrc32 command to UART\n");
            return -1;
        }
        if (bytesWritten != 12) {
            printf_("couldn't send enough bytes to UART\n");
            return -1;
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(8), &bytesRead, NULL)) {
        printf_("cannot get verify packet data response from UART\n");
        return -1;
    }
    if (bytesRead != 8) {
        printf_("don't receive enough bytes from UART\n");
        return -1;
    }
    if (!verifyAllData(buffer, true)) {
        return -1;
    }
    return 0;
}

int uart_updateStationCmd(uint8_t addr, bool all)
{
    DWORD bytesWritten;
    if (all) {
        updateStationCmd[CMD_ADR_OFFSET] = 0x00;
        updateStationCmd[CMD_DAT_OFFSET] = 0x52;
    } else {
        updateStationCmd[CMD_ADR_OFFSET] = addr;
        updateStationCmd[CMD_DAT_OFFSET] = 0x51;
    }
    uint16_t crc = crc16(&updateStationCmd[CMD_LEN_OFFSET], 5+1, 0xffff);
    updateStationCmd[CMD_CRC_OFFSET(5)] = crc & 0xFF;
    updateStationCmd[CMD_CRC_OFFSET(5) + 1] = (crc >> 8) & 0xFF;
    if (!WriteFile(gSerial, updateStationCmd, 9, &bytesWritten, NULL)) {
        printf_("cannot send out updateStation command to UART\n");
        return -1;
    }
    if (bytesWritten != 9) {
        printf_("couldn't send enough bytes to UART\n");
        return -1;
    }
    return 0;
}

int uart_getUpdateStatusCmd(uint8_t addr, uint8_t *resp)
{
    uint8_t buffer[16];
    DWORD bytesWritten, bytesRead;
    getUpdateStatusCmd[CMD_ADR_OFFSET] = addr;
    uint16_t crc = crc16(&getUpdateStatusCmd[CMD_LEN_OFFSET], 2+1, 0xffff);
    getUpdateStatusCmd[CMD_CRC_OFFSET(2)] = crc & 0xFF;
    getUpdateStatusCmd[CMD_CRC_OFFSET(2) + 1] = (crc >> 8) & 0xFF;
    if (!WriteFile(gSerial, getUpdateStatusCmd, 7, &bytesWritten, NULL)) {
        printf_("cannot send out updateStation command to UART\n");
        return -1;
    }
    if (bytesWritten != 7) {
        printf_("couldn't send enough bytes to UART\n");
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!ReadFile(gSerial, buffer, sizeof(8), &bytesRead, NULL)) {
        printf_("cannot get verify packet data response from UART\n");
        return -1;
    }
    if (bytesRead != 8) {
        printf_("don't receive enough bytes from UART\n");
        return -1;
    }
    if (!verifyGetUpdateStatus(buffer, true)) {
        return -1;
    }
    memcpy(resp, &buffer[RSP_DAT_OFFSET], 3);
    return 3;
}
