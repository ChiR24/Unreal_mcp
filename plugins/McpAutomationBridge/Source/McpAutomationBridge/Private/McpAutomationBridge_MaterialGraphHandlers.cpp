// =============================================================================
// McpAutomationBridge_MaterialGraphHandlers.cpp
// =============================================================================
// Material Graph Manipulation Handlers for MCP Automation Bridge.
//
// This file implements the following handlers:
// - manage_material_graph (main dispatcher)
//   - add_node: Add expression node to material
//   - remove_node: Remove expression from material
//   - connect_nodes/connect_pins: Connect expressions or to main material
//   - break_connections: Disconnect expression inputs
//   - get_node_details: Get node info or list all nodes
// - add_material_texture_sample: Add TextureSample expression
// - add_material_expression: Add generic expression by class name
// - create_material_nodes: Batch create multiple nodes
//
// UE VERSION COMPATIBILITY:
// - UE 5.0: Material->Expressions (direct TArray access)
// - UE 5.1+: Material->GetEditorOnlyData()->ExpressionCollection.Expressions
// - MCP_GET_MATERIAL_EXPRESSIONS macro abstracts this difference
//
// NODE IDENTIFICATION:
// - GUID string (MaterialExpressionGuid)
// - Node name (GetName())
// - Object path (GetPathName())
// - Parameter name (for parameter nodes)
// - Numeric index (0-based position in Expressions array)
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST BE FIRST - Version compatibility macros
#include "McpHandlerUtils.h"          // Utility functions for JSON parsing

#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR

// =============================================================================
// Engine Includes - Material System
// =============================================================================
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

// Material API compatibility macros are defined in McpAutomationBridgeHelpers.h

#endif // WITH_EDITOR

// =============================================================================
// Handler: manage_material_graph
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleMaterialGraphAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (Action != TEXT("manage_material_graph"))
    {
        return false;
    }

