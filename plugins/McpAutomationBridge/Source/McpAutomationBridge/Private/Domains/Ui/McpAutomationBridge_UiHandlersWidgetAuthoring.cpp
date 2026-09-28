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
    FString SavePath;
    Payload->TryGetStringField(TEXT("name"), WidgetName);
    Payload->TryGetStringField(TEXT("savePath"), SavePath);
    // widgetPath (/Game/UI/WBP_Menu) supplies whichever of name and savePath was left out.
    FString WidgetPath;
    Payload->TryGetStringField(TEXT("widgetPath"), WidgetPath);
    WidgetPath.TrimStartAndEndInline();
    if (!WidgetPath.IsEmpty()) {
      FString PathFolder;
      FString PathName;
      if (!WidgetPath.Split(TEXT("/"), &PathFolder, &PathName, ESearchCase::IgnoreCase, ESearchDir::FromEnd) ||
          PathFolder.IsEmpty() || PathName.IsEmpty()) {
        Message = FString::Printf(TEXT("widgetPath '%s' must be a full asset path such as /Game/UI/WBP_Menu"), *WidgetPath);
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
        return true;
      }
      PathName.Split(TEXT("."), &PathName, nullptr);
      WidgetName = WidgetName.IsEmpty() ? PathName : WidgetName;
      SavePath = SavePath.IsEmpty() ? PathFolder : SavePath;
    }
    if (WidgetName.IsEmpty()) {
      Message = TEXT("name (or widgetPath) is required for create_widget");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
      return true;
    }
    if (SavePath.IsEmpty()) {
      SavePath = TEXT("/Game/UI/Widgets");
    }

    FString WidgetType;
    Payload->TryGetStringField(TEXT("widgetType"), WidgetType);

    const FString NormalizedPath = SavePath.TrimStartAndEnd();
    const FString TargetPath =
        FString::Printf(TEXT("%s/%s"), *NormalizedPath, *WidgetName);
    if (UEditorAssetLibrary::DoesAssetExist(TargetPath)) {
      // An existing asset of another kind is not a widget that "already exists".
      if (!Cast<UWidgetBlueprint>(UEditorAssetLibrary::LoadAsset(TargetPath))) {
        Message = FString::Printf(TEXT("An asset that is not a Widget Blueprint already exists at %s"), *TargetPath);
        ErrorCode = TEXT("ASSET_TYPE_MISMATCH");
        Resp->SetStringField(TEXT("error"), Message);
        return true;
      }
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
    UClass *ParentUClass = UUserWidget::StaticClass();
    if (!WidgetType.IsEmpty() && !WidgetType.Equals(TEXT("UserWidget"), ESearchCase::IgnoreCase)) {
      UClass *Requested = FindFirstObject<UClass>(*WidgetType, EFindFirstObjectOptions::None);
      if (!Requested) {
        Requested = LoadClass<UUserWidget>(nullptr, *WidgetType);
      }
      // An unknown widgetType used to fall back to UserWidget silently.
      if (!Requested || !Requested->IsChildOf(UUserWidget::StaticClass())) {
        Message = FString::Printf(TEXT("widgetType '%s' is not a UserWidget class; nothing was created"), *WidgetType);
        ErrorCode = TEXT("INVALID_ARGUMENT");
        Resp->SetStringField(TEXT("error"), Message);
        return true;
      }
      ParentUClass = Requested;
    }
    UPackage *Package = CreatePackage(*SafeTargetPath);
    if (!Package) {
      Message = FString::Printf(TEXT("Failed to create package %s"), *SafeTargetPath);
      ErrorCode = TEXT("PACKAGE_CREATE_FAILED");
      Resp->SetStringField(TEXT("error"), Message);
      return true;
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
