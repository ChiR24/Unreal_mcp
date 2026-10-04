#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// A build_material_graph step written as the single call is, {"edit": "add_material_node", "nodeKind":
// "world_position"}, failed with "Missing 'nodeType'": the gateway turns nodeKind into the typed adder (the
// add_material_node fold), and batch steps run inside the plugin, past it. This is that fold's member map:
// add_<kind>, except the three below; "batch" resolves to build_material_graph, which a step may not run.
inline FString McpMaterialBatchStepEdit(const FString& Edit, const TSharedPtr<FJsonObject>& Step)
{
    FString Kind;
    if (Edit != TEXT("add_material_node") || !Step->TryGetStringField(TEXT("nodeKind"), Kind) || Kind.IsEmpty())
    {
        return Edit;
    }
    if (Kind == TEXT("node"))
    {
        return Edit;
    }
    if (Kind == TEXT("math"))
    {
        return TEXT("add_math_node");
    }
    if (Kind == TEXT("material_function"))
    {
        return TEXT("use_material_function");
    }
    return Kind == TEXT("batch") ? FString(TEXT("build_material_graph")) : TEXT("add_") + Kind;
}
