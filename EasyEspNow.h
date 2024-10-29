// Entry point for the library

#ifndef MESH_NOW_ESP_H
#define MESH_NOW_ESP_H

#if defined ESP32
#include "easy_esp_now.h"
#else
#error "Unsupported Platform"
#endif // ESP32

#endif // MESH_ESP_NOW_H