#if WITH_EDITOR

    // -------------------------------------------------------------------------
    // Payload Validation
    // -------------------------------------------------------------------------
    if (!Payload.IsValid())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Required Parameters
    // -------------------------------------------------------------------------
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *AssetPath);
    if (!Material)
    {
        SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
            TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FString SubAction;
    if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty())
    {
        SendAutomationError(Socket, RequestId,
            TEXT("Missing 'subAction' for manage_material_graph"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // =========================================================================
    // Helper: Find Expression by ID, Name, or Index
    // =========================================================================
    // Supports: GUID string, node name, object path, parameter name, or numeric index
    // -------------------------------------------------------------------------
    auto FindExpressionByIdOrNameOrIndex = [&](const FString &IdOrName, int32 Index = -1) -> UMaterialExpression *
    {
        // First, try index-based lookup if Index is valid
        if (Index >= 0)
        {
            const TArray<UMaterialExpression*> &Expressions = MCP_GET_MATERIAL_EXPRESSIONS(Material);
            if (Index < Expressions.Num())
            {
                return Expressions[Index];
            }
            return nullptr; // Index out of bounds
        }

        // If IdOrName is empty, return nullptr
        if (IdOrName.IsEmpty())
        {
            return nullptr;
        }

        // Try to parse as numeric index first (e.g., "0", "1", "2")
        int32 ParsedIndex = -1;
        if (IdOrName.IsNumeric())
        {
            ParsedIndex = FCString::Atoi(*IdOrName);
            const TArray<UMaterialExpression*> &Expressions = MCP_GET_MATERIAL_EXPRESSIONS(Material);
            if (ParsedIndex >= 0 && ParsedIndex < Expressions.Num())
            {
                return Expressions[ParsedIndex];
            }
        }

        // Search by GUID, name, path, or parameter name
        const FString Needle = IdOrName.TrimStartAndEnd();
        for (UMaterialExpression *Expr : MCP_GET_MATERIAL_EXPRESSIONS(Material))
        {
            if (!Expr)
            {
                continue;
            }

            // Match by GUID
            if (Expr->MaterialExpressionGuid.ToString() == Needle)
            {
                return Expr;
            }

            // Match by name
            if (Expr->GetName() == Needle)
            {
                return Expr;
            }

            // Match by full path
            if (Expr->GetPathName() == Needle)
            {
                return Expr;
            }

            // Match by parameter name (for parameter nodes)
            if (UMaterialExpressionParameter *ParamExpr = Cast<UMaterialExpressionParameter>(Expr))
            {
                if (ParamExpr->ParameterName.ToString() == Needle)
                {
                    return Expr;
                }
            }
        }
        return nullptr;
    };

    // =========================================================================
    // Helper: Find Expression from Payload Fields
    // =========================================================================
    // Wrapper that accepts both string ID and numeric index
    // -------------------------------------------------------------------------
    auto FindExpressionByPayload = [&](const FString &IdField, const FString &IndexField) -> UMaterialExpression*
    {
        // Check for numeric index first (e.g., fromExpression: 0)
        int32 Index = -1;
        if (Payload->TryGetNumberField(*IndexField, Index) && Index >= 0)
        {
            return FindExpressionByIdOrNameOrIndex(FString(), Index);
        }

        // Then check for string ID
        FString IdOrName;
        if (Payload->TryGetStringField(*IdField, IdOrName) && !IdOrName.IsEmpty())
        {
            return FindExpressionByIdOrNameOrIndex(IdOrName);
        }

        // Also check if IdField contains a numeric string
        if (Payload->TryGetStringField(*IdField, IdOrName) && IdOrName.IsNumeric())
        {
            return FindExpressionByIdOrNameOrIndex(IdOrName);
        }

        return nullptr;
    };

    // =========================================================================
    // subAction: add_node
    // =========================================================================
    // Adds a material expression node to the material
    // Parameters: nodeType, x, y, name (for parameters)
    // -------------------------------------------------------------------------
    if (SubAction == TEXT("add_node"))
    {
        FString NodeType;
        Payload->TryGetStringField(TEXT("nodeType"), NodeType);
        float X = 0.0f;
        float Y = 0.0f;
        Payload->TryGetNumberField(TEXT("x"), X);
        Payload->TryGetNumberField(TEXT("y"), Y);

        UClass *ExpressionClass = nullptr;

        // Common shorthand types
        if (NodeType == TEXT("TextureSample"))
        {
            ExpressionClass = UMaterialExpressionTextureSample::StaticClass();
        }
        else if (NodeType == TEXT("VectorParameter") || NodeType == TEXT("ConstantVectorParameter"))
        {
            ExpressionClass = UMaterialExpressionVectorParameter::StaticClass();
        }
        else if (NodeType == TEXT("ScalarParameter") || NodeType == TEXT("ConstantScalarParameter"))
        {
            ExpressionClass = UMaterialExpressionScalarParameter::StaticClass();
        }
        else if (NodeType == TEXT("Add"))
        {
            ExpressionClass = UMaterialExpressionAdd::StaticClass();
        }
        else if (NodeType == TEXT("Multiply"))
        {
            ExpressionClass = UMaterialExpressionMultiply::StaticClass();
        }
        else if (NodeType == TEXT("Constant") || NodeType == TEXT("Float") || NodeType == TEXT("Scalar"))
        {
            ExpressionClass = UMaterialExpressionConstant::StaticClass();
        }
        else if (NodeType == TEXT("Constant3Vector") || NodeType == TEXT("ConstantVector") ||
                 NodeType == TEXT("Color") || NodeType == TEXT("Vector3"))
        {
            ExpressionClass = UMaterialExpressionConstant3Vector::StaticClass();
        }
        else
        {
            // Try resolve class by full path or partial name
            ExpressionClass = ResolveClassByName(NodeType);

            // Also try with MaterialExpression prefix
            if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
            {
                FString PrefixedName = FString::Printf(TEXT("MaterialExpression%s"), *NodeType);
                ExpressionClass = ResolveClassByName(PrefixedName);
            }

            if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
            {
                SendAutomationError(Socket, RequestId,
                    FString::Printf(
                        TEXT("Unknown node type: %s. Available types: TextureSample, "
                             "VectorParameter, ScalarParameter, Add, Multiply, "
                             "Constant, Constant3Vector, Color, ConstantVectorParameter. "
                             "Or use full class name like 'MaterialExpressionLerp'."),
                        *NodeType),
                    TEXT("UNKNOWN_TYPE"));
                return true;
            }
        }

        UMaterialExpression *NewExpr = NewObject<UMaterialExpression>(
            Material, ExpressionClass, NAME_None, RF_Transactional);

        if (NewExpr)
        {
            NewExpr->MaterialExpressionEditorX = (int32)X;
            NewExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            if (Material->GetEditorOnlyData())
            {
                MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(NewExpr);
            }
#else
            // UE 5.0: Direct access
            Material->Expressions.Add(NewExpr);
#endif
#endif

            // Set parameter name if applicable
            FString ParamName;
            if (Payload->TryGetStringField(TEXT("name"), ParamName))
            {
                if (UMaterialExpressionParameter *ParamExpr = Cast<UMaterialExpressionParameter>(NewExpr))
                {
                    ParamExpr->ParameterName = FName(*ParamName);
                }
            }

            Material->PostEditChange();
            Material->MarkPackageDirty();

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            McpHandlerUtils::AddVerification(Result, Material);
            Result->SetStringField(TEXT("nodeId"), NewExpr->GetName());
            Result->SetStringField(TEXT("nodeType"), ExpressionClass->GetName());
            SendAutomationResponse(Socket, RequestId, true, TEXT("Node added."), Result);
        }
        else
        {
            SendAutomationError(Socket, RequestId,
                TEXT("Failed to create expression."),
                TEXT("CREATE_FAILED"));
        }
        return true;
    }

    // =========================================================================
    // subAction: remove_node
    // =========================================================================
    // Removes a material expression node by ID or index
    // Parameters: nodeId, expressionIndex
    // -------------------------------------------------------------------------
    else if (SubAction == TEXT("remove_node"))
    {
        FString NodeId;
        Payload->TryGetStringField(TEXT("nodeId"), NodeId);

        int32 ExpressionIndex = -1;
        Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex);

        if (NodeId.IsEmpty() && ExpressionIndex < 0)
        {
            SendAutomationError(Socket, RequestId,
                TEXT("Missing 'nodeId' or 'expressionIndex'."),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UMaterialExpression *TargetExpr = FindExpressionByIdOrNameOrIndex(NodeId, ExpressionIndex);

        if (TargetExpr)
        {
            FString RemovedNodeId = TargetExpr->GetName();

#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            if (Material->GetEditorOnlyData())
            {
                MCP_GET_MATERIAL_EXPRESSIONS(Material).Remove(TargetExpr);
            }
#else
            // UE 5.0: Direct access
            Material->Expressions.Remove(TargetExpr);
#endif
#endif

            Material->PostEditChange();
            Material->MarkPackageDirty();

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            McpHandlerUtils::AddVerification(Result, Material);
            Result->SetStringField(TEXT("nodeId"), RemovedNodeId);
            Result->SetBoolField(TEXT("removed"), true);
            SendAutomationResponse(Socket, RequestId, true, TEXT("Node removed."), Result);
        }
        else
        {
            SendAutomationError(Socket, RequestId, TEXT("Node not found."),
                TEXT("NODE_NOT_FOUND"));
        }
        return true;
    }

    // =========================================================================
    // subAction: connect_nodes / connect_pins
    // =========================================================================
    // Connects material expression outputs to inputs
    // Parameters: sourceNodeId/fromExpression, targetNodeId/toExpression, inputName
    // Target can be another expression or main material node (BaseColor, etc.)
    // -------------------------------------------------------------------------
    else if (SubAction == TEXT("connect_nodes") || SubAction == TEXT("connect_pins"))
    {
        // Material graph connections are complex because inputs are structs on the
        // expression, not EdGraph pins. We need to find the target expression and
        // set its input.

        FString SourceNodeId, TargetNodeId, InputName;
        Payload->TryGetStringField(TEXT("sourceNodeId"), SourceNodeId);
        Payload->TryGetStringField(TEXT("targetNodeId"), TargetNodeId);
        Payload->TryGetStringField(TEXT("inputName"), InputName);

        // Also check for newer parameter names
        if (SourceNodeId.IsEmpty())
        {
            Payload->TryGetStringField(TEXT("fromExpression"), SourceNodeId);
        }
        if (TargetNodeId.IsEmpty())
        {
            Payload->TryGetStringField(TEXT("toExpression"), TargetNodeId);
        }

        // Try numeric indices
        int32 SourceIndex = -1, TargetIndex = -1;
        Payload->TryGetNumberField(TEXT("fromExpression"), SourceIndex);
        Payload->TryGetNumberField(TEXT("toExpression"), TargetIndex);

        UMaterialExpression *SourceExpr = (SourceIndex >= 0)
            ? FindExpressionByIdOrNameOrIndex(FString(), SourceIndex)
            : FindExpressionByIdOrNameOrIndex(SourceNodeId);

        if (!SourceExpr)
        {
            SendAutomationError(Socket, RequestId, TEXT("Source node not found."),
                TEXT("NODE_NOT_FOUND"));
            return true;
        }

        // Target could be another expression OR the main material node
        if ((TargetNodeId.IsEmpty() || TargetNodeId == TEXT("Main")) && TargetIndex < 0)
        {
            // Connect to main material node
            bool bFound = false;
#if WITH_EDITORONLY_DATA
            if (InputName == TEXT("BaseColor"))
            {
                MCP_GET_MATERIAL_INPUT(Material, BaseColor).Expression = SourceExpr;
                bFound = true;
            }
            else if (InputName == TEXT("EmissiveColor"))
            {
                MCP_GET_MATERIAL_INPUT(Material, EmissiveColor).Expression = SourceExpr;
                bFound = true;
            }
            else if (InputName == TEXT("Roughness"))
            {
                MCP_GET_MATERIAL_INPUT(Material, Roughness).Expression = SourceExpr;
                bFound = true;
            }
            else if (InputName == TEXT("Metallic"))
            {
                MCP_GET_MATERIAL_INPUT(Material, Metallic).Expression = SourceExpr;
                bFound = true;
            }
            else if (InputName == TEXT("Specular"))
            {
                MCP_GET_MATERIAL_INPUT(Material, Specular).Expression = SourceExpr;
                bFound = true;
            }
            else if (InputName == TEXT("Normal"))
            {
                MCP_GET_MATERIAL_INPUT(Material, Normal).Expression = SourceExpr;
                bFound = true;
            }
            else if (InputName == TEXT("Opacity"))
            {
                MCP_GET_MATERIAL_INPUT(Material, Opacity).Expression = SourceExpr;
                bFound = true;
            }
            else if (InputName == TEXT("OpacityMask"))
            {
                MCP_GET_MATERIAL_INPUT(Material, OpacityMask).Expression = SourceExpr;
                bFound = true;
            }
            else if (InputName == TEXT("AmbientOcclusion"))
            {
                MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion).Expression = SourceExpr;
                bFound = true;
            }
            else if (InputName == TEXT("SubsurfaceColor"))
            {
                MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor).Expression = SourceExpr;
                bFound = true;
            }
#endif

            if (bFound)
            {
                Material->PostEditChange();
                Material->MarkPackageDirty();

                TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
                McpHandlerUtils::AddVerification(Result, Material);
                Result->SetStringField(TEXT("inputName"), InputName);
                SendAutomationResponse(Socket, RequestId, true,
                    TEXT("Connected to main material node."), Result);
            }
            else
            {
                SendAutomationError(Socket, RequestId,
                    FString::Printf(TEXT("Unknown input on main node: %s"), *InputName),
                    TEXT("INVALID_PIN"));
            }
            return true;
        }
        else
        {
            // Connect to another expression
            UMaterialExpression *TargetExpr = (TargetIndex >= 0)
                ? FindExpressionByIdOrNameOrIndex(FString(), TargetIndex)
                : FindExpressionByIdOrNameOrIndex(TargetNodeId);

            if (TargetExpr)
            {
                // Find the FExpressionInput property by name
                FProperty *Prop = TargetExpr->GetClass()->FindPropertyByName(FName(*InputName));
                if (Prop)
                {
                    if (FStructProperty *StructProp = CastField<FStructProperty>(Prop))
                    {
                        // Check if this is an FExpressionInput struct
                        if (StructProp->Struct->GetFName() == FName("ExpressionInput"))
                        {
                            FExpressionInput *InputPtr =
                                StructProp->ContainerPtrToValuePtr<FExpressionInput>(TargetExpr);
                            if (InputPtr)
                            {
                                InputPtr->Expression = SourceExpr;
                                Material->PostEditChange();
                                Material->MarkPackageDirty();

                                TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
                                McpHandlerUtils::AddVerification(Result, Material);
                                Result->SetStringField(TEXT("inputName"), InputName);
                                SendAutomationResponse(Socket, RequestId, true,
                                    TEXT("Nodes connected."), Result);
                                return true;
                            }
                        }
                    }
                }

                SendAutomationError(Socket, RequestId,
                    FString::Printf(TEXT("Input pin '%s' not found or not compatible."), *InputName),
                    TEXT("PIN_NOT_FOUND"));
            }
            else
            {
                SendAutomationError(Socket, RequestId, TEXT("Target node not found."),
                    TEXT("NODE_NOT_FOUND"));
            }
            return true;
        }
    }

    // =========================================================================
    // subAction: break_connections
    // =========================================================================
    // Breaks connections on a node or main material input
    // Parameters: nodeId, pinName, expressionIndex
    // -------------------------------------------------------------------------
    else if (SubAction == TEXT("break_connections"))
    {
        FString NodeId;
        Payload->TryGetStringField(TEXT("nodeId"), NodeId);
        FString PinName;
        Payload->TryGetStringField(TEXT("pinName"), PinName);

        // Check if disconnecting from main node
        if (NodeId.IsEmpty() || NodeId == TEXT("Main"))
        {
            if (!PinName.IsEmpty())
            {
                bool bFound = false;
#if WITH_EDITORONLY_DATA
                if (PinName == TEXT("BaseColor"))
                {
                    MCP_GET_MATERIAL_INPUT(Material, BaseColor).Expression = nullptr;
                    bFound = true;
                }
                else if (PinName == TEXT("EmissiveColor"))
                {
                    MCP_GET_MATERIAL_INPUT(Material, EmissiveColor).Expression = nullptr;
                    bFound = true;
                }
                else if (PinName == TEXT("Roughness"))
                {
                    MCP_GET_MATERIAL_INPUT(Material, Roughness).Expression = nullptr;
                    bFound = true;
                }
                else if (PinName == TEXT("Metallic"))
                {
                    MCP_GET_MATERIAL_INPUT(Material, Metallic).Expression = nullptr;
                    bFound = true;
                }
                else if (PinName == TEXT("Specular"))
                {
                    MCP_GET_MATERIAL_INPUT(Material, Specular).Expression = nullptr;
                    bFound = true;
                }
                else if (PinName == TEXT("Normal"))
                {
                    MCP_GET_MATERIAL_INPUT(Material, Normal).Expression = nullptr;
                    bFound = true;
                }
                else if (PinName == TEXT("Opacity"))
                {
                    MCP_GET_MATERIAL_INPUT(Material, Opacity).Expression = nullptr;
                    bFound = true;
                }
                else if (PinName == TEXT("OpacityMask"))
                {
                    MCP_GET_MATERIAL_INPUT(Material, OpacityMask).Expression = nullptr;
                    bFound = true;
                }
                else if (PinName == TEXT("AmbientOcclusion"))
                {
                    MCP_GET_MATERIAL_INPUT(Material, AmbientOcclusion).Expression = nullptr;
                    bFound = true;
                }
                else if (PinName == TEXT("SubsurfaceColor"))
                {
                    MCP_GET_MATERIAL_INPUT(Material, SubsurfaceColor).Expression = nullptr;
                    bFound = true;
                }
#endif

                if (bFound)
                {
                    Material->PostEditChange();
                    Material->MarkPackageDirty();

                    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
                    McpHandlerUtils::AddVerification(Result, Material);
                    Result->SetStringField(TEXT("pinName"), PinName);
                    SendAutomationResponse(Socket, RequestId, true,
                        TEXT("Disconnected from main material pin."), Result);
                    return true;
                }
            }
        }

        // Check for expression index
        int32 ExpressionIndex = -1;
        Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex);

        UMaterialExpression *TargetExpr = (ExpressionIndex >= 0)
            ? FindExpressionByIdOrNameOrIndex(FString(), ExpressionIndex)
            : FindExpressionByIdOrNameOrIndex(NodeId);

        if (TargetExpr)
        {
            // Note: Generic input clearing not implemented - requires property iteration
            Material->PostEditChange();
            Material->MarkPackageDirty();

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            McpHandlerUtils::AddVerification(Result, Material);
            SendAutomationResponse(Socket, RequestId, true,
                TEXT("Node disconnection partial (generic inputs not cleared)."), Result);
            return true;
        }

        SendAutomationError(Socket, RequestId, TEXT("Node not found."),
            TEXT("NODE_NOT_FOUND"));
        return true;
    }

    // =========================================================================
    // subAction: get_node_details
    // =========================================================================
    // Gets details of a specific node or lists all nodes if no ID provided
    // Parameters: nodeId, expressionIndex
    // -------------------------------------------------------------------------
    else if (SubAction == TEXT("get_node_details"))
    {
        FString NodeId;
        Payload->TryGetStringField(TEXT("nodeId"), NodeId);

        int32 ExpressionIndex = -1;
        Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex);

        UMaterialExpression *TargetExpr = nullptr;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        auto AllExpressions = Material->GetExpressions();
#else
        // UE 5.0: Direct access to Expressions array
        auto& AllExpressions = Material->Expressions;
#endif

        // Find specific node
        if (ExpressionIndex >= 0)
        {
            TargetExpr = FindExpressionByIdOrNameOrIndex(FString(), ExpressionIndex);
        }
        else if (!NodeId.IsEmpty())
        {
            TargetExpr = FindExpressionByIdOrNameOrIndex(NodeId);
        }

        if (TargetExpr)
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            McpHandlerUtils::AddVerification(Result, Material);
            Result->SetStringField(TEXT("nodeType"), TargetExpr->GetClass()->GetName());
            Result->SetStringField(TEXT("desc"), TargetExpr->Desc);
            Result->SetNumberField(TEXT("x"), TargetExpr->MaterialExpressionEditorX);
            Result->SetNumberField(TEXT("y"), TargetExpr->MaterialExpressionEditorY);

            SendAutomationResponse(Socket, RequestId, true,
                TEXT("Node details retrieved."), Result);
        }
        else
        {
            // List all available nodes
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            TArray<TSharedPtr<FJsonValue>> NodeList;

            for (int32 i = 0; i < AllExpressions.Num(); ++i)
            {
                UMaterialExpression *Expr = AllExpressions[i];
                TSharedPtr<FJsonObject> NodeInfo = McpHandlerUtils::CreateResultObject();
                NodeInfo->SetStringField(TEXT("nodeId"), Expr->GetName());
                NodeInfo->SetStringField(TEXT("nodeType"), Expr->GetClass()->GetName());
                NodeInfo->SetNumberField(TEXT("index"), i);
                if (!Expr->Desc.IsEmpty())
                {
                    NodeInfo->SetStringField(TEXT("desc"), Expr->Desc);
                }
                NodeList.Add(MakeShared<FJsonValueObject>(NodeInfo));
            }

            Result->SetArrayField(TEXT("availableNodes"), NodeList);
            Result->SetNumberField(TEXT("nodeCount"), AllExpressions.Num());

            if (NodeId.IsEmpty() && ExpressionIndex < 0)
            {
                FString Message = FString::Printf(
                    TEXT("Material has %d nodes. Available nodes listed."),
                    AllExpressions.Num());
                SendAutomationResponse(Socket, RequestId, true, Message, Result);
            }
            else
            {
                FString Message = FString::Printf(
                    TEXT("Node '%s' not found. Material has %d nodes."),
                    NodeId.IsEmpty() ? *FString::FromInt(ExpressionIndex) : *NodeId,
                    AllExpressions.Num());
                SendAutomationResponse(Socket, RequestId, false, Message, Result,
                    TEXT("NODE_NOT_FOUND"));
            }
        }
        return true;
    }

    SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Unknown subAction: %s"), *SubAction),
        TEXT("INVALID_SUBACTION"));
    return true;

