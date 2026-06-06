#include "McpAutomationBridge_LevelHandlersActions.h"

#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleAddSublevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString SubLevelPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("subLevelPath"), SubLevelPath);
    if (SubLevelPath.IsEmpty() && Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelPath"), SubLevelPath);

    if (SubLevelPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("subLevelPath required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    SubLevelPath = SanitizeProjectRelativePath(SubLevelPath);
    if (SubLevelPath.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Invalid subLevelPath"),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }

    // Robustness: Cleanup before adding
    if (GEditor) {
      GEditor->ForceGarbageCollection(true);
    }

    // Verify file existence (more robust than DoesPackageExist for new files)
    FString Filename;
    bool bFileFound = false;
    if (FPackageName::TryConvertLongPackageNameToFilename(
            SubLevelPath, Filename, FPackageName::GetMapPackageExtension())) {
      if (IFileManager::Get().FileExists(*Filename)) {
        bFileFound = true;
      }
    }

    // Fallback: Check without conversion if it's already a file path?
    if (!bFileFound && IFileManager::Get().FileExists(*SubLevelPath)) {
      bFileFound = true;
    }

    if (!bFileFound) {
      // Try checking DoesPackageExist as last resort
      if (!FPackageName::DoesPackageExist(SubLevelPath)) {
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Level file not found: %s"), *SubLevelPath),
            nullptr, TEXT("PACKAGE_NOT_FOUND"));
        return true;
      }
    }

    FString StreamingMethod = TEXT("Blueprint");
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("streamingMethod"), StreamingMethod);

    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor unavailable"), nullptr,
                             TEXT("NO_EDITOR"));
      return true;
    }

    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No world loaded"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }

    // CRITICAL FIX: Check if sublevel is already in the world BEFORE trying to add it
    // This prevents "A level with that name already exists in the world" modal dialog
    // which blocks execution and causes test timeouts
    // BUT: Also check if the existing level is actually loaded/valid
    for (ULevelStreaming* ExistingStreamingLevel : World->GetStreamingLevels()) {
      if (ExistingStreamingLevel) {
        FString ExistingPath = ExistingStreamingLevel->GetWorldAssetPackageName();
        // Compare normalized paths (without .umap extension)
        FString NormalizedExisting = ExistingPath;
        FString NormalizedNew = SubLevelPath;
        if (NormalizedExisting.EndsWith(TEXT(".umap"))) {
          NormalizedExisting = NormalizedExisting.LeftChop(5);
        }
        if (NormalizedNew.EndsWith(TEXT(".umap"))) {
          NormalizedNew = NormalizedNew.LeftChop(5);
        }
        if (NormalizedExisting.Equals(NormalizedNew, ESearchCase::IgnoreCase)) {
          // Check if the existing streaming level is actually valid/loaded
          // If it failed to load (file doesn't exist), it's a broken reference
          ULevel* ExistingLevel = ExistingStreamingLevel->GetLoadedLevel();
          bool bIsValidStreaming = ExistingLevel != nullptr ||
                                   ExistingStreamingLevel->IsStreamingStatePending();

          if (bIsValidStreaming) {
            // Sublevel already exists and is valid - return success with info
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("sublevelPath"), SubLevelPath);
            Result->SetStringField(TEXT("world"), World->GetName());
            Result->SetBoolField(TEXT("alreadyExists"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   FString::Printf(TEXT("Sublevel already in world: %s"), *SubLevelPath), Result);
            return true;
          } else {
            // Existing streaming level is broken (failed to load)
            // Remove it and continue to add the new one
            UE_LOG(LogTemp, Warning, TEXT("add_sublevel: Removing broken streaming level reference: %s"), *SubLevelPath);
            World->RemoveStreamingLevel(ExistingStreamingLevel);
            break;  // Exit the loop to continue adding
          }
        }
      }
    }

    // Determine streaming class
    UClass *StreamingClass = ULevelStreamingDynamic::StaticClass();
    if (StreamingMethod.Equals(TEXT("AlwaysLoaded"), ESearchCase::IgnoreCase)) {
      StreamingClass = ULevelStreamingAlwaysLoaded::StaticClass();
    }

    ULevelStreaming *NewLevel = UEditorLevelUtils::AddLevelToWorld(
        World, *SubLevelPath, StreamingClass);
    if (NewLevel) {
      // CRITICAL FIX: Verify the streaming level can actually be loaded
      // AddLevelToWorld() creates the streaming level object but doesn't verify
      // the level file exists. Check if the level loaded successfully.
      // Give the engine a moment to attempt loading
      FlushRenderingCommands();
      FPlatformProcess::Sleep(0.1f);

      // Check if the level is actually loaded or pending load
      // If the level file doesn't exist, GetLoadedLevel() will be null and
      // the streaming state will not be pending
      ULevel* LoadedLevel = NewLevel->GetLoadedLevel();
      bool bIsPendingLoad = NewLevel->IsStreamingStatePending();

      // If level is loaded or pending, it's a valid streaming level
      if (LoadedLevel || bIsPendingLoad) {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("sublevelPath"), SubLevelPath);
        Result->SetStringField(TEXT("world"), World->GetName());
        Result->SetStringField(TEXT("streamingMethod"), StreamingMethod);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Sublevel added successfully"), Result);
      } else {
        // CRITICAL FIX: Level file doesn't exist - return ERROR not success with warning
        // The streaming level was added to the world but the level file doesn't exist
        // This is an error condition, not a warning
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Sublevel file not found: %s"), *SubLevelPath),
            nullptr, TEXT("FILE_NOT_FOUND"));
      }
    } else {
      // Did we fail because it's already there?
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Failed to add sublevel %s (Check logs)"),
                          *SubLevelPath),
          nullptr, TEXT("ADD_FAILED"));
    }
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
