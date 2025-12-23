// Resolution of GameBoy Color is 160x144 (20x18 tiles)
// Color Palette is 1x4 BG and 2x3 OBJ
// 32768 colors for GameBoy (CGB)
// Video Ram is 16K bytes (2 Banks)
// Tile Maps are fixed for 1024 bytes so we only specify the lower limit

#include <cstdint>
#include <cstring>

// =============================================================
// LCDC: LCD Control Register (FF40)
// =============================================================
class LCDC {
public:
  enum Mask : uint8_t {
    BGDisplay = 1 << 0,  // BG Display / Priority
    OBJDisplay = 1 << 1, // OBJ Display Enable
    OBJSize = 1 << 2,    // OBJ Size (0=8x8, 1=8x16)
    BGTileMap = 1 << 3,  // BG Tile Map Area (0=9800-9BFF, 1=9C00-9FFF)
    TileDataSel =
        1 << 4, // BG & Window Tile Data Area (0=8800-97FF, 1=8000-8FFF)
    WindowEnable = 1 << 5,  // Window Display Enable
    WindowTileMap = 1 << 6, // Window Tile Map Area (0=9800-9BFF, 1=9C00-9FFF)
    LCDEnable = 1 << 7      // LCD Display Enable
  };

  uint8_t data = 0;

  bool is_bit_set(Mask mask) const { return (data & mask) != 0; }

  uint16_t get_bg_tile_map_addr() const {
    return (data & BGTileMap) ? 0x9C00 : 0x9800;
  }

  uint16_t get_window_map_start() const {
    return (data & WindowTileMap) ? 0x9C00 : 0x9800;
  }

  int get_sprite_height() const { return (data & OBJSize) ? 16 : 8; }

  uint16_t get_tile_data_addr(uint8_t tile_index) const {
    if (is_bit_set(TileDataSel)) {
      return 0x8000 + (tile_index * 16);
    } else {
      int8_t signed_index = static_cast<int8_t>(tile_index);
      return 0x9000 + (signed_index * 16);
    }
  }
};

// =============================================================
// STAT: LCD Status Register (FF41)
// =============================================================
class STAT {
public:
  enum Mask : uint8_t {
    Mode0Interrupt = 1 << 3, // H-Blank
    Mode1Interrupt = 1 << 4, // V-Blank
    Mode2Interrupt = 1 << 5, // OAM Search
    LYCInterrupt = 1 << 6    // LY=LYC Coincidence
  };

  enum Mode : uint8_t { HBlank = 0, VBlank = 1, OAMSearch = 2, Transfer = 3 };

  uint8_t data = 0;

  Mode get_mode() const { return static_cast<Mode>(data & 0x03); }

  void set_mode(Mode mode) {
    data = (data & 0xFC) | static_cast<uint8_t>(mode);
  }

  void set_lyc_flag(bool equal) {
    if (equal)
      data |= (1 << 2);
    else
      data &= ~(1 << 2);
  }

  bool is_interrupt_enabled(Mask mask) const { return (data & mask) != 0; }
};

// =============================================================
// LCD: Main PPU Class
// =============================================================
class LCD {
public:
  LCDC lcdc;
  STAT stat;

  // Registers
  uint8_t scy = 0; // FF42 - Scroll Y
  uint8_t scx = 0; // FF43 - Scroll X
  uint8_t ly = 0;  // FF44 - LCD Y Coordinate (Read Only)
  uint8_t lyc = 0; // FF45 - LY Compare
  uint8_t wy = 0;  // FF4A - Window Y Position
  uint8_t wx = 0;  // FF4B - Window X Position (minus 7)

  uint8_t bgpi = 0; // FF68 - BG Palette Index
  uint8_t obpi = 0; // FF6A - OBJ Palette Index

  uint8_t vbk = 0; // FF4F - VRAM Bank (0 or 1)

  // Memory
  uint8_t vram[2][8192];       // 16KB Video RAM (2 Banks)
  uint8_t oam_ram[160];        // OAM Memory (Sprites)
  uint8_t bg_palette_ram[64];  // CGB BG Palettes
  uint8_t obj_palette_ram[64]; // CGB OBJ Palettes

