# Sentinel-IoT Honeypot Suite

This repository now contains the full Sentinel-IoT stack in one place:

- `src/` and `hardware/` hold the Raspberry Pi honeypot bait.
- `network-programming-project/` holds the ESP32 dashboard and captive portal.
- `honybotrpa/` holds the Go CSV logger and the Python RPA bot.

Project flow

1. The Raspberry Pi honeypot listens on decoy services, captures the first payload, and forwards a JSON `AttackEvent` to the backend.
2. The backend stores logs and exposes them through HTTP.
3. The ESP32 dashboard can log its own page-hit/login events to the backend and expose control routes.
4. The RPA module consumes CSV attack records and automates a browser workflow for reporting or simulation.

Raspberry Pi honeypot

Build the C component from the repository root:

```bash
make
```

Run it with root privileges if you need to bind ports 23 and 80:

```bash
sudo ./bin/pi_bait
```

The backend target can be overridden at compile time:

```bash
make CFLAGS='-std=c11 -Wall -Wextra -O2 -g -DBACKEND_IP="\"192.168.1.10\"" -DBACKEND_PORT="\"5000\""'
```

ESP32 dashboard

The integrated ESP32 project lives under `network-programming-project/` and keeps the PlatformIO layout intact.

Useful files:

- `network-programming-project/platformio.ini`
- `network-programming-project/src/main.cpp`
- `network-programming-project/data/index.html`
- `network-programming-project/data/admin.html`
- `network-programming-project/data/config.json`

RPA tooling

The RPA project is preserved under `honybotrpa/RPA_Project/`.

Useful files:

- `honybotrpa/RPA_Project/backend/main.go`
- `honybotrpa/RPA_Project/backend/attacks.csv`
- `honybotrpa/RPA_Project/bot/report_bot.py`

Notes

- The Node backend from the dashboard project accepts POST `/log` and GET `/logs`.
- The Go backend in the RPA tree is a simple CSV simulator, not a live bridge.
- The ESP32 control button currently toggles the LED locally; there is no packet-filter kill switch implemented yet.
