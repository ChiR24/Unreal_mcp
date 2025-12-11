# Unreal MCP Server - Individual Tool Testing Guide

## Overview
Each MCP tool now has its own dedicated test file with focused test cases. This allows you to test one tool at a time for faster iteration and debugging.

## Test Structure

### Individual Test Files
Each tool has a dedicated test file in `tests/`:

- **test-manage-asset.mjs** - Asset management (import, delete, rename, duplicate, list, get info)
- **test-control-actor.mjs** - Actor operations (spawn, delete, transform, properties)
- **test-control-editor.mjs** - Editor control (PIE, camera, screenshots, console commands)
- **test-manage-level.mjs** - Level management (load, save, create, stream)
- **test-animation.mjs** - Animation tools (play, stop, speed, state)
- **test-blueprint.mjs** - Blueprint operations (execute function, get/set variables)
- **test-materials.mjs** - Material tools (create instance, set parameters, apply)
- **test-niagara.mjs** - Niagara VFX (spawn system, set parameters, stop)
- **test-render.mjs** - Rendering tools (render targets, Lumen, lighting)
- **test-landscape.mjs** - Landscape tools (create, sculpt, paint, get info)
- **test-sequence.mjs** - Sequencer (create sequence, add tracks, keyframes, play, export)
- **test-system.mjs** - System control (engine info, settings, plugins, Python execution)
- **test-behavior-tree.mjs** - Behavior Tree editing (add nodes, connect, properties)
- **test-audio.mjs** - Audio management (sound cues, play sounds, audio components)
- **test-inspect.mjs** - Object introspection (get/set properties, components)
- **test-console-command.mjs** - Console command execution
- **test-asset-advanced.mjs** - Advanced asset operations (dependencies, etc.)
- **test-world-partition.mjs** - World Partition management
- **test-graphql.mjs** - GraphQL API testing
- **test-wasm.mjs** - WebAssembly module testing

### Shared Test Runner
All test files use `test-runner.mjs` which provides:
- MCP client initialization
- Test execution and timing
- Result evaluation with proper error handling
- JSON report generation
- Summary statistics

## Running Tests

### Run a Specific Tool Test
```bash
npm run test:manage_asset     # Test asset management
npm run test:control_actor    # Test actor operations
npm run test:control_editor   # Test editor controls
npm run test:manage_level     # Test level management
npm run test:animation        # Test animation tools
npm run test:blueprint        # Test blueprint operations
npm run test:materials        # Test material tools
npm run test:niagara          # Test Niagara VFX
npm run test:render           # Test rendering tools
npm run test:landscape        # Test landscape tools
npm run test:sequence         # Test sequencer
npm run test:system           # Test system control
npm run test:behavior_tree    # Test behavior tree editing
npm run test:audio            # Test audio management
npm run test:inspect          # Test object introspection
npm run test:console_command  # Test console commands
npm run test:asset_advanced   # Test advanced asset tools
npm run test:world_partition  # Test World Partition
npm run test:graphql          # Test GraphQL API
npm run test:wasm             # Test WebAssembly
```

### Run Tests Directly
```bash
node tests/test-manage-asset.mjs
node tests/test-control-actor.mjs
# etc...
```

## Prerequisites

### Unreal Engine Setup
1. **Unreal Engine 5.0-5.7** must be running
2. **Required plugins enabled:**
   - Native Automation Bridge
   - Python Editor Script Plugin
   - Editor Scripting Utilities
   - Sequencer
   - Level Sequence Editor
   - Control Rig
   - Subobject Data Interface (UE 5.7+)

### Configuration
**DefaultEngine.ini:**
```ini
[HTTPServer.Listeners]
DefaultBindAddress=0.0.0.0

```

**Environment Variables (optional):**
```
UE_HOST=127.0.0.1
MCP_AUTOMATION_PORT=8091
```

## Test Output

### Console Output
Each test displays:
- Test scenario description
- Pass/Fail status with ✅/❌
- Execution time in milliseconds
- Error details for failed tests

