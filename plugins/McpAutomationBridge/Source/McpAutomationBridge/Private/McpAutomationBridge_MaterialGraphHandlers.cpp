#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Engine/Texture.h"

// UE 5.0 vs 5.1+ API compatibility macros
// UE 5.0: Direct member access (Material->Expressions, Material->BaseColor, etc.)
// UE 5.1+: Access via GetEditorOnlyData() struct
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_GET_MATERIAL_EXPRESSIONS(Material) (Material)->GetEditorOnlyData()->ExpressionCollection.Expressions
#define MCP_GET_MATERIAL_INPUT(Material, InputName) (Material)->GetEditorOnlyData()->InputName
#else
#define MCP_GET_MATERIAL_EXPRESSIONS(Material) (Material)->Expressions
#define MCP_GET_MATERIAL_INPUT(Material, InputName) (Material)->InputName
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandleMaterialGraphAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (Action != TEXT("manage_material_graph")) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
      AssetPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
  if (!Material) {
    SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FString SubAction;
  if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) ||
      SubAction.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Missing 'subAction' for manage_material_graph"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  auto FindExpressionByIdOrName =
      [&](const FString &IdOrName) -> UMaterialExpression * {
    if (IdOrName.IsEmpty()) {
      return nullptr;
    }

    const FString Needle = IdOrName.TrimStartAndEnd();
    for (UMaterialExpression *Expr : MCP_GET_MATERIAL_EXPRESSIONS(Material)) {
      if (!Expr) {
        continue;
      }
      if (Expr->MaterialExpressionGuid.ToString() == Needle) {
        return Expr;
      }
      if (Expr->GetName() == Needle) {
        return Expr;
      }
      // Some callers may pass a full object path.
      if (Expr->GetPathName() == Needle) {
        return Expr;
      }

      // Also check ParameterName if it's a parameter node
      if (UMaterialExpressionParameter *ParamExpr =
              Cast<UMaterialExpressionParameter>(Expr)) {
        if (ParamExpr->ParameterName.ToString() == Needle) {
          return Expr;
        }
      }
    }
    return nullptr;
  };

  if (SubAction == TEXT("add_node")) {
    FString NodeType;
    Payload->TryGetStringField(TEXT("nodeType"), NodeType);
    float X = 0.0f;
    float Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    UClass *ExpressionClass = nullptr;
    if (NodeType == TEXT("TextureSample"))
      ExpressionClass = UMaterialExpressionTextureSample::StaticClass();
    else if (NodeType == TEXT("VectorParameter") ||
             NodeType == TEXT("ConstantVectorParameter"))
      ExpressionClass = UMaterialExpressionVectorParameter::StaticClass();
    else if (NodeType == TEXT("ScalarParameter") ||
             NodeType == TEXT("ConstantScalarParameter"))
      ExpressionClass = UMaterialExpressionScalarParameter::StaticClass();
    else if (NodeType == TEXT("Add"))
      ExpressionClass = UMaterialExpressionAdd::StaticClass();
    else if (NodeType == TEXT("Multiply"))
      ExpressionClass = UMaterialExpressionMultiply::StaticClass();
    else if (NodeType == TEXT("Constant") || NodeType == TEXT("Float") ||
             NodeType == TEXT("Scalar"))
      ExpressionClass = UMaterialExpressionConstant::StaticClass();
    else if (NodeType == TEXT("Constant3Vector") ||
             NodeType == TEXT("ConstantVector") || NodeType == TEXT("Color") ||
             NodeType == TEXT("Vector3"))
      ExpressionClass = UMaterialExpressionConstant3Vector::StaticClass();
    else {
      // Try resolve class by full path or partial name
      ExpressionClass = ResolveClassByName(NodeType);
      // Also try with MaterialExpression prefix
      if (!ExpressionClass ||
          !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
        FString PrefixedName =
            FString::Printf(TEXT("MaterialExpression%s"), *NodeType);
        ExpressionClass = ResolveClassByName(PrefixedName);
      }
      if (!ExpressionClass ||
          !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
        // Provide helpful error with available types
        SendAutomationError(
            Socket, RequestId,
            FString::Printf(
                TEXT("Unknown node type: %s. Available types: TextureSample, "
                     "VectorParameter, ScalarParameter, Add, Multiply, "
                     "Constant, Constant3Vector, "
                     "Color, ConstantVectorParameter. Or use full class name "
                     "like 'MaterialExpressionLerp'."),
                *NodeType),
            TEXT("UNKNOWN_TYPE"));
        return true;
      }
    }

    UMaterialExpression *NewExpr = NewObject<UMaterialExpression>(
        Material, ExpressionClass, NAME_None, RF_Transactional);
    if (NewExpr) {
      NewExpr->MaterialExpressionEditorX = (int32)X;
      NewExpr->MaterialExpressionEditorY = (int32)Y;
#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      if (Material->GetEditorOnlyData()) {
        MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(NewExpr);
      }
#else
      // UE 5.0: Direct access
      Material->Expressions.Add(NewExpr);
#endif
#endif

      // If parameter, set name
      FString ParamName;
      if (Payload->TryGetStringField(TEXT("name"), ParamName)) {
        if (UMaterialExpressionParameter *ParamExpr =
                Cast<UMaterialExpressionParameter>(NewExpr)) {
          ParamExpr->ParameterName = FName(*ParamName);
        }
      }

      Material->PostEditChange();
      Material->MarkPackageDirty();

      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("nodeId"),
                             NewExpr->MaterialExpressionGuid.ToString());
      SendAutomationResponse(Socket, RequestId, true, TEXT("Node added."),
                             Result);
    } else {
      SendAutomationError(Socket, RequestId,
                          TEXT("Failed to create expression."),
                          TEXT("CREATE_FAILED"));
    }
    return true;
  } else if (SubAction == TEXT("remove_node")) {
    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);

    if (NodeId.IsEmpty()) {
      SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpression *TargetExpr = FindExpressionByIdOrName(NodeId);

    if (TargetExpr) {
      FString RemovedNodeId = TargetExpr->MaterialExpressionGuid.ToString();
#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      if (Material->GetEditorOnlyData()) {
        MCP_GET_MATERIAL_EXPRESSIONS(Material).Remove(TargetExpr);
      }
#else
      // UE 5.0: Direct access
      Material->Expressions.Remove(TargetExpr);
#endif
#endif
      Material->PostEditChange();
      Material->MarkPackageDirty();
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("nodeId"), RemovedNodeId);
      Result->SetBoolField(TEXT("removed"), true);
      SendAutomationResponse(Socket, RequestId, true, TEXT("Node removed."), Result);
    } else {
      SendAutomationError(Socket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
    }
    return true;
  } else if (SubAction == TEXT("connect_nodes") ||
             SubAction == TEXT("connect_pins")) {
    // Material graph connections are complex because inputs are structs on the
    // expression, not EdGraph pins We need to find the target expression and
    // set its input
    FString SourceNodeId, TargetNodeId, InputName;
    Payload->TryGetStringField(TEXT("sourceNodeId"), SourceNodeId);
    Payload->TryGetStringField(TEXT("targetNodeId"), TargetNodeId);
    Payload->TryGetStringField(TEXT("inputName"), InputName);

    UMaterialExpression *SourceExpr = FindExpressionByIdOrName(SourceNodeId);

    if (!SourceExpr) {
      SendAutomationError(Socket, RequestId, TEXT("Source node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    // Target could be another expression OR the main material node (if
    // TargetNodeId is empty or "Main")
    if (TargetNodeId.IsEmpty() || TargetNodeId == TEXT("Main")) {
      bool bFound = false;
#if WITH_EDITORONLY_DATA
      if (InputName == TEXT("BaseColor")) {
        MCP_GET_MATERIAL_INPUT(Material, BaseColor).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("EmissiveColor")) {
        MCP_GET_MATERIAL_INPUT(Material, EmissiveColor).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Roughness")) {
        MCP_GET_MATERIAL_INPUT(Material, Roughness).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Metallic")) {
        MCP_GET_MATERIAL_INPUT(Material, Metallic).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Specular")) {
        MCP_GET_MATERIAL_INPUT(Material, Specular).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Normal")) {
        MCP_GET_MATERIAL_INPUT(Material, Normal).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Opacity")) {
        MCP_GET_MATERIAL_INPUT(Material, Opacity).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("OpacityMask")) {
        MCP_GET_MATERIAL_INPUT(Material, OpacityMask).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("AmbientOcclusion")) {
        MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion).Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("SubsurfaceColor")) {
        MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor).Expression = SourceExpr;
        bFound = true;
      }
#endif

      if (bFound) {
        Material->PostEditChange();
        Material->MarkPackageDirty();
        SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Connected to main material node."));
      } else {
        SendAutomationError(
            Socket, RequestId,
            FString::Printf(TEXT("Unknown input on main node: %s"), *InputName),
            TEXT("INVALID_PIN"));
      }
      return true;
    } else {
      UMaterialExpression *TargetExpr = FindExpressionByIdOrName(TargetNodeId);

      if (TargetExpr) {
        // We have to iterate properties to find the FExpressionInput
        FProperty *Prop =
            TargetExpr->GetClass()->FindPropertyByName(FName(*InputName));
        if (Prop) {
          if (FStructProperty *StructProp = CastField<FStructProperty>(Prop)) {
            if (StructProp->Struct->GetFName() ==
                FName("ExpressionInput")) // Note: FExpressionInput struct name
                                          // check
            {
              FExpressionInput *InputPtr =
                  StructProp->ContainerPtrToValuePtr<FExpressionInput>(
                      TargetExpr);
              if (InputPtr) {
                InputPtr->Expression = SourceExpr;
                Material->PostEditChange();
                Material->MarkPackageDirty();
                SendAutomationResponse(Socket, RequestId, true,
                                       TEXT("Nodes connected."));
                return true;
              }
            }
          }
          // Also handle FColorMaterialInput, FScalarMaterialInput,
          // FVectorMaterialInput which inherit FExpressionInput Just check if
          // it has 'Expression' member? No, reflection doesn't work that way
          // easily. In 5.6, inputs are usually typed. Fallback: check known
          // input names for common nodes or generic implementation Since we
          // can't easily genericize this without iteration or casting, we might
          // fail if property isn't direct FExpressionInput. But typically they
          // are FExpressionInput derived.
        }

        SendAutomationError(
            Socket, RequestId,
            FString::Printf(TEXT("Input pin '%s' not found or not compatible."),
                            *InputName),
            TEXT("PIN_NOT_FOUND"));
      } else {
        SendAutomationError(Socket, RequestId, TEXT("Target node not found."),
                            TEXT("NODE_NOT_FOUND"));
      }
      return true;
    }
  } else if (SubAction == TEXT("break_connections")) {
    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    FString
        PinName; // If provided, break specific pin. If empty, break all inputs?
    Payload->TryGetStringField(TEXT("pinName"), PinName);

    // Check if main node
    if (NodeId.IsEmpty() || NodeId == TEXT("Main")) {
      // Disconnect from main material node
      if (!PinName.IsEmpty()) {
        bool bFound = false;
#if WITH_EDITORONLY_DATA
        if (PinName == TEXT("BaseColor")) {
          MCP_GET_MATERIAL_INPUT(Material, BaseColor).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("EmissiveColor")) {
          MCP_GET_MATERIAL_INPUT(Material, EmissiveColor).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Roughness")) {
          MCP_GET_MATERIAL_INPUT(Material, Roughness).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Metallic")) {
          MCP_GET_MATERIAL_INPUT(Material, Metallic).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Specular")) {
          MCP_GET_MATERIAL_INPUT(Material, Specular).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Normal")) {
          MCP_GET_MATERIAL_INPUT(Material, Normal).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Opacity")) {
          MCP_GET_MATERIAL_INPUT(Material, Opacity).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("OpacityMask")) {
          MCP_GET_MATERIAL_INPUT(Material, OpacityMask).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("AmbientOcclusion")) {
          MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion).Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("SubsurfaceColor")) {
          MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor).Expression = nullptr;
          bFound = true;
        }
#endif

        if (bFound) {
          Material->PostEditChange();
          Material->MarkPackageDirty();
          SendAutomationResponse(Socket, RequestId, true,
                                 TEXT("Disconnected from main material pin."));
          return true;
        } else {
          SendAutomationError(
              Socket, RequestId,
              FString::Printf(TEXT("Unknown or unsupported pin: %s"), *PinName),
              TEXT("INVALID_PIN"));
          return true;
        }
      }
    }

    UMaterialExpression *TargetExpr = FindExpressionByIdOrName(NodeId);

    if (TargetExpr) {
      // Disconnect all inputs of this node if no specific pin name
      // Since GetInputs() is not available, we skip generic breaking for now.
      // We can implement breaking for specific pin if needed via property
      // reflection.

      // For now, just acknowledge but warn.
      Material->PostEditChange();
      Material->MarkPackageDirty();
      SendAutomationResponse(
          Socket, RequestId, true,
          TEXT("Node disconnection partial (generic inputs not cleared)."));
      return true;
    }

    SendAutomationError(Socket, RequestId, TEXT("Node not found."),
                        TEXT("NODE_NOT_FOUND"));
    return true;
  } else if (SubAction == TEXT("get_node_details")) {
    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);

    UMaterialExpression *TargetExpr = nullptr;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    auto AllExpressions = Material->GetExpressions();