  // DMA State
  uint8_t dma_reg = 0; // FF46 (Legacy DMA)
  bool dma_transferring = false;
  int dma_timer = 0;

  struct HDMA {
    uint16_t src_addr = 0;
    uint16_t dest_addr = 0;
    uint16_t length = 0;
    bool active = false;
    bool general_mode = false;
    uint8_t reg_ff55 = 0xFF;
  } hdma;

  struct BGAttribute {
    uint8_t palette_id;
    bool use_bank_1;
    bool h_flip;
    bool v_flip;
    bool priority;
  };

  struct Color {
    uint8_t r, g, b;
  };

  struct Sprite {
    uint8_t y;       // Byte 0
    uint8_t x;       // Byte 1
    uint8_t tile_id; // Byte 2
    uint8_t flags;   // Byte 3

    uint8_t get_cgb_palette() const { return flags & 0x07; }
    bool use_vram_bank_1() const { return (flags & 0x08) != 0; }
    uint8_t get_dmg_palette() const { return (flags & 0x10) ? 1 : 0; }
    bool x_flip() const { return (flags & 0x20) != 0; }
    bool y_flip() const { return (flags & 0x40) != 0; }
    bool bg_priority() const { return (flags & 0x80) != 0; }
  };

  LCD() {
    std::memset(bg_palette_ram, 0xFF, sizeof(bg_palette_ram));
    std::memset(obj_palette_ram, 0x00, sizeof(obj_palette_ram));
    std::memset(vram, 0, sizeof(vram));
    std::memset(oam_ram, 0, sizeof(oam_ram));
    vbk = 0;
  }

  void reset_ly() {
    ly = 0;
    check_ly_coincidence();
  }

  void set_lyc(uint8_t value) {
    lyc = value;
    check_ly_coincidence();
  }

  void increment_ly() {
    ly++;
    if (ly > 153)
      ly = 0;
    check_ly_coincidence();
  }

  bool check_ly_coincidence() {
    bool match = (ly == lyc);
    stat.set_lyc_flag(match);
    if (match && stat.is_interrupt_enabled(STAT::LYCInterrupt)) {
      return true; // Request LCD STAT Interrupt
    }
    return false;
  }

  int get_window_x_screen_pos() const { return wx - 7; }

  bool is_lcd_enabled() const { return lcdc.is_bit_set(LCDC::LCDEnable); }

  void write_vbk(uint8_t value) { vbk = value & 0x01; }
  uint8_t read_vbk() const { return 0xFE | vbk; }

  void write_vram(uint16_t addr, uint8_t value) {
    if (is_lcd_enabled() && stat.get_mode() == STAT::Transfer)
      return;
    vram[vbk][addr - 0x8000] = value;
  }

  uint8_t read_vram(uint16_t addr) const {
    if (is_lcd_enabled() && stat.get_mode() == STAT::Transfer)
      return 0xFF;
    return vram[vbk][addr - 0x8000];
  }

  void write_oam(uint16_t addr, uint8_t value) {
    if (is_lcd_enabled()) {
      if (stat.get_mode() == STAT::OAMSearch ||
          stat.get_mode() == STAT::Transfer)
        return;
    }
    if ((addr - 0xFE00) < 160)
      oam_ram[addr - 0xFE00] = value;
  }

  uint8_t read_oam(uint16_t addr) const {
    if (is_lcd_enabled()) {
      if (stat.get_mode() == STAT::OAMSearch ||
          stat.get_mode() == STAT::Transfer)
        return 0xFF;
    }
    uint16_t offset = addr - 0xFE00;
    return (offset < 160) ? oam_ram[offset] : 0xFF;
  }

  void write_bgpi(uint8_t value) { bgpi = value; }
  void write_bgpd(uint8_t value) {
    if (stat.get_mode() == STAT::Transfer)
      return;
    bg_palette_ram[bgpi & 0x3F] = value;
    if (bgpi & 0x80)
      bgpi = (bgpi & 0x80) | ((bgpi + 1) & 0x3F);
  }
  uint8_t read_bgpd() const {
    return (stat.get_mode() == STAT::Transfer) ? 0xFF
                                               : bg_palette_ram[bgpi & 0x3F];
  }

