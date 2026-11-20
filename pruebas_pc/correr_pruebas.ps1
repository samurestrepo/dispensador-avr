# =====================================================================
# correr_pruebas.ps1 - Compila y corre las pruebas de logica pura en el PC
# Uso (desde cualquier carpeta):   powershell -ExecutionPolicy Bypass -File pruebas_pc\correr_pruebas.ps1
# Sale con codigo 0 si todo pasa; 1 si alguna prueba falla o no compila; 2 si no hay g++.
# (Sin tildes: Windows PowerShell 5.1 lee mal los .ps1 en UTF-8 sin BOM.)
# =====================================================================

Set-Location $PSScriptRoot

if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: g++ no esta en el PATH. Abra una terminal nueva o agregue WinLibs al PATH."
    exit 2
}

# Cada prueba se compila junto con el modulo de dispensador/ que prueba.
# prueba_fsm usa los modulos reales de logica (fsm, pedido, saldo); el hardware
# (lcd, teclado, monedas, reles, tarjeta) esta simulado dentro de prueba_fsm.cpp.
$pruebas = @(
    @{ Nombre = "prueba_pedido"; Modulos = @("pedido") },
    @{ Nombre = "prueba_saldo";  Modulos = @("saldo") },
    @{ Nombre = "prueba_fsm";    Modulos = @("fsm", "pedido", "saldo") }
)

$fallas = 0
foreach ($p in $pruebas) {
    $fuente  = "$($p.Nombre).cpp"
    $modulos = @()
    foreach ($m in $p.Modulos) {
        $modulos += "../dispensador/$m.cpp"
    }
    $exe = "$($p.Nombre).exe"

    Write-Host ""
    Write-Host "### g++ $fuente $($modulos -join ' ')"
    & g++ -std=c++17 -Wall -Wextra -I../dispensador $fuente $modulos -o $exe
    if ($LASTEXITCODE -ne 0) {
        Write-Host "NO COMPILO: $fuente"
        $fallas++
        continue
    }

    & ".\$exe"
    if ($LASTEXITCODE -ne 0) {
        $fallas++
    }
}

Write-Host ""
if ($fallas -gt 0) {
    Write-Host "RESULTADO: $fallas programa(s) de prueba con fallas"
    exit 1
}
Write-Host "RESULTADO: todas las pruebas pasaron"
exit 0
