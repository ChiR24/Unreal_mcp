// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridge_AssetWorkflowMaterialHost.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "McpSafeOperations.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleRemoveMaterialNode(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("remove_material_node"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("remove_material_node payload missing"),
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

  FString NodeId;
  int32 ExpressionIndex = -1;
  UMaterialExpression *ExpressionToRemove = nullptr;

  if (Payload->TryGetStringField(TEXT("nodeId"), NodeId) && !NodeId.IsEmpty()) {
    ExpressionToRemove = FindExpression(NodeId);
  } else if (Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex)) {
    if (ExpressionIndex >= 0 && ExpressionIndex < Expressions.Num()) {
      ExpressionToRemove = Expressions[ExpressionIndex];
    }
  }

  if (!ExpressionToRemove) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Node not found. Provide valid nodeId (stable name) or expressionIndex"),
                        TEXT("NODE_NOT_FOUND"));
    return true;
  }

  FString RemovedName = ExpressionToRemove->GetName();
  FString RemovedStableName = ExpressionToRemove->GetName();

  // Disconnect inbound links: walk all sibling expressions and clear any
  // FExpressionInput that references the node we're about to remove.
  for (UMaterialExpression *Expr : Expressions) {
    if (!Expr || Expr == ExpressionToRemove) continue;
    for (FProperty *Property = Expr->GetClass()->PropertyLink; Property;
         Property = Property->PropertyLinkNext) {
      FStructProperty *StructProp = CastField<FStructProperty>(Property);
      if (StructProp && StructProp->Struct &&
          StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"))) {
        FExpressionInput *Input =
            StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expr);
        if (Input && Input->Expression == ExpressionToRemove) {
          Input->Expression = nullptr;
          Input->OutputIndex = 0;
        }
      }
    }
  }

  // Disconnect from main material inputs (root node pins)
  if (Material) {
#if WITH_EDITORONLY_DATA
    auto ClearIfMatches = [&](FExpressionInput& Input) {
      if (Input.Expression == ExpressionToRemove) {
        Input.Expression = nullptr;
        Input.OutputIndex = 0;
      }
    };
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, BaseColor));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, EmissiveColor));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, Roughness));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, Metallic));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, Specular));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, Normal));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, Opacity));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, OpacityMask));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor));
    ClearIfMatches(MCP_GET_MATERIAL_INPUT(Material, WorldPositionOffset));
#endif
  }

  // Remove the expression from the appropriate container
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  if (Material) Material->GetExpressionCollection().RemoveExpression(ExpressionToRemove);
  else Function->GetExpressionCollection().RemoveExpression(ExpressionToRemove);
#else
  Expressions.Remove(ExpressionToRemove);
#endif

  // Also remove from the material's root node if connected (Material only)
  if (Material) Material->RemoveExpressionParameter(ExpressionToRemove);

  FinalizeHost(Material, Function);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  if (Material) McpHandlerUtils::AddVerification(Resp, Material);
  else if (Function) McpHandlerUtils::AddVerification(Resp, Function);
  Resp->SetStringField(TEXT("nodeId"), RemovedStableName);
  Resp->SetStringField(TEXT("removedName"), RemovedName);
  Resp->SetNumberField(TEXT("remainingExpressions"), Expressions.Num());
  Resp->SetBoolField(TEXT("removed"), true);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material node removed successfully"), Resp,
                         FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("remove_material_node requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleRebuildMaterial(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("rebuild_material"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) &&
      !Payload->TryGetStringField(TEXT("materialPath"), AssetPath)) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath or materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath cannot be empty"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid assetPath"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Load the material or material function
  UMaterial *Material = nullptr;
  UMaterialFunction *Function = nullptr;
  LoadMaterialOrFunctionAW(AssetPath, Material, Function);
  if (!Material && !Function) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Material or Material Function not found: %s"), *AssetPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Rebuild by triggering a recompile (already on game thread — no AsyncTask needed)
  UObject *Host = Material ? static_cast<UObject*>(Material) : static_cast<UObject*>(Function);
  Host->MarkPackageDirty();
  Host->PreEditChange(nullptr);
  Host->PostEditChange();

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (Material) McpHandlerUtils::AddVerification(Result, Material);
    else if (Function) McpHandlerUtils::AddVerification(Result, Function);
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetBoolField(TEXT("rebuilt"), true);
    Result->SetBoolField(TEXT("headlessSafe"), true);
    Result->SetBoolField(TEXT("saveSkipped"), true);

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Material rebuilt; save skipped under NullRHI"), Result, FString());
    return true;
  }

  if (!McpSafeAssetSave(Host)) {
    SendAutomationError(Socket, RequestId, TEXT("Failed to save rebuilt material"), TEXT("SAVE_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  if (Material) McpHandlerUtils::AddVerification(Result, Material);
  else if (Function) McpHandlerUtils::AddVerification(Result, Function);
  Result->SetStringField(TEXT("assetPath"), AssetPath);
  Result->SetBoolField(TEXT("rebuilt"), true);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material rebuilt successfully"), Result, FString());

  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor only."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif
}
