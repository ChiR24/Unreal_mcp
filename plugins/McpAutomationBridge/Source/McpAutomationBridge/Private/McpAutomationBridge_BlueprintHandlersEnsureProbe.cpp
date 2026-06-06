#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridge_BlueprintCreationHandlers.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"
#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintEnsureProbe(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_ensure_exists")) ||
      ActionMatchesPattern(TEXT("ensure_exists")) ||
      AlphaNumLower.Contains(TEXT("blueprintensureexists")) ||
      AlphaNumLower.Contains(TEXT("ensureexists"))) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Entered blueprint_ensure_exists handler: RequestId=%s"),
           *RequestId);
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_ensure_exists requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

    FString ParentClass;
    LocalPayload->TryGetStringField(TEXT("parentClass"), ParentClass);
    bool bCreateIfMissing = true;
    if (LocalPayload->HasField(TEXT("createIfMissing"))) {
      LocalPayload->TryGetBoolField(TEXT("createIfMissing"), bCreateIfMissing);
    }

#if WITH_EDITOR
    // Check if blueprint exists using lightweight check
    FString CheckPath = Path;
    if (!CheckPath.StartsWith(TEXT("/Game")) &&
        !CheckPath.StartsWith(TEXT("/Engine")) &&
        !CheckPath.StartsWith(TEXT("/Script"))) {
      if (CheckPath.StartsWith(TEXT("/"))) {
        CheckPath = TEXT("/Game") + CheckPath;
      } else {
        CheckPath = TEXT("/Game/") + CheckPath;
      }
    }
    if (CheckPath.EndsWith(TEXT(".uasset"))) {
      CheckPath = CheckPath.LeftChop(7);
    }

    bool bExists = UEditorAssetLibrary::DoesAssetExist(CheckPath);
    bool bCreated = false;

    if (!bExists && bCreateIfMissing) {
      // Delegate to HandleBlueprintCreate for creation
      TSharedPtr<FJsonObject> CreatePayload = McpHandlerUtils::CreateResultObject();
      CreatePayload->SetStringField(TEXT("blueprintPath"), Path);
      if (!ParentClass.IsEmpty()) {
        CreatePayload->SetStringField(TEXT("parentClass"), ParentClass);
      }
      // Use FBlueprintCreationHandlers to create the blueprint
      bool bCreateResult = FBlueprintCreationHandlers::HandleBlueprintCreate(
          &Bridge, RequestId, CreatePayload, RequestingSocket);
      // If creation handler returned true, it sent its own response
      if (bCreateResult) {
        return true;
      }
      // Check again after creation attempt
      bExists = UEditorAssetLibrary::DoesAssetExist(CheckPath);
      bCreated = bExists;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("exists"), bExists);
    Resp->SetBoolField(TEXT("created"), bCreated);
    Resp->SetStringField(TEXT("blueprintPath"), bExists ? CheckPath : Path);
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, true,
        bCreated ? TEXT("Blueprint created")
                 : (bExists ? TEXT("Blueprint exists")
                            : TEXT("Blueprint not found")),
        Resp, FString());
    return true;
#else
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("blueprint_ensure_exists requires editor build"), nullptr,
        TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  // blueprint_probe_handle: Lightweight check for blueprint existence without loading
  if (ActionMatchesPattern(TEXT("blueprint_probe_handle")) ||
      ActionMatchesPattern(TEXT("probe_handle")) ||
      AlphaNumLower.Contains(TEXT("blueprintprobehandle")) ||
      AlphaNumLower.Contains(TEXT("probehandle"))) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Entered blueprint_probe_handle handler: RequestId=%s"),
           *RequestId);
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_probe_handle requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }

#if WITH_EDITOR
    // Normalize path
    FString CheckPath = Path;
    if (!CheckPath.StartsWith(TEXT("/Game")) &&
        !CheckPath.StartsWith(TEXT("/Engine")) &&
        !CheckPath.StartsWith(TEXT("/Script"))) {
      if (CheckPath.StartsWith(TEXT("/"))) {
        CheckPath = TEXT("/Game") + CheckPath;
      } else {
        CheckPath = TEXT("/Game/") + CheckPath;
      }
    }
    if (CheckPath.EndsWith(TEXT(".uasset"))) {
      CheckPath = CheckPath.LeftChop(7);
    }

    bool bExists = UEditorAssetLibrary::DoesAssetExist(CheckPath);
    FString AssetClass;

    if (bExists) {
      // Try to get asset class without fully loading - use FindAssetData
      IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(CheckPath));
#else
      // UE 5.0: GetAssetByObjectPath takes FName
      FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FName(*CheckPath));
#endif
      if (AssetData.IsValid()) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();
#else
        // UE 5.0: AssetClass is FName
        AssetClass = AssetData.AssetClass.ToString();
#endif
      }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("exists"), bExists);
    Resp->SetStringField(TEXT("path"), bExists ? CheckPath : Path);
    if (!AssetClass.IsEmpty()) {
      Resp->SetStringField(TEXT("assetClass"), AssetClass);
    }
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           bExists ? TEXT("Blueprint handle found")
                                   : TEXT("Blueprint not found"),
                           Resp, FString());
    return true;
#else
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, false,
        TEXT("blueprint_probe_handle requires editor build"), nullptr,
        TEXT("NOT_AVAILABLE"));
    return true;
#endif
  }

  // blueprint_set_metadata: Set metadata on a blueprint asset
  return false;
}
#endif
} // namespace McpBlueprintHandlers
