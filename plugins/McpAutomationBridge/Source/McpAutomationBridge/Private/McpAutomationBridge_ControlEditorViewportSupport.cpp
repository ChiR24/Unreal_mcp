#include "McpAutomationBridge_ControlEditorSupport.h"

#if WITH_EDITOR
FEditorViewportClient *GetActiveEditorViewportClientForMcp() {
#if MCP_HAS_LEVEL_EDITOR_MODULE
  if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
    if (FLevelEditorModule *LevelEditorModule =
            FModuleManager::GetModulePtr<FLevelEditorModule>(
                TEXT("LevelEditor"))) {
      TSharedPtr<IAssetViewport> ActiveViewport =
          LevelEditorModule->GetFirstActiveViewport();
      if (ActiveViewport.IsValid()) {
        return &ActiveViewport->GetAssetViewportClient();
      }
    }
  }
#endif

  if (GEditor && GEditor->GetActiveViewport()) {
    return static_cast<FEditorViewportClient *>(
        GEditor->GetActiveViewport()->GetClient());
  }
  return nullptr;
}

TSharedPtr<FJsonObject> MakeVectorObjectForMcp(const FVector &Vector) {
  TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
  Obj->SetNumberField(TEXT("x"), Vector.X);
  Obj->SetNumberField(TEXT("y"), Vector.Y);
  Obj->SetNumberField(TEXT("z"), Vector.Z);
  return Obj;
}

TSharedPtr<FJsonObject> MakeRotatorObjectForMcp(const FRotator &Rotator) {
  TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
  Obj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
  Obj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
  Obj->SetNumberField(TEXT("roll"), Rotator.Roll);
  return Obj;
}
#endif
