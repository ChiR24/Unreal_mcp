#include "Domains/PCG/Editing/McpPCGConnect.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
bool ValidatePCGConnection(const UPCGPin* Source, const UPCGPin* Target, FString& OutError, FString& OutCode)
{
    OutCode = TEXT("INCOMPATIBLE_PINS");
    if (!Source || !Target || !Source->Node || !Target->Node || Source->Node == Target->Node ||
        !Source->IsOutputPin() || Target->IsOutputPin())
    {
        OutError = TEXT("A connection requires an output and an input on distinct nodes.");
        return false;
    }
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
    FText Reason;
    const EPCGDataTypeCompatibilityResult Compatibility = Target->GetCompatibilityWithOtherPin(Source, &Reason);
    if (Compatibility != EPCGDataTypeCompatibilityResult::Compatible)
    {
        OutCode = Compatibility == EPCGDataTypeCompatibilityResult::RequireFilter ? TEXT("PIN_FILTER_REQUIRED") :
            Compatibility == EPCGDataTypeCompatibilityResult::RequireConversion ? TEXT("PIN_CONVERSION_REQUIRED") : TEXT("INCOMPATIBLE_PINS");
        OutError = Reason.IsEmpty() ? TEXT("PCG pins cannot connect directly; no implicit conversion or filtering is performed.") : Reason.ToString();
        return false;
    }
#else
    if (!Target->IsCompatible(Source))
    {
        OutError = TEXT("PCG pins have incompatible native data types.");
        return false;
    }
#endif
    if ((!Target->AllowsMultipleConnections() && Target->EdgeCount() > 0) ||
        (!Source->AllowsMultipleConnections() && Source->EdgeCount() > 0))
    {
        if (!HasPCGEdge(Source, Target))
        {
            OutCode = TEXT("EXISTING_CONNECTION");
            OutError = TEXT("Connecting these pins would replace an existing connection; disconnect it explicitly first.");
            return false;
        }
    }
    return true;
}

TArray<TSharedPtr<FJsonValue>> ValidatePCGEdges(UPCGGraph* Graph)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    TArray<UPCGNode*> Nodes;
    Nodes.Add(Graph->GetInputNode());
    for (UPCGNode* Node : Graph->GetNodes()) Nodes.AddUnique(Node);
    Nodes.AddUnique(Graph->GetOutputNode());
    for (UPCGNode* Node : Nodes)
    {
        if (!Node) continue;
        for (UPCGPin* Pin : Node->GetOutputPins())
        {
            if (!Pin) continue;
            for (UPCGEdge* Edge : Pin->Edges)
            {
                const UPCGPin* Target = Edge && Edge->IsValid() ? Edge->GetOtherPin(Pin) : nullptr;
                FString Error, Code;
                if (ValidatePCGConnection(Pin, Target, Error, Code)) continue;
                auto Item = MakeShared<FJsonObject>();
                Item->SetStringField(TEXT("sourceNodeId"), Node->GetName());
                Item->SetStringField(TEXT("sourcePin"), Pin->Properties.Label.ToString());
                Item->SetStringField(TEXT("code"), Code);
                Item->SetStringField(TEXT("message"), Error);
                Result.Add(MakeShared<FJsonValueObject>(Item));
            }
        }
    }
    return Result;
}
}
#endif
