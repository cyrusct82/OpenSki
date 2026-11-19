/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/***************************************************
 * OpenSki - HiBean ESP32 BLE Roaster Control
 * Placa open de control pentru prajitor (inlocuieste placa originala).
 * Pornit din SkiBeanCommunity (MagnmCI, GPLv3).
 ***************************************************/

#include <Arduino.h>
#include <SPI.h>   // necesar aici ca PlatformIO LDF sa lege SPI pentru lib/SkiRTD.h
#include <PID_v1.h>
#include "../lib/SkiPins.h"
#include "../lib/SkiRTD.h"
#include "../lib/SkiOutputs.h"
#include "../lib/SkiDrum.h"
#include "../lib/SkiBLE.h"
#include "../lib/SkiLED.h"
#include "../lib/SkiCMD.h"
#include "../lib/SkiPIDConfig.h"

// -----------------------------------------------------------------------------
// Current Sketch and Release Version (for BLE device info)
// -----------------------------------------------------------------------------
#define FW_VERSION "v2.0.0-open"
String firmWareVersion = String(FW_VERSION);
String sketchName = String(__FILE__).substring(String(__FILE__).lastIndexOf('/')+1);

// -----------------------------------------------------------------------------
// Temperatura BT (boabe), in unitatea CorF
// -----------------------------------------------------------------------------
double temp = 0.0;
char CorF = 'C';

// -----------------------------------------------------------------------------
// Senzori si iesiri
// -----------------------------------------------------------------------------
SkiRTD    bt(CS_BT_PIN);   // PT100 boabe, MAX31865
SkiRTD    et(CS_ET_PIN);   // PT100 aer, MAX31865
SkiVent   vent;            // ventilator aer, PWM
SkiCool   cool;            // 2 ventilatoare racire, ON/OFF
SkiHeater heater;          // lampa IR, SSR
SkiDrum   drum;            // toba, PWM

// -----------------------------------------------------------------------------
// Comenzi primite de la HiBean prin BLE
// -----------------------------------------------------------------------------
std::queue<String> messageQueue;

// -----------------------------------------------------------------------------
// PID
// -----------------------------------------------------------------------------
double pInput, pOutput;
double pSetpoint = 0.0;
int manualHeatLevel = 50;

PIDConfig myPIDConfig;
PID myPID(&pInput, &pOutput, &pSetpoint,
        myPIDConfig.getKp(), myPIDConfig.getKi(), myPIDConfig.getKd(),
        myPIDConfig.getPMode(), DIRECT);

void setup() {
    // iesirile periculoase OFF imediat la pornire (pe langa pull-down-urile hardware)
    pinMode(HEATER_PIN, OUTPUT); digitalWrite(HEATER_PIN, LOW);
    pinMode(COOL_PIN, OUTPUT);   digitalWrite(COOL_PIN, LOW);

    Serial.begin(115200);
    ESP_LOGI("setup", "Starting OpenSki %s", FW_VERSION);
    delay(3000); // lasa upload-ul de firmware sa se termine

    heater.begin();
    cool.begin();
    vent.begin();
    drum.begin();
    bt.begin();
    et.begin();

    initBLE();

    myPID.SetMode(MANUAL);
    myPID.SetOutputLimits(0.0, myPIDConfig.getMaxPower());
    myPID.SetSampleTime(myPIDConfig.getSampleTime());

    shutdown();
}

void loop() {
    bt.update();
    et.update();
    drum.update();
    if (bt.valid()) temp = bt.get(CorF);

    // HiBean s-a deconectat => totul oprit imediat
    static bool wasConnected = false;
    if (wasConnected && !deviceConnected) { shutdown(); }
    wasConnected = deviceConnected;

    // nicio comanda de la HiBean de prea mult timp => totul oprit
    if (itsbeentoolong()) { shutdown(); }

    // comenzi de la HiBean (READ sau scriere)
    while (!messageQueue.empty()) {
        String msg = messageQueue.front();
        messageQueue.pop();
        parseAndExecuteCommands(msg);
    }

    handlePIDControl();   // calculeaza puterea ceruta
    applyOutputs();       // verificari de siguranta + trimite puterea la SSR
    heater.update();      // comuta SSR-ul in felii de 20 ms

    handleLED();
}
