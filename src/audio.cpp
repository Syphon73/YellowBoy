// The gameboy audio chip is called APU(audio processing unit)
// This runs off of the same master clock as the PPU and CPU perfectly in sync.
// a “256 Hz tick” means “1 ∕ 256th of a second”
#include <cstdint>

class Channel1 {
public:
  uint8_t nr10;
  uint8_t nr11;
  uint8_t nr12;
  uint8_t nr13;
  uint8_t nr14;

  bool enabled;

  int frequency_timer;
  int wave_position;

  int length_timer;

  int envelope_timer;
  int current_volume;

  int sweep_timer;
  int shadow_frequency;
  bool sweep_enabled;

  const uint8_t wave_patterns[4][8] = {{0, 0, 0, 0, 0, 0, 0, 1},
                                       {1, 0, 0, 0, 0, 0, 0, 1},
                                       {1, 0, 0, 0, 0, 1, 1, 1},
                                       {0, 1, 1, 1, 1, 1, 1, 0}};

  Channel1() {
    nr10 = 0;
    nr11 = 0;
    nr12 = 0;
    nr13 = 0;
    nr14 = 0;
    enabled = false;
    frequency_timer = 0;
    wave_position = 0;
    length_timer = 0;
    envelope_timer = 0;
    current_volume = 0;
    sweep_timer = 0;
    shadow_frequency = 0;
    sweep_enabled = false;
  }

  uint8_t read_byte(uint16_t addr) {
    switch (addr) {
    case 0xFF10:
      return nr10 | 0x80;
    case 0xFF11:
      return nr11 | 0x3F;
    case 0xFF12:
      return nr12;
    case 0xFF13:
      return 0xFF;
    case 0xFF14:
      return nr14 | 0xBF;
    default:
      return 0xFF;
    }
  }

  void write_byte(uint16_t addr, uint8_t value) {
    switch (addr) {
    case 0xFF10:
      nr10 = value;
      break;
    case 0xFF11:
      nr11 = value;
      length_timer = 64 - (nr11 & 0x3F);
      break;
    case 0xFF12:
      nr12 = value;
      break;
    case 0xFF13:
      nr13 = value;
      break;
    case 0xFF14:
      nr14 = value;
      if (value & 0x80) {
        trigger();
      }
      break;
    }
  }

  void trigger() {
    enabled = true;

    if (length_timer == 0) {
      length_timer = 64;
    }

    frequency_timer = (2048 - get_frequency()) * 4;

    envelope_timer = nr12 & 0x07;
    current_volume = (nr12 >> 4) & 0x0F;

    shadow_frequency = get_frequency();
    int sweep_period = (nr10 >> 4) & 0x07;
    int sweep_shift = nr10 & 0x07;

    sweep_timer = sweep_period;
    if (sweep_period == 0) {
      sweep_timer = 8;
    }

    sweep_enabled = (sweep_period > 0) || (sweep_shift > 0);
  }

  uint16_t get_frequency() { return nr13 | ((nr14 & 0x07) << 8); }

  uint8_t get_output() {
    if (!enabled)
      return 0;

    uint8_t duty_index = (nr11 >> 6) & 0x03;
    uint8_t wave_output = wave_patterns[duty_index][wave_position];

    return wave_output * current_volume;
  }
};
class Channel2 {
public:
  uint8_t nr21;
  uint8_t nr22;
  uint8_t nr23;
  uint8_t nr24;

  bool enabled;
  int frequency_timer;
  int wave_position;

  int length_timer;

  int envelope_timer;
  int current_volume;

  const uint8_t wave_patterns[4][8] = {
      {0, 0, 0, 0, 0, 0, 0, 1}, // 12.5%
      {1, 0, 0, 0, 0, 0, 0, 1}, // 25%
      {1, 0, 0, 0, 0, 1, 1, 1}, // 50%
      {0, 1, 1, 1, 1, 1, 1, 0}  // 75%
  };

  Channel2() {
    nr21 = 0;
    nr22 = 0;
    nr23 = 0;
    nr24 = 0;
    enabled = false;
    frequency_timer = 0;
    wave_position = 0;
    length_timer = 0;
    envelope_timer = 0;
    current_volume = 0;
  }

