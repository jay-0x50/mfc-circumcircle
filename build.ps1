param(
    [ValidateSet('Debug', 'Release', 'All')]
    [string]$Configuration = 'All',
    [switch]$Test,
    [switch]$UiTest
)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio with Desktop C++ and MFC is required.' }
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild was not found.' }
$artifacts = Join-Path $projectRoot 'artifacts'
New-Item -ItemType Directory -Path $artifacts -Force | Out-Null
$configs = if ($Configuration -eq 'All') { @('Debug', 'Release') } else { @($Configuration) }
foreach ($config in $configs) {
    & $msbuild (Join-Path $projectRoot 'Circumcircle.sln') /m /t:Build "/p:Configuration=$config" /p:Platform=x64 /v:minimal /fl "/flp:logfile=$artifacts\build-$config.log;verbosity=normal;encoding=UTF-8"
    if ($LASTEXITCODE -ne 0) { throw "$config MFC build failed. See artifacts/build-$config.log." }
    if ($Test) {
        & $msbuild (Join-Path $projectRoot 'tests\GeometryTests.vcxproj') /m /t:Build "/p:Configuration=$config" /p:Platform=x64 /v:minimal
        if ($LASTEXITCODE -ne 0) { throw "$config geometry test build failed." }
        & (Join-Path $projectRoot "bin\x64\$config\GeometryTests.exe") | Tee-Object -FilePath (Join-Path $artifacts "geometry-$config.txt")
        if ($LASTEXITCODE -ne 0) { throw "$config geometry tests failed." }
    }
    if ($UiTest) {
        & python (Join-Path $projectRoot 'tests\UiSmokeTest.py') --configuration $config --offscreen
        if ($LASTEXITCODE -ne 0) { throw "$config UI tests failed." }
    }
}
