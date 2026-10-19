param(
    [Parameter(Mandatory=$true)][string]$Version,
    [ValidateSet(32,64)][int]$Bits=32,
    [string]$Executable,
    [string]$OutputDirectory
)
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$repository=Split-Path -Parent $project
if($Version -notmatch '^[A-Za-z0-9._-]+$'){throw 'Version must be a safe filename component.'}
if(!$Executable){$Executable=Join-Path (Split-Path -Parent $project) $(if($Bits -eq 64){'uprising64.exe'}else{'uprising.exe'})}
if(!$OutputDirectory){$OutputDirectory=Join-Path $project 'dist'}
$bytes=[IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Executable))
if(![Text.Encoding]::ASCII.GetString($bytes).Contains('NATIVE_OPENGL1_CPU_FREE:')){throw 'Only the native reconstructed executable may be packaged.'}
$machine=[BitConverter]::ToUInt16($bytes,[BitConverter]::ToInt32($bytes,60)+4)
if($machine -ne $(if($Bits -eq 32){0x14c}else{0x8664})){throw 'Executable architecture does not match -Bits.'}
$staging=Join-Path $project "build/package-$Bits"
New-Item -ItemType Directory -Path $staging,$OutputDirectory -Force | Out-Null
Copy-Item -LiteralPath $Executable -Destination (Join-Path $staging 'uprising.exe') -Force
$documents=@('README.md','THIRD_PARTY_NOTICES.md')
foreach($name in $documents){Copy-Item -LiteralPath (Join-Path $repository $name) -Destination $staging -Force}
$license=Join-Path $staging 'libsmacker-COPYING.txt'
Copy-Item -LiteralPath (Join-Path $project 'third_party/libsmacker/COPYING') -Destination $license -Force
$sdlLicense=Join-Path $staging 'SDL3-LICENSE.txt'
Copy-Item -LiteralPath (Join-Path $project 'third_party/SDL3-LICENSE.txt') -Destination $sdlLicense -Force
$fontLicense=Join-Path $staging 'FreeType-FTL.txt'
Copy-Item -LiteralPath (Join-Path $project 'third_party/FreeType-FTL.txt') -Destination $fontLicense -Force
$vorbisLicense=Join-Path $staging 'stb-LICENSE.txt'
Copy-Item -LiteralPath (Join-Path $project 'third_party/stb-LICENSE.txt') -Destination $vorbisLicense -Force
$archive=Join-Path $OutputDirectory "uprising-$Version-windows-$Bits.zip"
$files=@(Join-Path $staging 'uprising.exe')+@($documents | ForEach-Object {Join-Path $staging $_})+@($license,$sdlLicense,$fontLicense,$vorbisLicense)
$files+=@('FONTS','GRIDS','Music','VIDEO','uprising.cln','README.TXT','hints.txt') | ForEach-Object {Join-Path $repository $_}
Compress-Archive -LiteralPath $files -DestinationPath $archive -Force
Write-Host "Created $archive (executable, notices and game data; no source/user settings/saves)."