  uint8_t read_byte(uint16_t addr) {
    switch (addr) {
    case 0xFF16:
      return nr21 | 0x3F; // Mask out length data (Write Only)
    case 0xFF17:
      return nr22;
    case 0xFF18:
      return 0xFF; // Write Only
    case 0xFF19:
      return nr24 | 0xBF; // Mask out Trigger bit
    default:
      return 0xFF;
    }
  }

  void write_byte(uint16_t addr, uint8_t value) {
    switch (addr) {
    case 0xFF16:
      nr21 = value;
      length_timer = 64 - (nr21 & 0x3F);
      break;
    case 0xFF17:
      nr22 = value;
      break;
    case 0xFF18:
      nr23 = value;
      break;
    case 0xFF19:
      nr24 = value;
      if (value & 0x80) {
        trigger();
      }
      break;
    }
  }

  void trigger() {
    enabled = true;

    if (length_timer == 0) {
      length_timer = 64;
    }

    frequency_timer = (2048 - get_frequency()) * 4;

    envelope_timer = nr22 & 0x07;
    current_volume = (nr22 >> 4) & 0x0F;
  }

  uint16_t get_frequency() { return nr23 | ((nr24 & 0x07) << 8); }

  uint8_t get_output() {
    if (!enabled)
      return 0;

    uint8_t duty_index = (nr21 >> 6) & 0x03;
    uint8_t wave_output = wave_patterns[duty_index][wave_position];

    return wave_output * current_volume;
  }
};

class Channel3 {
public:
  uint8_t nr30;
  uint8_t nr31;
  uint8_t nr32;
  uint8_t nr33;
  uint8_t nr34;

  uint8_t wave_ram[16];

  bool enabled;
  bool dac_enabled;

  int frequency_timer;
  int wave_position;
  int length_timer;

  Channel3() {
    nr30 = 0;
    nr31 = 0;
    nr32 = 0;
    nr33 = 0;
    nr34 = 0;
    for (int i = 0; i < 16; i++)
      wave_ram[i] = 0;

    enabled = false;
    dac_enabled = false;
    frequency_timer = 0;
    wave_position = 0;
    length_timer = 0;
  }

  uint8_t read_byte(uint16_t addr) {
    switch (addr) {
    case 0xFF1A:
      return nr30 | 0x7F;
    case 0xFF1B:
      return 0xFF;
    case 0xFF1C:
      return nr32 | 0x9F;
    case 0xFF1D:
      return 0xFF;
    case 0xFF1E:
      return nr34 | 0xBF;
    default:
      return 0xFF;
    }
  }

  void write_byte(uint16_t addr, uint8_t value) {
    switch (addr) {
    case 0xFF1A:
      nr30 = value;
      dac_enabled = (value & 0x80) != 0;
      if (!dac_enabled)
        enabled = false;
      break;
    case 0xFF1B:
      nr31 = value;
      length_timer = 256 - nr31;
      break;
    case 0xFF1C:
      nr32 = value;
      break;
    case 0xFF1D:
      nr33 = value;
      break;
    case 0xFF1E:
      nr34 = value;
      if (value & 0x80) {
        trigger();
      }
      break;
    }
  }

  uint8_t read_wave_ram(uint16_t addr) { return wave_ram[addr - 0xFF30]; }

  void write_wave_ram(uint16_t addr, uint8_t value) {
    wave_ram[addr - 0xFF30] = value;
  }

  void trigger() {
    if (dac_enabled) {
      enabled = true;
    }

    if (length_timer == 0) {
      length_timer = 256;
    }

    wave_position = 0;
    frequency_timer = (2048 - get_frequency()) * 2;
  }

  uint16_t get_frequency() { return nr33 | ((nr34 & 0x07) << 8); }

