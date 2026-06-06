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

bool UMcpAutomationBridgeSubsystem::HandleBreakMaterialConnections(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("break_material_connections"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("break_material_connections payload missing"),
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

  FString NodeId, PinName;
  bool bHasNodeId = Payload->TryGetStringField(TEXT("nodeId"), NodeId) && !NodeId.IsEmpty();
  bool bHasPinName = Payload->TryGetStringField(TEXT("pinName"), PinName) && !PinName.IsEmpty();

  // If nodeId is "Main" or empty with pinName, disconnect from main/output node
  if ((!bHasNodeId || NodeId == TEXT("Main")) && bHasPinName) {
    if (Material) {
      bool bFound = false;
#if WITH_EDITORONLY_DATA
      auto ClearMainPin = [&](FExpressionInput& Input) { Input.Expression = nullptr; Input.OutputIndex = 0; bFound = true; };
      if (PinName == TEXT("BaseColor")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, BaseColor)); }
      else if (PinName == TEXT("EmissiveColor")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor)); }
      else if (PinName == TEXT("Roughness")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Roughness)); }
      else if (PinName == TEXT("Metallic")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Metallic)); }
      else if (PinName == TEXT("Specular")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Specular)); }
      else if (PinName == TEXT("Normal")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Normal)); }
      else if (PinName == TEXT("Opacity")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, Opacity)); }
      else if (PinName == TEXT("OpacityMask")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, OpacityMask)); }
      else if (PinName == TEXT("AmbientOcclusion") || PinName == TEXT("AO")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion)); }
      else if (PinName == TEXT("SubsurfaceColor")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor)); }
      else if (PinName == TEXT("WorldPositionOffset")) { ClearMainPin(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset)); }
#endif
      if (bFound) {
        FinalizeHost(Material, Function);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        McpHandlerUtils::AddVerification(Resp, Material);
        Resp->SetStringField(TEXT("pinName"), PinName);
        Resp->SetBoolField(TEXT("disconnected"), true);
        SendAutomationResponse(Socket, RequestId, true, TEXT("Disconnected from main material pin"), Resp, FString());
      } else {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unknown main material pin: %s"), *PinName), TEXT("INVALID_PIN"));
      }
    } else {
      // MaterialFunction: clear FunctionOutput by name
      bool bCleared = false;
      for (UMaterialExpression *Expr : Expressions) {
        if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
          if (PinName.IsEmpty() || Out->OutputName.ToString().Equals(PinName)) {
            Out->A.Expression = nullptr; Out->A.OutputIndex = 0; bCleared = true;
            if (!PinName.IsEmpty()) break;
          }
        }
      }
      if (!bCleared && !PinName.IsEmpty()) {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Unknown function output pin: %s"), *PinName), TEXT("INVALID_PIN"));
      } else {
        if (bCleared) FinalizeHost(Material, Function);
        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("pinName"), PinName);
        Resp->SetBoolField(TEXT("disconnected"), bCleared);
        SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Disconnected from function output"),
                               Resp, FString());
      }
    }
    return true;
  }

  int32 ExpressionIndex = -1;
  UMaterialExpression *TargetExpression = nullptr;
  if (bHasNodeId) TargetExpression = FindExpression(NodeId);
  else if (Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex)) {
    if (ExpressionIndex >= 0 && ExpressionIndex < Expressions.Num()) TargetExpression = Expressions[ExpressionIndex];
  }

  if (!TargetExpression) {
    SendAutomationError(Socket, RequestId, TEXT("Node not found. Provide valid nodeId (stable name) or expressionIndex"), TEXT("NODE_NOT_FOUND"));
    return true;
  }

  FString InputName;
  bool bSpecificInput = Payload->TryGetStringField(TEXT("inputName"), InputName) && !InputName.IsEmpty();
  int32 BrokenConnections = 0;

  for (FProperty *Property = TargetExpression->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext) {
    if (FStructProperty *StructProp = CastField<FStructProperty>(Property)) {
      if (StructProp->Struct && StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        if (bSpecificInput && !Property->GetName().Equals(InputName, ESearchCase::IgnoreCase)) continue;
        FExpressionInput *Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(TargetExpression);
        if (Input && Input->Expression) { Input->Expression = nullptr; Input->OutputIndex = 0; BrokenConnections++; if (bSpecificInput) break; }
      }
    }
  }

  if (bSpecificInput && BrokenConnections == 0) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("No input named '%s' found on node '%s'"), *InputName, *TargetExpression->GetName()),
                        TEXT("INPUT_NOT_FOUND"));
    return true;
  }

  if (BrokenConnections > 0) {
    FinalizeHost(Material, Function);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (Material) McpHandlerUtils::AddVerification(Resp, Material);
  else if (Function) McpHandlerUtils::AddVerification(Resp, Function);
  Resp->SetStringField(TEXT("nodeId"), TargetExpression->MaterialExpressionGuid.ToString());
  Resp->SetNumberField(TEXT("brokenConnections"), BrokenConnections);
  if (bSpecificInput) Resp->SetStringField(TEXT("inputName"), InputName);

  SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Broken %d connection(s)"), BrokenConnections), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("break_material_connections requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
