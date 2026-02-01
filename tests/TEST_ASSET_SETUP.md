# Test Asset Setup Guide

This guide explains how to set up the required test assets for running integration tests.

## Overview

The integration tests require certain assets to exist in your Unreal Engine project. These assets are used to test various MCP tools like foliage configuration, spline mesh components, and geometry operations.

## Required Assets

| Asset | Path | Type | Required For |
|-------|------|------|--------------|
| TestFoliage | `/Game/Foliage/TestFoliage` | Foliage Type | `configure_foliage_density` tests |
| SplineMeshBP | `/Game/Blueprints/SplineMeshBP` | Blueprint | `create_spline_mesh_component` tests |
| WorldToolsTest | `/Game/WorldToolsTest` | Static Mesh | Geometry tests (optional fallback) |

## Setup Methods

### Method 1: Automated Setup (Recommended)

The setup script attempts to create all required assets automatically using the MCP server.

**Prerequisites:**
1. Unreal Engine project with MCP Automation Bridge plugin enabled
2. MCP server built (`npm run build:core`)
3. Unreal Editor running

**Steps:**

1. **Build the project** (if not already built):
   ```bash
   npm run build:core
   ```

2. **Run the setup script**:
   ```bash
   node tests/setup-test-assets.mjs
   ```

   Or with forced dist mode:
   ```bash
   UNREAL_MCP_FORCE_DIST=1 node tests/setup-test-assets.mjs
   ```

3. **Verify the output** - You should see:
   ```
   ✅ Connected to Unreal MCP Server
   Checking foliage_type: /Game/Foliage/TestFoliage
     ✅ Created successfully
   Checking blueprint: /Game/Blueprints/SplineMeshBP
     ✅ Created successfully
   Checking static_mesh: /Game/WorldToolsTest
     ✅ Created successfully (or skipped if optional)
   ✅ Setup completed successfully
   ```

### Method 2: Manual Creation (Fallback)

If the automated script fails or you prefer to create assets manually, follow these steps in Unreal Editor:

#### Creating TestFoliage (Foliage Type)

1. Open Unreal Editor
2. Navigate to **Content Browser** → **Foliage** folder (create if it doesn't exist)
3. Right-click → **Foliage** → **Foliage Type**
4. Name it `TestFoliage`
5. Double-click to open
6. In **Mesh** section, set the mesh to `/Engine/BasicShapes/Cube`
7. Save the asset

Alternative using MCP (if connected):
```json
{
  "tool": "build_environment",
  "action": "add_foliage_type",
  "foliageTypeName": "TestFoliage",
  "path": "/Game/Foliage/TestFoliage",
  "meshPath": "/Engine/BasicShapes/Cube"
}
```

#### Creating SplineMeshBP (Blueprint)

1. Open Unreal Editor
2. Navigate to **Content Browser** → **Blueprints** folder (create if it doesn't exist)
3. Right-click → **Blueprint Class**
4. Select `Actor` as parent class
5. Name it `SplineMeshBP`
6. Open the blueprint
7. Add a **Spline Component** (if needed for your tests)
8. Compile and save

Alternative using MCP (if connected):
```json
{
  "tool": "manage_asset",
  "action": "create_blueprint",
  "path": "/Game/Blueprints/SplineMeshBP",
  "parentClass": "Actor",
  "name": "SplineMeshBP"
}
```

#### Creating WorldToolsTest (Static Mesh) - Optional

1. Open Unreal Editor
2. In Content Browser, right-click → **Create Basic Asset** → **Static Mesh**
3. Name it `WorldToolsTest`
4. Alternatively, use any existing static mesh and update test references

Alternative using MCP (if connected):
```json
{
  "tool": "manage_geometry",
  "action": "create_static_mesh",
  "meshName": "WorldToolsTest",
  "path": "/Game/WorldToolsTest"
}
```

## Troubleshooting

### "Automation bridge not available" Error

**Cause:** Unreal Editor is not running or the MCP Automation Bridge plugin is not enabled.

**Solution:**
1. Start Unreal Editor
2. Enable **Edit** → **Plugins** → **MCP Automation Bridge**
3. Restart the editor
4. Try the setup script again

### "Connection refused" Errors

**Cause:** The MCP server cannot connect to the automation bridge WebSocket.

**Solution:**
1. Check that the plugin is listening on the correct port (default: 8090)
2. Verify firewall settings allow localhost connections
3. Check plugin settings in **Edit** → **Project Settings** → **MCP Automation Bridge**

### "Module not found" Errors

**Cause:** The TypeScript source files are newer than the compiled dist files.

**Solution:**
```bash
npm run build:core
UNREAL_MCP_FORCE_DIST=1 node tests/setup-test-assets.mjs
```

### Assets Already Exist

The setup script is idempotent - it checks if assets exist before creating them. If assets already exist, you'll see:
```
  ✅ Already exists (skipping)
```

This is normal and safe.

## Environment Variables

You can customize the setup script behavior:

| Variable | Default | Description |
|----------|---------|-------------|
| `UNREAL_MCP_FORCE_DIST` | `0` | Force using compiled dist instead of ts-node |
| `SKIP_OPTIONAL_ASSETS` | `0` | Skip creating optional assets (WorldToolsTest) |
| `MCP_AUTOMATION_WS_PORTS` | `8090,8091` | Comma-separated list of bridge ports to try |
| `MCP_AUTOMATION_WS_HOST` | `127.0.0.1` | Automation bridge host |

## Verification

After setup, verify assets exist:

1. In Unreal Editor, open **Content Browser**
2. Navigate to `/Game/Foliage/` - should see `TestFoliage`
3. Navigate to `/Game/Blueprints/` - should see `SplineMeshBP`
4. Navigate to `/Game/` - should see `WorldToolsTest` (optional)

## Running Tests

Once assets are set up, run integration tests:

```bash
npm test
```

Or run specific test categories:

```bash
node tests/category-tests/build-environment.test.mjs
node tests/category-tests/volumes.test.mjs
```

## Notes

- The setup script must be run before integration tests if assets don't exist
- Assets are persistent - once created, they remain in your project
- The script is safe to run multiple times (idempotent)
- If you delete an asset, re-run the setup script to recreate it
