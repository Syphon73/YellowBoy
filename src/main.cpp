#include "audio.cpp" // Your APU implementation
#include "raylib.h"
#include <iostream>
#include <vector>

// ============================================================================
// GLOBAL STATE
// ============================================================================
APU apu;
const int SAMPLE_RATE = 44100;
const int CYCLES_PER_SAMPLE = 4194304 / SAMPLE_RATE;

// ============================================================================
// AUDIO CALLBACK
// ============================================================================
void GameAudioCallback(void *buffer, unsigned int frames) {
  float *d = (float *)buffer;
  for (unsigned int i = 0; i < frames; i++) {
    apu.tick(CYCLES_PER_SAMPLE);
    APU::StereoSample sample = apu.get_sample();
    d[i * 2] = sample.left;
    d[i * 2 + 1] = sample.right;
  }
}

// ============================================================================
// HELPER: NOTE FREQUENCIES
// These are the raw 11-bit values the Game Boy uses for notes
// Formula: x = 2048 - (131072 / Frequency)
// ============================================================================
const int NOTE_E5 = 1650;
const int NOTE_B4 = 1546;
const int NOTE_C5 = 1576;
const int NOTE_D5 = 1602;
const int NOTE_A4 = 1496;
const int NOTE_G4 = 1428;
const int NOTE_F4 = 1360; // Approximate

struct Note {
  int frequency; // The raw 11-bit value
  int duration;  // In frames (60ths of a second)
};

// THE TETRIS THEME (Korobeiniki)
// Melody: E -> B -> C -> D -> C -> B -> A ...
std::vector<Note> tetris_melody = {
    {NOTE_E5, 20}, {NOTE_B4, 10}, {NOTE_C5, 10}, {NOTE_D5, 20}, {NOTE_C5, 10},
    {NOTE_B4, 10}, {NOTE_A4, 20}, {NOTE_A4, 10}, {NOTE_C5, 10}, {NOTE_E5, 20},
    {NOTE_D5, 10}, {NOTE_C5, 10}, {NOTE_B4, 30}, {NOTE_C5, 10}, {NOTE_D5, 20},
    {NOTE_E5, 20}, {NOTE_C5, 20}, {NOTE_A4, 20}, {NOTE_A4, 40}, {0, 10}, // Rest
    {NOTE_D5, 20}, {NOTE_F4, 10}, {NOTE_A4, 20}, {NOTE_G4, 10}, {NOTE_F4, 10},
    {NOTE_E5, 30}, {NOTE_C5, 10}, {NOTE_E5, 20}, {NOTE_D5, 10}, {NOTE_C5, 10},
    {NOTE_B4, 20}, {NOTE_B4, 10}, {NOTE_C5, 10}, {NOTE_D5, 20}, {NOTE_E5, 20},
    {NOTE_C5, 20}, {NOTE_A4, 20}, {NOTE_A4, 40}};

int current_note_index = 0;
int note_timer = 0;

// ============================================================================
// PLAY NEXT NOTE LOGIC
// This mimics the Game Boy Sound Engine updates
// ============================================================================
void UpdateMusic() {
  if (note_timer > 0) {
    note_timer--;
    return;
  }

  // Get the next note
  Note n = tetris_melody[current_note_index];
  current_note_index = (current_note_index + 1) % tetris_melody.size();

  // Reset timer
  note_timer = n.duration;

  if (n.frequency == 0) {
    // Rest (Silence)
    // We set volume to 0 (Envelope 0, Direction 0)
    apu.write_byte(0xFF12, 0x00);
    apu.write_byte(0xFF14, 0x80); // Trigger to apply
  } else {
    // Play Note on Channel 1
    // NR10: Sweep Off
    apu.write_byte(0xFF10, 0x00);

    // NR11: Duty 50% (0x80), Length doesn't matter much here
    apu.write_byte(0xFF11, 0x80);

    // NR12: Volume 10 (0xA), Decay (0), Speed 2
    // This gives it that "plucky" Game Boy sound
    apu.write_byte(0xFF12, 0xA2);

    // NR13: Frequency Low Byte
    apu.write_byte(0xFF13, n.frequency & 0xFF);

    // NR14: Frequency High + Trigger (0x80)
    apu.write_byte(0xFF14, 0x80 | ((n.frequency >> 8) & 0x07));
  }
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
  InitWindow(400, 300, "Tetris Theme Test");
  InitAudioDevice();
  SetTargetFPS(60);

  // Initial APU Setup
  apu.write_byte(0xFF26, 0x80); // Power On
  apu.write_byte(0xFF25, 0x11); // Pan Ch1 to Left & Right (Bit 0 and 4)
  apu.write_byte(0xFF24, 0x77); // Master Vol Max

  AudioStream stream = LoadAudioStream(SAMPLE_RATE, 32, 2);
  SetAudioStreamCallback(stream, GameAudioCallback);
  PlayAudioStream(stream);

  while (!WindowShouldClose()) {

    // Run our fake "Sound Engine" once per frame (60Hz)
    UpdateMusic();

    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawText("Playing: Tetris Theme (Korobeiniki)", 20, 100, 20, DARKGRAY);
    DrawText("Channel 1: Square Wave 50%", 20, 130, 10, GRAY);

    // Visualizer bar based on current frequency
    if (current_note_index > 0) {
      int freq = tetris_melody[current_note_index - 1].frequency;
      if (freq > 0) {
        float height = (freq - 1300) / 2.0f;
        DrawRectangle(150, 200 - height, 50, height, RED);
      }
    }
    EndDrawing();
  }

  CloseAudioDevice();
  CloseWindow();
  return 0;
}
