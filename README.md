# Phoenix Eye - 8 Channel Relay Controller

Target: classic ESP32 DOIT ESP32 DEVKIT V1 (ESP32-WROOM-32).

## Arduino IDE upload
1. Extract this folder and open phoenix_eye_8ch.ino inside phoenix_eye_8ch.
2. Install the Espressif ESP32 board package using Arduino Boards Manager.
3. Install ArduinoJson version 6.x using Library Manager.
4. Edit config.h and enter your Wi-Fi name and password.
5. Select DOIT ESP32 DEVKIT V1, choose the connected COM port, and Upload.
6. Open Serial Monitor at 115200 baud. If upload is stuck connecting, hold BOOT until upload begins.
7. On the same Wi-Fi, open http://192.168.8.132 .

Static network configuration remains 192.168.8.132 / gateway 192.168.8.1 / subnet 255.255.255.0. Change the IPAddress settings in the sketch for a different network. Ensure this IP is available; reserve it on your router. Wi-Fi association has a startup timeout of 20 seconds; the main loop effects use millis(), with no delay().

## Wiring
| Relay input | ESP32 GPIO | Effect group |
|---|---|---|
| IN1 | 25 | Left |
| IN2 | 26 | Right |
| IN3 | 27 | Left |
| IN4 | 32 | Right |
| IN5 | 33 | Left |
| IN6 | 18 | Right |
| IN7 | 19 | Left |
| IN8 | 23 | Right |

Use a relay module accepting 3.3 V logic, with a power supply matching its rated coil voltage and sufficient current for eight coils. Do not power relay coils from ESP32 GPIO or its 3.3 V output. Connect control-side grounds according to the module wiring; isolated JD-VCC modules require their manufacturer's wiring instructions. Do not connect 5 V output signals to ESP32 GPIO. For mains loads, use an enclosure and qualified wiring.

Active LOW is the default. For an active HIGH module change RELAY_ACTIVE_LOW to false in config.h. Firmware initializes every output OFF; reset/boot time before firmware runs may require external pull resistors or module-specific input protection.

All 16 original effects are retained and applied to odd/even channel groups. Individual relay commands stop effects and preserve the other seven current states without deliberately pulsing them OFF. OFF stops all channels. The status display reports commanded GPIO states, not physical relay-contact feedback. Fast blink/fire effects switch mechanical relays frequently; choose slow or static modes for normal operation.

## HTTP API
- GET / : embedded dashboard (no separate HTML upload required)
- GET /status : mode, modeId, relays[8], pins[8], leftEye, rightEye, ip, rssi
- GET /relay?ch=1&on=1 : channel 1 ON (channels 1 through 8)
- GET /relay?ch=all&on=0 : all channels OFF in direct mode
- GET /relay?eye=left&on=1 : odd channels ON; right uses even channels; both uses all
- GET /mode?m=off : stop effects and turn all OFF
- Mode keys: off, normal, fast, slow, random, leftwink, rightwink, alternate, double, heartbeat, scan, angry, fire, wakeup, sleep, show

The dashboard controls your local network device without login; do not expose it through router port forwarding.

GPIO reference: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html

## Verification
Source and dashboard JavaScript checks were performed. ESP32 compilation and a physical relay test were not performed in this environment; verify with Arduino IDE and your board before connecting loads.

