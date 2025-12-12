#include "McpAutomationBridgeGlobals.h"
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
    for (UMaterialExpression *Expr : Material->GetExpressions()) {
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
      if (Material->GetEditorOnlyData()) {
        Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(
            NewExpr);
      }
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
#if WITH_EDITORONLY_DATA
      if (Material->GetEditorOnlyData()) {
        Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Remove(
            TargetExpr);
      }
#endif
      Material->PostEditChange();
      Material->MarkPackageDirty();
      SendAutomationResponse(Socket, RequestId, true, TEXT("Node removed."));
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
        Material->GetEditorOnlyData()->BaseColor.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("EmissiveColor")) {
        Material->GetEditorOnlyData()->EmissiveColor.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Roughness")) {
        Material->GetEditorOnlyData()->Roughness.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Metallic")) {
        Material->GetEditorOnlyData()->Metallic.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Specular")) {
        Material->GetEditorOnlyData()->Specular.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Normal")) {
        Material->GetEditorOnlyData()->Normal.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("Opacity")) {
        Material->GetEditorOnlyData()->Opacity.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("OpacityMask")) {
        Material->GetEditorOnlyData()->OpacityMask.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("AmbientOcclusion")) {
        Material->GetEditorOnlyData()->AmbientOcclusion.Expression = SourceExpr;
        bFound = true;
      } else if (InputName == TEXT("SubsurfaceColor")) {
        Material->GetEditorOnlyData()->SubsurfaceColor.Expression = SourceExpr;
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
          Material->GetEditorOnlyData()->BaseColor.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("EmissiveColor")) {
          Material->GetEditorOnlyData()->EmissiveColor.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Roughness")) {
          Material->GetEditorOnlyData()->Roughness.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Metallic")) {
          Material->GetEditorOnlyData()->Metallic.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Specular")) {
          Material->GetEditorOnlyData()->Specular.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Normal")) {
          Material->GetEditorOnlyData()->Normal.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("Opacity")) {
          Material->GetEditorOnlyData()->Opacity.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("OpacityMask")) {
          Material->GetEditorOnlyData()->OpacityMask.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("AmbientOcclusion")) {
          Material->GetEditorOnlyData()->AmbientOcclusion.Expression = nullptr;
          bFound = true;
        } else if (PinName == TEXT("SubsurfaceColor")) {
          Material->GetEditorOnlyData()->SubsurfaceColor.Expression = nullptr;
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
    auto AllExpressions = Material->GetExpressions();

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
  return false;
}

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialExpression(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  return false;
}

bool UMcpAutomationBridgeSubsystem::HandleCreateMaterialNodes(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  return false;
}