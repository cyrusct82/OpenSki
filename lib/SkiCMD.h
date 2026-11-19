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

#pragma once
#include <PID_v1.h>
#include "SkiPins.h"
#include "SkiRTD.h"
#include "SkiOutputs.h"
#include "SkiDrum.h"

// -----------------------------------------------------------------------------
// Comenzile HiBean (TC4) catre prajitor. Aici nu mai exista protocolul serial al
// placii originale: fiecare comanda actioneaza direct iesirile placii.
// https://github.com/greencardigan/TC4-shield/blob/master/applications/Artisan/aArtisan/trunk/src/aArtisan/commands.txt
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Variabile externe (definite in OpenSki.ino / SkiBLE.h)
// -----------------------------------------------------------------------------
extern double temp;          // BT in unitatea curenta (CorF)
extern char CorF;
extern PID myPID;
extern PIDConfig myPIDConfig;
extern double pInput, pOutput, pSetpoint;
extern int manualHeatLevel;
extern SkiRTD bt;            // PT100 boabe (MAX31865)
extern SkiRTD et;            // PT100 aer (MAX31865)
extern SkiVent vent;
extern SkiCool cool;
extern SkiHeater heater;
extern SkiDrum drum;
extern bool deviceConnected; // din SkiBLE.h

int  drumSpeedPct = 100;     // viteza folosita cand se primeste DRUM;1
bool drumOn = false;

// ce a cerut HiBean / PID-ul, si ce ajunge efectiv la iesiri
int heatRequest = 0;         // cerut (0-100 %)
int heatApplied = 0;         // aplicat dupa verificarile de siguranta
int ventRequest = 0;
int coolRequest = 0;

// -----------------------------------------------------------------------------
// Heartbeat: orice comanda primita de la HiBean reimprospateaza timpul.
// Fara nicio comanda timp de COMM_TIMEOUT_MS => totul oprit (vezi loop()).
// -----------------------------------------------------------------------------
unsigned long lastEventTime = 0;   // micros()
const unsigned long LAST_EVENT_TIMEOUT = COMM_TIMEOUT_MS * 1000UL;

bool itsbeentoolong() {
    unsigned long duration = micros() - lastEventTime;
    return (duration > LAST_EVENT_TIMEOUT);
}

// Forward declarations
void handleCHAN();
void handleOT1(int value);
void handleREAD();
void handleHEAT(int value);
void handleVENT(int value);
void handleDRUM(int value);
void handleDRUMSPD(int value);
void handleFILTER(int value);
void handleCOOL(int value);
void eStop();
void handlePIDControl();
void setPIDMode(bool usePID);
void applyOutputs();

// -----------------------------------------------------------------------------
// Oprire completa: toate iesirile OFF, PID pe manual
// -----------------------------------------------------------------------------
void shutdown() {
    heatRequest = 0;
    manualHeatLevel = 0;
    myPID.SetMode(MANUAL);
    ventRequest = 0;
    vent.setPercent(0);
    coolRequest = 0;
    cool.setPercent(0);
    drumOn = false;
    drum.stop();
    heater.setPercent(0);
    heatApplied = 0;
}

// -----------------------------------------------------------------------------
// Siguranta lampa: se aplica la FIECARE iteratie din loop(), indiferent ce cere HiBean/PID
// -----------------------------------------------------------------------------
void applyOutputs() {
    int heat = heatRequest;
    const char* reason = nullptr;

    if (!deviceConnected)                              reason = "BLE deconectat";
    else if (itsbeentoolong())                         reason = "timeout comenzi HiBean";
    else if (!bt.valid())                              reason = "senzor BT defect";
    else if (bt.getC() > BT_MAX_C)                     reason = "BT peste limita";
    else if (et.valid() && et.getC() > ET_MAX_C)       reason = "ET peste limita";
    else if (ventRequest < VENT_MIN_FOR_HEAT)          reason = "aer insuficient";

    if (reason) heat = 0;

    static const char* lastReason = nullptr;
    if (reason != lastReason) {
        if (reason && heatRequest > 0) ESP_LOGW("SkiCMD", "Lampa oprita: %s", reason);
        lastReason = reason;
    }

    heatApplied = heat;
    heater.setPercent(heat);
}

