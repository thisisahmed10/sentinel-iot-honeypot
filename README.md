# Sentinel-IoT Honeypot Suite

This repository is organized into four top-level folders:

- `esp/` - the ESP32 dashboard and captive portal
- `rpi/` - the Raspberry Pi honeypot running on Ubuntu 24
- `backend/` - the shared Go backend used by the honeypot, ESP32, and testing tools
- `testing/` - the Windows attack simulator and RPA reporting bot

## Flow

1. The Raspberry Pi honeypot listens on decoy services and forwards attack events to the Go backend.
2. The Go backend stores the events and exposes them through HTTP and WebSocket endpoints.
3. The ESP32 dashboard reads and posts events using the same backend contract.
4. The Windows testing tools can simulate an attack and replay the stored events for reporting.

## Backend

Run the backend on the Ubuntu 24 machine that will host the shared log service:

```powershell
cd backend
go mod tidy
go run .
```

The backend exposes:

- `POST /log`
- `GET /logs`
- `GET /latest`
- `WS /ws`

## Raspberry Pi Honeypot

Build the honeypot from the `rpi/` folder:

```powershell
cd rpi
make
```

On Ubuntu 24, run the generated binary from `rpi/bin/` after building. If the honeypot needs to bind privileged ports, run it with the appropriate privileges.

The backend target is still configurable at build time through `BACKEND_IP` and `BACKEND_PORT` if the Ubuntu host is not the default in `rpi/src/backend_client.c`.

## ESP32 Dashboard

The PlatformIO project lives in `esp/`.

Useful files:

- `esp/platformio.ini`
- `esp/src/main.cpp`
- `esp/data/index.html`
- `esp/data/admin.html`
- `esp/data/config.json`

The ESP32 backend URL is configured from the admin page or `esp/data/config.json`.

## Testing

The `testing/` folder contains the Windows attack simulator and the Playwright reporting bot.

Windows attack simulation:

```powershell
cd testing
$env:BACKEND_URL = "http://192.168.1.50:5000/log"
.\attack_windows.ps1 -SrcIp "10.0.0.23" -SrcPort 5555 -DstPort 23 -Payload "telnet probe"
```

RPA reporting bot:

```powershell
cd testing
pip install -r requirements.txt
playwright install
$env:BACKEND_URL = "http://192.168.1.50:5000/logs"
python .\report_bot.py
```

## Notes

- Set `BACKEND_URL` to the Ubuntu 24 host that runs the Go backend when testing from Windows.
- `testing/report_bot.py` reads from `GET /logs`.
- `testing/attack_windows.ps1` posts a JSON attack event to `POST /log`.
