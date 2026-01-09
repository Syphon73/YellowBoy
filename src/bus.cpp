//https://gbdev.io/pandocs/Memory_Map.html
#include "Bus.hpp"
#include <iostream>
#include <algorithm>

Bus::Bus() {
    // Clear memory
    memory.fill(0);
}

Bus::~Bus() {
}

void Bus::loadROM(const std::vector<uint8_t>& romData) {
    // COPY the ROM data into the memory map: addresses 0x0000 to 0x7FFF 
    // don't copy more than 32KB (0x8000), thats the max in gba
    long sizeToCopy = std::min((long)romData.size(), 0x8000L);
    
    for (long i = 0; i < sizeToCopy; i++) {
        memory[i] = romData[i];
    }
    
    std::cout << "BUS: Copied " << sizeToCopy << " bytes of ROM to memory [0x0000 - " 
              << std::hex << (sizeToCopy - 1) << "]" << std::dec << std::endl;
}

uint8_t Bus::read(uint16_t address) {

    if (address >= 0x0000 && address <= 0x7FFF) {
        // ROM Data (Cartridge)
        return memory[address];
    }
    else if (address >= 0x8000 && address <= 0x9FFF) {

	// VRAM (Video RAM) - Eventually route this to PPU
	    if(address >= 0x8000 && address <= 0x97FF){
		    // Tile RAM
		    return memory[address];
	    }
	    else{
		    //background mapping (map tiles to section of screen)
		    return memory[address];
	    }
    }
    else if (address >= 0xA000 && address <= 0xBFFF) {
        // External RAM (Cartridge RAM)
        return memory[address];
    }
    else if (address >= 0xC000 && address <= 0xDFFF) {
        // WRAM (Work RAM)
        return memory[address];
    }
    else if(address >= 0xE000 && address <= 0xFDFF){
	    //prohibited area - Echo RAM
        return memory[address - 0x2000];
    }
    else if(address >= 0xFE00 && address <= 0xFE9F){
	    //OAM - object memory attribute
	    return memory[address];
    }
    else if (address >= 0xFEA0 && address <= 0xFEFF) {
        // not usable area
        return memory[address];
    }
    else if(address >= 0xFF00 && address <= 0xFF7F){
	    //I/O registers
	    return memory[address];
    }
    else if(address >= 0xFF80 && address <= 0xFFFE){
	    //HRAM
	    return memory[address];
    }
    else {
        // FFFF - interrupt enable registers
        return memory[address];
    }
}

void Bus::write(uint16_t address, uint8_t value) {
    
    if (address >= 0x0000 && address <= 0x7FFF) {
        // ERROR: Trying to write to ROM
        std::cout << "Attempted write to ROM at " << std::hex << address << std::endl;
        return; 
    }
    else if (address >= 0x8000 && address <= 0x9FFF) {
        // Write to VRAM
        memory[address] = value;
    }
    else {
        // Write to generic memory for now
        memory[address] = value;
    }
}
