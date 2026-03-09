#include <windows.h>
#include <thread>
#include <vector>
#include "uart.h"

static HANDLE gSerial;

const uint8_t highBaudRateCmd[] = {
    0x5A, 0xC0, 0x00, 0x00, 0x40, 0xA5,
}

const uint8_t lowBaudRateCmd[] = {
    0x5f, 0xC4, 0x00, 0x00, 0x3C, 0xF5, 
};

const uint8_t requestUpdateCmd[] = {
    0x5F, 0xC0, 0x00, 0x00, 0x40, 0xF5, 
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
    timeout.ReadTotalTimeoutConstant = 500;
    timeout.ReadTotalTimeoutMultiplier = 10;    //in ms for every byte
    timeout.WriteTotalTimeoutConstant = 500;
    timeout.WriteTotalTimeoutMultiplier = 10;   //in ms for every byte
    SetCommTimeouts(gSerial, &timeout);
    return true;
}

bool uart_changeBaudRate(uint32_t baud_rate)
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

bool uart_disconnect(void)
{
    return CloseHandle(gSerial);
}

bool uart_changeSlaveBaud()
{

}
int uart_requestUpgrade(uint8_t *char, int len, )

int uart_prepareCmd(uint8_t addr, uint8_t *resp)
{

}