## Summary

This PR implements **Phase 31: Data & Persistence** from the project Roadmap. It introduces a new canonical tool `manage_data` to handle operations related to Data Assets, Save Systems, Gameplay Tags, and Configs, aligning with the consolidated 22+ tools strategy from Phase 53.

## Changes

- **Added TypeScript schema & definitions**: Created `manage-data-tool.ts` exposing over 25+ subActions for data & persistence.
- **Added TypeScript dispatcher**: Implemented `data-handlers.ts` to forward `manage_data` commands with context validation.
- **Updated Tool Registry**: Registered `manage_data` into `all-tool-definitions.ts`, `consolidated-routing.ts` and `consolidated-handler-registration.ts`.
- **Added C++ Native Handlers**: Created `McpAutomationBridge_DataHandlers.h` and `.cpp` covering implementation for `UGameplayStatics`, `GConfig` and `UGameplayTagsManager`.
- **Updated Native Subsystem**: Wired the new native C++ data handlers into `McpAutomationBridgeSubsystemHandlerRegistration.cpp`.
- **Added Integration Tests**: Implemented test cases in `manage-data.test.mjs` to ensure the TypeScript side successfully parses and issues commands.

## Related Issues

Resolves Phase 31: Data & Persistence

## Type of Change

- [ ] 🐛 Bug fix (non-breaking change that fixes an issue)
- [x] ✨ New feature (non-breaking change that adds functionality)
- [ ] 💥 Breaking change (fix or feature that would cause existing functionality to change)
- [ ] 📚 Documentation update
- [ ] 🔧 Configuration/build change
- [ ] ♻️ Refactoring (no functional changes)
- [x] 🧪 Test addition/update

## Testing

- [ ] Tested with Unreal Engine (version: ___)
- [x] Tested MCP client integration (client: `Test Runner`)
- [x] Added/updated tests

## Pre-Merge Checklist

- [x] Code follows project style guidelines
- [x] Self-reviewed the code
- [ ] Updated relevant documentation (if needed)
- [x] Added/updated tests (if applicable)
- [x] CI passes
