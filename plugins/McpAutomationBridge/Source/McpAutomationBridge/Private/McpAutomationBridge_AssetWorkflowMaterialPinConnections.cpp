// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridge_AssetWorkflowMaterialHost.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "UObject/UnrealType.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleConnectMaterialPins(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("connect_material_pins"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("connect_material_pins payload missing"),
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

  // Helper to find expression by stable name, GUID (legacy), or index
  auto FindExpression = [&Expressions](const FString &Id) { return FindExpressionInHost(Expressions, Id); };

  // Accept both sourceNodeId/targetNodeId (stable name strings) and fromExpression/toExpression (indices)
  FString SourceNodeId, TargetNodeId;
  int32 FromExpressionIndex = -1, ToExpressionIndex = -1;

  UMaterialExpression *FromExpression = nullptr;
  UMaterialExpression *ToExpression = nullptr;

  if (Payload->TryGetStringField(TEXT("sourceNodeId"), SourceNodeId) && !SourceNodeId.IsEmpty()) {
    FromExpression = FindExpression(SourceNodeId);
  }
  if (Payload->TryGetStringField(TEXT("targetNodeId"), TargetNodeId) && !TargetNodeId.IsEmpty()) {
    ToExpression = FindExpression(TargetNodeId);
  }

  if (!FromExpression && Payload->TryGetNumberField(TEXT("fromExpression"), FromExpressionIndex)) {
    if (FromExpressionIndex >= 0 && FromExpressionIndex < Expressions.Num()) FromExpression = Expressions[FromExpressionIndex];
  }
  if (!ToExpression && Payload->TryGetNumberField(TEXT("toExpression"), ToExpressionIndex)) {
    if (ToExpressionIndex >= 0 && ToExpressionIndex < Expressions.Num()) ToExpression = Expressions[ToExpressionIndex];
  }

  FString SourcePin;
  Payload->TryGetStringField(TEXT("sourcePin"), SourcePin);
  int32 SourcePinIndex = 0;
  if (!SourcePin.IsEmpty()) {
    if (SourcePin.IsNumeric()) {
      SourcePinIndex = FCString::Atoi(*SourcePin);
    } else {
      SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("sourcePin must be a numeric index, got '%s'"), *SourcePin),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }
  }

  FString InputName;
  Payload->TryGetStringField(TEXT("inputName"), InputName);
  if (InputName.IsEmpty()) Payload->TryGetStringField(TEXT("targetPin"), InputName);

  // Handle connection to main material / function output node
  bool bConnectToMainNode = false;
  if ((TargetNodeId.IsEmpty() || TargetNodeId == TEXT("Main")) && !InputName.IsEmpty()) {
    bConnectToMainNode = true;
  } else if (!TargetNodeId.IsEmpty() && ToExpression == nullptr) {
    SendAutomationError(Socket, RequestId, TEXT("Target node not found"), TEXT("TARGET_NODE_NOT_FOUND"));
    return true;
  }

  if (bConnectToMainNode && FromExpression) {
    if (Material) {
      bool bFound = false;
      auto ConnectMainInput = [&](FExpressionInput& Input) {
        Input.Expression = FromExpression;
        Input.OutputIndex = SourcePinIndex;
        bFound = true;
      };
#if WITH_EDITORONLY_DATA
      if (InputName == TEXT("BaseColor")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, BaseColor)); }
      else if (InputName == TEXT("EmissiveColor")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor)); }
      else if (InputName == TEXT("Roughness")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, Roughness)); }
      else if (InputName == TEXT("Metallic")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, Metallic)); }
      else if (InputName == TEXT("Specular")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, Specular)); }
      else if (InputName == TEXT("Normal")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, Normal)); }
      else if (InputName == TEXT("Opacity")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, Opacity)); }
      else if (InputName == TEXT("OpacityMask")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, OpacityMask)); }
      else if (InputName == TEXT("AmbientOcclusion") || InputName == TEXT("AO")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion)); }
      else if (InputName == TEXT("SubsurfaceColor")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor)); }
      else if (InputName == TEXT("WorldPositionOffset")) { ConnectMainInput(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset)); }
#endif
      if (bFound) {
        FinalizeHost(Material, Function);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        McpHandlerUtils::AddVerification(Resp, Material);
        Resp->SetStringField(TEXT("inputName"), InputName);
        Resp->SetStringField(TEXT("sourceNodeId"), FromExpression->GetName());
        SendAutomationResponse(Socket, RequestId, true, TEXT("Connected to main material pin"), Resp, FString());
      } else {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unknown main material input: %s"), *InputName), TEXT("INVALID_PIN"));
      }
      return true;
    } else {
      // MaterialFunction: connect to FunctionOutput by name (name is required)
      if (InputName.IsEmpty()) {
        SendAutomationError(Socket, RequestId, TEXT("inputName is required when connecting to a function output"), TEXT("MISSING_INPUT_NAME"));
        return true;
      }
      UMaterialExpressionFunctionOutput *TargetOutput = nullptr;
      for (UMaterialExpression *Expr : Expressions) {
        if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          if (Out->OutputName.ToString().Equals(InputName)) { TargetOutput = Out; break; }
        }
      }
      if (TargetOutput) {
        TargetOutput->A.Expression = FromExpression;
        TargetOutput->A.OutputIndex = SourcePinIndex;
        FinalizeHost(Material, Function);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("inputName"), TargetOutput->OutputName.ToString());
        Resp->SetStringField(TEXT("sourceNodeId"), FromExpression->GetName());
        SendAutomationResponse(Socket, RequestId, true, TEXT("Connected to function output"), Resp, FString());
      } else {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No FunctionOutput named '%s' found"), *InputName), TEXT("INVALID_PIN"));
      }
      return true;
    }
  }

  if (!FromExpression) { SendAutomationError(Socket, RequestId, TEXT("Source node not found"), TEXT("SOURCE_NODE_NOT_FOUND")); return true; }
  if (!ToExpression) { SendAutomationError(Socket, RequestId, TEXT("Target node not found"), TEXT("TARGET_NODE_NOT_FOUND")); return true; }

  if (InputName.IsEmpty()) InputName = TEXT("Input");

  FExpressionInput *TargetInput = nullptr;
  for (FProperty *Property = ToExpression->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext) {
    if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        if (Property->GetName().Equals(InputName, ESearchCase::IgnoreCase)) {
          TargetInput = StructProp->ContainerPtrToValuePtr<FExpressionInput>(ToExpression);
          break;
        }
      }
    }
  }
  if (!TargetInput) {
    for (FProperty *Property = ToExpression->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext) {
      if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
        if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
          TargetInput = StructProp->ContainerPtrToValuePtr<FExpressionInput>(ToExpression);
          InputName = Property->GetName();
          break;
        }
      }
    }
  }

  if (!TargetInput) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No input found on target expression. Tried: %s"), *InputName), TEXT("INPUT_NOT_FOUND"));
    return true;
  }

  TargetInput->Expression = FromExpression;
  TargetInput->OutputIndex = SourcePinIndex;
  FinalizeHost(Material, Function);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (Material) McpHandlerUtils::AddVerification(Resp, Material);
  else if (Function) McpHandlerUtils::AddVerification(Resp, Function);
  Resp->SetStringField(TEXT("sourceNodeId"), FromExpression->GetName());
  Resp->SetStringField(TEXT("targetNodeId"), ToExpression->GetName());
  Resp->SetStringField(TEXT("inputName"), InputName);

  SendAutomationResponse(Socket, RequestId, true, TEXT("Material pins connected successfully"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("connect_material_pins requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
