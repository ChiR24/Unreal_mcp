#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Domains/MaterialAuthoring/McpMatPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "MaterialEditingLibrary.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleDeleteNode(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("delete_node")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    const TArray<FString> NodeIds = McpGetStringListField(Payload, TEXT("nodeIds"), TEXT("nodeId"));
    if (NodeIds.Num() == 0) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId' or 'nodeIds'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Every id resolved before any node goes: an expr_N index shifted as nodes were removed.
    TArray<FString> Removed, NotFound;
    TArray<UMaterialExpression *> Targets;
    for (const FString &NId : NodeIds) {
      UMaterialExpression *Expr = FIND_EXPR_IN_HOST(NId);
      (Expr ? Removed : NotFound).Add(NId);
      if (Expr) {
        Targets.AddUnique(Expr);
      }
    }

    // An id that matched nothing used to vanish from the reply, so deleting a
    // misspelled node answered "Deleted 0 node(s)" as a success.
    if (Removed.Num() == 0) {
      Bridge->SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("No node matched: %s. Read the ids with get_material_info or find_node."), *FString::Join(NotFound, TEXT(", "))),
          TEXT("NOT_FOUND"));
      return true;
    }
    // The engine's own delete: every input of every expression and every main pin (MaterialAttributes, the
    // customized UVs and the shading model among them) let go of the node, its parameter entry dropped, the node
    // marked garbage. The hand cleanup missed attribute, array-held and switch inputs and those pins.
    for (UMaterialExpression *Expr : Targets) {
      if (Material) {
        UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expr);
      } else {
        UMaterialEditingLibrary::DeleteMaterialExpressionInFunction(Function, Expr);
      }
    }
    FINALIZE_HOST();
    // A delete used to leave the material dirty and unsaved, so an editor restart brought the
    // node back. Saved by default, like the parameter edits.
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    const bool bSaved = bSave && (Material ? McpSafeAssetSave(Material) : McpSafeAssetSave(Function));

    TSharedPtr<FJsonObject> Result = McpMaterialHostResult(HostOuter);
    Result->SetBoolField(TEXT("saved"), bSaved);
    TArray<TSharedPtr<FJsonValue>> RemovedArr = McpHandlerUtils::ToJsonStringArray(Removed);
    Result->SetArrayField(TEXT("removed"), RemovedArr);
    Result->SetNumberField(TEXT("removedCount"), Removed.Num());
    TArray<TSharedPtr<FJsonValue>> NotFoundArr = McpHandlerUtils::ToJsonStringArray(NotFound);
    Result->SetArrayField(TEXT("notFound"), NotFoundArr);
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Deleted %d node(s)."), Removed.Num()),
                           Result);
    return true;
  }

  return false;
}
}
