#include "McpAutomationBridge_LevelHandlersActions.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleSpawnLightAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString LightType = TEXT("Point");
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("lightType"), LightType);
    const FString LT = LightType.ToLower();
    FString ClassName;
    if (LT == TEXT("directional"))
      ClassName = TEXT("DirectionalLight");
    else if (LT == TEXT("spot"))
      ClassName = TEXT("SpotLight");
    else if (LT == TEXT("rect"))
      ClassName = TEXT("RectLight");
    else
      ClassName = TEXT("PointLight");
    TSharedPtr<FJsonObject> Params = McpHandlerUtils::CreateResultObject();
    if (Payload.IsValid()) {
      const TSharedPtr<FJsonObject> *L = nullptr;
      if (Payload->TryGetObjectField(TEXT("location"), L) && L &&
          (*L).IsValid())
        Params->SetObjectField(TEXT("location"), *L);
      const TSharedPtr<FJsonObject> *R = nullptr;
      if (Payload->TryGetObjectField(TEXT("rotation"), R) && R &&
          (*R).IsValid())
        Params->SetObjectField(TEXT("rotation"), *R);
    }
    TSharedPtr<FJsonObject> P = McpHandlerUtils::CreateResultObject();
    P->SetStringField(TEXT("functionName"), TEXT("SPAWN_ACTOR_AT_LOCATION"));
    P->SetStringField(TEXT("class_path"), ClassName);
    P->SetObjectField(TEXT("params"), Params.ToSharedRef());
    return FMcpLevelHandlerAccess::ExecuteEditorFunction(
        Subsystem, RequestId, TEXT("execute_editor_function"), P,
        RequestingSocket);
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
