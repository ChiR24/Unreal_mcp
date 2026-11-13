#include "Domains/Level/McpLevelActions.h"
#include "Domains/Level/Lifecycle/McpLevelDirtyPackageLoad.h"

#include "Algo/Count.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

namespace McpLevelHandlers {
bool HandleGetCurrentLevelAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    UWorld* EditorWorld = McpHandlerUtils::GetEditorWorld();
    if (!EditorWorld) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("No editor world available"), TEXT("NO_WORLD"));
      return true;
    }

    ULevel* CurrentLevel = EditorWorld->GetCurrentLevel();
    if (!CurrentLevel) {
      Subsystem.SendAutomationError(RequestingSocket, RequestId,
                             TEXT("No current level available"), TEXT("NO_LEVEL"));
      return true;
    }

    const FString LevelPath = CurrentLevel->GetOutermost()->GetName();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("mapName"), EditorWorld->GetMapName());
    Result->SetStringField(TEXT("levelName"), FPackageName::GetShortName(LevelPath));
    Result->SetStringField(TEXT("levelPath"), LevelPath);
    // The editor's "current level" can be a streaming sub-level; also publish
    // the persistent map so callers do not mistake one for the other.
    Result->SetStringField(TEXT("persistentLevelPath"), EditorWorld->GetOutermost()->GetName());
    Result->SetStringField(TEXT("worldType"), LexToString(EditorWorld->WorldType));
    // Actors.Num() also counted the empty slots a Blueprint recompile leaves behind (36 extra after one).
    Result->SetNumberField(TEXT("actorCount"), static_cast<int32>(Algo::CountIf(CurrentLevel->Actors, [](const AActor *A) { return IsValid(A); })));
    Result->SetBoolField(TEXT("isPersistentLevel"), CurrentLevel == EditorWorld->PersistentLevel);
    // The capability's declared contract promises `loaded`; the current level is
    // loaded by definition, so it is stated rather than left absent.
    Result->SetBoolField(TEXT("loaded"), true);
    AddUnsavedState(Result, CurrentLevel);

    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Current level retrieved"), Result);
    return true;
}
} // namespace McpLevelHandlers