// -----------------------------------------------------------------------------
// Command handlers
// -----------------------------------------------------------------------------
void handleCHAN() {
    String message = "# Active channels set to 2100\n";
    notifyNimBLEClient(message);
}

void handleOT1(int value) {
    if (myPID.GetMode() == MANUAL) {
        manualHeatLevel = constrain(value, 0, 100);   // putere manuala
        handleHEAT(manualHeatLevel);
    } else if (myPID.GetMode() == AUTOMATIC) {
        myPIDConfig.setMaxPower(constrain(value, 0, 100));
        myPIDConfig.apply(myPID);
    }
}

void handleREAD() {
    String etStr = et.valid() ? String(et.get(CorF), 1) : String("-1.0");
    String btStr = bt.valid() ? String(bt.get(CorF), 1) : String("-1.0");
    String ch1 = (ET_CHANNEL == 1) ? etStr : btStr;
    String ch2 = (ET_CHANNEL == 1) ? btStr : etStr;
    // heat = puterea aplicata efectiv la SSR (dupa verificarile de siguranta)
    String readMsg = "0," + ch1 + "," + ch2 + "," +
          String(heatApplied) + "," +
          String(ventRequest) + "\n";
    notifyNimBLEClient(readMsg);
}

void handleHEAT(int value) {
    heatRequest = constrain(value, 0, 100);
}

void handleVENT(int value) {
    if (value >= 0 && value <= 100) {
        ventRequest = value;
        vent.setPercent(value);
    }
}

void handleDRUM(int value) {
    // DRUM;0   = oprit
    // DRUM;1   = pornit la ultima viteza folosita (implicit 100 %)
    // DRUM;2..100 = pornit la acea viteza in % (slider HiBean)
    if (value > 100) value = 100;
    if (value < 0) value = 0;
    if (value >= 2) drumSpeedPct = value;
    drumOn = (value != 0);
    drum.setSpeed(drumOn ? drumSpeedPct : 0);
}

void handleDRUMSPD(int value) {
    // DRUMSPD;XX = viteza tobei 1-100 %
    drumSpeedPct = constrain(value, 1, 100);
    if (drumOn) drum.setSpeed(drumSpeedPct);
}

void handleFILTER(int value) {
    // Aceasta placa nu are ventilator de filtru; comanda este acceptata si ignorata.
    ESP_LOGI("SkiCMD", "FILTER %i ignorat (fara ventilator de filtru)", value);
}

void handleCOOL(int value) {
    // Cele doua ventilatoare de racire merg ON/OFF: orice valoare > 0 = pornit.
    if (value >= 0 && value <= 100) {
        coolRequest = value;
        cool.setPercent(value);
    }
}

void eStop() {
    ESP_LOGE("SkiCMD", "Emergency Stop! Lampa oprita, aer 100%%.");
    setPIDMode(false);   // PID pe manual, putere 0 (altfel PID-ul ar reporni lampa)
    handleVENT(100);
}

// Reglarea puterii lampei: PID sau nivel manual
void handlePIDControl() {
    if (myPID.GetMode() == AUTOMATIC) {
        pInput = temp;                 // temperatura BT curenta
        myPID.Compute();
        int roundedHeat = (int)(std::round(pOutput / 5.0) * 5);
        handleHEAT(roundedHeat);
    } else {
        handleHEAT(manualHeatLevel);
    }
}