#else
    // UE 5.0: Direct access to Expressions array
    auto& AllExpressions = Material->Expressions;
#endif

    // If nodeId provided, try to find that specific node
    if (!NodeId.IsEmpty()) {
      TargetExpr = FindExpressionByIdOrName(NodeId);
    }

    if (TargetExpr) {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("nodeType"),
                             TargetExpr->GetClass()->GetName());
      Result->SetStringField(TEXT("desc"), TargetExpr->Desc);
      Result->SetNumberField(TEXT("x"), TargetExpr->MaterialExpressionEditorX);
      Result->SetNumberField(TEXT("y"), TargetExpr->MaterialExpressionEditorY);

      SendAutomationResponse(Socket, RequestId, true,
                             TEXT("Node details retrieved."), Result);
    } else {
      // List all available nodes to help the user
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      TArray<TSharedPtr<FJsonValue>> NodeList;

      for (int32 i = 0; i < AllExpressions.Num(); ++i) {
        UMaterialExpression *Expr = AllExpressions[i];
        TSharedPtr<FJsonObject> NodeInfo = MakeShared<FJsonObject>();
        NodeInfo->SetStringField(TEXT("nodeId"),
                                 Expr->MaterialExpressionGuid.ToString());
        NodeInfo->SetStringField(TEXT("nodeType"), Expr->GetClass()->GetName());
        NodeInfo->SetNumberField(TEXT("index"), i);
        if (!Expr->Desc.IsEmpty()) {
          NodeInfo->SetStringField(TEXT("desc"), Expr->Desc);
        }
        NodeList.Add(MakeShared<FJsonValueObject>(NodeInfo));
      }

      Result->SetArrayField(TEXT("availableNodes"), NodeList);
      Result->SetNumberField(TEXT("nodeCount"), AllExpressions.Num());

      FString Message =
          NodeId.IsEmpty()
              ? FString::Printf(
                    TEXT("No nodeId provided. Material has %d nodes."),
                    AllExpressions.Num())
              : FString::Printf(
                    TEXT("Node '%s' not found. Material has %d nodes."),
                    *NodeId, AllExpressions.Num());

      SendAutomationResponse(Socket, RequestId, false, Message, Result,
                             TEXT("NODE_NOT_FOUND"));
    }
    return true;
  }

  SendAutomationError(
      Socket, RequestId,
      FString::Printf(TEXT("Unknown subAction: %s"), *SubAction),
      TEXT("INVALID_SUBACTION"));
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor only."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialTextureSample(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) ||
      MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing 'materialPath'."),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString TexturePath;
  if (!Payload->TryGetStringField(TEXT("texturePath"), TexturePath) ||
      TexturePath.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing 'texturePath'."),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UMaterial *Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
  if (!Material) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Could not load Material: %s"),
                                        *MaterialPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UTexture *Texture = LoadObject<UTexture>(nullptr, *TexturePath);
  if (!Texture) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Could not load Texture: %s"),
                                        *TexturePath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Get optional coordinate index
  int32 CoordinateIndex = 0;
  Payload->TryGetNumberField(TEXT("coordinateIndex"), CoordinateIndex);

  // Get optional position
  float X = 0.0f, Y = 0.0f;
  Payload->TryGetNumberField(TEXT("x"), X);
  Payload->TryGetNumberField(TEXT("y"), Y);

  // Create the texture sample expression
  UMaterialExpressionTextureSample *TexSample =
      NewObject<UMaterialExpressionTextureSample>(
          Material, UMaterialExpressionTextureSample::StaticClass(), NAME_None,
          RF_Transactional);

  if (!TexSample) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create TextureSample expression."),
                        TEXT("CREATE_FAILED"));
    return true;
  }

  TexSample->Texture = Texture;
  TexSample->ConstCoordinate = CoordinateIndex;
  TexSample->MaterialExpressionEditorX = (int32)X;
  TexSample->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  if (Material->GetEditorOnlyData()) {
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(TexSample);
  }
