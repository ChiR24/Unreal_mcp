#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Ui/McpAutomationBridge_UiHandlersPrivate.h"

#include "AssetToolsModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "EditorAssetLibrary.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "WidgetBlueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPaths.h"


namespace McpUiHandlers {

bool HandleWidgetAuthoringAction(
    UMcpAutomationBridgeSubsystem &, const FString &LowerSub,
    const TSharedPtr<FJsonObject> &Payload,
    const TSharedPtr<FJsonObject> &Resp, bool &bSuccess, FString &Message,
    FString &ErrorCode) {
  if (LowerSub == TEXT("create_widget")) {
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

    // Create the Blueprint inside a real package. The factory call used to be
    // handed a null outer (the folder is not an asset), which is a fatal error
    // in StaticAllocateObject and took the editor down (crash #4).
    const FString SafeTargetPath = SanitizeProjectRelativePath(TargetPath);
    if (SafeTargetPath.IsEmpty()) {
      Message = FString::Printf(TEXT("Invalid or unsafe savePath: %s"), *NormalizedPath);
      ErrorCode = TEXT("SECURITY_VIOLATION");
      Resp->SetStringField(TEXT("error"), Message);
      return true;
    }
    if (FindObject<UBlueprint>(nullptr, *(SafeTargetPath + TEXT(".") + WidgetName)) != nullptr) {
      Message = FString::Printf(TEXT("Widget blueprint '%s' already exists in memory"), *WidgetName);
      ErrorCode = TEXT("ALREADY_EXISTS");
      Resp->SetStringField(TEXT("error"), Message);
      return true;
    }
    UPackage *Package = CreatePackage(*SafeTargetPath);
    if (!Package) {
      Message = FString::Printf(TEXT("Failed to create package %s"), *SafeTargetPath);
      ErrorCode = TEXT("PACKAGE_CREATE_FAILED");
      Resp->SetStringField(TEXT("error"), Message);
      return true;
    }
    UClass *ParentUClass = UUserWidget::StaticClass();
    if (!WidgetType.IsEmpty() && !WidgetType.Equals(TEXT("UserWidget"), ESearchCase::IgnoreCase)) {
      UClass *Requested = FindFirstObject<UClass>(*WidgetType, EFindFirstObjectOptions::None);
      if (!Requested) {
        Requested = LoadClass<UUserWidget>(nullptr, *WidgetType);
      }
      if (Requested && Requested->IsChildOf(UUserWidget::StaticClass())) {
        ParentUClass = Requested;
      }
    }
    UWidgetBlueprint *WidgetBlueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
        ParentUClass, Package, FName(*WidgetName), BPTYPE_Normal,
        UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
    if (WidgetBlueprint) {
      FAssetRegistryModule::AssetCreated(WidgetBlueprint);
      Package->MarkPackageDirty();
    }
    if (!WidgetBlueprint) {
      Message = TEXT("Failed to create widget blueprint asset");
      ErrorCode = TEXT("ASSET_CREATION_FAILED");
      Resp->SetStringField(TEXT("error"), Message);
      return true;
    }

    SaveLoadedAssetThrottled(WidgetBlueprint, true);
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
  }

  return false;
}

}
