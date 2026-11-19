#pragma once
#include <Arduino.h>
#include "SkiPins.h"

// ---------------- ventilator aer: PWM ----------------
class SkiVent {
public:
    void begin() {
        ledcAttach(VENT_PWM_PIN, VENT_PWM_FREQ, VENT_PWM_BITS);
        setPercent(0);
    }
    // 0 = oprit, 1..100 = de la VENT_MIN_PCT la 100 % PWM
    void setPercent(int v) {
        v = constrain(v, 0, 100);
        _pct = v;
        int duty = 0;
        if (v > 0) duty = VENT_MIN_PCT + (v - 1) * (100 - VENT_MIN_PCT) / 99;
        ledcWrite(VENT_PWM_PIN, (uint32_t)duty * ((1 << VENT_PWM_BITS) - 1) / 100);
    }
    int getPercent() const { return _pct; }
private:
    int _pct = 0;
};

// ---------------- ventilatoare racire: ON/OFF ----------------
class SkiCool {
public:
    void begin() { pinMode(COOL_PIN, OUTPUT); setPercent(0); }
    void setPercent(int v) {
        _pct = constrain(v, 0, 100);
        digitalWrite(COOL_PIN, _pct > 0 ? HIGH : LOW);
    }
    int getPercent() const { return _pct; }
private:
    int _pct = 0;
};

// ---------------- lampa: SSR comandat in felii de 20 ms ----------------
class SkiHeater {
public:
    void begin() { pinMode(HEATER_PIN, OUTPUT); digitalWrite(HEATER_PIN, LOW); _slots = 0; }
    // 0..100 %, rotunjit la pasi de 5 %
    void setPercent(int p) {
        p = constrain(p, 0, 100);
        _slots = (p * HEATER_SLOTS + 50) / 100;   // 0..20
    }
    int getPercent() const { return _slots * 100 / HEATER_SLOTS; }
    // se apeleaza des din loop()
    void update() {
        if (_slots <= 0)            { digitalWrite(HEATER_PIN, LOW);  return; }
        if (_slots >= HEATER_SLOTS) { digitalWrite(HEATER_PIN, HIGH); return; }
        uint32_t slot = (millis() / HEATER_SLOT_MS) % HEATER_SLOTS;
        // feliile pornite sunt distribuite uniform in fereastra
        bool on = (((slot + 1) * _slots) / HEATER_SLOTS) > ((slot * _slots) / HEATER_SLOTS);
        digitalWrite(HEATER_PIN, on ? HIGH : LOW);
    }
private:
    int _slots = 0;
};
