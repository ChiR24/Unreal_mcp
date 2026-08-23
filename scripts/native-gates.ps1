# Native gates for the McpAutomationBridge plugin.
#
# The TypeScript suite validates records, counts and drift, but it never
# compiles or runs the C++. That gap is not theoretical: a handler calling a
# nonexistent UBlueprint API passed the source-text contract tests and only
# failed at link time, and a malformed TEDS query compiled cleanly and then
# asserted inside a live editor. Both classes are invisible to Gate 1.
#
#   Gate 2  native compile   — does the plugin build against each engine?
#   Gate 3  runtime smoke    — does a headless editor survive the handlers?
#
# Usage:
#   pwsh scripts/native-gates.ps1 -Gate compile
#   pwsh scripts/native-gates.ps1 -Gate smoke
#   pwsh scripts/native-gates.ps1                # both
param(
    # smokecore runs with Fab and Bridge explicitly disabled, which is the
    # configuration that proves the core module carries no Fab imports. smokefab
    # re-runs with them enabled to exercise the adapter.
    [ValidateSet('compile', 'smoke', 'smokecore', 'smokefab', 'all')]
    [string]$Gate = 'all',

    # A suite that silently stops registering tests is indistinguishable from a
    # passing one unless a floor is asserted. 90 is the count both engines
    # actually register today; raise it when tests are added.
    [int]$MinTests = 90,

    # Engine roots to validate against. The plugin advertises 5.0-5.8, so a
    # green run on one version proves nothing about the range.
    [string[]]$EngineRoots = @('X:\UnrealEngine\UE_5.7', 'X:\UnrealEngine\UE_5.8'),

    # Empty means "build a throwaway project from the Gate 2 artifact". A gate
    # must not boot a developer's real project: a saved layout holding the Fab
    # tab crashes the headless editor inside Epic's FFabBrowser::OpenTab, which
    # says nothing about this plugin.
    [string]$Project = ''
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$uplugin = Join-Path $repoRoot 'plugins\McpAutomationBridge\McpAutomationBridge.uplugin'
$failures = @()

function Invoke-CompileGate {
    foreach ($root in $EngineRoots) {
        $uat = Join-Path $root 'Engine\Build\BatchFiles\RunUAT.bat'
        if (-not (Test-Path $uat)) {
            Write-Host "SKIP compile: no engine at $root"
            continue
        }
        $out = Join-Path $env:TEMP ("McpBridgeBuild_" + (Split-Path $root -Leaf))
        Write-Host "GATE2 compile against $root"
        & $uat BuildPlugin -Plugin="$uplugin" -Package="$out" -Rocket -TargetPlatforms=Win64 2>&1 |
            Select-Object -Last 6 | ForEach-Object { Write-Host "  $_" }
        if ($LASTEXITCODE -ne 0) {
            $script:failures += "compile failed against $root (exit $LASTEXITCODE)"
        }
    }
}

# Build a disposable project around the plugin Gate 2 just packaged. Fresh
# Saved/ means the editor opens its default layout, so no docked third-party tab
# gets spawned into a headless process.
function New-SmokeProject {
    param([string]$Root, [bool]$IncludeFab)

    $leaf = Split-Path $Root -Leaf                       # UE_5.7
    $version = $leaf -replace '^UE_', ''                 # 5.7
    $built = Join-Path $env:TEMP "McpBridgeBuild_$leaf"
    if (-not (Test-Path (Join-Path $built 'Binaries\Win64'))) {
        return $null
    }
    $suffix = if ($IncludeFab) { 'Fab' } else { 'Core' }
    $dir = Join-Path $env:TEMP "McpBridgeSmokeProject_${leaf}_$suffix"
    Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path (Join-Path $dir 'Content') | Out-Null

    $plugins = Join-Path $dir 'Plugins\McpAutomationBridge'
    New-Item -ItemType Directory -Force -Path $plugins | Out-Null
    foreach ($item in 'Binaries', 'Content', 'Resources', 'McpAutomationBridge.uplugin') {
        $src = Join-Path $built $item
        if (Test-Path $src) { Copy-Item $src $plugins -Recurse -Force }
    }

    # The module hard-links every optional group that existed at build time
    # (Fab, MovieRenderPipeline, GeometryScripting, ...). Those DLLs only land on
    # the search path when their plugin is mounted, and an Optional reference in
    # the .uplugin is not enough to mount them in a bare project, so the declared
    # dependency set is restated here as explicit project entries.
    $manifest = Get-Content (Join-Path $plugins 'McpAutomationBridge.uplugin') -Raw | ConvertFrom-Json
    $entries = @([ordered]@{ Name = 'McpAutomationBridge'; Enabled = $true })
    # Deliberately NOT carrying the manifest's Optional flag across: an optional
    # project reference leaves the plugin unmounted, and an unmounted plugin
    # keeps its Binaries off the DLL search path, which is what makes the
    # hard-linked imports unresolvable. A name this engine does not ship is
    # dropped instead, because a non-optional reference to a missing plugin is a
    # hard PluginManager fatal before anything can be measured.
    $available = @{}
    Get-ChildItem (Join-Path $Root 'Engine\Plugins') -Recurse -Filter '*.uplugin' -ErrorAction SilentlyContinue |
        ForEach-Object { $available[$_.BaseName] = $true }
    $fabPlugins = @('Fab', 'Bridge')
    foreach ($dep in $manifest.Plugins) {
        if ($fabPlugins -contains $dep.Name) { continue }
        if ($available.ContainsKey($dep.Name)) {
            $entries += [ordered]@{ Name = $dep.Name; Enabled = $true }
        }
        else {
            Write-Host "  note: engine has no plugin '$($dep.Name)'; not enabling it"
        }
    }
    # Fab and Bridge are EnabledByDefault engine plugins, so leaving them out of
    # the list still mounts them. Core mode has to say Enabled=false explicitly,
    # which is what makes this a real test of the decoupling.
    foreach ($name in $fabPlugins) {
        if ($available.ContainsKey($name)) {
            $entries += [ordered]@{ Name = $name; Enabled = $IncludeFab }
        }
    }

    $uproject = Join-Path $dir 'McpBridgeSmoke.uproject'
    [ordered]@{
        FileVersion       = 3
        EngineAssociation = $version
        Category          = ''
        Description       = 'Disposable project for the McpAutomationBridge runtime smoke gate.'
        Plugins           = $entries
    } | ConvertTo-Json -Depth 5 | Set-Content $uproject -Encoding utf8
    return $uproject
}

function Invoke-SmokeGate {
    param([string]$Mode, [bool]$IncludeFab, [string[]]$Roots)

    # -NullRHI keeps this headless; -unattended turns a modal assert into a
    # non-zero exit instead of a dialog that hangs CI forever.
    foreach ($root in $Roots) {
        $cmd = Join-Path $root 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
        if (-not (Test-Path $cmd)) {
            Write-Host "SKIP smoke: no editor at $root"
            continue
        }
        if ($Project) {
            $target = $Project
        }
        else {
            $target = New-SmokeProject -Root $root -IncludeFab $IncludeFab
            if (-not $target) {
                Write-Host "SKIP smoke: no Gate 2 build for $root (run native:compile first)"
                continue
            }
        }
        Write-Host "GATE3 smoke:$Mode against $root"
        $log = Join-Path $env:TEMP ("McpBridgeSmoke_" + (Split-Path $root -Leaf) + "_$Mode.log")
        Remove-Item $log -ErrorAction SilentlyContinue

        # A developer editor may already hold the default listen ports on this
        # project. Binding is not what this gate measures, so move the sockets
        # out of the way instead of failing on a collision that is not a defect.
        $settings = '/Script/McpAutomationBridge.McpAutomationBridgeSettings'
        $report = Join-Path $env:TEMP ("McpBridgeSmokeReport_" + (Split-Path $root -Leaf) + "_$Mode")
        Remove-Item $report -Recurse -Force -ErrorAction SilentlyContinue
        $env:MCP_NATIVE_PORT = '13000'
        # UE5 drives the automation controller through -ExecCmds; -run=Automation
        # looks for an AutomationCommandlet class that does not exist and exits
        # before a single test runs. -TestExit is what makes the editor quit when
        # the queue drains instead of sitting in an idle editor loop forever.
        & $cmd "$target" `
            -ExecCmds="Automation RunTests McpAutomationBridge" `
            -TestExit="Automation Test Queue Empty" `
            -ReportExportPath="$report" `
            -ini:Engine:["$settings"]:ListenPorts=18090,18091 `
            -NullRHI -unattended -nopause -nosplash -abslog="$log" 2>&1 | Out-Null
        $exit = $LASTEXITCODE
        Remove-Item Env:\MCP_NATIVE_PORT -ErrorAction SilentlyContinue

        if (-not (Test-Path $log)) {
            $script:failures += "smoke produced no log against $root"
            continue
        }
        # An assert still writes to the log even when the exit code is
        # swallowed by the crash handler, so scan both.
        $bad = Select-String -Path $log -Pattern 'Assertion failed|Fatal error|LogOutputDevice: Error' -ErrorAction SilentlyContinue
        if ($bad) {
            # "Assertion failed: IsValid()" alone does not say whose bug it is.
            # The first callstack frame outside Core/Slate does, and the answer
            # has already once been a third-party plugin rather than this one.
            $frame = Select-String -Path $log -Pattern 'Callstack\] 0x\w+ (UnrealEditor-(?!Core|Slate)\S+)' `
                -ErrorAction SilentlyContinue | Select-Object -First 1
            $blame = if ($frame) { ' in ' + $frame.Matches[0].Groups[1].Value } else { '' }
            $script:failures += "smoke hit an assert against ${root}${blame}: $($bad[0].Line.Trim())"
        }
        $loadFail = Select-String -Path $log -Pattern "failed to load because module" -ErrorAction SilentlyContinue
        if ($loadFail) {
            $script:failures += "smoke:$Mode plugin did not load against ${root}: $($loadFail[0].Line.Trim())"
        }

        # The exported report is authoritative; log scraping is the fallback when
        # the run died before the report was written.
        $passed = 0; $failed = 0
        $index = Join-Path $report 'index.json'
        if (Test-Path $index) {
            $json = Get-Content $index -Raw | ConvertFrom-Json
            $passed = [int]$json.succeeded + [int]$json.succeededWithWarnings
            $failed = [int]$json.failed + [int]$json.notRun
        }
        else {
            $results = Select-String -Path $log -Pattern 'Test Completed\. Result=\{(\w+)\}' -AllMatches
            $passed = @($results | Where-Object { $_.Matches[0].Groups[1].Value -match 'Passed|Success' }).Count
            $failed = @($results | Where-Object { $_.Matches[0].Groups[1].Value -notmatch 'Passed|Success' }).Count
        }
        Write-Host "  $passed passed, $failed failed"
        # A filter that matches nothing exits 0 and proves nothing, so an empty
        # run is a gate failure rather than a pass.
        if ($passed + $failed -lt $MinTests) {
            $script:failures += "smoke:$Mode discovered $($passed + $failed) tests against $root, expected at least $MinTests"
        }
        if ($failed -gt 0) { $script:failures += "smoke:$Mode had $failed failing tests against $root" }
        if ($exit -ne 0) { $script:failures += "smoke:$Mode exited $exit against $root" }
    }
}

if ($Gate -in 'compile', 'all') { Invoke-CompileGate }
if ($Gate -in 'smoke', 'smokecore', 'all') {
    Invoke-SmokeGate -Mode 'core' -IncludeFab $false -Roots $EngineRoots
}
if ($Gate -in 'smoke', 'smokefab', 'all') {
    $fabRoots = $EngineRoots | Where-Object { (Split-Path $_ -Leaf) -notmatch '5\.7' }
    foreach ($skipped in ($EngineRoots | Where-Object { (Split-Path $_ -Leaf) -match '5\.7' })) {
        Write-Host "SKIP smoke:fab on ${skipped}: Epic Fab 0.0.10 asserts in FFabBrowser::OpenTab under -NullRHI"
    }
    Invoke-SmokeGate -Mode 'fab' -IncludeFab $true -Roots $fabRoots
}

if ($failures.Count -gt 0) {
    Write-Host ''
    Write-Host 'NATIVE GATES FAILED:'
    $failures | ForEach-Object { Write-Host "  - $_" }
    exit 1
}
Write-Host 'native gates passed'
exit 0
