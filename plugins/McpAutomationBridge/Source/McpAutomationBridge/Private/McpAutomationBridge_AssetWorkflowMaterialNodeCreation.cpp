// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridge_AssetWorkflowMaterialHost.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "Engine/Texture.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVertexColor.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialNode(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("add_material_node"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("add_material_node payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) ||
      MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("materialPath is required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  MaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Invalid materialPath"),
                        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  FString NodeType;
  if (!Payload->TryGetStringField(TEXT("nodeType"), NodeType) ||
      NodeType.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("nodeType is required"),
                        TEXT("INVALID_ARGUMENT"));
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

  UObject *HostOuter = Material ? static_cast<UObject*>(Material)
                                : static_cast<UObject*>(Function);

  // Create material expression based on node type
  UMaterialExpression *NewExpression = nullptr;
  UClass *ExpressionClass = nullptr;

  // Map common node type names to expression classes
  if (NodeType.Equals(TEXT("Constant"), ESearchCase::IgnoreCase) ||
      NodeType.Equals(TEXT("Constant1"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant::StaticClass();
  } else if (NodeType.Equals(TEXT("Constant2"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Constant2Vector"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant2Vector::StaticClass();
  } else if (NodeType.Equals(TEXT("Constant3"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Constant3Vector"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant3Vector::StaticClass();
  } else if (NodeType.Equals(TEXT("Constant4"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Constant4Vector"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionConstant4Vector::StaticClass();
  } else if (NodeType.Equals(TEXT("TextureSample"), ESearchCase::IgnoreCase) ||
             NodeType.Equals(TEXT("Texture"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionTextureSample::StaticClass();
  } else if (NodeType.Equals(TEXT("Add"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionAdd::StaticClass();
  } else if (NodeType.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionMultiply::StaticClass();
  } else if (NodeType.Equals(TEXT("Sine"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionSine::StaticClass();
  } else if (NodeType.Equals(TEXT("Cosine"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionCosine::StaticClass();
  } else if (NodeType.Equals(TEXT("Time"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionTime::StaticClass();
  } else if (NodeType.Equals(TEXT("VertexColor"), ESearchCase::IgnoreCase)) {
    ExpressionClass = UMaterialExpressionVertexColor::StaticClass();
  } else {
    // Try to find the class dynamically
    FString FullClassName = FString::Printf(TEXT("/Script/Engine.MaterialExpression%s"), *NodeType);
    ExpressionClass = LoadClass<UMaterialExpression>(nullptr, *FullClassName);

    if (!ExpressionClass) {
      SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Unknown node type: %s"), *NodeType),
                          TEXT("INVALID_NODE_TYPE"));
      return true;
    }
  }

  // Create the expression
  NewExpression = NewObject<UMaterialExpression>(HostOuter, ExpressionClass, NAME_None, RF_Transactional);
  if (!NewExpression) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create material expression"),
                        TEXT("EXPRESSION_CREATION_FAILED"));
    return true;
  }

  // Set position
  double PosX = 0, PosY = 0;
  Payload->TryGetNumberField(TEXT("posX"), PosX);
  Payload->TryGetNumberField(TEXT("posY"), PosY);
  NewExpression->MaterialExpressionEditorX = static_cast<int32>(PosX);
  NewExpression->MaterialExpressionEditorY = static_cast<int32>(PosY);

  // Set node properties based on type
  if (UMaterialExpressionConstant *Const = Cast<UMaterialExpressionConstant>(NewExpression)) {
    double Value = 0;
    Payload->TryGetNumberField(TEXT("value"), Value);
    Const->R = static_cast<float>(Value);
  } else if (UMaterialExpressionConstant3Vector *Const3 = Cast<UMaterialExpressionConstant3Vector>(NewExpression)) {
    double R = 0, G = 0, B = 0;
    const TSharedPtr<FJsonObject> *ColorObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("color"), ColorObj) && ColorObj) {
      (*ColorObj)->TryGetNumberField(TEXT("r"), R);
      (*ColorObj)->TryGetNumberField(TEXT("g"), G);
      (*ColorObj)->TryGetNumberField(TEXT("b"), B);
    }
    Const3->Constant = FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B));
  } else if (UMaterialExpressionTextureSample *TexSample = Cast<UMaterialExpressionTextureSample>(NewExpression)) {
    FString TexturePath;
    if (Payload->TryGetStringField(TEXT("texturePath"), TexturePath) && !TexturePath.IsEmpty()) {
      TexturePath = SanitizeProjectRelativePath(TexturePath);
      if (TexturePath.IsEmpty()) {
        SendAutomationError(Socket, RequestId,
                            TEXT("Invalid texturePath"),
                            TEXT("SECURITY_VIOLATION"));
        return true;
      }
      UTexture *Texture = LoadObject<UTexture>(nullptr, *TexturePath);
      if (Texture) {
        TexSample->Texture = Texture;
      }
    }
  }

  // Add to host expression collection
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  if (Material) Material->GetEditorOnlyData()->ExpressionCollection.AddExpression(NewExpression);
  else Function->GetEditorOnlyData()->ExpressionCollection.AddExpression(NewExpression);
#else
  auto& Expressions = GetHostExpressions(Material, Function);
  Expressions.Add(NewExpression);
#endif

  // Only mark dirty — skip PostEditChange to avoid shader recompile per node.
  // Users batch-add nodes and compile once via compile_material.
  if (Material) { Material->MarkPackageDirty(); }
  if (Function) { Function->MarkPackageDirty(); }

  // Get the expression index for reference
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  int32 ExpressionIndex = Material
    ? Material->GetEditorOnlyData()->ExpressionCollection.Expressions.IndexOfByKey(NewExpression)
    : Function->GetEditorOnlyData()->ExpressionCollection.Expressions.IndexOfByKey(NewExpression);
#else
  int32 ExpressionIndex = Expressions.IndexOfByKey(NewExpression);
#endif

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("materialPath"), MaterialPath);
  Resp->SetStringField(TEXT("nodeType"), NodeType);
  Resp->SetNumberField(TEXT("expressionIndex"), ExpressionIndex);
  Resp->SetStringField(TEXT("expressionName"), NewExpression->GetName());
  Resp->SetStringField(TEXT("nodeId"), NewExpression->MaterialExpressionGuid.ToString());

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Material node added successfully"), Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("add_material_node requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
