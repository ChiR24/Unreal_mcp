#include "Domains/Blueprint/McpBPActionContext.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BlueprintCreation/McpBPCreateHandlers.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintEnsureProbe(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("ensure_exists"))) {
    // The declared form is name + savePath, and the create below lands there:
    // check that same place. The check used to resolve the bare name (to
    // /Game/<name>, or any same-named Blueprint elsewhere), so a Blueprint
    // ensured under another folder was never found again.
    FString EnsureName;
    FString EnsureSavePath;
    LocalPayload->TryGetStringField(TEXT("name"), EnsureName);
    LocalPayload->TryGetStringField(TEXT("savePath"), EnsureSavePath);
    const bool bNamedBySavePath = !EnsureName.IsEmpty() && !EnsureName.StartsWith(TEXT("/")) &&
        !EnsureSavePath.TrimStartAndEnd().IsEmpty();
    FString Path = bNamedBySavePath ? EnsureSavePath / EnsureName : ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("blueprint_ensure_exists requires a blueprint path."),
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
    const FString CheckPath = McpBlueprintCheckPath(Path);

    const bool bExists = McpAssetExists(CheckPath);

    if (!bExists && bCreateIfMissing) {
      // Delegate to HandleBlueprintCreate for creation
      TSharedPtr<FJsonObject> CreatePayload = McpHandlerUtils::CreateResultObject();
      CreatePayload->SetStringField(TEXT("blueprintPath"), Path);
      // blueprint_create needs name + savePath; derive them from the resolved
      // path unless the caller supplied them (it used to fail "requires a
      // name" for every ensure_exists that had to create).
      FString CreateName = EnsureName;
      FString CreateSavePath = EnsureSavePath;
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
      // The create answers for itself, a refusal included.
      FBlueprintCreationHandlers::HandleBlueprintCreate(&Bridge, RequestId, CreatePayload, RequestingSocket);
      return true;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("exists"), bExists);
    Resp->SetBoolField(TEXT("created"), false);
    Resp->SetStringField(TEXT("blueprintPath"), bExists ? CheckPath : Path);
    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, true,
        bExists ? TEXT("Blueprint exists") : TEXT("Blueprint not found"),
        Resp, FString());
    return true;
  }

  // blueprint_probe_handle: Lightweight check for blueprint existence without loading
  if (ActionMatchesPattern(TEXT("probe_handle"))) {
    FString Path = RequireBlueprintPath(Context, TEXT("blueprint_probe_handle"));
    if (Path.IsEmpty()) {
      return true;
    }

    const FString CheckPath = McpBlueprintCheckPath(Path);

    // The class comes from the same registry entry, without loading the asset.
    // (A lookup by object path, given this package path, never found it.)
    FAssetData AssetData;
    const bool bExists = McpAssetExists(CheckPath, &AssetData);
    FString AssetClass;
    if (bExists) {
      AssetClass = FPackageName::ObjectPathToObjectName(MCP_ASSET_DATA_GET_CLASS_PATH(AssetData));
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
