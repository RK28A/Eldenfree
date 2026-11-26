#pragma once
#include <windows.h>
#include <cstdint>

namespace overlay {
bool install();
HWND hwnd();
uint32_t pad_buttons();
void pad_stick(float& x, float& y);
bool pad_seen();
void block_inputs_until_release();
}

namespace ui {
void setup();
void draw();
bool draw_menu_button();
}
