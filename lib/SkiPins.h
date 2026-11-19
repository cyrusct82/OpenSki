#pragma once
// =============================================================================
// SkiPins.h - toti pinii si toate valorile care se pot regla, intr-un singur loc
// Placa: ESP32-S3 DevKitC-1 N16R8. NU folosi GPIO 26-37 (flash/PSRAM) si nici 0,3,45,46,19,20,43,44.
// =============================================================================

// ---- SPI comun pentru cele doua module MAX31865 (PT100) ----
#define SPI_SCK_PIN    12   // -> CLK pe ambele MAX31865
#define SPI_MOSI_PIN   11   // -> SDI pe ambele MAX31865
#define SPI_MISO_PIN   13   // <- SDO de la ambele MAX31865
#define CS_BT_PIN      10   // CS MAX31865 pentru BT (temperatura boabelor)
#define CS_ET_PIN      14   // CS MAX31865 pentru ET (temperatura aerului)

// ---- iesiri (prin module D4184) ----
// Toba (drum) este in SkiDrum.h: GPIO 4
#define VENT_PWM_PIN   5    // ventilator aer (PWM)
#define COOL_PIN       6    // ventilatoare racire (ON/OFF, ambele)
#define HEATER_PIN     7    // comanda SSR lampa (prin D4184)

// ---- ventilator aer ----
#define VENT_PWM_FREQ  20000   // Hz
#define VENT_PWM_BITS  10
#define VENT_MIN_PCT   20      // PWM-ul la comanda HiBean 1%, ca motorul sa nu se blocheze

// ---- lampa (SSR) ----
// Fereastra de 20 de felii x 20 ms = 400 ms. Fiecare felie = o perioada de retea la 50 Hz.
// Puterea se da in pasi de 5% (1 felie), felii pornite distribuite uniform.
#define HEATER_SLOTS     20
#define HEATER_SLOT_MS   20

// ---- siguranta ----
#define VENT_MIN_FOR_HEAT   10      // lampa porneste doar daca aerul e comandat la cel putin atat (%)
#define BT_MAX_C            270.0   // peste aceasta temperatura (BT) lampa se opreste
#define ET_MAX_C            330.0   // peste aceasta temperatura (ET) lampa se opreste
#define COMM_TIMEOUT_MS     10000UL // fara nicio comanda de la HiBean atata timp => totul oprit

// ---- senzori PT100 pe MAX31865 ----
#define RTD_NOMINAL_OHM     100.0   // PT100
#define RTD_REF_OHM         430.0   // rezistorul de referinta de pe modul (430 pentru PT100, 4300 pentru PT1000)
#define RTD_THREE_WIRE      1       // 1 = sonda cu 3 fire, 0 = 2 sau 4 fire
#define RTD_FILTER_50HZ     1       // 1 = filtru 50 Hz (Europa)
#define SENSOR_READ_INTERVAL_MS 250
#define SENSOR_STALE_MS         2000 // daca nu avem o citire buna atata timp, senzorul e considerat defect

// In raspunsul READ: pe ce canal TC4 apare ET (1 sau 2); BT e pe celalalt
#define ET_CHANNEL 1
