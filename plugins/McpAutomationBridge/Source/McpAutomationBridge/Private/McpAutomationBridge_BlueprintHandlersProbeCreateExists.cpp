#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridge_BlueprintCreationHandlers.h"
#include "McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintProbeCreateExists(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (ActionMatchesPattern(TEXT("blueprint_probe_subobject_handle")) ||
      ActionMatchesPattern(TEXT("probe_subobject_handle")) ||
      ActionMatchesPattern(TEXT("probehandle")) ||
      AlphaNumLower.Contains(TEXT("blueprintprobesubobjecthandle")) ||
      AlphaNumLower.Contains(TEXT("probesubobjecthandle")) ||
      AlphaNumLower.Contains(TEXT("probehandle"))) {
    return FBlueprintCreationHandlers::HandleBlueprintProbeSubobjectHandle(
        &Bridge, RequestId, LocalPayload, RequestingSocket);
  }

  // blueprint_create handler: parse payload and prepare coalesced creation
  // Support both explicit blueprint_create and the nested 'create' action from
  // manage_blueprint
  if (ActionMatchesPattern(TEXT("blueprint_create")) ||
      ActionMatchesPattern(TEXT("create_blueprint")) ||
      ActionMatchesPattern(TEXT("create")) ||
      AlphaNumLower.Contains(TEXT("blueprintcreate")) ||
      AlphaNumLower.Contains(TEXT("createblueprint"))) {
    return FBlueprintCreationHandlers::HandleBlueprintCreate(
        &Bridge, RequestId, LocalPayload, RequestingSocket);
  }

  // Other blueprint_* actions (modify_scs, compile, add_variable, add_function,
  // etc.) For simplicity, unhandled blueprint actions return NOT_IMPLEMENTED so
  // the server may fall back to Python helpers if available.

  // blueprint_exists: check whether a blueprint asset or registry entry exists
  if (ActionMatchesPattern(TEXT("blueprint_exists")) ||
      ActionMatchesPattern(TEXT("exists")) ||
      AlphaNumLower.Contains(TEXT("blueprintexists"))) {
    FString Path = ResolveBlueprintRequestedPath();
    if (Path.IsEmpty()) {
      Bridge.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("blueprint_exists requires a blueprint path."), nullptr,
          TEXT("INVALID_BLUEPRINT_PATH"));
      return true;
    }
    FString Normalized = Path;
    bool bFound = false;
#if WITH_EDITOR
    // Use lightweight existence check instead of LoadBlueprintAsset
    // to avoid Editor hangs on heavy/corrupted assets
    FString CheckPath = Path;
    // Ensure path starts with /Game if it doesn't have a valid root
    if (!CheckPath.StartsWith(TEXT("/Game")) &&
        !CheckPath.StartsWith(TEXT("/Engine")) &&
        !CheckPath.StartsWith(TEXT("/Script"))) {
      if (CheckPath.StartsWith(TEXT("/"))) {
        CheckPath = TEXT("/Game") + CheckPath;
      } else {
        CheckPath = TEXT("/Game/") + CheckPath;
      }
    }
    // Remove .uasset extension if present
    if (CheckPath.EndsWith(TEXT(".uasset"))) {
      CheckPath = CheckPath.LeftChop(7);
    }
    bFound = UEditorAssetLibrary::DoesAssetExist(CheckPath);
    if (bFound) {
      Normalized = CheckPath;
    }
#else
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("blueprint_exists requires editor build"),
                           nullptr, TEXT("NOT_AVAILABLE"));
    return true;
#endif
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("exists"), bFound);
    Resp->SetStringField(TEXT("blueprintPath"), bFound ? Normalized : Path);
    // Always return true (action succeeded), let propert "exists" convey state
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           bFound ? TEXT("Blueprint exists")
                                  : TEXT("Blueprint not found"),
                           Resp, FString());
    return true;
  }

  // blueprint_get: return the lightweight registry entry for a blueprint
  return false;
}
#endif
} // namespace McpBlueprintHandlers