  uint8_t get_output() {
    if (!enabled || !dac_enabled)
      return 0;

    uint8_t byte_index = wave_position / 2;
    uint8_t sample_byte = wave_ram[byte_index];

    uint8_t sample;
    if (wave_position % 2 == 0) {
      sample = (sample_byte >> 4) & 0x0F;
    } else {
      sample = sample_byte & 0x0F;
    }

    uint8_t volume_code = (nr32 >> 5) & 0x03;
    switch (volume_code) {
    case 0:
      return 0; // Mute
    case 1:
      return sample; // 100%
    case 2:
      return sample >> 1; // 50%
    case 3:
      return sample >> 2; // 25%
    default:
      return 0;
    }
  }
};

class Channel4 {
public:
  uint8_t nr41;
  uint8_t nr42;
  uint8_t nr43;
  uint8_t nr44;

  bool enabled;

  uint16_t lfsr; // The Linear Feedback Shift Register

  int frequency_timer;
  int length_timer;
  int envelope_timer;
  int current_volume;

  Channel4() {
    nr41 = 0;
    nr42 = 0;
    nr43 = 0;
    nr44 = 0;
    enabled = false;
    lfsr = 0x7FFF; // Initial seed (must not be 0)
    frequency_timer = 0;
    length_timer = 0;
    envelope_timer = 0;
    current_volume = 0;
  }

  uint8_t read_byte(uint16_t addr) {
    switch (addr) {
    case 0xFF20:
      return 0xFF; // NR41 is Write Only (usually reads FF)
    case 0xFF21:
      return nr42;
    case 0xFF22:
      return nr43;
    case 0xFF23:
      return nr44 | 0xBF;
    default:
      return 0xFF;
    }
  }

  void write_byte(uint16_t addr, uint8_t value) {
    switch (addr) {
    case 0xFF20:
      nr41 = value;
      length_timer = 64 - (nr41 & 0x3F);
      break;
    case 0xFF21:
      nr42 = value;
      break;
    case 0xFF22:
      nr43 = value;
      break;
    case 0xFF23:
      nr44 = value;
      if (value & 0x80) {
        trigger();
      }
      break;
    }
  }

  void trigger() {
    enabled = true;
    lfsr = 0x7FFF; // Reset LFSR to all 1s

    if (length_timer == 0) {
      length_timer = 64;
    }

    envelope_timer = nr42 & 0x07;
    current_volume = (nr42 >> 4) & 0x0F;
    frequency_timer = get_divisor();
  }

  int get_divisor() {
    int r = nr43 & 0x07;
    int s = (nr43 >> 4) & 0x0F;
    int base_divisor = (r == 0) ? 8 : (16 * r);

    return base_divisor << s; // 2^s shift
  }

  uint8_t get_output() {
    if (!enabled)
      return 0;

    return (~lfsr & 1) ? current_volume : 0;
  }

  void step_lfsr() {
    uint16_t xor_res = (lfsr & 0x01) ^ ((lfsr >> 1) & 0x01);

    lfsr >>= 1;

    lfsr |= (xor_res << 14);

    if (nr43 & 0x08) {
      lfsr &= ~(1 << 6);      // Clear bit 6
      lfsr |= (xor_res << 6); // Set bit 6
    }

    frequency_timer = get_divisor();
  }
};

class APU {
public:
  Channel1 ch1;
  Channel2 ch2;
  Channel3 ch3;
  Channel4 ch4;

  uint8_t nr50;
  uint8_t nr51;
  uint8_t nr52;

  int frame_sequencer;
  int frame_timer; // Counts CPU cycles to reach 512Hz

  struct StereoSample {
    float left;
    float right;
  };

  APU() {
    nr50 = 0;
    nr51 = 0;
    nr52 = 0;
    frame_sequencer = 0;
    frame_timer = 0;
  }

  void tick(int cpu_cycles) {
    if (!(nr52 & 0x80))
      return;
    frame_timer += cpu_cycles;
    if (frame_timer >= 8192) {
      frame_timer -= 8192;
      step_frame_sequencer();
    }
    tick_channel_timers(cpu_cycles, ch1.frequency_timer);
    tick_channel_timers(cpu_cycles, ch2.frequency_timer);
    tick_channel_timers(cpu_cycles, ch3.frequency_timer);
    tick_channel_timers(cpu_cycles, ch4.frequency_timer);
  }