Example:
```
============================================================
Starting Asset Management Tests
Total test cases: 10
============================================================

✅ Connected to Unreal MCP Server

[PASSED] Import FBX model into /Game/Meshes (1234.5 ms)
[PASSED] List all assets in /Game/Meshes (456.7 ms)
[FAILED] Delete non-existent asset (234.1 ms) => {"success":false,"error":"ASSET_NOT_FOUND"}

============================================================
Asset Management Test Summary
============================================================
Total cases: 10
✅ Passed: 8
❌ Failed: 2
⏭️  Skipped: 0
Pass rate: 80.0%
Results saved to: tests/reports/asset-management-test-results-2025-10-07T12-34-56.789Z.json
============================================================
```

### JSON Reports
Reports are saved to `tests/reports/` with format:
```
<toolname>-test-results-<timestamp>.json
```

Report structure:
```json
{
  "generatedAt": "2025-10-07T12:34:56.789Z",
  "toolName": "asset-management",
  "results": [
    {
      "scenario": "Import FBX model into /Game/Meshes",
      "toolName": "manage_asset",
      "arguments": { "action": "import_asset", ... },
      "status": "passed",
      "durationMs": 1234.5,
      "detail": "{\"success\":true,\"message\":\"Asset imported\"}"
    }
  ]
}
```

## Test Case Structure

Each test file contains an array of test cases with:
- **scenario**: Human-readable description
- **toolName**: MCP tool name to call
- **arguments**: Tool arguments (action + parameters)
- **expected**: Expected outcome (success/error keywords)

Example:
```javascript
{
  scenario: 'Spawn a cube at origin',
  toolName: 'control_actor',
  arguments: {
    action: 'spawn_actor',
    class_path: '/Script/Engine.StaticMeshActor',
    location: { x: 0, y: 0, z: 100 },
    actor_label: 'TestCube'
  },
  expected: 'success - actor spawned'
}
```

## Adding New Tests

### 1. Open the relevant test file
```bash
code tests/test-manage-asset.mjs
```

### 2. Add test case to the array
```javascript
{
  scenario: 'Your test description',
  toolName: 'manage_asset',
  arguments: {
    action: 'import_asset',
    source_path: 'C:\\path\\to\\asset.fbx',
    destination_path: '/Game/MyFolder'
  },
  expected: 'success - asset imported'
}
```

### 3. Run the updated test
```bash
npm run test:manage_asset
```

## Test Evaluation Logic

### Success Tests
Test passes when:
- Response has `success: true`
- Expected outcome contains success keywords: `success`, `returns`, `created`, `applied`, etc.

### Error Tests  
Test passes when:
- Response has `success: false` with expected error type
- Expected outcome contains failure keywords: `error`, `not found`, `invalid`, `missing`
- Actual error matches expected error type

### Connection Failures
Tests **always fail** if:
- Unreal Engine is not connected (`UE_NOT_CONNECTED`)
- Unless test explicitly expects disconnection

This prevents false positives from connection errors.

## Troubleshooting

### All Tests Fail with UE_NOT_CONNECTED
**Problem**: Unreal Engine is not running or not accessible

**Solutions**:
1. Launch Unreal Engine 5.6
2. Open your project
3. Verify Unreal Engine is running with the MCP plugin enabled
4. Check `DefaultEngine.ini` configuration
5. Test connection: `npm run test:system`

### Specific Tests Fail
**Problem**: Missing Unreal setup or incorrect test expectations

**Solutions**:
1. Check Unreal Output Log for Python errors
2. Verify required plugins for that tool category
3. Review test arguments for correctness
4. Check asset paths exist in your project

### Test Times Out
**Problem**: Operation takes too long or hangs

**Solutions**:
1. Check Unreal is responsive (not frozen)
2. Simplify test case (smaller assets, fewer operations)
3. Check Python Script Plugin is enabled
4. Review Unreal Output Log for errors

### Import Tests Fail
**Problem**: File paths don't exist

**Solutions**:
1. Update file paths in test cases to valid locations
2. Set environment variable: `UNREAL_MCP_FBX_FILE=C:\path\to\your\test.fbx`
3. Use absolute Windows paths with double backslashes

## Best Practices

### 1. Test Incrementally
Start with basic operations, then add complex scenarios:
```bash
npm run test:control_actor   # Basic actor operations
npm run test:control_editor  # Editor controls
npm run test:manage_level    # Level management
```

