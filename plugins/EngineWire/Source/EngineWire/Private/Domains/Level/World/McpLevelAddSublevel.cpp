#include "Domains/Level/McpLevelActions.h"
#include "Domains/Level/Lifecycle/McpLevelPathSafety.h"

#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace McpLevelHandlers {
bool HandleAddSublevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString SubLevelPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("subLevelPath"), SubLevelPath);
    if (SubLevelPath.IsEmpty() && Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelPath"), SubLevelPath);

    if (SubLevelPath.IsEmpty()) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("subLevelPath required"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    const FString SubLevelPathAsGiven = SubLevelPath;
    // An object path (/Game/Maps/Sub.Sub) or a .umap name reads as its package: it was refused as a missing file.
    SubLevelPath = NormalizeLevelPackagePath(SanitizeProjectRelativePath(SubLevelPath));
    if (SubLevelPath.IsEmpty()) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
                          McpPathRefusalMessage(TEXT("subLevelPath"), SubLevelPathAsGiven),
                          TEXT("SECURITY_VIOLATION"));
      return true;
    }

    // Any other method used to become Blueprint while the reply echoed what was asked.
    FString StreamingMethod = TEXT("Blueprint");
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("streamingMethod"), StreamingMethod);
    const bool bAlwaysLoaded = StreamingMethod.Equals(TEXT("AlwaysLoaded"), ESearchCase::IgnoreCase);
    if (!bAlwaysLoaded && !StreamingMethod.Equals(TEXT("Blueprint"), ESearchCase::IgnoreCase)) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
          FString::Printf(TEXT("streamingMethod %s is not one of Blueprint (streamed in and out by the game) or AlwaysLoaded"), *StreamingMethod),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    StreamingMethod = bAlwaysLoaded ? TEXT("AlwaysLoaded") : TEXT("Blueprint");

    // Robustness: Cleanup before adding
    if (GEditor) {
      GEditor->ForceGarbageCollection(true);
    }

    // Verify file existence (more robust than DoesPackageExist for new files)
    FString Filename;
    bool bFileFound = false;
    if (FPackageName::TryConvertLongPackageNameToFilename(
            SubLevelPath, Filename, FPackageName::GetMapPackageExtension())) {
      if (IFileManager::Get().FileExists(
              *FPaths::ConvertRelativePathToFull(Filename))) {
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
        Subsystem.SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Level file not found: %s"), *SubLevelPath),
            nullptr, TEXT("PACKAGE_NOT_FOUND"));
        return true;
      }
    }

    if (!GEditor) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor unavailable"), nullptr,
                             TEXT("NO_EDITOR"));
      return true;
    }

    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No world loaded"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }

    // A sub-level can only be added to the persistent level open in the
    // editor. parentLevel names that parent; a different one is refused rather
    // than silently adding the sub-level to whatever happens to be loaded.
    FString ParentLevel;
    Payload->TryGetStringField(TEXT("parentLevel"), ParentLevel);
    ParentLevel = NormalizeLevelPackagePath(ParentLevel);
    const FString OpenLevel = World->GetOutermost()->GetName();
    if (!ParentLevel.IsEmpty() && !ParentLevel.Equals(OpenLevel, ESearchCase::IgnoreCase)) {
      Subsystem.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("parentLevel %s is not the level open in the editor (%s); load it with manage_level load first, then add the sub-level"),
                          *ParentLevel, *OpenLevel),
          nullptr, TEXT("PARENT_LEVEL_NOT_LOADED"));
      return true;
    }

    // CRITICAL FIX: Check if sublevel is already in the world BEFORE trying to add it
    // This prevents "A level with that name already exists in the world" modal dialog
    // which blocks execution and causes test timeouts
    // BUT: Also check if the existing level is actually loaded/valid
    for (ULevelStreaming* ExistingStreamingLevel : World->GetStreamingLevels()) {
      if (ExistingStreamingLevel) {
        if (ExistingStreamingLevel->GetWorldAssetPackageName().Equals(SubLevelPath, ESearchCase::IgnoreCase)) {
          // Check if the existing streaming level is actually valid/loaded
          // If it failed to load (file doesn't exist), it's a broken reference
          ULevel* ExistingLevel = ExistingStreamingLevel->GetLoadedLevel();
          bool bIsValidStreaming = ExistingLevel != nullptr ||
                                   ExistingStreamingLevel->IsStreamingStatePending();

          if (bIsValidStreaming) {
            // Already there: the reply names the method it streams with, which may not be the one asked for.
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("subLevelPath"), SubLevelPath);
            Result->SetStringField(TEXT("world"), World->GetName());
            Result->SetStringField(TEXT("streamingMethod"),
                ExistingStreamingLevel->IsA<ULevelStreamingAlwaysLoaded>() ? TEXT("AlwaysLoaded") : TEXT("Blueprint"));
            Result->SetBoolField(TEXT("alreadyExists"), true);
            Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
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

    UClass *StreamingClass = bAlwaysLoaded ? ULevelStreamingAlwaysLoaded::StaticClass()
                                           : ULevelStreamingDynamic::StaticClass();

    ULevelStreaming *NewLevel = UEditorLevelUtils::AddLevelToWorld(
        World, *SubLevelPath, StreamingClass);
    if (NewLevel) {
      // AddLevelToWorld loads the level before it returns (RefreshStreamingLevels), so a null level here is a
      // file that did not load; the sleep that used to sit here only held the game thread for 100 ms.
      ULevel* LoadedLevel = NewLevel->GetLoadedLevel();
      bool bIsPendingLoad = NewLevel->IsStreamingStatePending();

      // If level is loaded or pending, it's a valid streaming level
      if (LoadedLevel || bIsPendingLoad) {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("subLevelPath"), SubLevelPath);
        Result->SetStringField(TEXT("world"), World->GetName());
        Result->SetStringField(TEXT("streamingMethod"), StreamingMethod);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Sublevel added successfully"), Result);
      } else {
        // The level file did not load: take back the entry AddLevelToWorld made, so a refused call leaves
        // no broken sublevel in the open level.
        World->RemoveStreamingLevel(NewLevel);
        Subsystem.SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Sublevel file not found: %s"), *SubLevelPath),
            nullptr, TEXT("FILE_NOT_FOUND"));
      }
    } else {
      Subsystem.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          FString::Printf(TEXT("Failed to add sublevel %s (Check logs)"),
                          *SubLevelPath),
          nullptr, TEXT("ADD_FAILED"));
    }
    return true;
}
} // namespace McpLevelHandlers