#else
  // UE 5.0: Direct access
  Material->Expressions.Add(TexSample);
#endif
#endif

  Material->PreEditChange(nullptr);
  Material->PostEditChange();
  McpSafeAssetSave(Material);

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetStringField(TEXT("nodeId"),
                         TexSample->MaterialExpressionGuid.ToString());
  Result->SetStringField(TEXT("materialPath"), Material->GetPathName());
  Result->SetStringField(TEXT("texturePath"), Texture->GetPathName());

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("TextureSample expression added to material."),
                         Result);
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor only."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialExpression(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) ||
      MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing 'materialPath'."),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ExpressionClassName;
  if (!Payload->TryGetStringField(TEXT("expressionClass"), ExpressionClassName) ||
      ExpressionClassName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing 'expressionClass'."),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UMaterial *Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
  if (!Material) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Could not load Material: %s"),
                                        *MaterialPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  // Get optional position
  float X = 0.0f, Y = 0.0f;
  Payload->TryGetNumberField(TEXT("x"), X);
  Payload->TryGetNumberField(TEXT("y"), Y);

  // Try to find the expression class by name
  UClass *ExpressionClass = nullptr;

  // Try with full path first
  ExpressionClass = FindObject<UClass>(
      nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ExpressionClassName));

  // If not found, try with MaterialExpression prefix
  if (!ExpressionClass ||
      !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
    FString PrefixedName =
        FString::Printf(TEXT("MaterialExpression%s"), *ExpressionClassName);
    ExpressionClass = FindObject<UClass>(
        nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *PrefixedName));
  }

  // Try ResolveClassByName helper as fallback
  if (!ExpressionClass ||
      !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
    ExpressionClass = ResolveClassByName(ExpressionClassName);
    if (ExpressionClass &&
        !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
      ExpressionClass = nullptr;
    }
  }

  // Final fallback with MaterialExpression prefix
  if (!ExpressionClass) {
    FString PrefixedName =
        FString::Printf(TEXT("MaterialExpression%s"), *ExpressionClassName);
    ExpressionClass = ResolveClassByName(PrefixedName);
    if (ExpressionClass &&
        !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
      ExpressionClass = nullptr;
    }
  }

  if (!ExpressionClass) {
    SendAutomationError(
        Socket, RequestId,
        FString::Printf(
            TEXT("Unknown expression class: %s. Try using the full class "
                 "name like 'MaterialExpressionAdd' or 'Add'."),
            *ExpressionClassName),
        TEXT("CLASS_NOT_FOUND"));
    return true;
  }

  UMaterialExpression *NewExpr = NewObject<UMaterialExpression>(
      Material, ExpressionClass, NAME_None, RF_Transactional);

  if (!NewExpr) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create expression."),
                        TEXT("CREATE_FAILED"));
    return true;
  }

  NewExpr->MaterialExpressionEditorX = (int32)X;
  NewExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  if (Material->GetEditorOnlyData()) {
    MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(NewExpr);
  }
