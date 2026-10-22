[CmdletBinding()]
param(
    [string]$Executable,
    [string]$Debugger,
    [switch]$Muted,
    [switch]$Check
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(!$Executable){
    $Executable=Join-Path $root 'uprising64.exe'
    if(!(Test-Path -LiteralPath $Executable)){$Executable=Join-Path $root 'uprising.exe'}
}
$Executable=(Resolve-Path -LiteralPath $Executable).Path
$identity=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($Executable))
if(!$identity.Contains('NATIVE_OPENGL1_CPU_FREE:')){throw 'Executable is not the native reconstruction; retail will not be launched.'}
if(!$Debugger){
    $command=Get-Command gdb.exe -ErrorAction SilentlyContinue
    if($command){$Debugger=$command.Source}
    elseif(Test-Path -LiteralPath 'C:/TDM-GCC-64/bin/gdb.exe'){$Debugger='C:/TDM-GCC-64/bin/gdb.exe'}
}
if(!$Debugger){throw 'GDB with Python support is required. Add gdb.exe to PATH or pass -Debugger.'}
$Debugger=(Resolve-Path -LiteralPath $Debugger).Path
$script=Join-Path $PSScriptRoot 'debug-game.gdb'
$run=Join-Path $root 'runtime-logs/debug-game'
$user=Join-Path $run 'user'
$log=Join-Path $run 'debugger.log'

