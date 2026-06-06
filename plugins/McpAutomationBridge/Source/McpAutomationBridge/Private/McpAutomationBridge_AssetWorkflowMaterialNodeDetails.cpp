// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridge_AssetWorkflowMaterialHost.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Engine/Texture.h"
#include "UObject/UnrealType.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleGetMaterialNodeDetails(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_material_node_details"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("get_material_node_details payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and materialPath
  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), MaterialPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid material path"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the material or material function
  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(MaterialPath, Material, Function);
  if (!Material && !Function) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material or Material Function not found: %s"), *MaterialPath),
                        TEXT("MATERIAL_NOT_FOUND"));
    return true;
  }

  auto& Expressions = GetHostExpressions(Material, Function);

  auto FindExpression = [&Expressions](const FString &Id) { return FindExpressionInHost(Expressions, Id); };

  FString NodeId;
  int32 ExpressionIndex = -1;
  UMaterialExpression *Expression = nullptr;

  if (Payload->TryGetStringField(TEXT("nodeId"), NodeId) && !NodeId.IsEmpty()) {
    Expression = FindExpression(NodeId);
  } else if (Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex)) {
    if (ExpressionIndex >= 0 && ExpressionIndex < Expressions.Num()) Expression = Expressions[ExpressionIndex];
  }

  if (!Expression) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    if (Material) McpHandlerUtils::AddVerification(Resp, Material);
    else if (Function) McpHandlerUtils::AddVerification(Resp, Function);

    TArray<TSharedPtr<FJsonValue>> NodeList;
    for (int32 i = 0; i < Expressions.Num(); ++i) {
      UMaterialExpression *Expr = Expressions[i];
      if (!Expr) continue;

      TSharedPtr<FJsonObject> NodeInfo = McpHandlerUtils::CreateResultObject();
      NodeInfo->SetStringField(TEXT("nodeId"), Expr->MaterialExpressionGuid.ToString());
      NodeInfo->SetStringField(TEXT("nodeType"), Expr->GetClass()->GetName());
      NodeInfo->SetNumberField(TEXT("index"), i);
      NodeInfo->SetNumberField(TEXT("editorX"), Expr->MaterialExpressionEditorX);
      NodeInfo->SetNumberField(TEXT("editorY"), Expr->MaterialExpressionEditorY);
      if (!Expr->Desc.IsEmpty()) {
        NodeInfo->SetStringField(TEXT("desc"), Expr->Desc);
      }
      // Add parameter name if applicable
      if (UMaterialExpressionParameter *Param = Cast<UMaterialExpressionParameter>(Expr)) {
        NodeInfo->SetStringField(TEXT("parameterName"), Param->ParameterName.ToString());
      }
      // Add function path for MaterialFunctionCall nodes
      if (UMaterialExpressionMaterialFunctionCall *FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expr)) {
        if (FuncCall->MaterialFunction) {
          NodeInfo->SetStringField(TEXT("functionPath"), FuncCall->MaterialFunction->GetPathName());
        }
      }
      // Add pin name for FunctionInput/Output expressions
      if (UMaterialExpressionFunctionInput *FuncIn = Cast<UMaterialExpressionFunctionInput>(Expr)) {
        NodeInfo->SetStringField(TEXT("inputName"), FuncIn->InputName.ToString());
      }
      if (UMaterialExpressionFunctionOutput *FuncOut = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
        NodeInfo->SetStringField(TEXT("outputName"), FuncOut->OutputName.ToString());
      }
      NodeList.Add(MakeShared<FJsonValueObject>(NodeInfo));
    }

    Resp->SetArrayField(TEXT("nodes"), NodeList);
    Resp->SetNumberField(TEXT("nodeCount"), Expressions.Num());

    FString Message = NodeId.IsEmpty()
        ? FString::Printf(TEXT("Material has %d nodes. Provide nodeId for specific node details."), Expressions.Num())
        : FString::Printf(TEXT("Node '%s' not found. Material has %d nodes."), *NodeId, Expressions.Num());

    SendAutomationResponse(Socket, RequestId, NodeId.IsEmpty(),
                           Message, Resp, NodeId.IsEmpty() ? FString() : TEXT("NODE_NOT_FOUND"));
    return true;
  }

  // Build response for specific node
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (Material) McpHandlerUtils::AddVerification(Resp, Material);
  else if (Function) McpHandlerUtils::AddVerification(Resp, Function);
  Resp->SetStringField(TEXT("nodeId"), Expression->MaterialExpressionGuid.ToString());
  Resp->SetStringField(TEXT("name"), Expression->GetName());
  Resp->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
  Resp->SetStringField(TEXT("classPath"), Expression->GetClass()->GetPathName());
  Resp->SetNumberField(TEXT("editorX"), Expression->MaterialExpressionEditorX);
  Resp->SetNumberField(TEXT("editorY"), Expression->MaterialExpressionEditorY);
  if (!Expression->Desc.IsEmpty()) {
    Resp->SetStringField(TEXT("desc"), Expression->Desc);
  }

  // Get inputs
  TArray<TSharedPtr<FJsonValue>> InputsArray;
  for (FProperty *Property = Expression->GetClass()->PropertyLink; Property;
       Property = Property->PropertyLinkNext) {
    if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        FExpressionInput *Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expression);
        TSharedPtr<FJsonObject> InputObj = McpHandlerUtils::CreateResultObject();
        InputObj->SetStringField(TEXT("name"), Property->GetName());
        InputObj->SetBoolField(TEXT("isConnected"), Input->Expression != nullptr);
        if (Input->Expression) {
          InputObj->SetStringField(TEXT("connectedToId"), Input->Expression->GetName());
          InputObj->SetStringField(TEXT("connectedToName"), Input->Expression->GetName());
        }
        InputsArray.Add(MakeShared<FJsonValueObject>(InputObj));
      }
    }
  }
  Resp->SetArrayField(TEXT("inputs"), InputsArray);

  // Get specific properties based on expression type
  if (UMaterialExpressionConstant *Const = Cast<UMaterialExpressionConstant>(Expression)) {
    Resp->SetNumberField(TEXT("value"), Const->R);
  } else if (UMaterialExpressionConstant2Vector *Const2 = Cast<UMaterialExpressionConstant2Vector>(Expression)) {
    TSharedPtr<FJsonObject> ValueObj = McpHandlerUtils::CreateResultObject();
    ValueObj->SetNumberField(TEXT("r"), Const2->R);
    ValueObj->SetNumberField(TEXT("g"), Const2->G);
    Resp->SetObjectField(TEXT("value"), ValueObj);
  } else if (UMaterialExpressionConstant3Vector *Const3 = Cast<UMaterialExpressionConstant3Vector>(Expression)) {
    TSharedPtr<FJsonObject> ValueObj = McpHandlerUtils::CreateResultObject();
    ValueObj->SetNumberField(TEXT("r"), Const3->Constant.R);
    ValueObj->SetNumberField(TEXT("g"), Const3->Constant.G);
    ValueObj->SetNumberField(TEXT("b"), Const3->Constant.B);
    Resp->SetObjectField(TEXT("value"), ValueObj);
  } else if (UMaterialExpressionConstant4Vector *Const4 = Cast<UMaterialExpressionConstant4Vector>(Expression)) {
    TSharedPtr<FJsonObject> ValueObj = McpHandlerUtils::CreateResultObject();
    ValueObj->SetNumberField(TEXT("r"), Const4->Constant.R);
    ValueObj->SetNumberField(TEXT("g"), Const4->Constant.G);
    ValueObj->SetNumberField(TEXT("b"), Const4->Constant.B);
    ValueObj->SetNumberField(TEXT("a"), Const4->Constant.A);
    Resp->SetObjectField(TEXT("value"), ValueObj);
  } else if (UMaterialExpressionTextureSample *TexSample = Cast<UMaterialExpressionTextureSample>(Expression)) {
    if (TexSample->Texture) {
      Resp->SetStringField(TEXT("texture"), TexSample->Texture->GetPathName());
      Resp->SetStringField(TEXT("textureName"), TexSample->Texture->GetName());
    }
  } else if (UMaterialExpressionScalarParameter *ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression)) {
    Resp->SetStringField(TEXT("parameterName"), ScalarParam->ParameterName.ToString());
    Resp->SetNumberField(TEXT("defaultValue"), ScalarParam->DefaultValue);
  } else if (UMaterialExpressionVectorParameter *VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression)) {
    Resp->SetStringField(TEXT("parameterName"), VectorParam->ParameterName.ToString());
    TSharedPtr<FJsonObject> DefaultObj = McpHandlerUtils::CreateResultObject();
    DefaultObj->SetNumberField(TEXT("r"), VectorParam->DefaultValue.R);
    DefaultObj->SetNumberField(TEXT("g"), VectorParam->DefaultValue.G);
    DefaultObj->SetNumberField(TEXT("b"), VectorParam->DefaultValue.B);
    DefaultObj->SetNumberField(TEXT("a"), VectorParam->DefaultValue.A);
    Resp->SetObjectField(TEXT("defaultValue"), DefaultObj);
  }

  // Expose function pin metadata for MaterialFunctionCall nodes
  if (UMaterialExpressionMaterialFunctionCall *FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression)) {
    if (FuncCall->MaterialFunction) {
      Resp->SetStringField(TEXT("functionPath"), FuncCall->MaterialFunction->GetPathName());
    }
    // Emit function inputs
    TArray<TSharedPtr<FJsonValue>> FuncInputs;
    for (int32 fi = 0; fi < FuncCall->FunctionInputs.Num(); ++fi) {
      TSharedPtr<FJsonObject> FIObj = McpHandlerUtils::CreateResultObject();
      const UMaterialExpressionFunctionInput* FuncInputExpr = FuncCall->FunctionInputs[fi].ExpressionInput;
      FIObj->SetStringField(TEXT("inputName"), FuncInputExpr ? FuncInputExpr->InputName.ToString() : FString());
      FIObj->SetNumberField(TEXT("index"), fi);
      FIObj->SetBoolField(TEXT("isConnected"), FuncCall->FunctionInputs[fi].Input.Expression != nullptr);
      if (FuncCall->FunctionInputs[fi].Input.Expression) {
        FIObj->SetStringField(TEXT("connectedToId"), FuncCall->FunctionInputs[fi].Input.Expression->GetName());
        FIObj->SetNumberField(TEXT("outputIndex"), FuncCall->FunctionInputs[fi].Input.OutputIndex);
      }
      FuncInputs.Add(MakeShared<FJsonValueObject>(FIObj));
    }
    Resp->SetArrayField(TEXT("functionInputs"), FuncInputs);

    // Emit function outputs
    TArray<TSharedPtr<FJsonValue>> FuncOutputs;
    for (int32 fo = 0; fo < FuncCall->FunctionOutputs.Num(); ++fo) {
      TSharedPtr<FJsonObject> FOObj = McpHandlerUtils::CreateResultObject();
      const UMaterialExpressionFunctionOutput* FuncOutputExpr = FuncCall->FunctionOutputs[fo].ExpressionOutput;
      FOObj->SetStringField(TEXT("outputName"), FuncOutputExpr ? FuncOutputExpr->OutputName.ToString() : FString());
      FOObj->SetNumberField(TEXT("index"), fo);
      FuncOutputs.Add(MakeShared<FJsonValueObject>(FOObj));
    }
    Resp->SetArrayField(TEXT("functionOutputs"), FuncOutputs);
  }

  // Expose function input/output pin metadata for FunctionInput/Output expressions
  if (UMaterialExpressionFunctionInput *FuncIn = Cast<UMaterialExpressionFunctionInput>(Expression)) {
    Resp->SetStringField(TEXT("inputName"), FuncIn->InputName.ToString());
  }
  if (UMaterialExpressionFunctionOutput *FuncOut = Cast<UMaterialExpressionFunctionOutput>(Expression)) {
    Resp->SetStringField(TEXT("outputName"), FuncOut->OutputName.ToString());
    Resp->SetBoolField(TEXT("isConnected"), FuncOut->A.Expression != nullptr);
    if (FuncOut->A.Expression) {
      Resp->SetStringField(TEXT("connectedToId"), FuncOut->A.Expression->GetName());
      Resp->SetNumberField(TEXT("sourceOutputIndex"), FuncOut->A.OutputIndex);
    }
  }

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material node details retrieved"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_material_node_details requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// SOURCE CONTROL STATE
// ============================================================================
