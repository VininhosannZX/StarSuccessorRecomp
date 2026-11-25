param([ValidateSet('Vulkan', 'OpenGL')][string]$Renderer = 'OpenGL', [ValidateSet('driver', 'persistent')][string]$VertexUpload = 'driver', [ValidateSet('StarSuccessor.exe', 'StarSuccessor-Diagnostico-O0.exe')][string]$ExecutableName = 'StarSuccessor.exe', [switch]$Probe, [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$package = Split-Path -Parent $MyInvocation.MyCommand.Path
$executable = Join-Path $package $ExecutableName
if (!(Test-Path -LiteralPath $executable -PathType Leaf)) { throw 'StarSuccessor.exe nao encontrado ao lado deste script.' }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('StarSuccessor-Diagnostico-' + [guid]::NewGuid().ToString('N'))
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) { $OutputDirectory = $package }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$cpuTag = if ($ExecutableName -eq 'StarSuccessor.exe') { 'O1' } else { 'O0' }
$destination = Join-Path $OutputDirectory ('Diagnostico-StarSuccessor-' + $Renderer + '-' + $cpuTag + '-' + $stamp + '.zip')
$started = [DateTime]::UtcNow
$testVariables = @('WIIKIT_PERF', 'STAR_GRAPHICS_DIAGNOSTICS', 'STAR_GL_SELFTEST', 'STAR_GL_FRESH_SHADERS', 'WIIKIT_SHADERDUMP', 'STAR_RENDERER', 'SDL_OPENGL_LIBRARY', 'GALLIUM_DRIVER', 'LIBGL_ALWAYS_SOFTWARE', 'ZINK_USE_LAVAPIPE', 'STAR_GL_VERTEX_UPLOAD', 'MESA_LOG_FILE', 'MESA_DEBUG', 'VK_LOADER_DEBUG', 'WIIKIT_PAD', 'STAR_TEST_SECONDS', 'STAR_TEST_NAND_ROOT')
$previousVariables = @{}
foreach ($name in $testVariables) { $previousVariables[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$preferences = Join-Path $env:APPDATA 'StarSuccessor\NativePort'
$graphics = Join-Path $preferences 'graphics-diagnostics'
New-Item -ItemType Directory -Path $graphics -Force | Out-Null
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
    $hardware = @('Star Successor - teste Windows', ('Inicio UTC: ' + $started.ToString('o')), ('Windows: ' + [Environment]::OSVersion.VersionString))
    try {
        $hardware += Get-CimInstance Win32_Processor | ForEach-Object { 'CPU: ' + $_.Name }
        $hardware += Get-CimInstance Win32_VideoController | ForEach-Object { 'GPU: ' + $_.Name + ' | Driver: ' + $_.DriverVersion }
    } catch { $hardware += 'Consulta de hardware indisponivel: ' + $_.Exception.Message }
    $hardware += 'Renderer solicitado: ' + $Renderer
    $hardware += 'Vertex upload: ' + $VertexUpload
    $hardware += 'Executavel: ' + $ExecutableName
    $vulkanLoader = Join-Path $env:WINDIR 'System32\vulkan-1.dll'
    $hardware += 'Vulkan loader presente: ' + (Test-Path -LiteralPath $vulkanLoader -PathType Leaf)
    $hardware += 'EXE SHA256: ' + (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
    $build = Join-Path $package 'BUILD.json'
    if (Test-Path -LiteralPath $build -PathType Leaf) { Copy-Item -LiteralPath $build -Destination (Join-Path $temporary 'BUILD.json') }
    $hardware | Set-Content -LiteralPath (Join-Path $temporary 'hardware.txt') -Encoding UTF8
    Write-Host 'Teste grafico: abra o jogo e aguarde cerca de 1 minuto. Se aparecer imagem, teste a partida em 1080p.'
    Write-Host 'Depois feche o jogo normalmente. O diagnostico sera criado nesta pasta.'
    if ($Probe) {
        $sandboxNand = Join-Path $temporary 'nand-test'
        New-Item -ItemType Directory -Path $sandboxNand | Out-Null
        $originalNand = Join-Path $preferences 'nand'
        if (Test-Path -LiteralPath $originalNand -PathType Container) {
            Get-ChildItem -LiteralPath $originalNand -Force | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $sandboxNand -Recurse -Force }
        }
        $env:STAR_TEST_NAND_ROOT = $sandboxNand
        $env:STAR_TEST_SECONDS = '25'
        $env:WIIKIT_PAD = '2:A 4:A 6:A 8:A 10:A 12:A 14:A 16:A 18:A 20:A 22:A'
        Write-Host ('Comparacao automatica ' + $cpuTag + ': aguarde o encerramento do jogo.')
    }
    $env:STAR_GL_VERTEX_UPLOAD = $VertexUpload
    $env:MESA_LOG_FILE = Join-Path $graphics 'mesa-driver.log'
    $env:MESA_DEBUG = '1'
    $env:VK_LOADER_DEBUG = 'error,warn'
    $env:STAR_RENDERER = $Renderer
    if ($Renderer -eq 'Vulkan') {
        $mesa = Join-Path $package 'Mesa\opengl32.dll'
        if (!(Test-Path -LiteralPath $mesa -PathType Leaf)) { throw 'Pasta Mesa ausente. Extraia o pacote completo.' }
        $env:SDL_OPENGL_LIBRARY = $mesa
        $env:GALLIUM_DRIVER = 'zink'
        $env:LIBGL_ALWAYS_SOFTWARE = '0'
        Remove-Item Env:ZINK_USE_LAVAPIPE -ErrorAction SilentlyContinue
    } else {
        $env:SDL_OPENGL_LIBRARY = Join-Path $env:WINDIR 'System32\opengl32.dll'
    }
    $env:WIIKIT_PERF = '1'
    $env:STAR_GRAPHICS_DIAGNOSTICS = '1'
    $env:STAR_GL_SELFTEST = '1'
    $env:STAR_GL_FRESH_SHADERS = '1'
    $env:WIIKIT_SHADERDUMP = $graphics
    $liveLog = Join-Path $preferences 'star-successor.log'
    $previousLogTime = [DateTime]::MinValue
    if (Test-Path -LiteralPath $liveLog -PathType Leaf) { $previousLogTime = (Get-Item -LiteralPath $liveLog).LastWriteTimeUtc }
    $stderr = Join-Path $temporary 'driver-stderr.txt'
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo.FileName = $executable
    $process.StartInfo.WorkingDirectory = $package
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.RedirectStandardError = $true
    $null = $process.Start()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $startupAt = $null
    $startupComplete = $false
    $startupResult = 'Encerrado normalmente ou pelo usuario'
    $liveLog = Join-Path $preferences 'star-successor.log'
    while (!$process.HasExited) {
        if (!$startupComplete -and (Test-Path -LiteralPath $liveLog -PathType Leaf)) {
            try {
                if ((Get-Item -LiteralPath $liveLog).LastWriteTimeUtc -gt $previousLogTime) {
                    if ($null -eq $startupAt) { $startupAt = [DateTime]::UtcNow }
                    $text = Get-Content -LiteralPath $liveLog -Raw
                    if ($text -match 'graphics-startup: render loop ready') { $startupComplete = $true }
                }
            } catch { }
        }
        if (!$startupComplete -and $null -ne $startupAt -and ([DateTime]::UtcNow - $startupAt).TotalSeconds -gt 60) {
            $startupResult = 'Inicializacao grafica sem concluir em 60 segundos; processo encerrado pelo teste'
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            break
        }
        Start-Sleep -Milliseconds 250
        $process.Refresh()
    }
    $process.WaitForExit()
    $stderrTask.Result | Set-Content -LiteralPath $stderr -Encoding UTF8
    $startupResult | Set-Content -LiteralPath (Join-Path $temporary 'inicializacao.txt') -Encoding UTF8
    @(('Codigo de saida: ' + $process.ExitCode), ('Fim UTC: ' + [DateTime]::UtcNow.ToString('o')), 'As amostras de FPS incluem menus e carregamentos; avalie o trecho da partida.') | Set-Content -LiteralPath (Join-Path $temporary 'resultado.txt') -Encoding UTF8
    $preferences = Join-Path $env:APPDATA 'StarSuccessor\NativePort'
    $log = Join-Path $preferences 'star-successor.log'
    if ((Test-Path -LiteralPath $log -PathType Leaf) -and (Get-Item -LiteralPath $log).LastWriteTimeUtc -gt $previousLogTime) {
        Copy-Item -LiteralPath $log -Destination (Join-Path $temporary 'star-successor.log')
    } else {
        'Esta execucao nao produziu um log novo. Pode ter sido encerrada antes de carregar o jogo.' | Add-Content -LiteralPath (Join-Path $temporary 'resultado.txt') -Encoding UTF8
    }
    foreach ($name in @('star-successor-settings.ini', 'keys.txt')) {
        $file = Join-Path $preferences $name
        if (Test-Path -LiteralPath $file -PathType Leaf) { Copy-Item -LiteralPath $file -Destination (Join-Path $temporary $name) }
    }
    $freshImages = @(Get-ChildItem -LiteralPath $graphics -File | Where-Object { $_.LastWriteTimeUtc -ge $started.AddSeconds(-1) })
    if ($freshImages.Count -gt 0) {
        $imageFolder = Join-Path $temporary 'graphics-diagnostics'
        New-Item -ItemType Directory -Path $imageFolder | Out-Null
        foreach ($image in $freshImages) { Copy-Item -LiteralPath $image.FullName -Destination (Join-Path $imageFolder $image.Name) }
    }
    $files = @(Get-ChildItem -LiteralPath $temporary | Where-Object { $_.Name -ne 'nand-test' } | ForEach-Object { $_.FullName })
    Compress-Archive -LiteralPath $files -DestinationPath $destination -Force
    Write-Host ('Diagnostico criado: ' + $destination)
} finally {
    foreach ($name in $testVariables) { [Environment]::SetEnvironmentVariable($name, $previousVariables[$name], 'Process') }
    Remove-Item -LiteralPath $temporary -Recurse -Force -ErrorAction SilentlyContinue
}
