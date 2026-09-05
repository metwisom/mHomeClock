<#
.SYNOPSIS
  Build (and optionally upload/monitor) the firmware via PlatformIO.
  Checks that PlatformIO is installed and installs it (via pip) if missing.

.EXAMPLE
  .\scripts\build.ps1
  .\scripts\build.ps1 -Upload
  .\scripts\build.ps1 --upload
  .\scripts\build.ps1 -Upload -Monitor
  .\scripts\build.ps1 -Clean
#>

$Upload = $false
$Monitor = $false
$Clean = $false

foreach ($arg in $args) {
    switch -Regex ($arg.TrimStart('-').ToLower()) {
        '^upload$'  { $Upload = $true }
        '^monitor$' { $Monitor = $true }
        '^clean$'   { $Clean = $true }
        '^h$|^help$' {
            Get-Help $PSCommandPath -Full | Out-String | Write-Host
            exit 0
        }
        default {
            Write-Error "Unknown option: $arg"
            exit 1
        }
    }
}

$ErrorActionPreference = "Stop"
Set-Location -Path (Split-Path -Parent $PSScriptRoot)

function Find-Python {
    if (Get-Command python -ErrorAction SilentlyContinue) { return "python" }
    if (Get-Command python3 -ErrorAction SilentlyContinue) { return "python3" }
    if (Get-Command py -ErrorAction SilentlyContinue) { return "py" }
    return $null
}

# NOTE: PowerShell unwraps a single-element array written to the output
# stream back into a scalar when captured (`$x = Func`). The leading comma
# forces these to stay arrays so callers can safely do $pio[0], $pio[1..].
function Find-Pio {
    if (Get-Command pio -ErrorAction SilentlyContinue) { return ,@("pio") }
    if (Get-Command platformio -ErrorAction SilentlyContinue) { return ,@("platformio") }
    foreach ($py in @("python", "python3", "py")) {
        if (Get-Command $py -ErrorAction SilentlyContinue) {
            try {
                & $py -m platformio --version *> $null
                if ($LASTEXITCODE -eq 0) { return ,@($py, "-m", "platformio") }
            } catch {}
        }
    }
    return $null
}

# $pio may have just 1 element ("pio") or 3 ("python","-m","platformio").
# $arr[1..($arr.Length-1)] breaks when Length is 1: 1..0 is a DESCENDING
# range (@(1,0)), not empty, so it must be guarded explicitly.
function Get-PioBaseArgs {
    if ($pio.Length -gt 1) { return ,@($pio[1..($pio.Length-1)]) }
    return ,@()
}

function Invoke-Pio {
    param([string[]]$PioArgs)
    & $pio[0] @((Get-PioBaseArgs) + $PioArgs)
    if ($LASTEXITCODE -ne 0) {
        throw "PlatformIO command failed: $($PioArgs -join ' ')"
    }
}

Write-Host "==> Checking dependencies" -ForegroundColor Cyan

$pio = Find-Pio
if ($pio) {
    $ver = & $pio[0] @((Get-PioBaseArgs) + @("--version"))
    Write-Host "PlatformIO found: $($pio -join ' ') ($ver)"
} else {
    Write-Host "PlatformIO not found, installing via pip..."
    $pyBin = Find-Python
    if (-not $pyBin) {
        Write-Error "Python not found. Install Python 3 first: https://www.python.org/downloads/"
        exit 1
    }
    & $pyBin -m pip install -U platformio
    if ($LASTEXITCODE -ne 0) {
        Write-Error "pip install platformio failed."
        exit 1
    }

    $pio = Find-Pio
    if (-not $pio) {
        Write-Error "PlatformIO install failed."
        exit 1
    }
    $ver = & $pio[0] @((Get-PioBaseArgs) + @("--version"))
    Write-Host "PlatformIO installed: $($pio -join ' ') ($ver)"
}

Write-Host "==> Installing project dependencies (platform + libraries)" -ForegroundColor Cyan
Invoke-Pio @("pkg", "install")

if ($Clean) {
    Write-Host "==> Cleaning" -ForegroundColor Cyan
    Invoke-Pio @("run", "--target", "clean")
}

Write-Host "==> Building" -ForegroundColor Cyan
Invoke-Pio @("run")

if ($Upload) {
    Write-Host "==> Uploading" -ForegroundColor Cyan
    Invoke-Pio @("run", "--target", "upload")
}

if ($Monitor) {
    Write-Host "==> Monitor (Ctrl+C to exit)" -ForegroundColor Cyan
    Invoke-Pio @("device", "monitor")
}
