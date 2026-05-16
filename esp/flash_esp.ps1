Param(
    [string]$Port
)

Set-StrictMode -Version Latest
Push-Location -ErrorAction Stop
try {
    # Change into the esp project directory (script lives in esp/)
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    Set-Location $scriptDir

    Write-Host "Ensuring PlatformIO is installed..."
    if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
        python -m pip install -U platformio
    }

    Write-Host "Building firmware..."
    pio run -e esp32dev

    Write-Host "Uploading LittleFS data..."
    pio run -e esp32dev -t uploadfs

    if (-not $Port) {
        # Prefer USB-serial adapters by querying Win32_SerialPort descriptions
        $candidatePorts = @()
        try {
            $devices = Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue
            if ($devices) {
                foreach ($d in $devices) {
                    $desc = $d.Description -as [string]
                    if ($desc -match 'CP210|CH340|FTDI|Silicon|USB Serial|USB-SERIAL|CP2102|CP210x|CH34|UART') {
                        $candidatePorts += $d.DeviceID
                    }
                }
            }
        } catch { }

        if ($candidatePorts.Count -eq 1) {
            $Port = $candidatePorts[0]
            Write-Host "Auto-selected serial port: $Port"
        } else {
            # fallback to all ports
            $ports = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
            if ($ports.Length -eq 0) {
                Write-Error "No serial ports found. Connect your ESP device and retry or pass -Port COMx."; exit 1
            } elseif ($ports.Length -eq 1) {
                $Port = $ports[0]
                Write-Host "Using sole detected port: $Port"
            } else {
                Write-Host "Multiple serial ports detected:`n$($ports -join "`n")"
                $default = $candidatePorts | Select-Object -First 1
                if ($default) { Write-Host "Press Enter to use $default or type another port." }
                $choice = Read-Host "Enter the port to use (e.g. COM3)" 
                if (-not $choice) {
                    if ($default) { $Port = $default } else { Write-Error "No port selected. Aborting."; exit 1 }
                } else {
                    $Port = $choice
                }
            }
        }
    }

    Write-Host "Uploading firmware to $Port..."
    # use explicit long-form flags to avoid CLI parsing inconsistencies
    try {
        pio run --target upload --upload-port $Port
    } catch {
        Write-Error ("Firmware upload failed: {0}" -f $_)
        Write-Host ("You can retry using: .\flash_esp.ps1 -Port {0}" -f $Port)
        exit 1
    }

    Write-Host "Opening serial monitor on $Port (115200). Press Ctrl+C to exit."
    try {
        pio device monitor -p $Port -b 115200
    } catch {
        Write-Error ("Failed to open serial monitor on {0}: {1}" -f $Port, $_)
        exit 1
    }

} finally {
    Pop-Location
}
