#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleGetMaterialInfo(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("get_material_info")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Validate path security BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterial *Material = nullptr;
    UMaterialFunction *Function = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, Function);
    if (!Material && !Function) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material or Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Optional filter: "parameters", "expressions", "connections", or "all" (default)
    FString Filter;
    Payload->TryGetStringField(TEXT("filter"), Filter);
    bool bWantParams      = Filter.IsEmpty() || Filter == TEXT("all") || Filter == TEXT("parameters");
    bool bWantExpressions = Filter.IsEmpty() || Filter == TEXT("all") || Filter == TEXT("expressions");
    bool bWantConnections = Filter.IsEmpty() || Filter == TEXT("all") || Filter == TEXT("connections");

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetType"),
                           Material ? TEXT("Material") : TEXT("MaterialFunction"));

    // Collect the expression array (shared by both Material and MF paths)
    auto& AllExpressions = Material
        ? MCP_GET_MATERIAL_EXPRESSIONS(Material)
        : MCP_GET_FUNCTION_EXPRESSIONS(Function);
    Result->SetNumberField(TEXT("nodeCount"), AllExpressions.Num());

    if (Material) {
      // Domain
      switch (Material->MaterialDomain) {
      case EMaterialDomain::MD_Surface:
        Result->SetStringField(TEXT("domain"), TEXT("Surface"));
        break;
      case EMaterialDomain::MD_DeferredDecal:
        Result->SetStringField(TEXT("domain"), TEXT("DeferredDecal"));
        break;
      case EMaterialDomain::MD_LightFunction:
        Result->SetStringField(TEXT("domain"), TEXT("LightFunction"));
        break;
      case EMaterialDomain::MD_Volume:
        Result->SetStringField(TEXT("domain"), TEXT("Volume"));
        break;
      case EMaterialDomain::MD_PostProcess:
        Result->SetStringField(TEXT("domain"), TEXT("PostProcess"));
        break;
      case EMaterialDomain::MD_UI:
        Result->SetStringField(TEXT("domain"), TEXT("UI"));
        break;
      default:
        Result->SetStringField(TEXT("domain"), TEXT("Unknown"));
        break;
      }

      // Blend mode
      switch (Material->BlendMode) {
      case EBlendMode::BLEND_Opaque:
        Result->SetStringField(TEXT("blendMode"), TEXT("Opaque"));
        break;
      case EBlendMode::BLEND_Masked:
        Result->SetStringField(TEXT("blendMode"), TEXT("Masked"));
        break;
      case EBlendMode::BLEND_Translucent:
        Result->SetStringField(TEXT("blendMode"), TEXT("Translucent"));
        break;
      case EBlendMode::BLEND_Additive:
        Result->SetStringField(TEXT("blendMode"), TEXT("Additive"));
        break;
      case EBlendMode::BLEND_Modulate:
        Result->SetStringField(TEXT("blendMode"), TEXT("Modulate"));
        break;
      default:
        Result->SetStringField(TEXT("blendMode"), TEXT("Unknown"));
        break;
      }

      Result->SetBoolField(TEXT("twoSided"), Material->TwoSided);
    } else {
      // UMaterialFunction basic info
      Result->SetStringField(TEXT("description"), Function->Description);
      Result->SetBoolField(TEXT("exposeToLibrary"), Function->bExposeToLibrary);
    }

    // --- Parameters (always separated for both Material and MF) ---
    if (bWantParams) {
      TArray<TSharedPtr<FJsonValue>> ParamsArray;
      for (UMaterialExpression *Expr : AllExpressions) {
        if (!Expr) continue;
        if (UMaterialExpressionParameter *Param = Cast<UMaterialExpressionParameter>(Expr)) {
          TSharedPtr<FJsonObject> ParamObj = McpHandlerUtils::CreateResultObject();
          ParamObj->SetStringField(TEXT("name"), Param->ParameterName.ToString());
          ParamObj->SetStringField(TEXT("type"), Expr->GetClass()->GetName());
          ParamObj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
          ParamsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
        }
      }
      Result->SetArrayField(TEXT("parameters"), ParamsArray);
    }

    // --- MF-specific: FunctionInput/FunctionOutput enumeration ---
    if (!Material) {
      TArray<TSharedPtr<FJsonValue>> InputsArray;
      TArray<TSharedPtr<FJsonValue>> OutputsArray;
      for (UMaterialExpression *Expr : AllExpressions) {
        if (!Expr) continue;
        if (UMaterialExpressionFunctionInput *In = Cast<UMaterialExpressionFunctionInput>(Expr)) {
          TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
          Obj->SetStringField(TEXT("name"), In->InputName.ToString());
          Obj->SetStringField(TEXT("type"), FunctionInputTypeToString(In->InputType));
          Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(In));
          Obj->SetBoolField(TEXT("usePreviewValueAsDefault"), In->bUsePreviewValueAsDefault);
          Obj->SetNumberField(TEXT("sortPriority"), In->SortPriority);
          Obj->SetStringField(TEXT("description"), In->Description);
          const auto PV = In->PreviewValue;
          TSharedPtr<FJsonObject> PreviewObj = MakeShared<FJsonObject>();
          PreviewObj->SetNumberField(TEXT("x"), PV.X);
          PreviewObj->SetNumberField(TEXT("y"), PV.Y);
          PreviewObj->SetNumberField(TEXT("z"), PV.Z);
          PreviewObj->SetNumberField(TEXT("w"), PV.W);
          Obj->SetObjectField(TEXT("previewValue"), PreviewObj);
          InputsArray.Add(MakeShared<FJsonValueObject>(Obj));
        } else if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
          Obj->SetStringField(TEXT("name"), Out->OutputName.ToString());
          Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Out));
          Obj->SetNumberField(TEXT("sortPriority"), Out->SortPriority);
          Obj->SetStringField(TEXT("description"), Out->Description);
          OutputsArray.Add(MakeShared<FJsonValueObject>(Obj));
        }
      }
      Result->SetArrayField(TEXT("inputs"), InputsArray);
      Result->SetArrayField(TEXT("outputs"), OutputsArray);
    }

    // --- Full expression list (types, nodeIds, positions) ---
    if (bWantExpressions) {
      TArray<TSharedPtr<FJsonValue>> ExprsArray;
      for (UMaterialExpression *Expr : AllExpressions) {
        if (!Expr) continue;
        TSharedPtr<FJsonObject> ExprObj = MakeShared<FJsonObject>();
        ExprObj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Expr));
        ExprObj->SetStringField(TEXT("type"), Expr->GetClass()->GetName());
        ExprObj->SetStringField(TEXT("desc"), Expr->GetDescription());
        ExprObj->SetNumberField(TEXT("x"), Expr->MaterialExpressionEditorX);
        ExprObj->SetNumberField(TEXT("y"), Expr->MaterialExpressionEditorY);
        // Add name for parameter/input/output nodes
        if (UMaterialExpressionParameter *P = Cast<UMaterialExpressionParameter>(Expr)) {
          ExprObj->SetStringField(TEXT("name"), P->ParameterName.ToString());
        } else if (UMaterialExpressionFunctionInput *FI = Cast<UMaterialExpressionFunctionInput>(Expr)) {
          ExprObj->SetStringField(TEXT("name"), FI->InputName.ToString());
        } else if (UMaterialExpressionFunctionOutput *FO = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          ExprObj->SetStringField(TEXT("name"), FO->OutputName.ToString());
        }
        // Include code for CustomExpression nodes
        if (UMaterialExpressionCustom *CE = Cast<UMaterialExpressionCustom>(Expr)) {
          ExprObj->SetStringField(TEXT("code"), CE->Code);
          ExprObj->SetNumberField(TEXT("inputCount"), CE->Inputs.Num());
          ExprObj->SetNumberField(TEXT("additionalOutputCount"), CE->AdditionalOutputs.Num());
        }
        ExprsArray.Add(MakeShared<FJsonValueObject>(ExprObj));
      }
      Result->SetArrayField(TEXT("expressions"), ExprsArray);
    }

    if (bWantConnections) {
      AppendMaterialInfoConnections(Material, Function, Payload, Result);
    }

    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           Material ? TEXT("Material info retrieved.")
                                    : TEXT("Material function info retrieved."),
                           Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // get_material_function_info (explicit MF introspection)
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
