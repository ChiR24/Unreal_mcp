#include "McpAutomationBridge_LevelHandlersActions.h"
#include "McpAutomationBridge_LevelHandlersWorldAccess.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleGetLevelLightingScenariosAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
      return true;
    }

    TArray<TSharedPtr<FJsonValue>> Scenarios;
    TArray<ULevel*> Levels = GetAllLevelsFromWorld(World);
    for (ULevel* Level : Levels) {
      if (Level && Level->bIsLightingScenario) {
        TSharedPtr<FJsonObject> ScenarioInfo = McpHandlerUtils::CreateResultObject();
        ScenarioInfo->SetStringField(TEXT("levelPath"), Level->GetOutermost() ? Level->GetOutermost()->GetName() : TEXT(""));
        ScenarioInfo->SetStringField(TEXT("levelName"), Level->GetName());
        Scenarios.Add(MakeShared<FJsonValueObject>(ScenarioInfo));
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("scenarios"), Scenarios);
    Result->SetNumberField(TEXT("count"), Scenarios.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Lighting scenarios retrieved"), Result);
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