### 2. Isolate Failures
When a test fails:
1. Run only that tool's tests: `npm run test:<toolname>`
2. Check the JSON report for details
3. Review Unreal Output Log
4. Manually verify in Unreal if needed

### 3. Clean State
Between test runs:
- Delete test actors/assets created
- Reset to known level state
- Clear console output

### 4. Update Paths
Customize test cases for your project:
```javascript
// Update asset paths
asset_path: '/Game/YourProject/Meshes/YourAsset'

// Update file system paths
source_path: 'C:\\YourPath\\YourFile.fbx'
```

## Performance Notes

### Typical Test Durations
- **Asset operations**: 500-2000ms (depends on asset size)
- **Actor spawning**: 100-500ms
- **Editor commands**: 50-200ms
- **Python scripts**: 200-1000ms
- **Material operations**: 300-800ms
- **Sequence operations**: 400-1200ms

### Optimization Tips
- Run tests on smaller assets first
- Use Preview quality for lighting builds
- Minimize PIE sessions during testing
- Close unnecessary Unreal Editor windows

## Tool-Specific Notes

### Asset Management Tests
- Requires valid file paths on your system
- Import times vary by asset size
- Some operations may prompt for user confirmation

### Actor Tests
- Creates actors in current level
- Remember to clean up test actors
- Use unique actor labels to avoid conflicts

### Editor Tests
- PIE state affects other tests
- Screenshots save to project's Saved folder
- Console commands persist between tests

### Level Tests
- Save prompt may block tests
- Streaming requires sub-levels to exist
- Level paths must be valid package paths

### Animation Tests
- Requires SkeletalMeshActor in scene
- Animation assets must exist in project
- Skeletal mesh must have animation support

### Blueprint Tests
- Requires Blueprint actors in scene
- Functions must be callable from editor
- Variables must be exposed/public

### Sequence Tests
- Creates sequences in Content Browser
- Export requires Movie Render Queue plugin
- Keyframes stored in sequence asset

### Performance Tests
- Results vary by hardware
- Some stats require specific console commands
- Profiling may impact frame rate

## Exit Codes

- **0**: All tests passed
- **1**: One or more tests failed or connection error

Use exit codes in CI/CD pipelines:
```bash
npm run test:manage_asset && npm run test:control_actor && npm run test:control_editor
```

## Contributing

### Adding a New Tool Test File

1. **Create test file**: `tests/test-newtool.mjs`
```javascript
#!/usr/bin/env node
import { runToolTests } from './test-runner.mjs';

const testCases = [
  // Your test cases here
];

await runToolTests('New Tool', testCases);
```

2. **Add npm script**: In `package.json`
```json
"test:newtool": "node tests/test-newtool.mjs"
```

3. **Update this guide**: Add tool to the list above

## FAQ

**Q: Can I run all tests at once?**  
A: Not recommended. Run individual tools for focused testing. If needed, create a script that runs them sequentially.

**Q: How do I skip a test?**  
A: Comment out the test case or remove it from the array temporarily.

**Q: Can I run tests without Unreal running?**  
A: No, tests require active Unreal Engine connection. They will fail with `UE_NOT_CONNECTED` error.

**Q: How do I test custom tools?**  
A: Create a new test file following the structure, add test cases with your tool's name and actions.

**Q: Are tests destructive?**  
A: Some tests create/delete assets and actors. Use a test project or backup your work.

**Q: Can I run tests in CI/CD?**  
A: Yes, if you can run Unreal Engine headless with the MCP plugin enabled. Most tests require active editor.

## Support

For issues with:
- **MCP Server**: Check server logs, ensure plugins enabled
- **Test Framework**: Review test-runner.mjs and test file structure  
- **Specific Tool**: Check tool implementation in `src/tools/<toolname>.ts`
- **Unreal Integration**: Review MCP plugin logs in Unreal Output Log

## Next Steps

1. **Build the server**: `npm run build`
2. **Start Unreal Engine 5.6**
3. **Run your first test**: `npm run test:system`
4. **Review the report**: Check `tests/reports/` folder
5. **Test other tools**: Run `npm run test:<toolname>` for each tool you use
