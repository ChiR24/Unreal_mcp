## Summary

This PR implements the requested features for Phase 34: Editor Utilities. It expands the functionality of the existing tools (`control_editor`, `manage_asset`, and `control_actor`) and introduces a new standalone tool (`manage_project_settings`) to handle project configurations cleanly. The implementation natively maps these commands into the Unreal Engine editor via the McpAutomationBridge.

## Changes

- ✨ Created new `manage_project_settings` tool for collision and physical material configurations (`manage_collisions`, `manage_physical_materials`).
- ✨ Added grid, snap, layout, and editor utility widget actions to `control_editor`.
- ✨ Added Content Browser actions (navigation, sync, collections, coloration) to `manage_asset`.
- ✨ Added selection, grouping, and camera focus functionality to `control_actor`.
- 🔧 Updated `smoke-test.ts` to accommodate 25 functional tools.
- ♻️ Registered C++ native definitions for `McpTool_ManageProjectSettings` and connected the dispatcher subsystems (`McpAutomationBridge_ControlEditorUtilities`, `McpAutomationBridge_AssetWorkflowBrowser`, `McpAutomationBridge_ControlActorSelection`).

## Related Issues

Related to #467

## Type of Change

- [ ] 🐛 Bug fix (non-breaking change that fixes an issue)
- [x] ✨ New feature (non-breaking change that adds functionality)
- [ ] 💥 Breaking change (fix or feature that would cause existing functionality to change)
- [ ] 📚 Documentation update
- [ ] 🔧 Configuration/build change
- [x] ♻️ Refactoring (no functional changes)
- [x] 🧪 Test addition/update

## Testing

- [x] Tested with Unreal Engine (version: 5.0-5.8 Preview)
- [x] Tested MCP client integration (client: Mock Smoke Test runner)
- [x] Added/updated tests

## Pre-Merge Checklist

- [x] Code follows project style guidelines
- [x] Self-reviewed the code
- [x] Updated relevant documentation (if needed)
- [x] Added/updated tests (if applicable)
- [x] CI passes
