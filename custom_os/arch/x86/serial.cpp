#include "arch/x86/serial.h"
#include "arch/x86/x86.h"
#include "kernel/panic.h"

namespace serial {

bool SerialPort::IsTransmitEmpty() noexcept {
    return x86::Inb(io_port_ + 5) & 0x20;
}

void SerialPort::Write(char ch) noexcept {
    for (;;) {
        if (IsTransmitEmpty()) {
            break;
        }
    }

    x86::Outb(io_port_, ch);
}

void SerialPort::Write(std::string_view view) noexcept {
    for (char ch : view) {
        Write(ch);
    }
}

void SerialPort::Write(const char* str) noexcept {
    while (*str != '\0') {
        Write(*str);
        str++;
    }
}

void SerialPort::Init() noexcept {
    x86::Outb(io_port_ + 1, 0x00);
    x86::Outb(io_port_ + 3, 0x80);
    x86::Outb(io_port_ + 0, 0x03);
    x86::Outb(io_port_ + 1, 0x00);
    x86::Outb(io_port_ + 3, 0x03);
    x86::Outb(io_port_ + 2, 0xC7);
    x86::Outb(io_port_ + 4, 0x0B);
    x86::Outb(io_port_ + 4, 0x1E);
    x86::Outb(io_port_ + 0, 0xAE);
    x86::Outb(io_port_ + 4, 0x0F);
}

SerialPort Com1{0x3f8};
SerialPort Com2{0x2F8};

void Init() noexcept {
    Com1.Init();
    Com2.Init();
}

}