void setPIDMode(bool usePID) {
    if (usePID) {
        myPID.SetMode(AUTOMATIC);
        ESP_LOGI("SkiCMD", "PID mode set to AUTOMATIC");
    } else {
        myPID.SetMode(MANUAL);
        manualHeatLevel = 0;           // putere 0 pentru siguranta
        handleHEAT(manualHeatLevel);
        ESP_LOGI("SkiCMD", "PID mode set to MANUAL");
    }
}

void parseAndExecuteCommands(String input) {
    lastEventTime = micros();          // heartbeat: HiBean este activ

    input.trim();
    input.toUpperCase();

    int split1 = input.indexOf(';');
    String command = "";
    String param = "";
    String subcommand = "";

    if (split1 >= 0) {
        command = input.substring(0, split1);
        String remainder = input.substring(split1 + 1);
        int split2 = remainder.indexOf(';');

        if (split2 >= 0) {
            subcommand = remainder.substring(0, split2);
            param = remainder.substring(split2 + 1);
        } else {
            param = remainder;
        }
    } else {
        command = input;
    }

    if (command == "PID") {
        if (param == "ON") {
            setPIDMode(true);
        } else if (param == "OFF") {
            setPIDMode(false);
        } else if (subcommand == "SV") {
            double newSetpoint = param.toDouble();
            if (newSetpoint > 0 && newSetpoint <= 300) {
                pSetpoint = newSetpoint;
                ESP_LOGI("SkiCMD", "New Setpoint: %f", pSetpoint);
            }
        } else if (subcommand == "T") {
            double pidTune[3] = { myPIDConfig.getKp(), myPIDConfig.getKi(), myPIDConfig.getKd() };
            int paramCount = 0;
            while (param.length() > 0 && paramCount < 3) {
                int index = param.indexOf(';');
                if (index == -1) {
                    pidTune[paramCount++] = param.toDouble();
                    break;
                } else {
                    pidTune[paramCount++] = param.substring(0, index).toDouble();
                    param = param.substring(index + 1);
                }
            }
            myPIDConfig.setKp(pidTune[0]);
            myPIDConfig.setKi(pidTune[1]);
            myPIDConfig.setKd(pidTune[2]);
            myPIDConfig.apply(myPID);
        } else if (subcommand == "PM") {
            ESP_LOGI("SkiCMD", "Setting PMode to: %s", param.c_str());
            if (param == "M") {
                myPIDConfig.setPMode(P_ON_M);
            } else {
                myPIDConfig.setPMode(P_ON_E);
            }
            myPIDConfig.apply(myPID);
        } else if (subcommand == "CT") {
            ESP_LOGI("SkiCMD", "Setting Cycle Time to: %i", (int)param.toInt());
            myPIDConfig.setSampleTime(param.toInt());
            myPIDConfig.apply(myPID);
        }
    } else if (command == "OT1") {
        ESP_LOGI("SkiCMD", "Setting OT1: %i", (int)param.toInt());
        handleOT1(param.toInt());
    } else if (command == "READ") {
        handleREAD();
    } else if (command == "OT2") {
        ESP_LOGI("SkiCMD", "Setting OT2: %i", (int)param.toInt());
        handleVENT(param.toInt());
    } else if (command == "OFF") {
        shutdown();
    } else if (command == "ESTOP") {
        eStop();
    } else if (command == "DRUMSPD") {
        ESP_LOGI("SkiCMD", "Setting Drum speed: %i", (int)param.toInt());
        handleDRUMSPD(param.toInt());
    } else if (command == "DRUM") {
        ESP_LOGI("SkiCMD", "Setting Drum: %i", (int)param.toInt());
        handleDRUM(param.toInt());
    } else if (command == "FILTER") {
        handleFILTER(param.toInt());
    } else if (command == "COOL") {
        ESP_LOGI("SkiCMD", "Setting Cool: %i", (int)param.toInt());
        handleCOOL(param.toInt());
    } else if (command == "CHAN") {
        handleCHAN();
    } else if (command == "UNITS") {
        if (split1 >= 0) CorF = input.charAt(split1 + 1);
    }
}
