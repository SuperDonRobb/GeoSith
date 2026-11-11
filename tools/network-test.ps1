param([string]$Executable = '', [int]$Port = 27029, [int]$LagMs = 25, [int]$LossPercent = 2, [string]$Level = '', [string]$Map = 'sithed.jkl', [switch]$SkipNative, [int]$TimeoutSeconds = 100, [switch]$SequentialJoin, [switch]$CustomResources, [switch]$PngMaterials, [string]$Python="python")
if($PngMaterials){$CustomResources=$true}
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$testRoot = Join-Path $projectRoot ('build/network-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$previousPath = $env:PATH
$previousRoot = $env:OPENJKDF2_ROOT
$previousRole = $env:GEOSITH_NET_ROLE
$previousDir = $env:GEOSITH_NET_DIR
$previousStress = $env:GEOSITH_STRESS
$previousSelf = $env:GEOSITH_SELFTEST
$previousLevel = $env:GEOSITH_NET_LEVEL
$previousPort=$env:GEOSITH_NET_PORT
$previousLag=$env:GEOSITH_NET_LAG_MS
$previousLoss=$env:GEOSITH_NET_LOSS_PERCENT
$previousMods=$env:GEOSITH_MODS_DIR
$previousResourcesTest=$env:GEOSITH_CUSTOM_RESOURCES_TEST
$processes = @()
$bootstrap = $null
Push-Location $projectRoot
try {
    if(-not $SkipNative){
        & ./build-tests/Release/editor_tests.exe JK/editor-resources.jkl JK/Episode/sithed_smoke.gob
        if ($LASTEXITCODE) { throw 'Native tests failed.' }
    }
    $env:PATH = (Join-Path $projectRoot 'build') + ';' + $previousPath
    $env:OPENJKDF2_ROOT = Join-Path $projectRoot 'JK'
    $env:GEOSITH_NET_DIR = $testRoot
    $env:GEOSITH_STRESS = '1'
    $env:GEOSITH_SELFTEST = $null
    $env:GEOSITH_NET_LEVEL = if($Level){$Level}else{$null}
    $env:GEOSITH_NET_PORT="$Port"
    $env:GEOSITH_NET_LAG_MS="$LagMs"
    $env:GEOSITH_NET_LOSS_PERCENT="$LossPercent"
    $exe = if($Executable){$Executable}else{Join-Path $projectRoot 'build/Release/openjkdf2-64.exe'}
    if($CustomResources){
        $fixtureArgs=@($env:OPENJKDF2_ROOT,(Join-Path $testRoot 'host-mods'))
        if($PngMaterials){$fixtureArgs+='--png'}
        & $Python "$PSScriptRoot/resource-fixture.py" @fixtureArgs
        if($LASTEXITCODE){throw 'Could not create resource fixtures'}
        $env:GEOSITH_CUSTOM_RESOURCES_TEST='1'
    }
    foreach ($role in 1..3) {
        if($CustomResources){$env:GEOSITH_MODS_DIR=Join-Path $testRoot $(if($role -eq 1){'host-mods'}else{'empty-mods'})}

        $env:GEOSITH_NET_ROLE = "$role"
        $episode = if ($role -eq 1) { if($Level){$Level}else{'sithed_smoke'} } else { $bootstrap }
        $process = Start-Process $exe -ArgumentList '-headless','-autostart','-sp','-episode',$episode,'-map',$Map -WorkingDirectory $env:OPENJKDF2_ROOT -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $testRoot "$role.stdout.log") -RedirectStandardError (Join-Path $testRoot "$role.stderr.log")
        $null = $process.Handle # Keep the Windows process handle so ExitCode remains available after exit.
        $processes += $process
        if ($role -eq 1) {
            $deadline = [datetime]::UtcNow.AddSeconds(30)
            while (-not (Test-Path -LiteralPath (Join-Path $testRoot 'host.ready'))) {
                if ($process.HasExited -or [datetime]::UtcNow -gt $deadline) { throw "Host startup failed. See $testRoot" }
                Start-Sleep -Milliseconds 100
            }
            $bootstrap = Get-Content -LiteralPath (Join-Path $testRoot 'bootstrap.name') -Raw
            if ($bootstrap -notmatch '^netedit[0-9]{4}$') { throw 'Unexpected bootstrap episode name.' }
        } elseif ($SequentialJoin -and $role -eq 2) {
            $deadline = [datetime]::UtcNow.AddSeconds($TimeoutSeconds)
            while (-not (Select-String -LiteralPath (Join-Path $testRoot '2.stdout.log') -Pattern 'GEOSITH_NET_JOIN PASS' -SimpleMatch -Quiet)) {
                if ($process.HasExited -or [datetime]::UtcNow -gt $deadline) { throw "First client join failed. See $testRoot" }
                Start-Sleep -Milliseconds 200
            }
        } else { Start-Sleep -Milliseconds 600 }
    }
    foreach ($process in $processes) { if (-not $process.WaitForExit($TimeoutSeconds * 1000)) { throw "Network test timed out. See $testRoot" } }
    if($env:GEOSITH_NET_EDITOR_LOOK_TEST){
        foreach($role in 1..3){if($processes[$role-1].ExitCode -ne 0 -or !(Select-String -LiteralPath (Join-Path $testRoot "$role.stdout.log") -Pattern 'GEOSITH_NET_EDITOR_LOOK PASS' -Quiet)){throw "Editor join look test failed: $testRoot"}}
        Write-Host "PASS: both joining clients can look up/down while host stays editing. Logs: $testRoot"
        return
    }
    if($env:GEOSITH_NET_SURFACE_SESSION_TEST){
        foreach($role in 1..3){if($processes[$role-1].ExitCode -ne 0 -or !(Select-String -LiteralPath (Join-Path $testRoot "$role.stdout.log") -Pattern 'GEOSITH_NET_SURFACE_SESSION PASS' -Quiet)){throw "Surface session test failed: $testRoot"}}
        foreach($prefix in @('surface.shared','surface.undo','surface.hostshared')){if(@(1..3 | ForEach-Object {Get-Content -LiteralPath (Join-Path $testRoot "$prefix$_") -Raw} | Select-Object -Unique).Count -ne 1){throw "Surface session contents differ: $testRoot"}}
        Write-Host "PASS: surface leases, deferred paint, concurrent edits, single shared undo. Logs: $testRoot"
        return
    }
    if($env:GEOSITH_NET_TERRAIN_SESSION_TEST){
        foreach($role in 1..3){if($processes[$role-1].ExitCode -ne 0 -or !(Select-String -LiteralPath (Join-Path $testRoot "$role.stdout.log") -Pattern 'GEOSITH_NET_TERRAIN_SESSION PASS' -Quiet)){throw "Terrain session test failed: $testRoot"}}
        foreach($prefix in @('terrain.shared','terrain.undo','terrain.hostshared')){if(@(1..3 | ForEach-Object {Get-Content -LiteralPath (Join-Path $testRoot "$prefix$_") -Raw} | Select-Object -Unique).Count -ne 1){throw "Terrain session contents differ: $testRoot"}}
        Write-Host "PASS: terrain leases, deferred sculpt/paint, concurrent edits, single shared undo. Logs: $testRoot"
        return
    }
    if($Level -and -not $env:GEOSITH_NET_CLIENT_DECAL_TEST){
        foreach($role in 1..3){if($processes[$role-1].ExitCode -ne 0 -or -not (Select-String -LiteralPath (Join-Path $testRoot "$role.stdout.log") -Pattern 'GEOSITH_NET_LOADED PASS' -SimpleMatch -Quiet)){throw "Saved-level join failed: $testRoot"}}
        Write-Host "PASS: $Level host and two clients, host visibility after welcome. Logs: $testRoot"
        return
    }
    if($env:GEOSITH_NET_CLIENT_DECAL_TEST){
        if(-not (Select-String -LiteralPath (Join-Path $testRoot '2.stdout.log') -Pattern 'GEOSITH_NET_CLIENT_DECAL_DELTA PASS' -SimpleMatch -Quiet)){throw "First client decal did not use a shared snapshot delta: $testRoot"}
        foreach($role in 1..3){if($processes[$role-1].ExitCode -ne 0 -or !(Select-String -LiteralPath (Join-Path $testRoot "$role.stdout.log") -Pattern 'GEOSITH_NET_CLIENT_DECAL PASS' -Quiet)){throw "Client decal test failed: $testRoot"}}
        foreach($prefix in @('decal.seen','decal.redo','decal.host')){
            $hashes=@(1..3 | ForEach-Object {Get-Content -LiteralPath (Join-Path $testRoot "$prefix$_") -Raw} | Select-Object -Unique)
            if($hashes.Count -ne 1){throw "Decal contents differ between peers: $testRoot"}
        }
        Write-Host "PASS: client and host decals, matching pixels/documents, shared undo/redo. Logs: $testRoot"
        return
    }
    foreach ($role in 1..3) {
        if ($processes[$role-1].ExitCode -ne 0 -or -not (Select-String -LiteralPath (Join-Path $testRoot "$role.stdout.log") -Pattern 'GEOSITH_NET_.* PASS' -Quiet)) { throw "Role $role failed (exit $($processes[$role-1].ExitCode)). See $testRoot" }
        if ($role -ne 1 -and -not (Select-String -LiteralPath (Join-Path $testRoot "$role.stdout.log") -Pattern 'GEOSITH_NET_JOIN PASS' -Quiet)) { throw "Client $role did not complete the actual join/welcome handshake. See $testRoot" }
        if ($role -ne 1 -and -not (Select-String -LiteralPath (Join-Path $testRoot "$role.stdout.log") -Pattern 'GEOSITH_NET_PREDICTION PASS' -Quiet)) { throw "Client $role did not verify immediate prediction or rejection rollback. See $testRoot" }
        if ($role -eq 3 -and -not (Select-String -LiteralPath (Join-Path $testRoot "$role.stdout.log") -Pattern 'GEOSITH_NET_MAP PASS' -Quiet)) { throw "Map cleave/deletion replication was not verified. See $testRoot" }
    }
    if(-not (Select-String -LiteralPath (Join-Path $testRoot '2.stdout.log') -Pattern 'GEOSITH_NET_SYNC_MENU PASS' -SimpleMatch -Quiet)){throw 'The actual synchronization menu did not complete joining.'}
    if(-not (Select-String -LiteralPath (Join-Path $testRoot '1.stdout.log') -Pattern 'GEOSITH_NET_MODAL PASS' -SimpleMatch -Quiet)){throw 'Host dialog processing or delta transfers were not verified.'}
    foreach($role in 2..3){if(-not (Select-String -LiteralPath (Join-Path $testRoot "$role.stdout.log") -Pattern 'GEOSITH_GNS_DISCOVERY PASS' -SimpleMatch -Quiet)){throw 'Discovery/authentication checks were not verified.'}}
    if(-not (Select-String -LiteralPath (Join-Path $testRoot '3.stdout.log') -Pattern 'GEOSITH_NET_PERMISSION PASS' -SimpleMatch -Quiet)){throw 'Host edit permissions were not verified.'}
    foreach($role in 2..3){if(-not (Select-String -LiteralPath (Join-Path $testRoot "$role.stdout.log") -Pattern 'GEOSITH_NET_POSITION PASS' -SimpleMatch -Quiet)){throw 'Remote player movement was not verified.'}}
    foreach($role in 2..3){if(-not (Select-String -LiteralPath (Join-Path $testRoot "$role.stdout.log") -Pattern 'GEOSITH_NET_ENEMY PASS' -SimpleMatch -Quiet)){throw 'Host enemy death with editing and playing clients was not verified.'}}
    foreach($role in 2..3){if(-not (Select-String -LiteralPath (Join-Path $testRoot "$role.stdout.log") -Pattern 'GEOSITH_NET_ENEMY_RESET PASS' -SimpleMatch -Quiet)){throw 'Enemy kill/reset/resume with editing clients was not verified.'}}
    if(-not (Select-String -LiteralPath (Join-Path $testRoot '2.stdout.log'),(Join-Path $testRoot '3.stdout.log') -Pattern 'GEOSITH_NET_ACK_LOSS PASS' -SimpleMatch -Quiet)){throw 'Lost-final-ACK reconciliation was not verified.'}
    if($CustomResources){foreach($role in 2..3){if(!(Select-String -LiteralPath (Join-Path $testRoot "$role.stdout.log") -Pattern 'GEOSITH_NET_RESOURCES PASS' -Quiet)){throw 'Custom resource transfer not verified'}}}
    Write-Host "PASS: real host + two clients, join/welcome, immediate previews, drag reconciliation, rejection rollback, undo/redo, packet loss/duplicates. Logs: $testRoot"
} finally {
    foreach ($process in $processes) { if (-not $process.HasExited) { Stop-Process -Id $process.Id } }
    if ($bootstrap -match '^netedit[0-9]{4}$') { Remove-Item -LiteralPath (Join-Path $projectRoot "JK/Episode/$bootstrap.gob") -ErrorAction SilentlyContinue }
    $env:GEOSITH_MODS_DIR=$previousMods
    $env:GEOSITH_CUSTOM_RESOURCES_TEST=$previousResourcesTest
    $env:PATH = $previousPath
    $env:OPENJKDF2_ROOT = $previousRoot
    $env:GEOSITH_NET_ROLE = $previousRole
    $env:GEOSITH_NET_DIR = $previousDir
    $env:GEOSITH_STRESS = $previousStress
    $env:GEOSITH_SELFTEST = $previousSelf
    $env:GEOSITH_NET_LEVEL = $previousLevel
    $env:GEOSITH_NET_PORT=$previousPort
    $env:GEOSITH_NET_LAG_MS=$previousLag
    $env:GEOSITH_NET_LOSS_PERCENT=$previousLoss
    Pop-Location
}
