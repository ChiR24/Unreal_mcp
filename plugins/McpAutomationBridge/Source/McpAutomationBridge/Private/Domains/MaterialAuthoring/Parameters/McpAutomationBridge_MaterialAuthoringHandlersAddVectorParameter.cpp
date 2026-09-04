#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleAddVectorParameter(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_vector_parameter")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString ParamName, Group;
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetStringField(TEXT("group"), Group);

    UMaterialExpressionVectorParameter *VecParam =
        NewObject<UMaterialExpressionVectorParameter>(
            HostOuter, UMaterialExpressionVectorParameter::StaticClass(),
            NAME_None, RF_Transactional);
    VecParam->ParameterName = FName(*ParamName);
    if (!Group.IsEmpty()) {
      VecParam->Group = FName(*Group);
    }

    const TSharedPtr<FJsonObject> *DefaultObj;
    const TArray<TSharedPtr<FJsonValue>> *DefaultArr;
    if (Payload->TryGetObjectField(TEXT("defaultValue"), DefaultObj)) {
      double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
      // Accept both colour spellings — r/g/b/a and x/y/z(/w) — so callers
      // using the x/y/z form no longer get white silently.
      if ((*DefaultObj)->TryGetNumberField(TEXT("r"), R) ||
          (*DefaultObj)->TryGetNumberField(TEXT("x"), R)) {
        (*DefaultObj)->TryGetNumberField(TEXT("g"), G);
        (*DefaultObj)->TryGetNumberField(TEXT("b"), B);
        (*DefaultObj)->TryGetNumberField(TEXT("a"), A);
        (*DefaultObj)->TryGetNumberField(TEXT("z"), B);
        (*DefaultObj)->TryGetNumberField(TEXT("w"), A);
      }
      VecParam->DefaultValue = FLinearColor(R, G, B, A);
    } else if (Payload->TryGetArrayField(TEXT("defaultValue"), DefaultArr) &&
               DefaultArr->Num() >= 3) {
      auto Num = [&](int32 Idx, double Def) {
        double V = Def;
        return (*DefaultArr)[Idx]->TryGetNumber(V) ? V : Def;
      };
      VecParam->DefaultValue = FLinearColor(
          Num(0, 1.0), Num(1, 1.0), Num(2, 1.0),
          DefaultArr->Num() > 3 ? Num(3, 1.0) : 1.0);
    }

    VecParam->MaterialExpressionEditorX = (int32)X;
    VecParam->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, VecParam);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(VecParam));
    const FString PlacementWarning =
        AddMaterialNodePlacementFields(Result, Material, VecParam);
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        PlacementWarning.IsEmpty()
            ? FString::Printf(TEXT("Vector parameter '%s' added."), *ParamName)
            : FString::Printf(TEXT("Vector parameter '%s' added. %s"), *ParamName,
                              *PlacementWarning),
        Result);
    return true;
  }

  return false;
}
}
#endif
