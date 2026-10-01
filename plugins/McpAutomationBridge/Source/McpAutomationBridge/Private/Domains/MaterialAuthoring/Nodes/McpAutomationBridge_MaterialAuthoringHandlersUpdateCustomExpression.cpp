#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#include "MaterialShared.h"
#include "RHI.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleUpdateCustomExpression(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("update_custom_expression")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    if (NodeId.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpression *Expr = FIND_EXPR_IN_HOST(NodeId);
    if (!Expr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    UMaterialExpressionCustom *CustomExpr = Cast<UMaterialExpressionCustom>(Expr);
    if (!CustomExpr) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Node is not a Custom Expression."), TEXT("INVALID_NODE_TYPE"));
      return true;
    }

    FString NewCode;
    if (Payload->TryGetStringField(TEXT("code"), NewCode)) {
      CustomExpr->Code = NewCode;
    }

    FString NewDesc;
    if (Payload->TryGetStringField(TEXT("description"), NewDesc)) {
      CustomExpr->Description = NewDesc;
    }

    FString NewOutputType;
    if (Payload->TryGetStringField(TEXT("outputType"), NewOutputType)) {
      CustomExpr->OutputType = ParseCustomOutputType(NewOutputType, CustomExpr->OutputType);
    }

    ApplyCustomInputs(CustomExpr, Payload);

    ApplyCustomAdditionalOutputs(CustomExpr, Payload);

    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpMaterialHostResult(HostOuter);
    Result->SetStringField(TEXT("nodeId"), NodeId);
    Result->SetStringField(TEXT("code"), CustomExpr->Code);
    Result->SetNumberField(TEXT("inputCount"), CustomExpr->Inputs.Num());
    Result->SetNumberField(TEXT("additionalOutputCount"), CustomExpr->AdditionalOutputs.Num());
    FString Message = TEXT("Custom expression updated.");
    // A step of build_material_graph leaves the compile and the save to the batch, which does both once at its end.
    // A call of its own does both here, as compile_material does: it used to leave the material unsaved and say
    // nothing of a compile error. Translation ran inside PostEditChange, so its errors are known now.
    if (!FMcpResponseCaptureRegistry::Get().IsCapturing(RequestId)) {
      TArray<FString> CompileErrors;
      if (Material) {
        if (const FMaterialResource *Resource = MCP_GET_MATERIAL_RESOURCE(Material)) {
          CompileErrors = Resource->GetCompileErrors();
        }
      }
      TArray<TSharedPtr<FJsonValue>> ErrorValues;
      for (const FString &Error : CompileErrors) {
        ErrorValues.Add(MakeShared<FJsonValueString>(Error));
      }
      Result->SetBoolField(TEXT("compiled"), CompileErrors.Num() == 0);
      Result->SetArrayField(TEXT("compileErrors"), ErrorValues);
      Result->SetBoolField(TEXT("saved"), Material ? McpSafeAssetSave(Material) : McpSafeAssetSave(Function));
      if (CompileErrors.Num() > 0) {
        Message += FString::Printf(TEXT(" WARNING: the material does not compile (the default material renders in its place): %s"),
                                   *CompileErrors[0]);
      }
    }
    Bridge->SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
  }

  return false;
}
}
