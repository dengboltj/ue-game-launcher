# Finalize deploy: write ini, replace root bootstrap with our launcher renamed to project name.
$ErrorActionPreference = 'Stop'
$target = [IO.Path]::GetFullPath($env:LAUNCHER_TARGET).TrimEnd('\')
$ram = $env:LAUNCHER_RAM
$vram = $env:LAUNCHER_VRAM

function Write-FinalExe([string]$name) {
  Write-Host "FINAL_EXE=$name"
  $out = $env:LAUNCHER_FINAL_OUT
  if (-not [string]::IsNullOrWhiteSpace($out)) {
    [IO.File]::WriteAllText($out, $name, [Text.UTF8Encoding]::new($false))
  }
}

$shipping = Get-ChildItem -LiteralPath $target -Recurse -Filter '*Shipping.exe' -File -ErrorAction SilentlyContinue |
  Where-Object { $_.Name -ne 'launcher.exe' } |
  Select-Object -First 1

$ini = Join-Path $target 'launcher.ini'
$c = Get-Content -LiteralPath $ini -Raw -Encoding UTF8

if ($shipping) {
  $rel = $shipping.FullName.Substring($target.Length).TrimStart('\')
  Write-Host "[auto] ExePath=$rel"
  $c = [regex]::Replace($c, '(?m)^(ExePath\s*=).*', ('ExePath=' + $rel))
} else {
  Write-Host '[warn] Shipping.exe not found, edit ExePath manually'
}

$c = [regex]::Replace($c, '(?m)^(MinFreeRamGB\s*=).*', ('MinFreeRamGB=' + $ram))
$c = [regex]::Replace($c, '(?m)^(MinVramGB\s*=).*', ('MinVramGB=' + $vram))
[IO.File]::WriteAllText($ini, $c, [Text.UTF8Encoding]::new($false))
Write-Host "[auto] MinFreeRamGB=$ram"
Write-Host "[auto] MinVramGB=$vram"
Write-Host '[ok] launcher.ini updated'

$projectExeName = $null
if ($shipping) {
  $base = $shipping.BaseName
  foreach ($suffix in @('-Win64-Shipping', '-Win64-DebugGame', '-Win64-Development', '-WindowsShipping')) {
    if ($base.EndsWith($suffix, [StringComparison]::OrdinalIgnoreCase)) {
      $base = $base.Substring(0, $base.Length - $suffix.Length)
      break
    }
  }
  if (-not [string]::IsNullOrWhiteSpace($base)) {
    $projectExeName = $base + '.exe'
  }
}

if (-not $projectExeName) {
  $rootExe = Get-ChildItem -LiteralPath $target -Filter '*.exe' -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -ne 'launcher.exe' } |
    Select-Object -First 1
  if ($rootExe) { $projectExeName = $rootExe.Name }
}

if (-not $projectExeName) {
  Write-Host '[warn] cannot infer project name, keep launcher.exe'
  Write-FinalExe 'launcher.exe'
  exit 0
}

$launcherPath = Join-Path $target 'launcher.exe'
$finalPath = Join-Path $target $projectExeName

if (-not (Test-Path -LiteralPath $launcherPath)) {
  Write-Host '[error] launcher.exe missing in target'
  exit 1
}

if ((Test-Path -LiteralPath $finalPath) -and ($finalPath -ne $launcherPath)) {
  Remove-Item -LiteralPath $finalPath -Force
  Write-Host "[replace] removed old starter: $projectExeName"
}

Move-Item -LiteralPath $launcherPath -Destination $finalPath -Force
Write-Host "[rename] launcher.exe -> $projectExeName"
Write-FinalExe $projectExeName