  void tick_channel_timers(int cycles, int &timer) {
    timer -= cycles;
    if (timer <= 0) {
      if (&timer == &ch1.frequency_timer) {
        ch1.wave_position = (ch1.wave_position + 1) & 7;
        ch1.frequency_timer += (2048 - ch1.get_frequency()) * 4;
      } else if (&timer == &ch2.frequency_timer) {
        ch2.wave_position = (ch2.wave_position + 1) & 7;
        ch2.frequency_timer += (2048 - ch2.get_frequency()) * 4;
      } else if (&timer == &ch3.frequency_timer) {
        ch3.wave_position = (ch3.wave_position + 1) & 31;
        ch3.frequency_timer += (2048 - ch3.get_frequency()) * 2;
      } else if (&timer == &ch4.frequency_timer) {
        ch4.step_lfsr();
      }
    }
  }

  void step_frame_sequencer() {
    frame_sequencer = (frame_sequencer + 1) & 7;

    switch (frame_sequencer) {
    case 0:
      step_length();
      break;
    case 1:
      break;
    case 2:
      step_length();
      step_sweep();
      break;
    case 3:
      break;
    case 4:
      step_length();
      break;
    case 5:
      break;
    case 6:
      step_length();
      step_sweep();
      break;
    case 7:
      step_envelope();
      break;
    }
  }

  void step_length() {
    if (ch1.nr14 & 0x40 && ch1.length_timer > 0) {
      ch1.length_timer--;
      if (ch1.length_timer == 0)
        ch1.enabled = false;
    }
    if (ch2.nr24 & 0x40 && ch2.length_timer > 0) {
      ch2.length_timer--;
      if (ch2.length_timer == 0)
        ch2.enabled = false;
    }
    if (ch3.nr34 & 0x40 && ch3.length_timer > 0) {
      ch3.length_timer--;
      if (ch3.length_timer == 0)
        ch3.enabled = false;
    }
    if (ch4.nr44 & 0x40 && ch4.length_timer > 0) {
      ch4.length_timer--;
      if (ch4.length_timer == 0)
        ch4.enabled = false;
    }
  }

  void step_envelope() {
    if (ch1.envelope_timer > 0) {
      ch1.envelope_timer--;
      if (ch1.envelope_timer == 0) {
        ch1.envelope_timer = ch1.nr12 & 0x07;
        if (ch1.envelope_timer != 0) {
          if ((ch1.nr12 & 0x08) && ch1.current_volume < 15)
            ch1.current_volume++;
          else if (!(ch1.nr12 & 0x08) && ch1.current_volume > 0)
            ch1.current_volume--;
        }
      }
    }
    if (ch2.envelope_timer > 0) {
      ch2.envelope_timer--;
      if (ch2.envelope_timer == 0) {
        ch2.envelope_timer = ch2.nr22 & 0x07;
        if (ch2.envelope_timer != 0) {
          if ((ch2.nr22 & 0x08) && ch2.current_volume < 15)
            ch2.current_volume++;
          else if (!(ch2.nr22 & 0x08) && ch2.current_volume > 0)
            ch2.current_volume--;
        }
      }
    }
    if (ch4.envelope_timer > 0) {
      ch4.envelope_timer--;
      if (ch4.envelope_timer == 0) {
        ch4.envelope_timer = ch4.nr42 & 0x07;
        if (ch4.envelope_timer != 0) {
          if ((ch4.nr42 & 0x08) && ch4.current_volume < 15)
            ch4.current_volume++;
          else if (!(ch4.nr42 & 0x08) && ch4.current_volume > 0)
            ch4.current_volume--;
        }
      }
    }
  }