  void write_obpi(uint8_t value) { obpi = value; }
  void write_obpd(uint8_t value) {
    if (stat.get_mode() == STAT::Transfer)
      return;
    obj_palette_ram[obpi & 0x3F] = value;
    if (obpi & 0x80)
      obpi = (obpi & 0x80) | ((obpi + 1) & 0x3F);
  }
  uint8_t read_obpd() const {
    return (stat.get_mode() == STAT::Transfer) ? 0xFF
                                               : obj_palette_ram[obpi & 0x3F];
  }

  void write_dma(uint8_t value) {
    dma_reg = value;
    dma_transferring = true;
    dma_timer = 0;
  }

  void write_hdma1(uint8_t value) {
    hdma.src_addr = (hdma.src_addr & 0x00FF) | (value << 8);
  }
  void write_hdma2(uint8_t value) {
    hdma.src_addr = (hdma.src_addr & 0xFF00) | (value & 0xF0);
  }
  void write_hdma3(uint8_t value) {
    hdma.dest_addr = (hdma.dest_addr & 0x00FF) | ((value & 0x1F) << 8);
  }
  void write_hdma4(uint8_t value) {
    hdma.dest_addr = (hdma.dest_addr & 0xFF00) | (value & 0xF0);
  }

  void write_hdma5(uint8_t value) {
    if (hdma.active && !(value & 0x80)) { // Cancel Active HDMA
      hdma.active = false;
      hdma.reg_ff55 = 0x80 | (hdma.reg_ff55 & 0x7F);
      return;
    }
    hdma.length = ((value & 0x7F) + 1);
    hdma.general_mode = (value & 0x80) == 0;
    hdma.reg_ff55 = value & 0x7F;

    if (hdma.general_mode) {
      hdma.active = false; // GDMA finishes instantly in emulator tick
      hdma.reg_ff55 = 0xFF;
    } else {
      hdma.active = true; // HDMA waits for H-Blank
    }
  }

  uint8_t read_hdma5() const {
    return hdma.active ? ((hdma.length - 1) & 0x7F) : 0xFF;
  }

  uint8_t get_tile_pixel_id(uint16_t tile_data_addr, uint8_t line, uint8_t bit,
                            uint8_t bank_num) const {
    uint16_t offset = (tile_data_addr - 0x8000) + (line * 2);

    uint8_t byte1 = vram[bank_num & 1][offset];
    uint8_t byte2 = vram[bank_num & 1][offset + 1];

    uint8_t shift = 7 - bit;
    return ((byte2 >> shift) & 1) << 1 | ((byte1 >> shift) & 1);
  }

  BGAttribute get_bg_attribute(uint16_t map_addr) const {
    uint8_t raw = vram[1][map_addr - 0x8000]; // Always Bank 1
    return {static_cast<uint8_t>(raw & 0x07), (raw & 0x08) != 0,
            (raw & 0x20) != 0, (raw & 0x40) != 0, (raw & 0x80) != 0};
  }

  Sprite get_sprite(int index) const {
    int offset = index * 4;
    return {oam_ram[offset], oam_ram[offset + 1], oam_ram[offset + 2],
            oam_ram[offset + 3]};
  }

  Color get_bg_color(uint8_t palette_num, uint8_t color_num) const {
    return get_color_from_ram(bg_palette_ram, palette_num, color_num);
  }

  Color get_obj_color(uint8_t palette_num, uint8_t color_num) const {
    return get_color_from_ram(obj_palette_ram, palette_num, color_num);
  }

private:
  Color get_color_from_ram(const uint8_t *ram, uint8_t palette_num,
                           uint8_t color_num) const {
    int index = (palette_num * 8) + (color_num * 2);
    uint16_t val = ram[index] | (ram[index + 1] << 8);
    uint8_t r = (val & 0x1F);
    uint8_t g = (val >> 5) & 0x1F;
    uint8_t b = (val >> 10) & 0x1F;
    // Convert 5-bit to 8-bit using (x << 3) | (x >> 2)
    return {static_cast<uint8_t>((r << 3) | (r >> 2)),
            static_cast<uint8_t>((g << 3) | (g >> 2)),
            static_cast<uint8_t>((b << 3) | (b >> 2))};
  }
};
