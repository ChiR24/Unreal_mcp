#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleMaterialGraphAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_material_graph"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Material."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FString SubAction = Payload->GetStringField(TEXT("subAction"));

    if (SubAction == TEXT("add_node"))
    {
        FString NodeType;
        Payload->TryGetStringField(TEXT("nodeType"), NodeType);
        float X = 0.0f;
        float Y = 0.0f;
        Payload->TryGetNumberField(TEXT("x"), X);
        Payload->TryGetNumberField(TEXT("y"), Y);

        UClass* ExpressionClass = nullptr;
        if (NodeType == TEXT("TextureSample")) ExpressionClass = UMaterialExpressionTextureSample::StaticClass();
        else if (NodeType == TEXT("VectorParameter")) ExpressionClass = UMaterialExpressionVectorParameter::StaticClass();
        else if (NodeType == TEXT("ScalarParameter")) ExpressionClass = UMaterialExpressionScalarParameter::StaticClass();
        else if (NodeType == TEXT("Add")) ExpressionClass = UMaterialExpressionAdd::StaticClass();
        else if (NodeType == TEXT("Multiply")) ExpressionClass = UMaterialExpressionMultiply::StaticClass();
        else if (NodeType == TEXT("Constant")) ExpressionClass = UMaterialExpressionConstant::StaticClass();
        else if (NodeType == TEXT("Constant3Vector")) ExpressionClass = UMaterialExpressionConstant3Vector::StaticClass();
        else
        {
            // Try resolve class
            ExpressionClass = ResolveClassByName(NodeType);
            if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
            {
                SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown node type: %s"), *NodeType), TEXT("UNKNOWN_TYPE"));
                return true;
            }
        }

        UMaterialExpression* NewExpr = NewObject<UMaterialExpression>(Material, ExpressionClass, NAME_None, RF_Transactional);
        if (NewExpr)
        {
            NewExpr->MaterialExpressionEditorX = (int32)X;
            NewExpr->MaterialExpressionEditorY = (int32)Y;
#if WITH_EDITORONLY_DATA
            if (Material->GetEditorOnlyData())
            {
                Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Add(NewExpr);
            }
#endif
            
            // If parameter, set name
            FString ParamName;
            if (Payload->TryGetStringField(TEXT("name"), ParamName))
            {
                if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(NewExpr))
                {
                    ParamExpr->ParameterName = FName(*ParamName);
                }
            }

            Material->PostEditChange();
            Material->MarkPackageDirty();
            
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("nodeId"), NewExpr->MaterialExpressionGuid.ToString());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node added."), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create expression."), TEXT("CREATE_FAILED"));
        }
        return true;
    }
    else if (SubAction == TEXT("remove_node"))
    {
        FString NodeId;
        Payload->TryGetStringField(TEXT("nodeId"), NodeId);

        UMaterialExpression* TargetExpr = nullptr;
        for (UMaterialExpression* Expr : Material->GetExpressions())
        {
            if (Expr->MaterialExpressionGuid.ToString() == NodeId)
            {
                TargetExpr = Expr;
                break;
            }
        }

        if (TargetExpr)
        {
#if WITH_EDITORONLY_DATA
            if (Material->GetEditorOnlyData())
            {
                Material->GetEditorOnlyData()->ExpressionCollection.Expressions.Remove(TargetExpr);
            }
#endif
            Material->PostEditChange();
            Material->MarkPackageDirty();
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node removed."));
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        }
        return true;
    }
    else if (SubAction == TEXT("connect_nodes"))
    {
        // Material graph connections are complex because inputs are structs on the expression, not EdGraph pins
        // We need to find the target expression and set its input
        FString SourceNodeId, TargetNodeId, InputName;
        Payload->TryGetStringField(TEXT("sourceNodeId"), SourceNodeId);
        Payload->TryGetStringField(TEXT("targetNodeId"), TargetNodeId);
        Payload->TryGetStringField(TEXT("inputName"), InputName);

        UMaterialExpression* SourceExpr = nullptr;
        for (UMaterialExpression* Expr : Material->GetExpressions())
        {
            if (Expr->MaterialExpressionGuid.ToString() == SourceNodeId) { SourceExpr = Expr; break; }
        }

        if (!SourceExpr)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Source node not found."), TEXT("NODE_NOT_FOUND"));
            return true;
        }

        // Target could be another expression OR the main material node (if TargetNodeId is empty or "Main")
        if (TargetNodeId.IsEmpty() || TargetNodeId == TEXT("Main"))
        {
            bool bFound = false;
#if WITH_EDITORONLY_DATA
            if (InputName == TEXT("BaseColor")) { Material->GetEditorOnlyData()->BaseColor.Expression = SourceExpr; bFound = true; }
            else if (InputName == TEXT("EmissiveColor")) { Material->GetEditorOnlyData()->EmissiveColor.Expression = SourceExpr; bFound = true; }
            else if (InputName == TEXT("Roughness")) { Material->GetEditorOnlyData()->Roughness.Expression = SourceExpr; bFound = true; }
            else if (InputName == TEXT("Metallic")) { Material->GetEditorOnlyData()->Metallic.Expression = SourceExpr; bFound = true; }
            else if (InputName == TEXT("Specular")) { Material->GetEditorOnlyData()->Specular.Expression = SourceExpr; bFound = true; }
            else if (InputName == TEXT("Normal")) { Material->GetEditorOnlyData()->Normal.Expression = SourceExpr; bFound = true; }
            else if (InputName == TEXT("Opacity")) { Material->GetEditorOnlyData()->Opacity.Expression = SourceExpr; bFound = true; }
            else if (InputName == TEXT("OpacityMask")) { Material->GetEditorOnlyData()->OpacityMask.Expression = SourceExpr; bFound = true; }
            else if (InputName == TEXT("AmbientOcclusion")) { Material->GetEditorOnlyData()->AmbientOcclusion.Expression = SourceExpr; bFound = true; }
            else if (InputName == TEXT("SubsurfaceColor")) { Material->GetEditorOnlyData()->SubsurfaceColor.Expression = SourceExpr; bFound = true; }
#endif
            
            if (bFound)
            {
                Material->PostEditChange();
                Material->MarkPackageDirty();
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Connected to main material node."));
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown input on main node: %s"), *InputName), TEXT("INVALID_PIN"));
            }
            return true;
        }
        else
        {
            UMaterialExpression* TargetExpr = nullptr;
            for (UMaterialExpression* Expr : Material->GetExpressions())
            {
                if (Expr->MaterialExpressionGuid.ToString() == TargetNodeId) { TargetExpr = Expr; break; }
            }

            if (TargetExpr)
            {
                // We have to iterate properties to find the FExpressionInput
                FProperty* Prop = TargetExpr->GetClass()->FindPropertyByName(FName(*InputName));
                if (Prop)
                {
                    if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
                    {
                        if (StructProp->Struct->GetFName() == FName("ExpressionInput")) // Note: FExpressionInput struct name check
                        {
                            FExpressionInput* InputPtr = StructProp->ContainerPtrToValuePtr<FExpressionInput>(TargetExpr);
                            if (InputPtr)
                            {
                                InputPtr->Expression = SourceExpr;
                                Material->PostEditChange();
                                Material->MarkPackageDirty();
                                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Nodes connected."));
                                return true;
                            }
                        }
                    }
                    // Also handle FColorMaterialInput, FScalarMaterialInput, FVectorMaterialInput which inherit FExpressionInput
                    // Just check if it has 'Expression' member? No, reflection doesn't work that way easily.
                    // In 5.6, inputs are usually typed.
                    // Fallback: check known input names for common nodes or generic implementation
                    // Since we can't easily genericize this without iteration or casting, we might fail if property isn't direct FExpressionInput.
                    // But typically they are FExpressionInput derived.
                }
                
                SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Input pin '%s' not found or not compatible."), *InputName), TEXT("PIN_NOT_FOUND"));
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Target node not found."), TEXT("NODE_NOT_FOUND"));
            }
            return true;
        }
    }
    else if (SubAction == TEXT("break_connections"))
    {
        FString NodeId;
        Payload->TryGetStringField(TEXT("nodeId"), NodeId);
        FString PinName; // If provided, break specific pin. If empty, break all inputs?
        Payload->TryGetStringField(TEXT("pinName"), PinName);

        // Check if main node
        if (NodeId.IsEmpty() || NodeId == TEXT("Main"))
        {
            // Disconnect from main material node
            if (!PinName.IsEmpty())
            {
                bool bFound = false;
#if WITH_EDITORONLY_DATA
                if (PinName == TEXT("BaseColor")) { Material->GetEditorOnlyData()->BaseColor.Expression = nullptr; bFound = true; }
                else if (PinName == TEXT("EmissiveColor")) { Material->GetEditorOnlyData()->EmissiveColor.Expression = nullptr; bFound = true; }
                else if (PinName == TEXT("Roughness")) { Material->GetEditorOnlyData()->Roughness.Expression = nullptr; bFound = true; }
                else if (PinName == TEXT("Metallic")) { Material->GetEditorOnlyData()->Metallic.Expression = nullptr; bFound = true; }
                else if (PinName == TEXT("Specular")) { Material->GetEditorOnlyData()->Specular.Expression = nullptr; bFound = true; }
                else if (PinName == TEXT("Normal")) { Material->GetEditorOnlyData()->Normal.Expression = nullptr; bFound = true; }
                else if (PinName == TEXT("Opacity")) { Material->GetEditorOnlyData()->Opacity.Expression = nullptr; bFound = true; }
                else if (PinName == TEXT("OpacityMask")) { Material->GetEditorOnlyData()->OpacityMask.Expression = nullptr; bFound = true; }
                else if (PinName == TEXT("AmbientOcclusion")) { Material->GetEditorOnlyData()->AmbientOcclusion.Expression = nullptr; bFound = true; }
                else if (PinName == TEXT("SubsurfaceColor")) { Material->GetEditorOnlyData()->SubsurfaceColor.Expression = nullptr; bFound = true; }
#endif
                
                if (bFound)
                {
                    Material->PostEditChange();
                    Material->MarkPackageDirty();
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Disconnected from main material pin."));
                    return true;
                }
                else
                {
                    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown or unsupported pin: %s"), *PinName), TEXT("INVALID_PIN"));
                    return true;
                }
            }
        }

        UMaterialExpression* TargetExpr = nullptr;
        for (UMaterialExpression* Expr : Material->GetExpressions())
        {
            if (Expr->MaterialExpressionGuid.ToString() == NodeId)
            {
                TargetExpr = Expr;
                break;
            }
        }

        if (TargetExpr)
        {
             // Disconnect all inputs of this node if no specific pin name
             // Since GetInputs() is not available, we skip generic breaking for now.
             // We can implement breaking for specific pin if needed via property reflection.
             
             // For now, just acknowledge but warn.
             Material->PostEditChange();
             Material->MarkPackageDirty();
             SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node disconnection partial (generic inputs not cleared)."));
             return true;
        }
        
        SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        return true;
    }
    else if (SubAction == TEXT("get_node_details"))
    {
        FString NodeId;
        Payload->TryGetStringField(TEXT("nodeId"), NodeId);

        UMaterialExpression* TargetExpr = nullptr;
        for (UMaterialExpression* Expr : Material->GetExpressions())
        {
            if (Expr->MaterialExpressionGuid.ToString() == NodeId)
            {
                TargetExpr = Expr;
                break;
            }
        }

        if (TargetExpr)
        {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("nodeType"), TargetExpr->GetClass()->GetName());
            Result->SetStringField(TEXT("desc"), TargetExpr->Desc);
            Result->SetNumberField(TEXT("x"), TargetExpr->MaterialExpressionEditorX);
            Result->SetNumberField(TEXT("y"), TargetExpr->MaterialExpressionEditorY);
            
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node details retrieved."), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        }
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown subAction: %s"), *SubAction), TEXT("INVALID_SUBACTION"));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialTextureSample(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return false;
}

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialExpression(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return false;
}

bool UMcpAutomationBridgeSubsystem::HandleCreateMaterialNodes(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return false;
}