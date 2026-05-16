# Testing

This folder holds the Windows attack simulator and the RPA report bot.

## Windows attack simulator

Use `attack_windows.ps1` from a Windows device to send an attack event to the shared Go backend.

```powershell
$env:BACKEND_URL = "http://192.168.1.50:5000/log"
.\attack_windows.ps1 -SrcIp "10.0.0.23" -SrcPort 5555 -DstPort 23 -Payload "telnet probe"
```

If `BACKEND_URL` is not set, the script defaults to `http://127.0.0.1:5000/log`.

## RPA report bot

`report_bot.py` reads `GET /logs` from the Go backend and automates the reporting workflow.

```powershell
$env:BACKEND_URL = "http://192.168.1.50:5000/logs"
python .\report_bot.py
```

Install the Python dependency first:

```powershell
pip install -r requirements.txt
playwright install
```
