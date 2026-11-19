# OpenSki

Open-source control board and firmware that **replaces the proprietary controller board** of a Skywalker-style
coffee roaster and talks to the **HiBean** app over BLE.

The ESP32-S3 drives everything directly: infrared lamp (SSR), air fan (PWM), drum motor (PWM),
two bean-cooling fans (ON/OFF) and two PT100 probes (MAX31865). No serial link to the original board is used any more.

The firmware is derived from [SkiBeanCommunity](https://github.com/MagnmCI/SkiBeanCommunity) (MagnmCI, GPLv3),
which in turn builds on [Skywalker Roaster Labs](https://github.com/jmoore52/SkywalkerRoaster). BLE layer, PID
and HiBean/TC4 command set are kept; the roaster-serial layer was replaced by direct hardware drivers.

> **Mains voltage warning.** The lamp is switched at 230 V. Build the mains part only if you know how to work with it,
> keep it physically separated from the 24 V / ESP32 side, fuse it, and test everything with the lamp disconnected first.
> This is a volunteer project, provided as-is, without warranty.

## Hardware (see `hardware/schema_conexiuni_fire.pdf`, wiring page by page)

| Function | Module | ESP32-S3 pin |
|---|---|---|
| Bean temperature (BT) | MAX31865 + PT100 (3-wire), CS | GPIO 10 |
| Air temperature (ET) | MAX31865 + PT100 (3-wire), CS | GPIO 14 |
| SPI (shared) | SCK / MOSI / MISO | GPIO 12 / 11 / 13 |
| Drum motor (24 V) | D4184 MOSFET module, PWM | GPIO 4 |
| Air fan (24 V) | D4184, PWM 20 kHz | GPIO 5 |
| Cooling fans (2x 24 V) | D4184, ON/OFF | GPIO 6 |
| Lamp 230 V / 1000 W | SSR 25 A zero-cross, driven through a D4184 from 24 V | GPIO 7 |

All pins and tunables (PWM frequency, temperature limits, timeouts, PT100 reference resistor) are in `lib/SkiPins.h`.
Board: ESP32-S3 DevKitC-1 (N16R8 is fine; GPIO 26-37 are not used).

## Safety logic in the firmware

The lamp is forced to 0 % whenever any of these is true, regardless of what HiBean or the PID asks:
- no BLE client connected, or no command received for 10 s (`COMM_TIMEOUT_MS`)
- the BT probe has a fault or no valid reading
- BT above 270 °C or ET above 330 °C (`BT_MAX_C`, `ET_MAX_C`)
- the air fan is commanded below 10 % (`VENT_MIN_FOR_HEAT`)

On BLE disconnect, timeout or `OFF`, all outputs are switched off and the PID goes to manual. `ESTOP` sets heater 0 % and air 100 %.
Outputs are forced LOW at the very start of `setup()`; add 10 kΩ pull-downs on the four control GPIOs in hardware too.

The lamp is switched in 20 ms slices (one mains cycle at 50 Hz), 20 slices per window, 5 % steps, evenly distributed.

## Commands (HiBean / TC4 style, case-insensitive)

`READ`, `OT1;xx` (heater %), `OT2;xx` (air %), `DRUM;xx`, `DRUMSPD;xx`, `COOL;xx` (>0 = fans on), `FILTER;xx` (accepted, ignored),
`PID;ON|OFF`, `PID;SV;xxx`, `PID;T;p;i;d`, `PID;CT;ms`, `PID;PM;M|E`, `OFF`, `ESTOP`, `CHAN`, `UNITS;C|F`.
`READ` returns `0,ET,BT,heat,vent` (heat = power actually applied to the SSR after the safety checks; swap with `ET_CHANNEL`).

## Build and flash

1. Install VS Code + the [pioarduino](https://github.com/pioarduino) extension, open this folder.
2. `pio run -e esp32-s3-devkitc-1`, then upload over USB.
3. Tag a release (`git tag v2.0.0 && git push origin v2.0.0`) to get a ready-to-flash `.bin` from GitHub Actions.

### First power-up checklist (lamp disconnected!)
1. Power only the 24 V side and the ESP32; open the serial monitor (115200).
2. Connect from HiBean (or a BLE terminal such as nRF Connect, Nordic UART service) and send `READ`: BT and ET must show room temperature (-1.0 means sensor fault).
3. Test each output with the lamp still disconnected: `OT2;50`, `DRUM;50`, `COOL;1`.
4. Set `OT1;10` with `OT2;50` and a connected client: the SSR input LED should blink. Remove air (`OT2;0`) and it must stop.
5. Only then connect the 230 V side.

## Status

Written without access to a physical board: the logic and the PT100 conversion were checked on a PC, but the firmware has **not yet been run on hardware**. Please report what you find.

## License

Code: GNU GPL v3 (see `LICENSE`), as the upstream project. Hardware documents in `hardware/`: license to be chosen by the maintainer
(CERN-OHL-S or CC-BY-SA are the usual choices for open hardware).
