# Build + Upload aus zentraler Version (src/version.h)
# Nutzung:
#   .\build.ps1
#   .\build.ps1 -NoUpload
#   .\build.ps1 -SyncOnly
#   .\build.ps1 -UploadPort COM34
param(
  [switch]$NoUpload,
  [switch]$SyncOnly,
  [string]$UploadPort = $env:UPLOAD_PORT,
  [string]$EnvName = $(if ($env:PIO_ENV) { $env:PIO_ENV } else { "esp32-c3" })
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

function Get-FwVersion {
  $verFile = Join-Path $Root "src\version.h"
  $line = Select-String -Path $verFile -Pattern '^\s*#define\s+FW_VERSION_STR\s+"([^"]+)"' | Select-Object -First 1
  if (-not $line) { throw "FW_VERSION_STR in src/version.h nicht gefunden" }
  return $line.Matches[0].Groups[1].Value
}

function Sync-VersionFiles([string]$Version) {
  $readme = Join-Path $Root "README.md"
  if (Test-Path $readme) {
    $text = Get-Content -Raw -Path $readme
    $updated = [regex]::Replace($text, '(<!--FW_VERSION-->)(.*?)(<!--/FW_VERSION-->)', "`${1}$Version`${3}")
    if ($updated -ne $text) {
      Set-Content -Path $readme -Value $updated -NoNewline -Encoding utf8
    }
  }
  Set-Content -Path (Join-Path $Root "VERSION") -Value $Version -Encoding ascii
  Write-Host "Version synced: $Version"
}

$version = Get-FwVersion
Write-Host "==> Firmware-Version: $version (aus src/version.h)"
Sync-VersionFiles $version

if ($SyncOnly) { exit 0 }

if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
  throw "PlatformIO (pio) nicht im PATH."
}

Write-Host "==> Compile ($EnvName)"
pio run -e $EnvName
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $NoUpload) {
  Write-Host "==> Upload ($EnvName)"
  if ($UploadPort) {
    pio run -e $EnvName -t upload --upload-port $UploadPort
  } else {
    pio run -e $EnvName -t upload
  }
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  Write-Host "==> Fertig: $version auf MCU"
} else {
  Write-Host "==> Fertig: $version gebaut (kein Upload)"
}