  void step_sweep() {
    if (ch1.sweep_enabled && ch1.sweep_timer > 0) {
      ch1.sweep_timer--;
      if (ch1.sweep_timer == 0) {
        int period = (ch1.nr10 >> 4) & 0x07;
        ch1.sweep_timer = (period == 0) ? 8 : period;
        if (ch1.sweep_enabled && period > 0) {
          int shift = ch1.nr10 & 0x07;
          int delta = ch1.shadow_frequency >> shift;
          int new_freq = ch1.shadow_frequency;

          if (ch1.nr10 & 0x08)
            new_freq -= delta; // Subtraction
          else
            new_freq += delta; // Addition

          if (new_freq > 2047) {
            ch1.enabled = false;
          } else if (new_freq >= 0) {
            ch1.shadow_frequency = new_freq;
            ch1.nr13 = new_freq & 0xFF;
            ch1.nr14 = (ch1.nr14 & 0xF8) | ((new_freq >> 8) & 0x07);
          }
        }
      }
    }
  }

  StereoSample get_sample() {
    if (!(nr52 & 0x80))
      return {0.0f, 0.0f};

    float left_out = 0.0f;
    float right_out = 0.0f;

    uint8_t out1 = ch1.get_output();
    uint8_t out2 = ch2.get_output();
    uint8_t out3 = ch3.get_output();
    uint8_t out4 = ch4.get_output();

    if (nr51 & 0x01)
      right_out += out1;
    if (nr51 & 0x10)
      left_out += out1;

    if (nr51 & 0x02)
      right_out += out2;
    if (nr51 & 0x20)
      left_out += out2;

    if (nr51 & 0x04)
      right_out += out3;
    if (nr51 & 0x40)
      left_out += out3;

    if (nr51 & 0x08)
      right_out += out4;
    if (nr51 & 0x80)
      left_out += out4;

    float vol_left = ((nr50 >> 4) & 0x07) + 1;
    float vol_right = (nr50 & 0x07) + 1;

    left_out *= vol_left;
    right_out *= vol_right;

    return {left_out / 480.0f, right_out / 480.0f};
  }

  uint8_t read_byte(uint16_t addr) {
    // Route to Channels
    if (addr >= 0xFF10 && addr <= 0xFF14)
      return ch1.read_byte(addr);
    if (addr >= 0xFF16 && addr <= 0xFF19)
      return ch2.read_byte(addr);
    if (addr >= 0xFF1A && addr <= 0xFF1E)
      return ch3.read_byte(addr);
    if (addr >= 0xFF20 && addr <= 0xFF23)
      return ch4.read_byte(addr);
    if (addr >= 0xFF30 && addr <= 0xFF3F)
      return ch3.read_wave_ram(addr);

    switch (addr) {
    case 0xFF24:
      return nr50;
    case 0xFF25:
      return nr51;
    case 0xFF26:
      uint8_t status = nr52 & 0x80;
      if (ch1.enabled)
        status |= 0x01;
      if (ch2.enabled)
        status |= 0x02;
      if (ch3.enabled)
        status |= 0x04;
      if (ch4.enabled)
        status |= 0x08;
      return status | 0x70; // Top bits usually 1
    }
    return 0xFF;
  }

  void write_byte(uint16_t addr, uint8_t value) {
    if (!(nr52 & 0x80) && addr != 0xFF26)
      return;

    if (addr >= 0xFF10 && addr <= 0xFF14)
      ch1.write_byte(addr, value);
    else if (addr >= 0xFF16 && addr <= 0xFF19)
      ch2.write_byte(addr, value);
    else if (addr >= 0xFF1A && addr <= 0xFF1E)
      ch3.write_byte(addr, value);
    else if (addr >= 0xFF20 && addr <= 0xFF23)
      ch4.write_byte(addr, value);
    else if (addr >= 0xFF30 && addr <= 0xFF3F)
      ch3.write_wave_ram(addr, value);

    else if (addr == 0xFF24)
      nr50 = value;
    else if (addr == 0xFF25)
      nr51 = value;
    else if (addr == 0xFF26) {
      bool turn_on = value & 0x80;
      if (turn_on && !(nr52 & 0x80)) {
        frame_sequencer = 0;
        frame_timer = 0;
      } else if (!turn_on && (nr52 & 0x80)) {
        clear_all_registers();
      }
      nr52 = value;
    }
  }

  void clear_all_registers() {
    nr50 = 0;
    nr51 = 0;
    ch1.enabled = false;
    ch2.enabled = false;
    ch3.enabled = false;
    ch4.enabled = false;
  }
};
