#include "raylib.h"
#include "video.cpp"
#include <algorithm> // For std::min
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
int main(int argc, char *argv[]) {
  // 1. Load ROM
  if (argc < 2) {
    std::cerr << "Usage: ./Game <rom_file>" << std::endl;
    return 1;
  }
  std::string rom_path = argv[1];
  std::ifstream file(rom_path, std::ios::binary | std::ios::ate);
  if (!file.is_open())
    return 1;

  std::streamsize rom_size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<char> buffer(rom_size);
  if (!file.read(buffer.data(), rom_size))
    return 1;

  std::cout << "DEBUG: ROM Loaded. Size: " << rom_size << " bytes."
            << std::endl;

  // 2. Setup LCD & Palette
  LCD lcd;

  // Setup Grayscale Palette
  uint16_t colors[4] = {0x7FFF, 0x5294, 0x294A, 0x0000};
  for (int c = 0; c < 4; c++) {
    lcd.bg_palette_ram[c * 2] = colors[c] & 0xFF;
    lcd.bg_palette_ram[(c * 2) + 1] = (colors[c] >> 8) & 0xFF;
  }

  // Config LCDC: Enable LCD, Display BG, Unsigned Data ($8000)
  lcd.lcdc.data = LCDC::LCDEnable | LCDC::BGDisplay | LCDC::TileDataSel;

  // 3. Raylib Init
  InitWindow(1000, 800, "Game Boy Tile Viewer"); // Bigger window
  SetTargetFPS(60);

  // Navigation State
  int rom_offset = 0;
  int selected_tile_id = 0;

  while (!WindowShouldClose()) {
    // --- INPUT HANDLING ---
    // Scroll through ROM in 4KB chunks (Size of one full tile set)
    if (IsKeyPressed(KEY_RIGHT)) {
      if (rom_offset + 4096 < rom_size)
        rom_offset += 4096;
    }
    if (IsKeyPressed(KEY_LEFT)) {
      if (rom_offset - 4096 >= 0)
        rom_offset -= 4096;
    }

    // --- INJECTION ---
    // Copy the current "Page" of the ROM into VRAM for display
    // We copy 4096 bytes (256 tiles * 16 bytes/tile)
    int chunk_size = std::min((int)4096, (int)(rom_size - rom_offset));

    // Clear VRAM first (to avoid ghosting if we hit end of file)
    std::memset(lcd.vram[0], 0, 8192);

    // Inject!
    for (int i = 0; i < chunk_size; i++) {
      lcd.vram[0][i] = (uint8_t)buffer[rom_offset + i];
    }

    // --- DRAWING ---
    BeginDrawing();
    ClearBackground(GetColor(0x1a1a1aff)); // Dark Grey background

    // UI Info
    DrawText(TextFormat("ROM Offset: 0x%X / 0x%X", rom_offset, rom_size), 20,
             10, 20, LIGHTGRAY);
    DrawText("Use LEFT/RIGHT Arrows to browse ROM memory", 20, 35, 10, GRAY);

    int start_x = 20;
    int start_y = 60;
    int tile_scale = 2;

    // Draw all 256 Tiles (16x16 Grid)
    for (int tile_id = 0; tile_id < 256; tile_id++) {
      int grid_x = tile_id % 16;
      int grid_y = tile_id / 16;

      // Screen position for this tile
      int draw_pos_x =
          start_x + (grid_x * 8 * tile_scale) + (grid_x * 2); // +2 for spacing
      int draw_pos_y = start_y + (grid_y * 8 * tile_scale) + (grid_y * 2);

      // Interaction: Detect Mouse Hover
      if (GetMouseX() >= draw_pos_x &&
          GetMouseX() < draw_pos_x + (8 * tile_scale) &&
          GetMouseY() >= draw_pos_y &&
          GetMouseY() < draw_pos_y + (8 * tile_scale)) {

        selected_tile_id = tile_id;
        DrawRectangleLines(draw_pos_x - 1, draw_pos_y - 1, (8 * tile_scale) + 2,
                           (8 * tile_scale) + 2, YELLOW);
      }

      // Draw the Tile
      uint16_t tile_addr = lcd.lcdc.get_tile_data_addr(tile_id);
      for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
          uint8_t cid = lcd.get_tile_pixel_id(tile_addr, y, x, 0);
          LCD::Color rgb = lcd.get_bg_color(0, cid);

          DrawRectangle(draw_pos_x + (x * tile_scale),
                        draw_pos_y + (y * tile_scale), tile_scale, tile_scale,
                        {rgb.r, rgb.g, rgb.b, 255});
        }
      }
    }

    // --- INSPECTOR PANEL (Right Side) ---
    int inspect_x = 600;
    int inspect_y = 100;
    int inspect_scale = 20;

    DrawText(
        TextFormat("Tile ID: %d (0x%X)", selected_tile_id, selected_tile_id),
        inspect_x, inspect_y - 30, 20, WHITE);

    // Draw the selected tile HUGE
    uint16_t sel_addr = lcd.lcdc.get_tile_data_addr(selected_tile_id);
    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < 8; x++) {
        uint8_t cid = lcd.get_tile_pixel_id(sel_addr, y, x, 0);
        LCD::Color rgb = lcd.get_bg_color(0, cid);

        DrawRectangle(inspect_x + (x * inspect_scale),
                      inspect_y + (y * inspect_scale), inspect_scale,
                      inspect_scale, {rgb.r, rgb.g, rgb.b, 255});
      }
    }
    // Draw border around inspector
    DrawRectangleLines(inspect_x - 2, inspect_y - 2, (8 * inspect_scale) + 4,
                       (8 * inspect_scale) + 4, WHITE);

    EndDrawing();
  }
  CloseWindow();
  return 0;
}
