// https://gbdev.io/pandocs/Memory_Map.html
#include <cstddef>
#include <vector>

constexpr std::size_t WRAM_SIZE = /* your size */;

struct Wram {
  std::vector<uint8_t> bytes;

  static Wram New() { return Wram{std::vector<uint8_t>(WRAM_SIZE, 0)}; }
};
