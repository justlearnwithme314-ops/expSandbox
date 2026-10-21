$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot
$packageRoot = Join-Path $projectRoot 'build\package\VoxelLab'
New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'build\native\bin\Release\voxel-lab.exe') -Destination $packageRoot -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'content') -Destination $packageRoot -Recurse -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md') -Destination $packageRoot -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'build\native\Release\voxel-cli.exe') -Destination $packageRoot -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'build\native\Release\voxel-api.dll') -Destination $packageRoot -Force
$packageDocs = Join-Path $packageRoot 'docs'
$packageTools = Join-Path $packageRoot 'tools'
$packageApi = Join-Path $packageRoot 'src'
New-Item -ItemType Directory -Force -Path $packageDocs,$packageTools,$packageApi | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\PORT_STATUS.md') -Destination $packageDocs -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\generated') -Destination $packageDocs -Recurse -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'tools\python_world_example.py') -Destination $packageTools -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'src\c_api.h') -Destination $packageApi -Force
@('@echo off','cd /d "%~dp0"','start "" "voxel-lab.exe"') | Set-Content (Join-Path $packageRoot 'launch.cmd')
$modelsTarget = Join-Path $packageRoot 'assets\kenney_survival-kit\Models\GLB format'
$audioTarget = Join-Path $packageRoot 'assets\kenney_impact-sounds\Audio'
$noticesTarget = Join-Path $packageRoot 'licenses'
New-Item -ItemType Directory -Force -Path $modelsTarget,$audioTarget,$noticesTarget | Out-Null
foreach ($modelName in @('campfire-pit.glb','barrel.glb')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot "assets\kenney_survival-kit\Models\GLB format\$modelName") -Destination $modelsTarget -Force
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\kenney_survival-kit\Models\GLB format\Textures') -Destination $modelsTarget -Recurse -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\kenney_impact-sounds\Audio\footstep_wood_000.ogg') -Destination $audioTarget -Force
$licenses = @{
    'Transvoxel-MIT.txt' = 'references\Transvoxel\LICENSE'
    'FastNoiseLite-MIT.txt' = 'references\FastNoiseLite\LICENSE'
    'raylib-zlib.txt' = 'references\raylib\LICENSE'
    'json-MIT.txt' = 'references\json\LICENSE.MIT'
    'Kenney-survival-CC0.txt' = 'assets\kenney_survival-kit\License.txt'
    'Kenney-audio-CC0.txt' = 'assets\kenney_impact-sounds\License.txt'
}
foreach ($licenseName in $licenses.Keys) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $licenses[$licenseName]) -Destination (Join-Path $noticesTarget $licenseName) -Force
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\THIRD_PARTY.md') -Destination $noticesTarget -Force
Get-Content -LiteralPath (Join-Path $projectRoot 'references\SQLiteCpp\sqlite3\sqlite3.c') -TotalCount 45 | Set-Content (Join-Path $noticesTarget 'SQLite-public-domain.txt')
# Copy contents into consistently named folders. This remains idempotent and
# never removes packaged user worlds when refreshing a build.
foreach ($sourceAndTarget in @(
    @('references\raylib\src\external','raylib-third-party-sources'),
    @('references\json\LICENSES','json-supplemental')
)) {
    $sourcePath = Join-Path $projectRoot $sourceAndTarget[0]
    $noticePath = Join-Path $noticesTarget $sourceAndTarget[1]
    New-Item -ItemType Directory -Force -Path $noticePath | Out-Null
    Get-ChildItem -LiteralPath $sourcePath | Copy-Item -Destination $noticePath -Recurse -Force
}
Write-Output "Portable game folder: $packageRoot"
