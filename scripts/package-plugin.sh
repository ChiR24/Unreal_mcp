#!/usr/bin/env bash
#
# Package McpAutomationBridge plugin as pre-built binaries.
# Output can be distributed to Blueprint-only projects (no C++ compilation needed).
#
# Usage:
#   ./scripts/package-plugin.sh /path/to/UE_5.6
#   ./scripts/package-plugin.sh /path/to/UE_5.6 /custom/output/dir
#   ./scripts/package-plugin.sh /path/to/UE_5.6 /custom/output/dir -NoDefaultPlugins
#
# The script will:
#   1. Build the plugin via RunUAT
#   2. Set "Installed": true in the output .uplugin
#   3. Create a zip archive ready for distribution
#

set -euo pipefail

# ─── Arguments ──────────────────────────────────────────────────────────────

ENGINE_DIR="${1:?Usage: $0 <UnrealEngineDir> [OutputDir] [extra RunUAT args...]}"
shift

OUTPUT_DIR=""
EXTRA_ARGS=""
for arg in "$@"; do
    case "$arg" in
        -*) EXTRA_ARGS="$EXTRA_ARGS $arg" ;;
        *)  [ -z "$OUTPUT_DIR" ] && OUTPUT_DIR="$arg" ;;
    esac
done
OUTPUT_DIR="${OUTPUT_DIR:-$(pwd)/build}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PLUGIN_FILE="$REPO_ROOT/plugins/McpAutomationBridge/McpAutomationBridge.uplugin"

if [ ! -f "$PLUGIN_FILE" ]; then
    echo "ERROR: Plugin file not found: $PLUGIN_FILE"
    exit 1
fi

# ─── Detect platform and RunUAT path ────────────────────────────────────────

case "$(uname -s)" in
    Darwin)
        PLATFORM="Mac"
        RUN_UAT="$ENGINE_DIR/Engine/Build/BatchFiles/RunUAT.sh"
        ;;
    Linux)
        PLATFORM="Linux"
        RUN_UAT="$ENGINE_DIR/Engine/Build/BatchFiles/RunUAT.sh"
        ;;
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
        PLATFORM="Win64"
        RUN_UAT="$ENGINE_DIR/Engine/Build/BatchFiles/RunUAT.bat"
        ;;
    *)
        echo "ERROR: Unsupported platform: $(uname -s)"
        exit 1
        ;;
esac

if [ ! -f "$RUN_UAT" ]; then
    echo "ERROR: RunUAT not found: $RUN_UAT"
    echo "Make sure the first argument points to your UE installation root."
    exit 1
fi

# ─── Extract version info ───────────────────────────────────────────────────

# Get UE version from the engine
UE_VERSION_FILE="$ENGINE_DIR/Engine/Build/Build.version"
if [ -f "$UE_VERSION_FILE" ]; then
    UE_MAJOR=$(python3 -c "import json; print(json.load(open('$UE_VERSION_FILE'))['MajorVersion'])" 2>/dev/null || echo "5")
    UE_MINOR=$(python3 -c "import json; print(json.load(open('$UE_VERSION_FILE'))['MinorVersion'])" 2>/dev/null || echo "x")
    UE_VER="${UE_MAJOR}.${UE_MINOR}"
else
    UE_VER="unknown"
fi

PLUGIN_VER=$(python3 -c "import json; print(json.load(open('$PLUGIN_FILE'))['VersionName'])" 2>/dev/null || echo "0.0.0")

PACKAGE_DIR="$OUTPUT_DIR/McpAutomationBridge"
ZIP_NAME="McpAutomationBridge-v${PLUGIN_VER}-UE${UE_VER}-${PLATFORM}.zip"

echo "============================================"
echo "  Package McpAutomationBridge Plugin"
echo "============================================"
echo "  Plugin version : $PLUGIN_VER"
echo "  UE version     : $UE_VER"
echo "  Platform        : $PLATFORM"
echo "  Engine          : $ENGINE_DIR"
echo "  Output          : $OUTPUT_DIR/$ZIP_NAME"
echo "============================================"
echo ""

# ─── Build ──────────────────────────────────────────────────────────────────

echo "Building plugin..."
"$RUN_UAT" BuildPlugin \
    -Plugin="$PLUGIN_FILE" \
    -Package="$PACKAGE_DIR" \
    -TargetPlatforms="$PLATFORM" \
    -Rocket $EXTRA_ARGS

echo ""
echo "Build complete."

# ─── Post-process: set Installed=true ────────────────────────────────────────

OUTPUT_UPLUGIN="$PACKAGE_DIR/McpAutomationBridge.uplugin"
if [ -f "$OUTPUT_UPLUGIN" ]; then
    echo "Setting Installed=true in output .uplugin..."
    python3 -c "
import json, sys
with open('$OUTPUT_UPLUGIN', 'r') as f:
    data = json.load(f)
data['Installed'] = True
with open('$OUTPUT_UPLUGIN', 'w') as f:
    json.dump(data, f, indent=2)
    f.write('\n')
"
fi

# ─── Zip ─────────────────────────────────────────────────────────────────────

echo "Creating archive: $ZIP_NAME"
cd "$OUTPUT_DIR"
zip -r "$ZIP_NAME" McpAutomationBridge/ -x "*.pdb" "Intermediate/*"
echo ""

FINAL_SIZE=$(du -sh "$OUTPUT_DIR/$ZIP_NAME" | cut -f1)
echo "============================================"
echo "  Done!"
echo "  Archive: $OUTPUT_DIR/$ZIP_NAME ($FINAL_SIZE)"
echo "============================================"
echo ""
echo "To install: unzip into YourProject/Plugins/"
