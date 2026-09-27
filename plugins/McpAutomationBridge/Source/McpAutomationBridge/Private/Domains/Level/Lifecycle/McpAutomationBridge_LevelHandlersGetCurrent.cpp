#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

namespace McpLevelHandlers {
bool HandleGetCurrentLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!EditorWorld) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }

    ULevel* CurrentLevel = EditorWorld->GetCurrentLevel();
    if (!CurrentLevel) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No current level available"), nullptr, TEXT("NO_LEVEL"));
      return true;
    }

    UPackage* WorldPackage = EditorWorld->GetOutermost();
    UPackage* LevelPackage = CurrentLevel->GetOutermost();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("mapName"), EditorWorld->GetMapName());
    Result->SetStringField(TEXT("mapPath"), WorldPackage ? WorldPackage->GetName() : TEXT(""));
    Result->SetStringField(TEXT("levelName"),
                           LevelPackage ? FPackageName::GetShortName(LevelPackage->GetName())
                                        : CurrentLevel->GetName());
    Result->SetStringField(TEXT("levelPath"), LevelPackage ? LevelPackage->GetName() : TEXT(""));
    // The editor's "current level" can be a streaming sub-level; also publish
    // the persistent map so callers do not mistake one for the other (dogfood #155).
    Result->SetStringField(TEXT("persistentLevelPath"), WorldPackage ? WorldPackage->GetName() : TEXT(""));
    Result->SetBoolField(TEXT("currentLevelIsSubLevel"), CurrentLevel != EditorWorld->PersistentLevel);
    // Include editor-world identity separately from the map package so agents
    // can distinguish persistent map state from transient PIE/editor worlds.
    Result->SetStringField(TEXT("editorWorldName"), EditorWorld->GetName());
    Result->SetStringField(TEXT("editorWorldPath"), WorldPackage ? WorldPackage->GetPathName() : TEXT(""));
    Result->SetStringField(TEXT("worldType"), LexToString(EditorWorld->WorldType));
    Result->SetNumberField(TEXT("actorCount"), CurrentLevel->Actors.Num());
    Result->SetBoolField(TEXT("isPersistentLevel"), CurrentLevel == EditorWorld->PersistentLevel);
    // The capability's declared contract promises `loaded`; the current level is
    // loaded by definition, so it is stated rather than left absent.
    Result->SetBoolField(TEXT("loaded"), true);

    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Current level retrieved"), Result);
    return true;
}
} // namespace McpLevelHandlers
