#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include "SkiPins.h"

// Driver minimal pentru MAX31865 + PT100 (fara biblioteci externe).
// Doua instante (BT si ET) impart aceeasi magistrala SPI, fiecare cu CS-ul lui.

inline SPIClass& skiSPI() {
    static SPIClass spi(FSPI);
    return spi;
}

class SkiRTD {
public:
    explicit SkiRTD(int csPin) : _cs(csPin) {}

    void begin() {
        pinMode(_cs, OUTPUT);
        digitalWrite(_cs, HIGH);
        startBus();
        writeConfig(0);
        _last = 0;
    }

    // se apeleaza din loop(); nu blocheaza, citeste cel mult la SENSOR_READ_INTERVAL_MS
    void update() {
        uint32_t now = millis();
        if (now - _last < SENSOR_READ_INTERVAL_MS) return;
        _last = now;

        uint8_t b[2] = {0, 0};
        readN(0x01, b, 2);
        uint16_t raw = ((uint16_t)b[0] << 8) | b[1];

        if (raw & 0x0001) {                 // bit de eroare
            uint8_t f = 0;
            readN(0x07, &f, 1);
            _fault = f;
            writeConfig(0x02);              // sterge eroarea
            _ok = false;
            return;
        }
        raw >>= 1;
        if (raw == 0 || raw == 0x7FFF) {    // modul lipsa / scurtcircuit
            _fault = 0xFF;
            _ok = false;
            return;
        }
        double r = (double)raw * RTD_REF_OHM / 32768.0;
        double t = resistanceToC(r);
        if (isnan(t) || t < -60.0 || t > 600.0) {
            _ok = false;
            return;
        }
        _tempC = t;
        _fault = 0;
        _ok = true;
        _lastGood = now;
    }

    bool   valid() const { return _ok && (millis() - _lastGood) < SENSOR_STALE_MS; }
    double getC()  const { return _tempC; }
    double get(char unit) const { return unit == 'F' ? _tempC * 9.0 / 5.0 + 32.0 : _tempC; }
    uint8_t fault() const { return _fault; }

private:
    int      _cs;
    uint32_t _last = 0, _lastGood = 0;
    double   _tempC = 0.0;
    bool     _ok = false;
    uint8_t  _fault = 0xFF;

    static bool& busStarted() { static bool s = false; return s; }
    static void startBus() {
        if (!busStarted()) {
            skiSPI().begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, -1);
            busStarted() = true;
        }
    }

    // registru 0x00: D7 bias, D6 conversie automata, D4 3 fire, D1 sterge eroare, D0 filtru 50 Hz
    void writeConfig(uint8_t extra) {
        uint8_t cfg = 0xC0 | extra;
        if (RTD_THREE_WIRE)   cfg |= 0x10;
        if (RTD_FILTER_50HZ)  cfg |= 0x01;
        writeReg(0x00, cfg);
    }

    void writeReg(uint8_t addr, uint8_t val) {
        skiSPI().beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
        digitalWrite(_cs, LOW);
        skiSPI().transfer(addr | 0x80);
        skiSPI().transfer(val);
        digitalWrite(_cs, HIGH);
        skiSPI().endTransaction();
    }

    void readN(uint8_t addr, uint8_t* buf, int n) {
        skiSPI().beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
        digitalWrite(_cs, LOW);
        skiSPI().transfer(addr & 0x7F);
        for (int i = 0; i < n; i++) buf[i] = skiSPI().transfer(0x00);
        digitalWrite(_cs, HIGH);
        skiSPI().endTransaction();
    }

    // Callendar-Van Dusen (ca in biblioteca Adafruit MAX31865)
    static double resistanceToC(double Rt) {
        const double A = 3.9083e-3, B = -5.775e-7;
        double Z1 = -A;
        double Z2 = A * A - (4.0 * B);
        double Z3 = (4.0 * B) / RTD_NOMINAL_OHM;
        double Z4 = 2.0 * B;
        double t = Z2 + Z3 * Rt;
        t = (sqrt(t) + Z1) / Z4;
        if (t >= 0) return t;
        Rt /= RTD_NOMINAL_OHM;
        Rt *= 100.0;
        double rp = Rt;
        t = -242.02;
        t += 2.2228 * rp;     rp *= Rt;
        t += 2.5859e-3 * rp;  rp *= Rt;
        t -= 4.8260e-6 * rp;  rp *= Rt;
        t -= 2.8183e-8 * rp;  rp *= Rt;
        t += 1.5243e-10 * rp;
        return t;
    }
};
