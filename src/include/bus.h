#pragma once
#include <cstdint>
#include <vector>
#include <array>

class Bus {
public:
    Bus();
    ~Bus();

    // Load the ROM data we read from the file into the Bus
    void loadROM(const std::vector<uint8_t>& romData);

    // CPU should call these
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);

private:
    // This internal array mimics the 64KB memory space of the Game Boy.
    // replace parts of this with pointers to specific components (like PPU VRAM or Cartridge RAM).
    std::array<uint8_t, 65536> memory;
};

