param(
    [Parameter(Mandatory=$true)][ValidateSet('voice','motor')][string]$Target,
    [Parameter(Mandatory=$true)][string]$Port,
    [string]$EsptoolPath
)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
if(!$EsptoolPath){
    $candidate=Get-ChildItem -LiteralPath (Join-Path $env:LOCALAPPDATA 'Arduino15/packages/esp32/tools/esptool_py') -Filter esptool.exe -Recurse | Sort-Object FullName -Descending | Select-Object -First 1
    if(!$candidate){throw 'esptool.exe was not found. Use -EsptoolPath.'}
    $EsptoolPath=$candidate.FullName
}
$manifest=Get-Content -LiteralPath (Join-Path $root 'SHA256.json') -Raw | ConvertFrom-Json
$paths=if($Target -eq 'voice'){@('firmware/voice/bootloader.bin','firmware/voice/partition-table.bin','firmware/voice/ota_data_initial.bin','firmware/voice/xiaozhi.bin','firmware/voice/generated_assets.bin')}else{@('firmware/motor/Coco.ino.bin')}
foreach($relative in $paths){
    $entry=$manifest.PSObject.Properties[$relative]
    if(!$entry){throw ('Hash missing: '+$relative)}
    if((Get-FileHash -LiteralPath (Join-Path $root $relative) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value){throw ('Hash mismatch: '+$relative)}
}
$expected=if($Target -eq 'voice'){'84:c7:bb:5a:ae:dc'}else{'84:c7:bb:58:e7:88'}
$identity=& $EsptoolPath --chip esp32s3 --port $Port read-mac 2>&1 | Out-String
if($LASTEXITCODE -ne 0 -or $identity -notmatch [regex]::Escape($expected)){throw ('Wrong board or no connection. Expected MAC: '+$expected)}
$backupDir=Join-Path $root 'private-backups'
New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
$backup=Join-Path $backupDir ($Target+'-'+(Get-Date -Format yyyyMMdd-HHmmss)+'-first64k.bin')
& $EsptoolPath --chip esp32s3 --port $Port --baud 460800 read-flash 0x0 0x10000 $backup
if($LASTEXITCODE -ne 0){throw 'Backup failed; no firmware was written.'}
if($Target -eq 'voice'){
    & $EsptoolPath --chip esp32s3 --port $Port --baud 460800 write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x0 (Join-Path $root $paths[0]) 0x8000 (Join-Path $root $paths[1]) 0xd000 (Join-Path $root $paths[2]) 0x20000 (Join-Path $root $paths[3]) 0x800000 (Join-Path $root $paths[4])
}else{
    & $EsptoolPath --chip esp32s3 --port $Port --baud 460800 write-flash 0x10000 (Join-Path $root $paths[0])
}
if($LASTEXITCODE -ne 0){throw 'Flashing failed. Check the esptool output.'}
Write-Host 'Firmware verified. No motor motion command was sent.'
