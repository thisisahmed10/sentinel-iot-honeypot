# pi_bait — Raspberry Pi Honeypot Bait

Lightweight C-based honeypot for Raspberry Pi 4. Implements a `select()`-based multiplexer that listens on decoy ports (default: 23 and 80), captures initial payloads, logs events locally, and forwards JSON records to a central Go backend over TCP.

Quick start

Prerequisites: `gcc`, build-essential, network access to your backend. Binding ports <1024 requires root or an alternative (authbind or using high ports for testing).

Build:

```bash
make
```

Run (as root to bind 23/80):

```bash
sudo ./bin/pi_bait
```

Configuration

- Backend host/port and logfile are set in `hardware/include/honeypot.h` and `src/backend_stub.c` — update `BACKEND_IP` and `BACKEND_PORT` to point at your aggregator.
- The listened ports are defined in `src/main.c` under the `ports` array.

Logs

- Events are appended to `/var/log/pi_bait.log` by default in the earlier local-only version of the codebase; the current sender writes directly to the backend.

Systemd unit (example)

Save this as `/etc/systemd/system/pi_bait.service` and adjust `ExecStart` to the installed binary path.

```ini
[Unit]
Description=pi_bait honeypot
After=network.target

[Service]
Type=simple
User=root
ExecStart=/home/ahmed/sentinel-iot-honeypot/bin/pi_bait
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

Testing

- From the Pi itself, trigger a connection and send a test payload:

```bash
printf "HELLO" | nc 127.0.0.1 80
curl -v http://127.0.0.1/
```

Notes and next steps

- The implementation follows a minimal UNP-style approach (BSD sockets + `select()`).
- Consider running the binary under a dedicated unprivileged user and using `authbind` or `setcap` if you must avoid full root.