# Capture both GDB and the game's output in one overwritten, bounded file.
# The helper has no shell or background service and only owns its GDB process.
if(-not ('UprisingDebugCapture' -as [type])){
Add-Type -TypeDefinition @'
using System;
using System.Diagnostics;
using System.IO;
using System.Text;
public sealed class UprisingDebugCapture : IDisposable {
    readonly object gate=new object();
    readonly StreamWriter writer;
    readonly long limit;
    long bytes;
    public Process Process { get; private set; }
    public bool Truncated { get; private set; }
    public UprisingDebugCapture(string executable,string arguments,string cwd,string log,long maxBytes) {
        limit=maxBytes;
        writer=new StreamWriter(new FileStream(log,FileMode.Create,FileAccess.Write,FileShare.Read),new UTF8Encoding(false));
        writer.AutoFlush=true;
        Process=new Process();
        Process.StartInfo=new ProcessStartInfo(executable,arguments) {
            WorkingDirectory=cwd,UseShellExecute=false,CreateNoWindow=true,
            RedirectStandardOutput=true,RedirectStandardError=true
        };
        Process.OutputDataReceived+=(sender,e)=>Write(e.Data);
        Process.ErrorDataReceived+=(sender,e)=>Write(e.Data);
        try {
            Process.Start();Process.BeginOutputReadLine();Process.BeginErrorReadLine();
        } catch { Process.Dispose();writer.Dispose();throw; }
    }
    void Write(string line) {
        if(line==null)return;
        lock(gate) {
            if(Truncated)return;
            long count=Encoding.UTF8.GetByteCount(line)+2;
            if(bytes+count>limit-80) {
                writer.WriteLine("DIAGNOSTIC LOG LIMIT REACHED; further output omitted.");
                Truncated=true;return;
            }
            writer.WriteLine(line);bytes+=count;
        }
    }
    public void Dispose() {
        try {
            if(!Process.HasExited) {
                // Terminate only this launcher's owned debugger and inferior.
                using(var stop=System.Diagnostics.Process.Start(new ProcessStartInfo("taskkill.exe","/PID "+Process.Id+" /T /F") {
                    UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true
                })) { stop.WaitForExit(); }
            }
            Process.WaitForExit(); // Drain asynchronous output before closing.
        } finally { Process.Dispose();lock(gate)writer.Dispose(); }
    }
}
'@
}
if($Check){
    & $Debugger -q -batch -ex 'python print("GDB_PYTHON_READY")' -ex 'break gl1_fail' -ex 'break application_failure' -ex 'break game_memory_fail' $Executable
    if($LASTEXITCODE){throw 'Debugger preflight failed.'}
    Write-Host "Ready: $Executable"
    Write-Host "Logs:  $log (overwritten each run, maximum 8 MiB)"
    return
}
if(Get-Process uprising,uprising64 -ErrorAction SilentlyContinue){throw 'Close the existing game before starting another session.'}
foreach($directory in @($run,$user,(Join-Path $user 'CONFIG'))){
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    if((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Diagnostic directory must not be a filesystem link: $directory"}
}
foreach($file in @($log,(Join-Path $user 'CONFIG/settings.ini'),(Join-Path $user 'uprising-portable.log'))){
    if((Test-Path -LiteralPath $file) -and ((Get-Item -LiteralPath $file).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Diagnostic output must not be a filesystem link: $file"}
}
# Borrow bindings/preferences, but never overwrite the player's normal settings.
# Keep diagnostic saves between runs; assets are shared directly from the root.
$settings=Join-Path $root 'CONFIG/settings.ini'
$text=if(Test-Path -LiteralPath $settings){[IO.File]::ReadAllText($settings)}else{''}
$text=[regex]::Replace($text,'(?ms)^\[display\]\r?\n.*?(?=^\[|\z)',{
    param($match)
    $section=$match.Value
    foreach($entry in @('fullscreen','width','height')){$section=[regex]::Replace($section,"(?m)^$entry=[^\r\n]*\r?\n?",'')}
    return $section.TrimEnd()+"`r`nfullscreen=0`r`nwidth=960`r`nheight=720`r`n`r`n"
})
if($text -notmatch '(?m)^\[display\]\r?$'){$text+="`r`n[display]`r`nfullscreen=0`r`nwidth=960`r`nheight=720`r`n"}
[IO.File]::WriteAllText((Join-Path $user 'CONFIG/settings.ini'),$text)
$names=@('UPRISING_ASSET_DIRECTORY','UPRISING_USER_DIRECTORY','UPRISING_RECOMP_WINDOWED',
    'UPRISING_RECOMP_HEADLESS','UPRISING_RECOMP_ISOLATED_INPUT','UPRISING_RECOMP_MUTE',
    'UPRISING_RECOMP_SILENT_NATIVE_SOUND','UPRISING_RECOMP_NATIVE_OBSERVE','UPRISING_RECOMP_LOG_PATH',
    'UPRISING_RENDERER','UPRISING_VULKAN_FSR3','UPRISING_VULKAN_VALIDATION')
$before=@{};foreach($name in $names){$before[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$mutex=[Threading.Mutex]::new($false,'Local\UprisingDebugGame')
try {$locked=$mutex.WaitOne(0)}catch [Threading.AbandonedMutexException] {$locked=$true}
if(!$locked){$mutex.Dispose();throw 'Another debugger launcher is already running.'}
$capture=$null
try {
    if(Get-Process uprising,uprising64 -ErrorAction SilentlyContinue){throw 'Close the existing game before starting another session.'}
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:UPRISING_ASSET_DIRECTORY=$root
    $env:UPRISING_USER_DIRECTORY=$user
    $env:UPRISING_RECOMP_WINDOWED='1'
    $env:UPRISING_RECOMP_ISOLATED_INPUT='0'
    if($Muted){$env:UPRISING_RECOMP_MUTE='1';$env:UPRISING_RECOMP_SILENT_NATIVE_SOUND='1'}
    # No .lfl argument: it enables direct-level mode, which skips debriefing.
    $arguments='--batch -x "'+$script.Replace('\','/')+'" --args "'+$Executable.Replace('\','/')+'" -dontshowintro'
    Write-Host 'Starting normal menus with keyboard/mouse enabled (960 x 720).'
    Write-Host "Diagnostic output: $log"
    Write-Host 'Close the game normally when finished. Ctrl+C stops this owned session.'
    $capture=[UprisingDebugCapture]::new($Debugger,$arguments,$root,$log,8MB)
    while(!$capture.Process.WaitForExit(250)){}
    $capture.Process.WaitForExit()
    $code=$capture.Process.ExitCode
    Write-Host "Session finished: exit code $code."
    if($capture.Truncated){Write-Warning 'Diagnostic log reached its 8 MiB limit.'}
    Write-Host "Keep these files for investigation: $log and $user/uprising-portable.log"
} finally {
    if($capture){$capture.Dispose()}
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$before[$name],'Process')}
    $mutex.ReleaseMutex();$mutex.Dispose()
}
exit $code
