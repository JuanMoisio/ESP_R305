#pragma once
#include <Arduino.h>

// Solicita modo escaneo: muestra "Ponga su huella" y espera el dedo.
// timeoutMs = 0 => sin timeout (se puede cancelar manualmente con cancelScan)
void requestScan(unsigned long timeoutMs = 15000);

// Cancela la petición de escaneo
void cancelScan();

// Consultar si está activo el modo "esperando dedo"
bool isScanRequested();