//only Support RS485 uart
//Author : richard xu (junzexu@outlook.com)
//Date : Apr 12, 2026

#define _CRT_SECURE_NO_WARNINGS

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <thread>
#include <chrono>
#include <Windows.h>

#define UART_LOW_BAUDRATE 9600
#define UART_HIGH_BAUDRATE 115200

typedef void (*out_fct_type)(char character, void* buffer, size_t idx, size_t maxlen);
typedef void (*RegisterInternalPutchar)(out_fct_type custom_putchar);
typedef bool (*UartConnect)(const char* port, uint32_t baud_rate);
typedef bool (*UartDisconnect)(void);
typedef bool (*UartChangeHostBaud)(uint32_t baud_rate);
typedef bool (*UartRequestSlaveBaud)(bool to_high);
typedef bool (*UartRequestUpgrade)(void);
typedef bool (*UartSendFrameCount)(uint16_t cnt);
typedef int (*UartSendFrameData)(uint16_t seq, uint16_t len, uint8_t* dat);
typedef bool (*UartRequestComplete)(void);

static RegisterInternalPutchar register_internal_putchar = NULL;
static UartConnect uart_connect = NULL;
static UartDisconnect uart_disconnect = NULL;
static UartChangeHostBaud uart_changeHostBaud = NULL;
static UartRequestSlaveBaud uart_requestSlaveBaud = NULL;
static UartRequestUpgrade uart_requestUpgrade = NULL;
static UartSendFrameCount uart_sendFrameCount = NULL;
static UartSendFrameData uart_sendFrameData = NULL;
static UartRequestComplete uart_requestComplete = NULL;

inline void putchar_(char character, void* buffer, size_t idx, size_t maxlen)
{
    putchar(character);
}

inline void print_usage(void)
{
    printf("Usage: wifi_update_app.exe -p <portName> -f <dfuFile>\n");
    printf("port : wifi uart port name, etc \"COM5\"\n");
}

int main(int argc, char** argv)
{
    char portName[0x08] = "COM1";
    uint16_t packetLen = 0x200; //wifi upgrading only supports 512 bytes packet
    uint16_t packetCnt = 0;
    uint16_t seq = 0x01;
    int retCode = 0;

    //load library
    HINSTANCE handle = LoadLibraryA("uart_update.dll");
    if (handle == NULL) {
        printf("could not load uart_update.dll\n");
        return false;
    }
    register_internal_putchar = (RegisterInternalPutchar)GetProcAddress(handle, "register_internal_putchar");
    uart_connect = (UartConnect)GetProcAddress(handle, "uart_connect");
    uart_disconnect = (UartDisconnect)GetProcAddress(handle, "uart_disconnect");
    uart_changeHostBaud = (UartChangeHostBaud)GetProcAddress(handle, "uart_changeHostBaud");
    uart_requestSlaveBaud = (UartRequestSlaveBaud)GetProcAddress(handle, "uart_requestSlaveBaud");
    uart_requestUpgrade = (UartRequestUpgrade)GetProcAddress(handle, "uart_requestUpgrade");
    uart_sendFrameCount = (UartSendFrameCount)GetProcAddress(handle, "uart_sendFrameCount");
    uart_sendFrameData = (UartSendFrameData)GetProcAddress(handle, "uart_sendFrameData");
    uart_requestComplete = (UartRequestComplete)GetProcAddress(handle, "uart_requestComplete");

    fflush(stdout);
    if (argc != 3 && argc != 5) {
        print_usage();
        return -1;
    }
    int i = 1;
    int filePos = 1;
    while (i < argc) {
        if (argv[i][0] == '-') {
            char ch = argv[i][1];
            switch (ch) {
            case 'p':
                ++i;
                strcpy(portName, argv[i]);
                break;
            case 'f':
                ++i;
                filePos = i;
                break;
            default:
                printf("illegal arguments, only supports f\n");
                print_usage();
                return -1;
            }
            ++i;
        }
    }
    printf("packet length for wifi upgrading is fixed to 512\n");
    register_internal_putchar(putchar_);
    FILE* fd = fopen(argv[filePos], "rb");
    if (fd == nullptr) {
        printf("Cannot open file %s\n", argv[filePos]);
        return -1;
    }
    fseek(fd, 0, SEEK_END);
    uint32_t fileLen = ftell(fd);
    rewind(fd);
    uint32_t newFileLen = fileLen;
    if ((fileLen & (packetLen - 1)) != 0) {
        printf("file length is not multiple of packetLen, need padding");
        newFileLen = fileLen + packetLen - (fileLen & (packetLen - 1));
    }
    uint8_t* buffer = (uint8_t*)malloc(newFileLen * sizeof(uint8_t));
    if (buffer == nullptr) {
        printf("Could not allocate buffer\n");
        fclose(fd);
        return -1;
    }
    fread(buffer, sizeof(uint8_t), fileLen, fd);
    if (fileLen != newFileLen) {
        for (int i = fileLen; i < newFileLen; ++i) {
            buffer[i] = 0xFF;   //padding with 0xFF
        }
        fileLen = newFileLen;
    }
    fclose(fd);
    packetCnt = fileLen / packetLen;
    if (!uart_connect(portName, UART_LOW_BAUDRATE)) {
        printf("Cannot connect to uart port %s\n", portName);
        free(buffer);
        return -1;
    }
    auto start = std::chrono::high_resolution_clock::now();
    auto deadline = start + std::chrono::milliseconds(5000);   //3s retrying
    while (true) {
        if (!uart_requestSlaveBaud(true)) { //request slave high baud rate
            printf("send high baudrate command to slave device failed\n");
            free(buffer);
            return -1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (!uart_changeHostBaud(UART_HIGH_BAUDRATE)) {
            printf("swtiching host side's baudrate to %d bps failed\n", UART_HIGH_BAUDRATE);
            free(buffer);
            return -1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (uart_requestUpgrade()) {
            break;  //successfully switching to high baud rate and have response 
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (!uart_changeHostBaud(UART_LOW_BAUDRATE)) {
            printf("swtiching host side's baudrate to %d bps failed\n", UART_LOW_BAUDRATE);
            free(buffer);
            return -1;
        }
        if (std::chrono::high_resolution_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else {
            printf("Slave device has no response, timeout...\n");
            free(buffer);
            return -1;
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (!uart_sendFrameCount(packetCnt)) {
        printf("send framecount %d command failed\n", packetCnt);
        free(buffer);
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    while (seq <= packetCnt) {
        int ret = uart_sendFrameData(seq, packetLen, buffer + (seq-1) * packetLen);
        if (ret < 0) {
            printf("send frame %d 's data failed\n", seq);
            free(buffer);
            return -1;
        } else if (ret ==0) {
            printf("slave request repeating\n");
        } else {
            ++seq;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!uart_requestComplete()) {
        printf("send request upgrade complete command failed\n");
        free(buffer);
        return -1;
    }
    if (!uart_requestSlaveBaud(false)) {    //request slave low baud
        printf("send low baudrate command to slave device failed\n");
        free(buffer);
        return -1;
    }
    if (!uart_disconnect()) {
        printf("try to disconnect uart %s failed\n", portName);
        free(buffer);
        return -1;
    }
    return 0;
}
