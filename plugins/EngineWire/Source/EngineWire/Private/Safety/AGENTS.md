# Safety Wrappers (`Private/Safety/`)

Hazardous UE editor APIs live here behind wrappers. Every domain handler that saves, loads, or deletes must call these wrappers, never the raw API. Umbrella header `McpSafeOperations.h` includes the editor-gated subset. The wrappers sit in namespace `McpSafeOperations` (the render guard's two functions are global, `McpMovieRender...`); log via `LogMcpSafeOperations` (`McpSafeOperationsLog.h`).

## WRAPPER TABLE

| Wrapper | Replaces | Prevents | Header |
|---------|----------|----------|--------|
| `McpSafeAssetSave` | `UPackage::SavePackage`, `FileHelpers::SaveDirtyPackages` | Picks the correct `RF_Public`/`RF_Standalone` asset object, retries, refuses transient/unsaved packages and all engine content (`/Engine/`, shared by every project). Prevents corrupt saves and editor crashes. A handler that edits an asset in place still refuses engine content itself (`ENGINE_CONTENT`) before changing it in memory. | `McpSafeOperationsAssetSave.h` |
| `McpSafeLevelSave` | `FEditorFileUtils::SaveMap` | Rejects `/Temp/`, `/Engine/Transient`, `Untitled` (transient levels cannot be saved). Prevents silent data loss. | `McpSafeOperationsLevelSave.h` |
| `McpSafeLoadMap` + `ResolveExpectedMapPackageName` | raw `UEngine::LoadMap` | Closes asset editors, unloads the prior world, defers GC. Prevents GC-vs-load races / editor lockup. | `McpSafeOperationsMapLoad.h` |
| `McpLoadMaterialWithFallback` | a null material where one is required | For code that must end with SOME material: falls back to `/Engine/EngineMaterials/*`. Not for a material the caller named: that one is loaded directly and a missing one refused, since a fallback would hide the error. | `McpSafeOperationsMaterial.h` |
| `DeleteWorldPackagesByPath` | direct `.umap` file delete | Unloads loaded packages first. Prevents dangling references. | `McpSafeOperationsWorldDelete.h` |
| `McpDeleteAssetAndFile` | bare `UEditorAssetLibrary::DeleteAsset` | The engine delete can succeed and leave the `.uasset` (a Blueprint loaded from disk came back on the next start). Removes a surviving file only once its package has unloaded; true only when the file is gone. | `McpSafeOperationsAssetDelete.h` |
| `McpSafeDeleteFolder` (+`FolderDeleteAssets.h`/`FolderDeleteVerify.h`) | `UEditorAssetLibrary::DeleteDirectory` | Partitions world vs non-world assets, switches away from open worlds, verifies afterwards. | `McpSafeOperationsFolderDelete.h` |
| `IsAssetEditorOpen` + `OpenAssetEditorRefusal` | a direct write to an asset whose editor is open | That editor holds its own copy and writes it back over the asset when it closes: the write is reported, shown, then discarded. Refuse the write instead. | `McpSafeOperationsOpenEditorGuard.h` |
| `McpMovieRenderRunning` + `McpMovieRenderRunningMessage` | saving everything or restarting the editor during a Movie Render Queue job | A render keeps its sequence in a temporary state until the job ends; a save writes that state to disk and a restart cuts the render off. | `McpSafeOperationsRenderGuard.h` |
| `UnloadLoadedPackagesForAssets` | delete-while-loaded | Quiesces compilation and the asset-editor subsystem. Prevents delete-while-compiling crashes. | `McpSafeOperationsDeleteCompilation.h` |
| `FMcpEnumUndo` | `Enum->Modify()` inside a transaction (and the engine's enum edits, which call it) | Records an enum edit as one `FCommandChange` (values and reflected properties before and after, applied with `SetEnums`), the enum not transactional while the edit runs. From UE 5.7 applying a step that holds an enum's saved state asserts in `UEnum::FNameData::Initialize` and takes the editor down; `McpEnumChangeSteps()` lets the undo guard pass the steps it recorded. | `McpSafeOperationsEnumUndo.h` |

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

`UPackage::SavePackage` is **machine-enforced forbidden** in plugin source. Vitest reads the C++ text: `tests/unit/plugin/security_contracts.test.ts` scans every plugin source and fails if any file contains `UPackage::SavePackage`; the wrappers save through other engine paths. A raw call fails CI. This is not advisory.

## ANTI-PATTERNS

- Calling `UPackage::SavePackage`, `FEditorFileUtils::SaveMap` or `UEditorAssetLibrary::DeleteDirectory` directly from a domain handler.
- Reaching for `McpLoadMaterialWithFallback` when the caller named the material: load it and refuse a missing one.
- Hand-rolling save/load/delete outside `Private/Safety/`.
- Removing verification/cleanup from a wrapper to shorten it.
- Calling these wrappers from a socket/worker thread instead of the game-thread queue.
- Importing a raw UE save/delete symbol when a wrapper exists for the same job.
