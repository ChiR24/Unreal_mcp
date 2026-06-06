#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_UiHandlersPrivate.h"

#include "AssetToolsModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "EditorAssetLibrary.h"
#include "McpAutomationBridgeHelpers.h"
#include "WidgetBlueprint.h"

#if __has_include("Factories/WidgetBlueprintFactory.h")
#include "Factories/WidgetBlueprintFactory.h"
#define MCP_HAS_WIDGET_FACTORY 1
#else
#define MCP_HAS_WIDGET_FACTORY 0
#endif

#if WITH_EDITOR
namespace McpUiHandlers {

bool HandleWidgetAuthoringAction(
    UMcpAutomationBridgeSubsystem &, const FString &LowerSub,
    const TSharedPtr<FJsonObject> &Payload,
    const TSharedPtr<FJsonObject> &Resp, bool &bSuccess, FString &Message,
    FString &ErrorCode) {
  if (LowerSub == TEXT("create_widget")) {
#if MCP_HAS_WIDGET_FACTORY
    FString WidgetName;
    if (!Payload->TryGetStringField(TEXT("name"), WidgetName) ||
        WidgetName.IsEmpty()) {
      Message = TEXT("name field required for create_widget");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
      return true;
    }

    FString SavePath;
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    if (SavePath.IsEmpty()) {
      SavePath = TEXT("/Game/UI/Widgets");
    }

    FString WidgetType;
    Payload->TryGetStringField(TEXT("widgetType"), WidgetType);

    const FString NormalizedPath = SavePath.TrimStartAndEnd();
    const FString TargetPath =
        FString::Printf(TEXT("%s/%s"), *NormalizedPath, *WidgetName);
    if (UEditorAssetLibrary::DoesAssetExist(TargetPath)) {
      bSuccess = true;
      Message =
          FString::Printf(TEXT("Widget blueprint already exists at %s"),
                          *TargetPath);
      Resp->SetStringField(TEXT("widgetPath"), TargetPath);
      Resp->SetBoolField(TEXT("exists"), true);
      if (!WidgetType.IsEmpty()) {
        Resp->SetStringField(TEXT("widgetType"), WidgetType);
      }
      Resp->SetStringField(TEXT("widgetName"), WidgetName);
      return true;
    }

    UWidgetBlueprintFactory *Factory = NewObject<UWidgetBlueprintFactory>();
    if (!Factory) {
      Message = TEXT("Failed to create widget blueprint factory");
      ErrorCode = TEXT("FACTORY_CREATION_FAILED");
      Resp->SetStringField(TEXT("error"), Message);
      return true;
    }

    UObject *NewAsset = Factory->FactoryCreateNew(
        UWidgetBlueprint::StaticClass(),
        UEditorAssetLibrary::DoesAssetExist(NormalizedPath)
            ? UEditorAssetLibrary::LoadAsset(NormalizedPath)
            : nullptr,
        FName(*WidgetName), RF_Standalone, nullptr, GWarn);

    UWidgetBlueprint *WidgetBlueprint = Cast<UWidgetBlueprint>(NewAsset);
    if (!WidgetBlueprint) {
      Message = TEXT("Failed to create widget blueprint asset");
      ErrorCode = TEXT("ASSET_CREATION_FAILED");
      Resp->SetStringField(TEXT("error"), Message);
      return true;
    }

    SaveLoadedAssetThrottled(WidgetBlueprint, -1.0, true);
    ScanPathSynchronous(WidgetBlueprint->GetOutermost()->GetName());

    bSuccess = true;
    Message = FString::Printf(TEXT("Widget blueprint created at %s"),
                              *WidgetBlueprint->GetPathName());
    Resp->SetStringField(TEXT("widgetPath"), WidgetBlueprint->GetPathName());
    Resp->SetStringField(TEXT("widgetName"), WidgetName);
    if (!WidgetType.IsEmpty()) {
      Resp->SetStringField(TEXT("widgetType"), WidgetType);
    }
    return true;
#else
    Message =
        TEXT("create_widget requires editor build with widget factory support");
    ErrorCode = TEXT("NOT_AVAILABLE");
    Resp->SetStringField(TEXT("error"), Message);
    return true;
#endif
  }

  if (LowerSub != TEXT("add_widget_child")) {
    return false;
  }

#if MCP_HAS_WIDGET_FACTORY
  FString WidgetPath;
  if (!Payload->TryGetStringField(TEXT("widgetPath"), WidgetPath) ||
      WidgetPath.IsEmpty()) {
    Message = TEXT("widgetPath required for add_widget_child");
    ErrorCode = TEXT("INVALID_ARGUMENT");
    Resp->SetStringField(TEXT("error"), Message);
    return true;
  }

  UWidgetBlueprint *WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *WidgetPath);
  if (!WidgetBP) {
    Message =
        FString::Printf(TEXT("Could not find Widget Blueprint at %s"),
                        *WidgetPath);
    ErrorCode = TEXT("ASSET_NOT_FOUND");
    Resp->SetStringField(TEXT("error"), Message);
    return true;
  }

