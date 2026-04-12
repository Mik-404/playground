#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>

namespace serial {

class SerialPort {
private:
    uint16_t io_port_ = 0;

private:
    bool IsTransmitEmpty() noexcept;

public:
    SerialPort(uint16_t io_port) : io_port_(io_port) {}

    void Init() noexcept;

    void Write(char ch) noexcept;

    void Write(std::string_view view) noexcept;

    void Write(const char* str) noexcept;
};

extern SerialPort Com1;
extern SerialPort Com2;

void Init() noexcept;

}
