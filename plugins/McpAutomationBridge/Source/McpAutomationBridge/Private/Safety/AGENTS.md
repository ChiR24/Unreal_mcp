# Safety Wrappers (`Private/Safety/`)

Hazardous UE editor APIs live here behind wrappers. Every domain handler that saves, loads, or deletes must call these wrappers, never the raw API. Umbrella header `McpSafeOperations.h` includes the editor-gated subset. Everything sits in namespace `McpSafeOperations`; log via `LogMcpSafeOperations` (`McpSafeOperationsLog.h`).

## WRAPPER TABLE

| Wrapper | Replaces | Prevents | Header |
|---------|----------|----------|--------|
| `McpSafeAssetSave` | `UPackage::SavePackage`, `FileHelpers::SaveDirtyPackages` | Picks the correct `RF_Public`/`RF_Standalone` asset object, retries, refuses transient/unsaved packages. Prevents corrupt saves and editor crashes. | `McpSafeOperationsAssetSave.h` |
| `McpSafeLevelSave` | `FEditorFileUtils::SaveMap` | Rejects `/Temp/`, `/Engine/Transient`, `Untitled` (transient levels cannot be saved). Prevents silent data loss. | `McpSafeOperationsLevelSave.h` |
| `McpSafeLoadMap` + `ResolveExpectedMapPackageName` | raw `UEngine::LoadMap` | Closes asset editors, unloads the prior world, defers GC. Prevents GC-vs-load races / editor lockup. | `McpSafeOperationsMapLoad.h` |
| `McpLoadMaterialWithFallback` | bare `LoadObject<UMaterialInterface>` | Falls back to `/Engine/EngineMaterials/*`. Prevents null-material crashes. | `McpSafeOperationsMaterial.h` |
| `DeleteWorldPackagesByPath` | direct `.umap` file delete | Unloads loaded packages first. Prevents dangling references. | `McpSafeOperationsWorldDelete.h` |
| `McpSafeDeleteFolder` (+`FolderDeleteAssets.h`/`FolderDeleteVerify.h`) | `UEditorAssetLibrary::DeleteDirectory` | Partitions world vs non-world assets, switches away from open worlds, verifies afterwards. | `McpSafeOperationsFolderDelete.h` |
| `PrepareAssetBatchForDelete` (+`McpSafeOperationsAssetClassification.h`) | ad-hoc batch delete | Separates file-backed vs in-memory-only, classifies world assets. Prevents partial deletes / orphaned files. | `McpSafeOperationsAssetDeletePreparation.h` |
| `UnloadLoadedPackagesForAssets` (+`McpSafeOperationsAssetEditorSubsystem.h`) | delete-while-loaded | Quiesces compilation and the asset-editor subsystem. Prevents delete-while-compiling crashes. | `McpSafeOperationsDeleteCompilation.h` |

## WHEN TO ADD A NEW WRAPPER

Wrap a raw API when any of these hold:
- It can **crash** the editor (raw save/GC/map transition, delete while compiling).
- It can **corrupt** data (wrong object flags, unsaved package written).
- It can **lose** data silently (transient level saved, partial folder delete).
- It needs **pre/post verification** (unload before delete, verify-after-delete, fallback material).

If the raw call is safe and stateless, leave it alone. Do not wrap for style.

## CONVENTIONS

- All wrappers run on the game thread via the Core queue. Never call them from socket threads.
- Each wrapper owns its verification step (unload check, presence check, post-delete verify). Preserve it.
- Use `LogMcpSafeOperations` for all diagnostics; never strip a wrapper's verification to "simplify".
- 250 pure-line ceiling and ≤25 files per folder apply here too.

## AUTOMATED ENFORCEMENT

`UPackage::SavePackage` is **machine-enforced forbidden** in plugin source. Vitest reads the C++ text: `tests/unit/plugin/instanced_struct_contracts.test.ts` and `inspect_struct_contracts.test.ts` (plus the enum/datatable/native-discovery contract tests) assert its absence. A raw call fails CI. This is not advisory.

## ANTI-PATTERNS

- Calling `UPackage::SavePackage`, `FEditorFileUtils::SaveMap`, `LoadObject<UMaterialInterface>`, or `UEditorAssetLibrary::DeleteDirectory` directly from a domain handler.
- Hand-rolling save/load/delete outside `Private/Safety/`.
- Removing verification/cleanup from a wrapper to shorten it.
- Calling these wrappers from a socket/worker thread instead of the game-thread queue.
- Importing a raw UE save/delete symbol when a wrapper exists for the same job.
