#pragma once
#include <windows.h>

namespace overlay {
bool install();
HWND hwnd();
}

namespace ui {
void setup();
void draw();
}
