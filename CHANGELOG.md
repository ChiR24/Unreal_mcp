# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Added
- **UE 5.7 Compatibility**: Updated McpAutomationBridge for UE 5.7, including ControlRig dynamic loading and improved sequence handling (ec5409b)
- **GraphQL API**: Broadened automation bridge with GraphQL API support, WASM integration, UI/editor integrations, and extended tools + tests (ffdd814)
- **WebAssembly Integration**: High-performance JSON parsing and initialization (23f63c7); integrated into bridge and validator for 5-8x perf gains
- **Automation Bridge Enhancements**: Server mode on port 8091, client connections, heartbeat tracking, output capturing (bf0fa56, 267aa42, 28242e1)
- **New Handlers & Tools**: Blueprint/Niagara functionalities, asset management features, event handling (aff4d55, d10e1e2)
- **Comprehensive Test Suites**: Added/enhanced tests for animation, assets, materials, sequences, world partition, and more (31c6db9, 85817c9, fc47839)

### Changed
- **Refactors & Improvements**:
  - Test runner enhancements for timeouts/content extraction (c9766b0)
  - PhysicsTools vehicle config logic (6dba9f7)
  - AnimationTools logging/response normalization (7666c31)
  - Error handling utilities, INI reader (f5444e4)
  - Automation dispatch, editor-native handlers modernization (c9db1a4)
  - Removed Python fallbacks, AutomationBridge-first architecture (fe65968)
  - SequenceTools to use Automation Bridge (c2fb1 5a)
  - Blueprint actions, timeout handling (65d2738)
- **Connection & Logging**: Improved UnrealBridge connection/command execution, asset error messages (7bd48d8, 41350b3)
- **Docs & Configs**: README.md updates, Roadmap, handler mappings, testing guide (8d72f28, local changes)

### Fixed
- McpAutomationBridgeSubsystem refactors: header removal, logging category, heartbeat methods (498f644)
- McpBridgeWebSocket implementation (861ad91)

### Documentation
- Updated README.md with WASM integration documentation
- Added WebAssembly Performance section with setup instructions

## [0.4.6] - 2025-10-04
### Fixed
- Fixed duplicate response output issue where tool responses were being displayed twice in MCP content
- Response validator now emits concise summaries in text content instead of duplicating full JSON payloads
- Structured content is preserved for validation and tests while user-facing output is streamlined

## [0.4.5] - 2025-10-03
### Added
- Expose `UE_PROJECT_PATH` environment variable across runtime config, Smithery manifest, and client example configs. This allows tools that need an absolute .uproject path (e.g., engine_start) to work without additional manual configuration.
- Added `projectPath` to the runtime `configSchema` so Smithery's session UI can inject a project path into the server environment.

### Changed
- Make `createServer` a synchronous factory (removed `async`) and updated `createServerDefault` and `startStdioServer` to use the synchronous factory. This aligns the exported default with Smithery’s expectations and prevents auto-start mismatches in the bundled output.
- Provide a default for `ueHost` in the exported `configSchema` so the Smithery configuration dialog pre-fills the host input.

### Documentation
- Updated `README.md`, `claude_desktop_config_example.json`, and `mcp-config-example.json` to include `UE_PROJECT_PATH` and usage notes.
- Updated `smithery.yaml` and `server.json` manifest to declare `UE_PROJECT_PATH` and default values.

### Build
- Rebuilt the Smithery bundle and TypeScript output to ensure schema and defaults are exported in the distributed artifact.

### Fixes
- Fixes Smithery UI blank ueHost field by defining a default in the runtime schema.


## [0.4.4] - 2025-09-30
- Previous release notes retained in upstream repo.