  FString ChildClassPath;
  if (!Payload->TryGetStringField(TEXT("childClass"), ChildClassPath) ||
      ChildClassPath.IsEmpty()) {
    Message = TEXT("childClass required (e.g. /Script/UMG.Button)");
    ErrorCode = TEXT("INVALID_ARGUMENT");
    Resp->SetStringField(TEXT("error"), Message);
    return true;
  }

  UClass *WidgetClass =
      UEditorAssetLibrary::FindAssetData(ChildClassPath).GetAsset().IsValid()
          ? LoadClass<UObject>(nullptr, *ChildClassPath)
          : FindObject<UClass>(nullptr, *ChildClassPath);
  if (!WidgetClass) {
    WidgetClass = ChildClassPath.Contains(TEXT("."))
                      ? FindObject<UClass>(nullptr, *ChildClassPath)
                      : FindObject<UClass>(
                            nullptr, *FString::Printf(TEXT("/Script/UMG.%s"),
                                                      *ChildClassPath));
  }

  if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass())) {
    Message = FString::Printf(
        TEXT("Could not resolve valid UWidget class from '%s'"),
        *ChildClassPath);
    ErrorCode = TEXT("CLASS_NOT_FOUND");
    Resp->SetStringField(TEXT("error"), Message);
    return true;
  }

  FString ParentName;
  Payload->TryGetStringField(TEXT("parentName"), ParentName);

  WidgetBP->Modify();
  UWidget *NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(
      WidgetClass);

  bool bAdded = false;
  if (ParentName.IsEmpty()) {
    if (WidgetBP->WidgetTree->RootWidget == nullptr) {
      WidgetBP->WidgetTree->RootWidget = NewWidget;
      bAdded = true;
    } else if (UPanelWidget *RootPanel =
                   Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget)) {
      RootPanel->AddChild(NewWidget);
      bAdded = true;
    } else {
      Message =
          TEXT("Root widget is not a panel and already exists. Specify parentName.");
      ErrorCode = TEXT("ROOT_Full");
    }
  } else {
    UWidget *ParentWidget =
        WidgetBP->WidgetTree->FindWidget(FName(*ParentName));
    if (UPanelWidget *ParentPanel = Cast<UPanelWidget>(ParentWidget)) {
      ParentPanel->AddChild(NewWidget);
      bAdded = true;
    } else {
      Message = FString::Printf(
          TEXT("Parent '%s' not found or is not a PanelWidget"), *ParentName);
      ErrorCode = TEXT("PARENT_NOT_FOUND");
    }
  }

  if (bAdded) {
    bSuccess = true;
    Message = FString::Printf(TEXT("Added %s to %s"),
                              *WidgetClass->GetName(), *WidgetBP->GetName());
    Resp->SetStringField(TEXT("widgetName"), NewWidget->GetName());
    Resp->SetStringField(TEXT("childClass"), WidgetClass->GetName());
  } else {
    if (Message.IsEmpty()) {
      Message = TEXT("Failed to add widget child.");
    }
    Resp->SetStringField(TEXT("error"), Message);
  }
  return true;
#else
  Message = TEXT("add_widget_child requires editor build");
  ErrorCode = TEXT("NOT_AVAILABLE");
  Resp->SetStringField(TEXT("error"), Message);
  return true;
#endif
}

}
#endif