#else
  // UE 5.0: Direct access
  Material->Expressions.Add(NewExpr);
#endif
#endif

  Material->PreEditChange(nullptr);
  Material->PostEditChange();
  McpSafeAssetSave(Material);

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetStringField(TEXT("nodeId"),
                         NewExpr->MaterialExpressionGuid.ToString());
  Result->SetStringField(TEXT("expressionClass"),
                         ExpressionClass->GetName());
  Result->SetStringField(TEXT("materialPath"), Material->GetPathName());

  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Expression '%s' added to material."),
                      *ExpressionClass->GetName()),
      Result);
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor only."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleCreateMaterialNodes(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) ||
      MaterialPath.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing 'materialPath'."),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UMaterial *Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
  if (!Material) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Could not load Material: %s"),
                                        *MaterialPath),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  const TArray<TSharedPtr<FJsonValue>> *NodesArray;
  if (!Payload->TryGetArrayField(TEXT("nodes"), NodesArray) || !NodesArray) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Missing 'nodes' array."),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> CreatedNodes;
  int32 SuccessCount = 0;
  int32 FailCount = 0;

  for (const auto &NodeVal : *NodesArray) {
    TSharedPtr<FJsonObject> NodeObj = NodeVal->AsObject();
    if (!NodeObj.IsValid()) {
      FailCount++;
      continue;
    }

    FString NodeType;
    if (!NodeObj->TryGetStringField(TEXT("type"), NodeType) ||
        NodeType.IsEmpty()) {
      FailCount++;
      continue;
    }

    float X = 0.0f, Y = 0.0f;
    NodeObj->TryGetNumberField(TEXT("x"), X);
    NodeObj->TryGetNumberField(TEXT("y"), Y);

    // Resolve expression class
    UClass *ExpressionClass = nullptr;

    // Try common shorthand types first
    if (NodeType == TEXT("TextureSample"))
      ExpressionClass = UMaterialExpressionTextureSample::StaticClass();
    else if (NodeType == TEXT("VectorParameter"))
      ExpressionClass = UMaterialExpressionVectorParameter::StaticClass();
    else if (NodeType == TEXT("ScalarParameter"))
      ExpressionClass = UMaterialExpressionScalarParameter::StaticClass();
    else if (NodeType == TEXT("Add"))
      ExpressionClass = UMaterialExpressionAdd::StaticClass();
    else if (NodeType == TEXT("Multiply"))
      ExpressionClass = UMaterialExpressionMultiply::StaticClass();
    else if (NodeType == TEXT("Constant"))
      ExpressionClass = UMaterialExpressionConstant::StaticClass();
    else if (NodeType == TEXT("Constant3Vector") || NodeType == TEXT("Color"))
      ExpressionClass = UMaterialExpressionConstant3Vector::StaticClass();
    else {
      // Try to resolve by name
      ExpressionClass = FindObject<UClass>(
          nullptr,
          *FString::Printf(TEXT("/Script/Engine.%s"), *NodeType));
      if (!ExpressionClass ||
          !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
        FString PrefixedName =
            FString::Printf(TEXT("MaterialExpression%s"), *NodeType);
        ExpressionClass = FindObject<UClass>(
            nullptr,
            *FString::Printf(TEXT("/Script/Engine.%s"), *PrefixedName));
      }
      if (!ExpressionClass ||
          !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
        ExpressionClass = ResolveClassByName(NodeType);
        if (ExpressionClass &&
            !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())) {
          ExpressionClass = nullptr;
        }
      }
    }

    if (!ExpressionClass) {
      FailCount++;
      continue;
    }

    UMaterialExpression *NewExpr = NewObject<UMaterialExpression>(
        Material, ExpressionClass, NAME_None, RF_Transactional);

    if (!NewExpr) {
      FailCount++;
      continue;
    }

    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;

    // Handle parameter name if applicable
    FString ParamName;
    if (NodeObj->TryGetStringField(TEXT("name"), ParamName)) {
      if (UMaterialExpressionParameter *ParamExpr =
              Cast<UMaterialExpressionParameter>(NewExpr)) {
        ParamExpr->ParameterName = FName(*ParamName);
      }
    }

    // Handle texture path for texture samples
    FString TexturePath;
    if (NodeObj->TryGetStringField(TEXT("texturePath"), TexturePath)) {
      if (UMaterialExpressionTextureSample *TexSample =
              Cast<UMaterialExpressionTextureSample>(NewExpr)) {
        UTexture *Texture = LoadObject<UTexture>(nullptr, *TexturePath);
        if (Texture) {
          TexSample->Texture = Texture;
        }
      }
    }

    // Handle default value for constant expressions
    double DefaultValue = 0.0;
    if (NodeObj->TryGetNumberField(TEXT("value"), DefaultValue)) {
      if (UMaterialExpressionConstant *ConstExpr =
              Cast<UMaterialExpressionConstant>(NewExpr)) {
        ConstExpr->R = (float)DefaultValue;
      }
    }

#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    if (Material->GetEditorOnlyData()) {
      MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(NewExpr);
    }
#else
    // UE 5.0: Direct access
    Material->Expressions.Add(NewExpr);
#endif
#endif

    // Record created node info
    TSharedPtr<FJsonObject> NodeInfo = MakeShared<FJsonObject>();
    NodeInfo->SetStringField(TEXT("nodeId"),
                             NewExpr->MaterialExpressionGuid.ToString());
    NodeInfo->SetStringField(TEXT("type"), ExpressionClass->GetName());
    CreatedNodes.Add(MakeShared<FJsonValueObject>(NodeInfo));

    SuccessCount++;
  }

  Material->PreEditChange(nullptr);
  Material->PostEditChange();
  McpSafeAssetSave(Material);

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetArrayField(TEXT("createdNodes"), CreatedNodes);
  Result->SetNumberField(TEXT("successCount"), SuccessCount);
  Result->SetNumberField(TEXT("failCount"), FailCount);
  Result->SetStringField(TEXT("materialPath"), Material->GetPathName());

  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Created %d nodes (%d failed)."), SuccessCount,
                      FailCount),
      Result);
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor only."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif // WITH_EDITOR
}
