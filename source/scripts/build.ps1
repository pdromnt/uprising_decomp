param(
    [ValidateSet(32,64)][int]$Bits=32,
    [string]$Compiler,
    [string]$OutputDirectory,
    [ValidateSet('uprising.exe','uprising64.exe')][string]$ExecutableName,
    [ValidateSet('Release','RelWithDebInfo')][string]$Configuration='RelWithDebInfo',
    [switch]$Tests
)
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
if(!$OutputDirectory){$OutputDirectory=Split-Path -Parent $project}
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
$cmake=Get-Command cmake.exe -ErrorAction SilentlyContinue
if($cmake){$cmake=$cmake.Source}else{
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if(Test-Path -LiteralPath $vswhere){$cmake=@(& $vswhere -latest -products '*' -find 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe') | Select-Object -First 1}
}
if(!$cmake){throw 'CMake 3.24+ is required; add it to PATH.'}
if(!$Compiler){
    $gcc=Get-Command gcc.exe -ErrorAction SilentlyContinue
    if($gcc){$Compiler=$gcc.Source}elseif(Test-Path -LiteralPath 'C:/TDM-GCC-64/bin/gcc.exe'){$Compiler='C:/TDM-GCC-64/bin/gcc.exe'}
}
if(!$Compiler){throw 'MinGW-w64 GCC is required; pass -Compiler or add gcc.exe to PATH.'}
$Compiler=(Resolve-Path -LiteralPath $Compiler).Path
$tools=Split-Path -Parent $Compiler
$prefix=[IO.Path]::GetFileNameWithoutExtension($Compiler) -replace 'gcc$',''
$windres=Join-Path $tools 'windres.exe'
if(!(Test-Path -LiteralPath $windres)){$windres=Join-Path $tools ($prefix+'windres.exe')}
$nm=Join-Path $tools 'nm.exe'
if(!(Test-Path -LiteralPath $nm)){$nm=Join-Path $tools ($prefix+'nm.exe')}
$ninja=Get-Command ninja.exe -ErrorAction SilentlyContinue
if($ninja){$ninja=$ninja.Source}else{$ninja=[IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $cmake) '../../Ninja/ninja.exe'))}
if(!(Test-Path -LiteralPath $ninja)){throw 'Ninja is required; add ninja.exe to PATH.'}
if(!$ExecutableName){$ExecutableName=if($Bits -eq 64){'uprising64.exe'}else{'uprising.exe'}}
$output=Join-Path $OutputDirectory $ExecutableName
if(Test-Path -LiteralPath $output){
    $text=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($output))
    if(!$text.Contains('NATIVE_OPENGL1_CPU_FREE:')){throw 'Output is an original/unknown executable. Preserve it outside the asset directory before building.'}
    $backupDirectory=Join-Path $project 'build/previous'
    New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null
    $previous=Join-Path $backupDirectory ('uprising-'+$Bits+'.exe')
    if((Get-FileHash -LiteralPath $output).Hash -ne $($(if(Test-Path -LiteralPath $previous){(Get-FileHash -LiteralPath $previous).Hash}else{''}))){Copy-Item -LiteralPath $output -Destination $previous -Force}
}
$build=Join-Path $project ('build/windows-'+$Bits)
$testOption=if($Tests){'ON'}else{'OFF'}
$arguments=@('-S',$project,'-B',$build,'-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$($ninja.Replace('\','/'))","-DCMAKE_C_COMPILER=$($Compiler.Replace('\','/'))",
    "-DCMAKE_RC_COMPILER=$($windres.Replace('\','/'))","-DCMAKE_C_FLAGS=-m$Bits","-DCMAKE_BUILD_TYPE=$Configuration",
    "-DUPRISING_ASSET_DIRECTORY=$($OutputDirectory.Replace('\','/'))","-DUPRISING_EXECUTABLE_NAME=$([IO.Path]::GetFileNameWithoutExtension($ExecutableName))","-DBUILD_TESTING=$testOption")
& $cmake @arguments
if($LASTEXITCODE){throw 'Configuration failed'}
& $cmake --build $build --parallel ([Math]::Min(4,[Environment]::ProcessorCount))
if($LASTEXITCODE){throw 'Build failed'}
$symbols=& $nm -g --defined-only $output
if($LASTEXITCODE -or $symbols -match '\s_?(cpu_|x87_)'){throw 'CPU/generated exclusion check failed'}
$text=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($output))
if(!$text.Contains('NATIVE_OPENGL1_CPU_FREE:') -or $text.Contains('glide2x.dll')){throw 'Native OpenGL exclusion check failed'}
if($Tests){
    & (Join-Path (Split-Path -Parent $cmake) 'ctest.exe') --test-dir $build --output-on-failure
    if($LASTEXITCODE){throw 'Native checks failed'}
}
Write-Host "Built $output; native execution/OpenGL gates passed."
