# OpenSki

Placă de control și firmware open-source care **înlocuiește placa proprietară** a prăjitorului și vorbește cu aplicația **HiBean** prin BLE.

ESP32-S3 comandă direct: lampa IR (SSR), ventilatorul de aer (PWM), toba (PWM), cele 2 ventilatoare de răcire (ON/OFF) și 2 sonde PT100 (MAX31865).
Nu mai există legătura serială cu placa originală.

Firmware-ul pornește din [SkiBeanCommunity](https://github.com/MagnmCI/SkiBeanCommunity) (MagnmCI, GPLv3). Partea de BLE, PID și comenzile HiBean/TC4 au rămas; stratul care vorbea cu prăjitorul a fost înlocuit cu drivere directe.

> **Atenție, 230 V.** Lampa se comută la 230 V. Montează partea de rețea doar dacă știi să lucrezi cu ea, separată fizic de partea de 24 V / ESP32, cu siguranță, și testează mai întâi cu lampa deconectată.

## Conexiuni

Schema cu fire, pagină cu pagină: `hardware/schema_conexiuni_fire.pdf`.

| Funcție | Modul | Pin ESP32-S3 |
|---|---|---|
| BT (boabe) | MAX31865 + PT100 3 fire, CS | GPIO 10 |
| ET (aer) | MAX31865 + PT100 3 fire, CS | GPIO 14 |
| SPI comun | SCK / MOSI / MISO | GPIO 12 / 11 / 13 |
| Tobă 24 V | D4184, PWM | GPIO 4 |
| Aer 24 V | D4184, PWM 20 kHz | GPIO 5 |
| Răcire 2× 24 V | D4184, ON/OFF | GPIO 6 |
| Lampă 230 V / 1000 W | SSR 25 A zero-cross, comandat prin D4184 de la 24 V | GPIO 7 |

Toți pinii și valorile reglabile sunt în `lib/SkiPins.h`.

## Siguranță în firmware

Lampa e forțată la 0% dacă: HiBean nu e conectat sau nu a trimis nicio comandă 10 s, senzorul BT e defect, BT > 270 °C sau ET > 330 °C, sau aerul e comandat sub 10%.
La deconectare, timeout sau `OFF` se opresc toate ieșirile. `ESTOP`: lampa 0%, aer 100%. Ieșirile sunt puse pe LOW la începutul `setup()`; pune și pull-down de 10 kΩ pe cele 4 GPIO de comandă.

## Compilare și flash

1. VS Code + extensia pioarduino, deschide folderul.
2. `pio run -e esp32-s3-devkitc-1`, apoi upload pe USB.

### Prima pornire (lampa deconectată!)
1. Alimentează doar 24 V și ESP32; deschide monitorul serial (115200).
2. Conectează-te din HiBean și trimite `READ`: BT și ET trebuie să arate temperatura camerei (-1.0 = senzor defect).
3. Testează ieșirile cu lampa încă deconectată: `OT2;50`, `DRUM;50`, `COOL;1`.
4. Cu `OT2;50` și HiBean conectat, trimite `OT1;10`: LED-ul de pe intrarea SSR trebuie să pâlpâie. Cu `OT2;0` trebuie să se oprească.
5. Abia apoi conectezi partea de 230 V.

## Stare

Scris fără acces la placa fizică: logica și conversia PT100 au fost verificate pe calculator, dar firmware-ul **nu a rulat încă pe hardware**.

## Licență

Cod: GNU GPL v3 (`LICENSE`), ca proiectul de bază. Documentele din `hardware/`: licența o alege cine administrează repository-ul.
