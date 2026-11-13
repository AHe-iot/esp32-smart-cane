param(
  [Parameter(Mandatory = $true)][string]$Distance,
  [int]$Seconds = 8
)
$cli = "D:\dev\Arduino_IDE\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$dir = "D:\learning\AIoT学习\05-毕设真机\H1\标定实验"
$out = Join-Path $dir ("raw_{0}cm.txt" -f $Distance)
if (Test-Path $out) { Remove-Item $out -Force }

$proc = Start-Process -FilePath $cli -ArgumentList 'monitor','-p','COM5','-c','baudrate=115200','--quiet' `
  -RedirectStandardOutput $out -PassThru -NoNewWindow
Start-Sleep -Seconds $Seconds
if (-not $proc.HasExited) { $proc.Kill(); $proc.WaitForExit() }

$count = (Get-Content $out | Where-Object { $_ -match '\d' } | Measure-Object).Count
Write-Output ("Saved {0}  ({1} lines)" -f $out, $count)
