param(
    [string]$BackendUrl = $env:BACKEND_URL,
    [string]$SrcIp = "192.168.56.10",
    [int]$SrcPort = 4444,
    [int]$DstPort = 23,
    [string]$Payload = "test probe"
)

if (-not $BackendUrl) {
    $BackendUrl = "http://127.0.0.1:5000/log"
}

$body = @{
    src_ip = $SrcIp
    src_port = $SrcPort
    dst_port = $DstPort
    timestamp = (Get-Date).ToString("o")
    payload = $Payload
} | ConvertTo-Json -Depth 4

Invoke-RestMethod -Uri $BackendUrl -Method Post -ContentType "application/json" -Body $body | Out-Null
Write-Host "Sent attack event to $BackendUrl"
