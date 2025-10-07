#pragma once
#include <Arduino.h>

// Encolar petición de escaneo (llamar desde API o Serial)
void requestScan();

// Consumir la petición (devuelve true la primera vez y la limpia)
bool consumeScanRequest();