#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Domains/BlueprintCreation/McpAutomationBridge_BlueprintCreationHandlers.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintEnsureProbe(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("ensure_exists"))) {
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

    bool bExists = McpAssetExists(CheckPath);
    bool bCreated = false;

    if (!bExists && bCreateIfMissing) {
      // Delegate to HandleBlueprintCreate for creation
      TSharedPtr<FJsonObject> CreatePayload = McpHandlerUtils::CreateResultObject();
      CreatePayload->SetStringField(TEXT("blueprintPath"), Path);
      // blueprint_create needs name + savePath; derive them from the resolved
      // path unless the caller supplied them (it used to fail "requires a
      // name" for every ensure_exists that had to create).
      FString CreateName;
      LocalPayload->TryGetStringField(TEXT("name"), CreateName);
      FString CreateSavePath;
      LocalPayload->TryGetStringField(TEXT("savePath"), CreateSavePath);
      if (CreateName.TrimStartAndEnd().IsEmpty()) {
        CreateName = FPaths::GetBaseFilename(CheckPath);
        int32 DotIndex = INDEX_NONE;
        if (CreateName.FindChar(TEXT('.'), DotIndex)) {
          CreateName = CreateName.Left(DotIndex);
        }
      }
      if (CreateSavePath.TrimStartAndEnd().IsEmpty()) {
        CreateSavePath = FPaths::GetPath(CheckPath);
      }
      CreatePayload->SetStringField(TEXT("name"), CreateName);
      CreatePayload->SetStringField(TEXT("savePath"), CreateSavePath);
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
      bExists = McpAssetExists(CheckPath);
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
  }

  // blueprint_probe_handle: Lightweight check for blueprint existence without loading
  if (ActionMatchesPattern(TEXT("probe_handle"))) {
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

    // The class comes from the same registry entry, without loading the asset.
    // (A lookup by object path, given this package path, never found it.)
    FAssetData AssetData;
    const bool bExists = McpAssetExists(CheckPath, &AssetData);
    FString AssetClass;
    if (bExists) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      AssetClass = AssetData.AssetClassPath.GetAssetName().ToString();
#else
      // UE 5.0: AssetClass is FName
      AssetClass = AssetData.AssetClass.ToString();
#endif
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("exists"), bExists);
    Resp->SetBoolField(TEXT("reachable"), bExists);
    Resp->SetStringField(TEXT("path"), bExists ? CheckPath : Path);
    if (!AssetClass.IsEmpty()) {
      Resp->SetStringField(TEXT("assetClass"), AssetClass);
    }
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           bExists ? TEXT("Blueprint handle found")
                                   : TEXT("Blueprint not found"),
                           Resp, FString());
    return true;
  }

  return false;
}
} // namespace McpBlueprintHandlers
