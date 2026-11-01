# Check if a compatible version of ViGEmBus is already installed (1.17 or later)
try {
    $vigemBusPath = "$env:SystemRoot\System32\drivers\ViGEmBus.sys"
    $fileVersion = (Get-Item $vigemBusPath).VersionInfo.FileVersion

    # The driver file can survive without the bus device, so require a working device too
    $busDevice = Get-CimInstance Win32_PnPEntity -Filter "Service='ViGEmBus'" | Where-Object { $_.ConfigManagerErrorCode -eq 0 }
    if (([version]$fileVersion -ge [version]"1.17") -and $busDevice) {
        Write-Information "The installed version is 1.17 or later, no update needed. Exiting."
        exit 0
    }
}
catch {
    Write-Information "ViGEmBus driver not found or inaccessible, proceeding with installation."
}

# Install Virtual Gamepad
$scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
$installerPath = Join-Path $scriptPath "vigembus_installer.exe"
Start-Process `
    -FilePath $installerPath `
    -ArgumentList "/passive", "/promptrestart" `
    -Wait
