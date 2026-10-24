[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Version,
    [Parameter(Mandatory=$true)][ValidateSet('windows-x64','windows-arm64','linux-x64','linux-arm64','macos-arm64')][string]$Platform,
    [Parameter(Mandatory=$true)][string]$StageDirectory,
    [string]$OutputDirectory
)
$ErrorActionPreference='Stop'
if($Version -notmatch '^[A-Za-z0-9._-]+$'){throw 'Version must be a safe filename component.'}
$stage=(Resolve-Path -LiteralPath $StageDirectory).Path
if(!$OutputDirectory){$OutputDirectory=Join-Path (Split-Path -Parent $PSScriptRoot) 'dist'}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output=[IO.Path]::GetFullPath($OutputDirectory)
if($output.StartsWith($stage+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $output -eq $stage){throw 'Archive output must be outside the runtime stage.'}
$assets=$stage
$exe=Join-Path $stage 'uprising'
if($Platform.StartsWith('windows-')){$exe+='.exe'}
if($Platform -eq 'macos-arm64'){
    $assets=Join-Path $stage 'Uprising.app/Contents/Resources'
    $exe=Join-Path $stage 'Uprising.app/Contents/MacOS/Uprising'
}
foreach($name in @('FONTS','GRIDS','Music','VIDEO','uprising.cln','README.TXT','README.md','THIRD_PARTY_NOTICES.md','libsmacker-COPYING.txt','SDL3-LICENSE.txt','FreeType-FTL.txt','stb-LICENSE.txt','enet-LICENSE.txt','FSR3-LICENSE.txt')){
    if(!(Test-Path -LiteralPath (Join-Path $assets $name))){throw "Missing runtime content: $name"}
}
foreach($name in @('source','recomp-project','CONFIG','saves','scrshts')){
    if(Test-Path -LiteralPath (Join-Path $assets $name)){throw "User/private/source content in runtime stage: $name"}
}
if(Get-ChildItem -LiteralPath (Join-Path $assets 'FONTS') -Filter '*.cfg'){throw 'Legacy user configuration must not ship.'}
$bytes=[IO.File]::ReadAllBytes($exe)
$text=[Text.Encoding]::ASCII.GetString($bytes)
if(!$text.Contains('NATIVE_VULKAN_CPU_FREE:') -or $text.Contains('glide2x.dll')){throw 'Native renderer/execution identity gate failed.'}
if($Platform.StartsWith('windows-')){
    $machine=[BitConverter]::ToUInt16($bytes,[BitConverter]::ToInt32($bytes,60)+4)
    $expected=@{'windows-x64'=0x8664;'windows-arm64'=0xaa64}[$Platform]
    if($machine -ne $expected){throw 'PE architecture does not match the package.'}
}else{
    $description=(& file -b $exe) -join ' '
    if($LASTEXITCODE){throw 'Cannot inspect native executable.'}
    $expected=@{'linux-x64'='ELF 64-bit.*x86-64';'linux-arm64'='ELF 64-bit.*(ARM aarch64|ARM64)';'macos-arm64'='Mach-O 64-bit.*arm64'}[$Platform]
    if($description -notmatch $expected){throw "Wrong executable architecture: $description"}
    if($Platform -eq 'macos-arm64'){
        # Ad-hoc signing enables native Apple Silicon execution; not notarization.
        & codesign --force --deep --sign - (Join-Path $stage 'Uprising.app')
        if($LASTEXITCODE){throw 'App signing failed.'}
        & codesign --verify --deep --strict (Join-Path $stage 'Uprising.app')
        if($LASTEXITCODE){throw 'App signature verification failed.'}
        if(!(Test-Path -LiteralPath (Join-Path $stage 'README.TXT'))){throw 'DMG root needs the original README.TXT.'}
    }
}
$base=Join-Path $output "uprising-$Version-$Platform"
if($Platform.StartsWith('windows-')){
    $archive="$base.zip"
    # tar avoids Compress-Archive silently omitting hidden runtime entries.
    & tar -a -cf $archive -C $stage .
}elseif($Platform -eq 'macos-arm64'){
    $archive="$base.dmg"
    & hdiutil create -ov -volname 'Uprising' -srcfolder $stage -format UDZO $archive
}else{
    $archive="$base.tar.gz"
    & tar -czf $archive -C $stage .
}
if($LASTEXITCODE){throw 'Runtime packaging failed.'}
Write-Host "Created $archive"
