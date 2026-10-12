param([ValidateSet('doctor','build','flash')][string]$Command = 'doctor')
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
function Need([string]$Name) {
  if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) { throw "Install $Name and add it to PATH. See README.txt." }
}
function Invoke-Step([string]$Program, [string[]]$StepArgs) {
  & $Program @StepArgs
  if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
function Build {
  Need 'arm-none-eabi-gcc'; Need 'cmake'; Need 'ninja'
  Invoke-Step 'cmake' @('-S','.','-B','build','-G','Ninja')
  Invoke-Step 'cmake' @('--build','build')
  if (-not (Test-Path -LiteralPath 'build/firmware.bin')) { throw 'Build did not produce firmware.bin.' }
}
try {
  $cfg = @{}
  foreach ($line in Get-Content -LiteralPath 'rts.env') {
    if (-not $line.Trim() -or $line.TrimStart().StartsWith('#')) { continue }
    $pair = $line.Split('=',2)
    if ($pair.Length -ne 2) { throw 'Invalid rts.env line.' }
    if ($pair[0] -notin @('BOARD_LABEL','BOARD_CONFIG_CONFIRMED','MEMORY_CONFIG_CONFIRMED','TOOL','INTERFACE','FLASH_ADDRESS','OPENOCD_CFG','PROBE_SERIAL')) { throw "Unknown configuration key: $($pair[0])" }
    $cfg[$pair[0]] = $pair[1]
  }
  if ($Command -eq 'doctor') {
    Need 'arm-none-eabi-gcc'; Need 'cmake'; Need 'ninja'
    Write-Output 'Build tools found. No hardware was contacted. Configure rts.env before flash.'
  } elseif ($Command -eq 'build') { Build } else {
    if ($cfg['BOARD_CONFIG_CONFIRMED'] -ne 'true') { throw 'Confirm board/pins/clocks in rts.env before flashing.' }
    if ($cfg['MEMORY_CONFIG_CONFIRMED'] -ne 'true') { throw 'Verify core/linker.ld against the exact part and confirm memory in rts.env.' }
    if (-not $cfg['BOARD_LABEL'] -or $cfg['BOARD_LABEL'] -like 'SET_THIS*') { throw 'Set your exact BOARD_LABEL.' }
    if ($cfg['FLASH_ADDRESS'] -ne '0x08000000' -or $cfg['INTERFACE'] -ne 'SWD') { throw 'This cold-reset starter supports SWD at flash base 0x08000000 only.' }
    switch ($cfg['TOOL']) {
      'stm32_programmer_cli' { Need 'STM32_Programmer_CLI' }
      'openocd' { Need 'openocd'; if (-not $cfg['OPENOCD_CFG']) { throw 'Set OPENOCD_CFG to a board config containing both probe and target.' } }
      default { throw 'TOOL must be stm32_programmer_cli or openocd.' }
    }
    Build
    if ($cfg['TOOL'] -eq 'stm32_programmer_cli') {
      $flashArgs = @('-c','port=SWD')
      if ($cfg['PROBE_SERIAL']) { $flashArgs += "sn=$($cfg['PROBE_SERIAL'])" }
      $flashArgs += @('-w','build/firmware.bin','0x08000000','-v','-rst')
      Invoke-Step 'STM32_Programmer_CLI' $flashArgs
    } else {
      Invoke-Step 'openocd' @('-f',$cfg['OPENOCD_CFG'],'-c','program build/firmware.bin 0x08000000 verify reset exit')
    }
    Write-Output 'Firmware programmed, verified and reset. Verify application behaviour on the board.'
  }
} catch { Write-Error $_ -ErrorAction Continue; exit 1 }
