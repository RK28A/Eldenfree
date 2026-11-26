#pragma once
#include <windows.h>
#include <cstdint>

namespace overlay {
bool install();
HWND hwnd();
uint32_t pad_buttons();
void pad_stick(float& x, float& y);
bool pad_seen();
}

namespace ui {
void setup();
void draw();
}
