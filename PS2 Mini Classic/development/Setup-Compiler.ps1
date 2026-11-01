[CmdletBinding()]
param([switch]$VerifyOnly)
$ErrorActionPreference = 'Stop'
$layout = Join-Path $PSScriptRoot 'msvc'
$temporaryLink = $null
# Microsoft's layout tools require a short path. A temporary junction also works after extraction into Downloads.
if ($layout.Length -ge 80) {
    $temporaryLink = Join-Path ([IO.Path]::GetTempPath()) ('PS2MiniSDK-' + [guid]::NewGuid().ToString('N').Substring(0,8))
    if ($temporaryLink.Length -ge 80) { throw 'Bitte das Paket in einen kuerzeren Ordner entpacken, zum Beispiel C:\PS2Mini.' }
    New-Item -ItemType Junction -Path $temporaryLink -Target $layout | Out-Null
    $layout = $temporaryLink
}
$installer = Join-Path $layout 'vs_buildtools.exe'
if (-not (Test-Path -LiteralPath $installer)) { throw 'Das Offline-Installationspaket fehlt.' }
Write-Host 'Das Microsoft-Setup richtet Compiler, ATL und Windows-SDK ein. Bitte die Dialoge und Lizenzbedingungen im Setup pruefen.'
$arguments = @('--noWeb','--wait','--norestart','--add','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','--add','Microsoft.VisualStudio.Component.VC.ATL','--add','Microsoft.VisualStudio.Component.Windows11SDK.26100')
# This is deliberately a visible, user-initiated setup; it does not silently accept a licence.
try {
    if ($VerifyOnly) {
        $arguments = @('--layout',('"'+$layout+'"'),'--verify','--quiet','--wait')
        $process = Start-Process -FilePath $installer -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    } else {
        $process = Start-Process -FilePath $installer -ArgumentList $arguments -Wait -PassThru
    }
} finally {
    if ($temporaryLink -and (Test-Path -LiteralPath $temporaryLink)) {
        $expectedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
        if ([IO.Path]::GetFullPath($temporaryLink).StartsWith($expectedTemp,[StringComparison]::OrdinalIgnoreCase) -and ((Get-Item -LiteralPath $temporaryLink).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            [IO.Directory]::Delete($temporaryLink)
        }
    }
}
if ($process.ExitCode -notin @(0,3010)) { throw "Microsoft-Setup meldet Fehler $($process.ExitCode)." }
if ($process.ExitCode -eq 3010) { Write-Host 'Windows bitte neu starten, bevor ein Emulator-Update gebaut wird.' }
else { Write-Host 'Setup beendet. Mit Werkzeuge pruefen.cmd kann die Einrichtung geprueft werden.' }
