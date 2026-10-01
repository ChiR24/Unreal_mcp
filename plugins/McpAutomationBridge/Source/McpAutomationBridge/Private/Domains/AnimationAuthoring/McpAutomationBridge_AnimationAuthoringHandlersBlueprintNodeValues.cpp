#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"

namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleBlueprintNodeValueActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("set_anim_graph_node_value"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("blueprintPath"), TEXT("")));
        FString NodeName = GetJsonStringField(Params, TEXT("nodeName"), TEXT(""));
        if (NodeName.IsEmpty())
        {
            NodeName = GetJsonStringField(Params, TEXT("nodeId"), TEXT(""));
        }
        FString PropertyName = GetJsonStringField(Params, TEXT("propertyName"), TEXT(""));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        if (NodeName.IsEmpty() || PropertyName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("nodeName (or nodeId) and propertyName are required"), TEXT("MISSING_PARAMETERS"));
        }

        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }

        // The AnimGraph and the graphs under it (a state's graph is where its Sequence Player is), by object name or
        // GUID first: a node asked for by the name create_node reported was not found, and two Sequence Players share a title.
        TArray<UEdGraphNode*> Ambiguous;
        UEdGraphNode* FoundNode = FindAnimGraphNode(AnimBP, NodeName, Ambiguous);
        if (!FoundNode && Ambiguous.Num() > 1)
        {
            TArray<TSharedPtr<FJsonValue>> Candidates;
            for (const UEdGraphNode* Candidate : Ambiguous)
            {
                Candidates.Add(DescribeAnimGraphNode(Candidate));
            }
            Response->SetArrayField(TEXT("candidates"), Candidates);
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("'%s' names %d nodes; name one by its object name or nodeId (candidates lists them)."), *NodeName, Ambiguous.Num()), TEXT("AMBIGUOUS_NODE"));
        }
        if (!FoundNode)
        {
            Response->SetArrayField(TEXT("nodes"), ListAnimGraphNodes(AnimBP, 60));
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Node '%s' not found in the AnimGraph or the graphs under it; nodes lists them (name, nodeId, title, graph)."), *NodeName), TEXT("NODE_NOT_FOUND"));
        }

        // The property, and the memory it lives in: a setting of the embedded Node struct (Alpha, PlayRate, Sequence) is
        // stored in that struct, which is not the graph node the write used to be aimed at.
        void* Container = nullptr;
        FString Missing;
        FProperty* Property = ResolveAnimNodeProperty(FoundNode, PropertyName, Container, Missing);
        if (!Property)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Property '%s' not found on node '%s'"), Missing.IsEmpty() ? *PropertyName : *Missing, *FoundNode->GetName()), TEXT("PROPERTY_NOT_FOUND"));
        }

        // Get the value from params and apply it
        TSharedPtr<FJsonValue> ValueField = Params->TryGetField(TEXT("value"));
        if (!ValueField.IsValid())
        {
            ANIM_ERROR_RESPONSE(TEXT("value parameter is required"), TEXT("MISSING_VALUE"));
        }

        FString Stored;
        FString SetError;
        FString SetErrorCode;
        if (IsAnimPlayerAssetProperty(FoundNode, Property))
        {
            // The animation an asset player plays goes through the node's own setter and is read back from the node.
            if (!SetAnimPlayerAsset(FoundNode, ValueField, Stored, SetError, SetErrorCode))
            {
                ANIM_ERROR_RESPONSE(SetError, SetErrorCode);
            }
        }
        else
        {
            FoundNode->Modify();
            if (!ApplyJsonValueToProperty(Container, Property, ValueField, SetError))
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Failed to set property: %s"), *SetError), TEXT("PROPERTY_SET_FAILED"));
            }
            Property->ExportText_Direct(Stored, Property->ContainerPtrToValuePtr<void>(Container), nullptr, nullptr, PPF_None);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);

        Response->SetStringField(TEXT("nodeName"), FoundNode->GetName());
        Response->SetStringField(TEXT("nodeId"), FoundNode->NodeGuid.ToString());
        Response->SetStringField(TEXT("propertyName"), PropertyName);
        Response->SetStringField(TEXT("value"), Stored);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Property '%s' set on node '%s' (reads back %s)"), *PropertyName, *FoundNode->GetName(), *Stored));
        return Response;
    }

    return nullptr;
}

} // namespace McpAnimationAuthoring
