# Changelog

All notable changes to the MCP Automation Bridge plugin will be documented in this file.

---

## [0.1.4] - 2026-04-03

### Security
- Command injection fixes in bump-version action and editor tools with mixed-context sanitization (#327, #322)
- Path traversal fixes in `export_level` action and screenshot filenames (#305)
- Replaced synchronous file operations with async to prevent blocking (#318)

### Added
- Custom content mount points via `MCP_ADDITIONAL_PATH_PREFIXES` environment variable (#326)
- New `manage_project_settings` tool for runtime project configuration
- Audio authoring capabilities: sound wave creation, sound cues, MetaSounds, attenuation settings
- Success flags in audio asset creation responses
- Optional plugin dependencies: IKRig, ChaosVehiclesPlugin, AnimationData

### Fixed
- UE 5.0 API incompatibilities in IK Rig and widget authoring
- Crash when deleting animation/rig assets on UE 5.7+ (9ea2db4)
- Folder deletion crashes with safe deletion implementation (f0f4e44, ed56353)
- Widget creation crash (#306)
- Asset loading reliability for newly created AI assets (bb5e3bb)
- Asset query parameter bugs and expanded classNames support (#311)
- Replaced custom asset directory checks with `UEditorAssetLibrary` to avoid stale cache
- Fixed searchText filtering in `search_assets` action (4b1cb0e)
- Unified pin serialization across blueprint graph handlers (#309, 10f8f2b)
- Actor lookup to match subsystem behavior (checks both label and name)
- Console command settings delegated to C++ handler for performance
- Delay-load for optional plugin modules to prevent missing dependency errors (#317)
- IK retargeter initialization using controller API (UE 5.7+) with backward compatibility
- Rate limiting defaults and missing GraphQL heading in docs (d023284)
- `get_ai_info` schema alignment (#310)

### Dependencies
- `github/codeql-action` 4.33.0 → 4.34.1
- `picomatch` 4.0.3 → 4.0.4

---

## [0.1.3] - 2026-03-21

### Security
- Path traversal fix in `export_asset` action to prevent directory traversal attacks

### Added
- External actors support for World Partition in level structure handlers
- Streaming reference creation for external actor packages

### Fixed
- UE 5.0 compatibility using `bIsWorldInitialized` direct access
- Tick task manager crashes during world operations with proper cleanup
- World cleanup issues with `FlushRenderingCommands` safety
- Sublevel creation process with enhanced path handling
- Missing includes for UE 5.7 build (contributed by @a2448825647)

### Changed
- Enhanced `McpAutomationBridgeHelpers.h` with additional safety helpers
- Improved `McpSafeOperations.h` for safer world operations

---

## [0.1.2] - 2026-03-18

### Security
- Command injection prevention via semicolon sanitization in all user inputs
- Path traversal fixes in validateSnapshotPath and asset handlers
- Blueprint creation savePath sanitization to prevent traversal attacks

### Added
- `McpAutomationBridge_ConsoleCommandHandlers.cpp` - Batch and single command execution (302 lines)
- `McpHandlerUtils.h/cpp` - Standardized JSON response builders (1,900 lines)
- `McpPropertyReflection.h/cpp` - Property reflection utilities (1,356 lines)
- `McpSafeOperations.h` - Safe asset/level save for UE 5.7 (659 lines)
- `McpVersionCompatibility.h` - UE 5.0-5.7 API compatibility macros (225 lines)
- `McpHandlerDeclarations.h` - Forward declarations (844 lines)
- Debug visualization shapes for better testing feedback
- `list_objects`, `set_property`, `get_property` actions to control handlers

### Fixed
- EditorFunctionHandlers: use-after-free bug
- EffectHandlers: truncated condition + missing braces
- InventoryHandlers: duplicate TArray with undefined variables
- MaterialAuthoringHandlers: duplicate include + missing UE 5.0 fallback
- NavigationHandlers: case-sensitivity error
- SkeletonHandlers: duplicate verification + redundant code + duplicate parsing
- WidgetAuthoringHandlers: unreachable code block
- Volume attachment to movable actors by checking mobility
- World memory leaks in UE 5.7 by properly cleaning up created worlds
- Texture property modification errors using PreEditChange/PostEditChange lifecycle
- Blueprint loading to properly find in-memory blueprints first
- Level save/load operations for correct package name matching
- GeometryScript AppendCapsule segment steps for UE 5.5+ compatibility

### Changed
- Complete deep-level refactoring of 57 handler files with line-by-line review
- Centralized utility infrastructure for consistent error handling
- UE 5.0-5.7 cross-version compatibility with API abstraction macros
- All handlers now use standardized response builders

### Compatibility
- Unreal Engine 5.0 - 5.7
- Platforms: Win64, Mac, Linux

---

## [0.1.1] - 2026-02-16

### Added
- 200+ automation action handlers across all domains (AI, Combat, Character, Inventory, GAS, Audio, Materials, Textures, Levels, Volumes, Performance, Input)
- Progress heartbeat protocol for long-running operations
- Dynamic tool management via `manage_tools` MCP tool
- IPv6 support with hostname resolution and zone ID handling
- TLS/SSL support for secure WebSocket connections
- Per-connection rate limiting (600 messages/min, 120 automation requests/min)
- Handler verification metadata in responses (actor/asset/component identity)

### Security
- Path validation helpers: `SanitizeProjectRelativePath`, `SanitizeProjectFilePath`, `ValidateAssetCreationPath`
- Input sanitization for asset names and paths
- Loopback-only binding by default
- Handshake required before automation requests
- Command validation blocks dangerous console commands

### Fixed
- Landscape handler silent fallback bug (now returns `LANDSCAPE_NOT_FOUND` error)
- Rotation yaw bug in lighting handlers
- Integer overflow in heightmap operations (int16 → int32)
- Intel GPU crash prevention with `McpSafeLevelSave` helper
- UE 5.7 compatibility (GetProtocolType API, SCS save, Niagara graph init)

### Compatibility
- Unreal Engine 5.0 - 5.7
- Platforms: Win64, Mac, Linux

---

## [0.1.0] - 2025-12-01

### Added
- Initial release
- WebSocket-based automation bridge
- Core automation handlers for assets, actors, levels
- Blueprint graph editing support
- Niagara authoring support
- Animation and physics handlers

---

For full MCP server changelog, see: https://github.com/ChiR24/Unreal_mcp/blob/main/CHANGELOG.md