#else
    SendAutomationError(Socket, RequestId, TEXT("Editor only."),
        TEXT("EDITOR_ONLY"));
    return true;
#endif
}

// =============================================================================
// Handler: add_material_texture_sample
// =============================================================================
// Adds a TextureSample expression to a material with texture assignment
// Parameters: materialPath, texturePath, coordinateIndex, x, y
// -----------------------------------------------------------------------------

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialTextureSample(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR

    // -------------------------------------------------------------------------
    // Payload Validation
    // -------------------------------------------------------------------------
    if (!Payload.IsValid())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Required Parameters
    // -------------------------------------------------------------------------
    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) || MaterialPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'materialPath'."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString TexturePath;
    if (!Payload->TryGetStringField(TEXT("texturePath"), TexturePath) || TexturePath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'texturePath'."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
    {
        SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Could not load Material: %s"), *MaterialPath),
            TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    UTexture *Texture = LoadObject<UTexture>(nullptr, *TexturePath);
    if (!Texture)
    {
        SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Could not load Texture: %s"), *TexturePath),
            TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Optional Parameters
    // -------------------------------------------------------------------------
    int32 CoordinateIndex = 0;
    Payload->TryGetNumberField(TEXT("coordinateIndex"), CoordinateIndex);

    float X = 0.0f, Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    // -------------------------------------------------------------------------
    // Create TextureSample Expression
    // -------------------------------------------------------------------------
    UMaterialExpressionTextureSample *TexSample = NewObject<UMaterialExpressionTextureSample>(
        Material, UMaterialExpressionTextureSample::StaticClass(), NAME_None, RF_Transactional);

    if (!TexSample)
    {
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
    if (Material->GetEditorOnlyData())
    {
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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Material);
    Result->SetStringField(TEXT("nodeId"), TexSample->GetName());
    Result->SetStringField(TEXT("texturePath"), Texture->GetPathName());

    SendAutomationResponse(Socket, RequestId, true,
        TEXT("TextureSample expression added to material."), Result);
    return true;

#else
    SendAutomationError(Socket, RequestId, TEXT("Editor only."),
        TEXT("EDITOR_ONLY"));
    return true;
#endif
}

// =============================================================================
// Handler: add_material_expression
// =============================================================================
// Adds a generic material expression by class name
// Parameters: materialPath, expressionClass, x, y
// -----------------------------------------------------------------------------

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialExpression(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR

    // -------------------------------------------------------------------------
    // Payload Validation
    // -------------------------------------------------------------------------
    if (!Payload.IsValid())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Required Parameters
    // -------------------------------------------------------------------------
    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) || MaterialPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'materialPath'."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString ExpressionClassName;
    if (!Payload->TryGetStringField(TEXT("expressionClass"), ExpressionClassName) || ExpressionClassName.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'expressionClass'."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
    {
        SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Could not load Material: %s"), *MaterialPath),
            TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Optional Parameters
    // -------------------------------------------------------------------------
    float X = 0.0f, Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    // -------------------------------------------------------------------------
    // Resolve Expression Class
    // -------------------------------------------------------------------------
    UClass *ExpressionClass = nullptr;

    // Try with full path first
    ExpressionClass = FindObject<UClass>(
        nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ExpressionClassName));

    // If not found, try with MaterialExpression prefix
    if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
    {
        FString PrefixedName = FString::Printf(TEXT("MaterialExpression%s"), *ExpressionClassName);
        ExpressionClass = FindObject<UClass>(
            nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *PrefixedName));
    }

    // Try ResolveClassByName helper as fallback
    if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
    {
        ExpressionClass = ResolveClassByName(ExpressionClassName);
        if (ExpressionClass && !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
        {
            ExpressionClass = nullptr;
        }
    }

    // Final fallback with MaterialExpression prefix
    if (!ExpressionClass)
    {
        FString PrefixedName = FString::Printf(TEXT("MaterialExpression%s"), *ExpressionClassName);
        ExpressionClass = ResolveClassByName(PrefixedName);
        if (ExpressionClass && !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
        {
            ExpressionClass = nullptr;
        }
    }

    if (!ExpressionClass)
    {
        SendAutomationError(Socket, RequestId,
            FString::Printf(
                TEXT("Unknown expression class: %s. Try using the full class "
                     "name like 'MaterialExpressionAdd' or 'Add'."),
                *ExpressionClassName),
            TEXT("CLASS_NOT_FOUND"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Create Expression
    // -------------------------------------------------------------------------
    UMaterialExpression *NewExpr = NewObject<UMaterialExpression>(
        Material, ExpressionClass, NAME_None, RF_Transactional);

    if (!NewExpr)
    {
        SendAutomationError(Socket, RequestId,
            TEXT("Failed to create expression."),
            TEXT("CREATE_FAILED"));
        return true;
    }

    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;

#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    if (Material->GetEditorOnlyData())
    {
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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Material);
    Result->SetStringField(TEXT("nodeId"), NewExpr->GetName());
    Result->SetStringField(TEXT("expressionClass"), ExpressionClass->GetName());

    SendAutomationResponse(Socket, RequestId, true,
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

// =============================================================================
// Handler: create_material_nodes
// =============================================================================
// Batch creates multiple material expression nodes
// Parameters: materialPath, nodes[] (array of node definitions)
// -----------------------------------------------------------------------------

bool UMcpAutomationBridgeSubsystem::HandleCreateMaterialNodes(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR

    // -------------------------------------------------------------------------
    // Payload Validation
    // -------------------------------------------------------------------------
    if (!Payload.IsValid())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Required Parameters
    // -------------------------------------------------------------------------
    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) || MaterialPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'materialPath'."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UMaterial *Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
    {
        SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Could not load Material: %s"), *MaterialPath),
            TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>> *NodesArray;
    if (!Payload->TryGetArrayField(TEXT("nodes"), NodesArray) || !NodesArray)
    {
        SendAutomationError(Socket, RequestId,
            TEXT("Missing 'nodes' array."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // -------------------------------------------------------------------------
    // Process Nodes
    // -------------------------------------------------------------------------
    TArray<TSharedPtr<FJsonValue>> CreatedNodes;
    int32 SuccessCount = 0;
    int32 FailCount = 0;

    for (const auto &NodeVal : *NodesArray)
    {
        TSharedPtr<FJsonObject> NodeObj = NodeVal->AsObject();
        if (!NodeObj.IsValid())
        {
            FailCount++;
            continue;
        }

        FString NodeType;
        if (!NodeObj->TryGetStringField(TEXT("type"), NodeType) || NodeType.IsEmpty())
        {
            FailCount++;
            continue;
        }

        float X = 0.0f, Y = 0.0f;
        NodeObj->TryGetNumberField(TEXT("x"), X);
        NodeObj->TryGetNumberField(TEXT("y"), Y);

        // Resolve expression class
        UClass *ExpressionClass = nullptr;

        // Common shorthand types
        if (NodeType == TEXT("TextureSample"))
        {
            ExpressionClass = UMaterialExpressionTextureSample::StaticClass();
        }
        else if (NodeType == TEXT("VectorParameter"))
        {
            ExpressionClass = UMaterialExpressionVectorParameter::StaticClass();
        }
        else if (NodeType == TEXT("ScalarParameter"))
        {
            ExpressionClass = UMaterialExpressionScalarParameter::StaticClass();
        }
        else if (NodeType == TEXT("Add"))
        {
            ExpressionClass = UMaterialExpressionAdd::StaticClass();
        }
        else if (NodeType == TEXT("Multiply"))
        {
            ExpressionClass = UMaterialExpressionMultiply::StaticClass();
        }
        else if (NodeType == TEXT("Constant"))
        {
            ExpressionClass = UMaterialExpressionConstant::StaticClass();
        }
        else if (NodeType == TEXT("Constant3Vector") || NodeType == TEXT("Color"))
        {
            ExpressionClass = UMaterialExpressionConstant3Vector::StaticClass();
        }
        else
        {
            // Try to resolve by name
            ExpressionClass = FindObject<UClass>(
                nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *NodeType));
            if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
            {
                FString PrefixedName = FString::Printf(TEXT("MaterialExpression%s"), *NodeType);
                ExpressionClass = FindObject<UClass>(
                    nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *PrefixedName));
            }
            if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
            {
                ExpressionClass = ResolveClassByName(NodeType);
                if (ExpressionClass && !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
                {
                    ExpressionClass = nullptr;
                }
            }
        }

        if (!ExpressionClass)
        {
            FailCount++;
            continue;
        }

        UMaterialExpression *NewExpr = NewObject<UMaterialExpression>(
            Material, ExpressionClass, NAME_None, RF_Transactional);

        if (!NewExpr)
        {
            FailCount++;
            continue;
        }

        NewExpr->MaterialExpressionEditorX = (int32)X;
        NewExpr->MaterialExpressionEditorY = (int32)Y;

        // Handle parameter name
        FString ParamName;
        if (NodeObj->TryGetStringField(TEXT("name"), ParamName))
        {
            if (UMaterialExpressionParameter *ParamExpr = Cast<UMaterialExpressionParameter>(NewExpr))
            {
                ParamExpr->ParameterName = FName(*ParamName);
            }
        }

        // Handle texture path for texture samples
        FString TexturePath;
        if (NodeObj->TryGetStringField(TEXT("texturePath"), TexturePath))
        {
            if (UMaterialExpressionTextureSample *TexSample = Cast<UMaterialExpressionTextureSample>(NewExpr))
            {
                UTexture *Texture = LoadObject<UTexture>(nullptr, *TexturePath);
                if (Texture)
                {
                    TexSample->Texture = Texture;
                }
            }
        }

        // Handle default value for constants
        double DefaultValue = 0.0;
        if (NodeObj->TryGetNumberField(TEXT("value"), DefaultValue))
        {
            if (UMaterialExpressionConstant *ConstExpr = Cast<UMaterialExpressionConstant>(NewExpr))
            {
                ConstExpr->R = (float)DefaultValue;
            }
        }

#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        if (Material->GetEditorOnlyData())
        {
            MCP_GET_MATERIAL_EXPRESSIONS(Material).Add(NewExpr);
        }
#else
        // UE 5.0: Direct access
        Material->Expressions.Add(NewExpr);
#endif
#endif

        // Record created node
        TSharedPtr<FJsonObject> NodeInfo = McpHandlerUtils::CreateResultObject();
        NodeInfo->SetStringField(TEXT("nodeId"), NewExpr->GetName());
        NodeInfo->SetStringField(TEXT("type"), ExpressionClass->GetName());
        CreatedNodes.Add(MakeShared<FJsonValueObject>(NodeInfo));

        SuccessCount++;
    }

    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    McpSafeAssetSave(Material);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Material);
    Result->SetArrayField(TEXT("createdNodes"), CreatedNodes);
    Result->SetNumberField(TEXT("successCount"), SuccessCount);
    Result->SetNumberField(TEXT("failCount"), FailCount);

    SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Created %d nodes (%d failed)."), SuccessCount, FailCount),
        Result);
    return true;

#else
    SendAutomationError(Socket, RequestId, TEXT("Editor only."),
        TEXT("EDITOR_ONLY"));
    return true;
#endif // WITH_EDITOR
}
