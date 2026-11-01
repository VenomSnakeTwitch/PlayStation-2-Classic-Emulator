param([switch]$SelfTest)
$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
$source=Join-Path $taskRoot 'PS2 Mini Classic\development\pcsx2-source'
$toolRoot=Join-Path $taskRoot 'PS2 Mini Classic\development\tools'
$cmake=Join-Path $toolRoot 'cmake\bin\cmake.exe'
$ninja=Join-Path $toolRoot 'ninja\ninja.exe'
$git=Join-Path $toolRoot 'git\cmd\git.exe'
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) {throw 'Bitte Compiler einrichten unter Systemkonfiguration / Updates ausfuehren.'}
$vcvars=Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
$build=Join-Path $taskRoot 'build\native'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$envScript=Join-Path $build 'environment.cmd'
@"
@echo off
call "$vcvars" >nul
set
"@ | Set-Content -LiteralPath $envScript -Encoding ascii
foreach($line in (& $envScript)){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
& $cmake -S $source -B $build -G Ninja "-DCMAKE_PREFIX_PATH=$source\deps" "-DGIT_EXECUTABLE=$git" "-DCMAKE_MAKE_PROGRAM=$ninja" -DENABLE_QT_UI=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_NINJA_CMCLDEPS_RC=OFF "-DCLASSIC_MINI_DATA_SELFTEST=$($SelfTest.IsPresent.ToString().ToUpperInvariant())"
if($LASTEXITCODE -ne 0){throw 'Konfiguration fehlgeschlagen'}
& $cmake --build $build --target pcsx2-qt --parallel 6
if($LASTEXITCODE -ne 0){throw 'Build fehlgeschlagen'}
