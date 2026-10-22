#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Domains/PCG/Discovery/Schema/McpPCGNodeSchemaAction.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Domains/PCG/Editing/McpPCGConnect.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "MCP/Gateway/McpNativeGatewayCanonicalJson.h"
#include "Domains/PCG/Editor/McpPCGEditor.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
namespace
{
TArray<TSharedPtr<FJsonValue>> Page(const TArray<TSharedPtr<FJsonValue>>& Items, int32 Offset, int32 Limit)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    for (int32 Index = Offset; Index < Items.Num() && Result.Num() < Limit; ++Index) Result.Add(Items[Index]);
    return Result;
}

}

TSharedPtr<FJsonObject> ReadPCGEditorGraphSnapshot(UPCGGraph* Graph)
{
    auto Result = MakeShared<FJsonObject>();
    bool bAmbiguous = false;
    UEdGraph* EditorGraph = FindExistingPCGEditorGraph(Graph, bAmbiguous);
    Result->SetBoolField(TEXT("available"), EditorGraph != nullptr);
    Result->SetBoolField(TEXT("ambiguous"), bAmbiguous);
    TArray<TSharedPtr<FJsonValue>> Nodes, Edges;
    if (EditorGraph)
    {
        for (UEdGraphNode* Node : EditorGraph->Nodes)
        {
            if (!Node) continue;
            auto Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("editorNodeId"), Node->GetName());
            Item->SetStringField(TEXT("class"), Node->GetClass()->GetPathName());
            Item->SetNumberField(TEXT("x"), Node->NodePosX);
            Item->SetNumberField(TEXT("y"), Node->NodePosY);
            const FObjectPropertyBase* ModelProperty = FindFProperty<FObjectPropertyBase>(Node->GetClass(), TEXT("PCGNode"));
            const UObject* Model = ModelProperty ? ModelProperty->GetObjectPropertyValue_InContainer(Node) : nullptr;
            if (Model) Item->SetStringField(TEXT("nodeId"), Model->GetName());
            else if (ModelProperty) Item->SetBoolField(TEXT("unmappedNode"), true);
            else Item->SetBoolField(TEXT("extraNode"), true);
            TArray<TSharedPtr<FJsonValue>> Pins;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin) continue;
                auto PinData = MakeShared<FJsonObject>();
                PinData->SetStringField(TEXT("label"), Pin->PinName.ToString());
                PinData->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Output ? TEXT("output") : TEXT("input"));
                PinData->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
                PinData->SetStringField(TEXT("subCategory"), Pin->PinType.PinSubCategory.ToString());
                Pins.Add(MakeShared<FJsonValueObject>(PinData));
            }
            Item->SetArrayField(TEXT("pins"), Pins);
            Item->SetStringField(TEXT("comment"), Node->NodeComment);
            Item->SetObjectField(TEXT("layout"), McpPropertyReflection::ExportPropertiesToJson(Node,
                {FName(TEXT("NodeWidth")), FName(TEXT("NodeHeight")), FName(TEXT("CommentColor"))}));
            Nodes.Add(MakeShared<FJsonValueObject>(Item));
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin || Pin->Direction != EGPD_Output) continue;
                for (const UEdGraphPin* Target : Pin->LinkedTo)
                {
                    if (!Target || !Target->GetOwningNode()) continue;
                    auto Edge = MakeShared<FJsonObject>();
                    Edge->SetStringField(TEXT("sourceEditorNodeId"), Node->GetName());
                    Edge->SetStringField(TEXT("sourcePin"), Pin->PinName.ToString());
                    Edge->SetStringField(TEXT("targetEditorNodeId"), Target->GetOwningNode()->GetName());
                    Edge->SetStringField(TEXT("targetPin"), Target->PinName.ToString());
                    Edges.Add(MakeShared<FJsonValueObject>(Edge));
                }
            }
        }
    }
    Edges.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B) {
        FString Left, Right;
        McpCanonicalJsonObject(A->AsObject(), Left, true);
        McpCanonicalJsonObject(B->AsObject(), Right, true);
        return Left.Compare(Right, ESearchCase::CaseSensitive) < 0;
    });
    Result->SetArrayField(TEXT("nodes"), Nodes);
    Result->SetArrayField(TEXT("edges"), Edges);
    return Result;
}

bool HandlePCGDiscovery(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (SubAction == TEXT("get_pcg_node_schema")) return HandlePCGNodeSchema(Bridge, RequestId, Payload, Socket);
    if (SubAction != TEXT("list_pcg_node_types") && SubAction != TEXT("get_pcg_settings_schema") &&
        SubAction != TEXT("get_pcg_graph") && SubAction != TEXT("get_pcg_editor_graph") && SubAction != TEXT("validate_pcg_graph")) return false;
    auto Result = McpHandlerUtils::CreateResultObject();
    const int32 Offset = FMath::Max(0, GetJsonIntField(Payload, TEXT("offset"), 0));
    const int32 Limit = FMath::Clamp(GetJsonIntField(Payload, TEXT("limit"), 50), 1, 100);
    TArray<TSharedPtr<FJsonValue>> Items;
    bool bPaged = false;
    int32 ItemTotal = 0;
    if (SubAction == TEXT("list_pcg_node_types"))
    {
        const FString Filter = GetJsonStringField(Payload, TEXT("filter"));
        TArray<UClass*> Classes;
        for (TObjectIterator<UClass> It; It; ++It)
        {
            if (It->IsChildOf(UPCGSettings::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) &&
                (Filter.IsEmpty() || It->GetPathName().Contains(Filter))) Classes.Add(*It);
        }
        Classes.Sort([](const UClass& A, const UClass& B) { return A.GetPathName().Compare(B.GetPathName(), ESearchCase::CaseSensitive) < 0; });
        for (UClass* Class : Classes)
        {
            auto Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("settingsClass"), Class->GetPathName());
            Item->SetStringField(TEXT("name"), Class->GetName());
            Items.Add(MakeShared<FJsonValueObject>(Item));
        }
        Result->SetBoolField(TEXT("loadedClassesOnly"), true);
    }
    else if (SubAction == TEXT("get_pcg_settings_schema"))
    {
        UClass* Class = ResolvePCGSettingsClass(GetJsonStringField(Payload, TEXT("settingsClass")));
        if (!Class)
        {
            Bridge->SendAutomationError(Socket, RequestId, TEXT("PCG settings class was not found."), TEXT("CLASS_NOT_FOUND"));
            return true;
        }
        FPCGReadBudget Budget;
        if (!Budget.CheckObject(Class->GetDefaultObject()))
        {
            Bridge->SendAutomationError(Socket, RequestId, TEXT("Settings defaults exceed the bounded authored read budget."), TEXT("PCG_SNAPSHOT_UNAVAILABLE"));
            return true;
        }
        Result->SetStringField(TEXT("settingsClass"), Class->GetPathName());
        bPaged = true;
        for (TFieldIterator<FProperty> It(Class); It; ++It)
        {
            if (!It->HasAnyPropertyFlags(CPF_Edit) || It->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;
            if (ItemTotal++ < Offset || Items.Num() >= Limit) continue;
            auto Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("name"), It->GetName());
            Item->SetStringField(TEXT("type"), McpPropertyReflection::GetPropertyTypeName(*It));
            Item->SetBoolField(TEXT("readOnly"), It->HasAnyPropertyFlags(CPF_EditConst));
            Item->SetStringField(TEXT("category"), It->GetMetaData(TEXT("Category")));
            Item->SetField(TEXT("defaultValue"), McpPropertyReflection::ExportPropertyToJsonValueOrText(Class->GetDefaultObject(), *It));
            Items.Add(MakeShared<FJsonValueObject>(Item));
        }
    }
    else
    {
        FString GraphPath, Error;
        UPCGGraph* Graph = LoadPCGAsset<UPCGGraph>(GetJsonStringField(Payload, TEXT("graphPath")), TEXT("PCG graph"), GraphPath, Error);
        if (!Graph)
        {
            Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("ASSET_NOT_FOUND"));
            return true;
        }
        Result->SetStringField(TEXT("graphPath"), GraphPath);
        const FString Revision = GetPCGGraphRevision(Graph);
        if (Revision.IsEmpty())
        {
            Bridge->SendAutomationError(Socket, RequestId, TEXT("Complete authored PCG state exceeds the bounded read budget or has invalid dependencies."), TEXT("PCG_SNAPSHOT_UNAVAILABLE"));
            return true;
        }
        Result->SetStringField(TEXT("revision"), Revision);
        const int32 EdgeOffset = FMath::Max(0, GetJsonIntField(Payload, TEXT("edgeOffset"), 0));
        const int32 EdgeLimit = FMath::Clamp(GetJsonIntField(Payload, TEXT("edgeLimit"), 500), 1, 1000);
        const FString NodeId = GetJsonStringField(Payload, TEXT("nodeId"));
        bPaged = SubAction != TEXT("get_pcg_editor_graph");
        auto Snapshot = bPaged ? ReadPCGGraph(Graph, GetJsonBoolField(Payload, TEXT("includeSettings"), false), Offset, Limit, EdgeOffset, EdgeLimit, NodeId)
            : ReadPCGEditorGraphSnapshot(Graph);
        Items = Snapshot->GetArrayField(TEXT("nodes"));
        if (bPaged) ItemTotal = static_cast<int32>(Snapshot->GetNumberField(TEXT("nodeTotal")));
        else if (!NodeId.IsEmpty()) Items = Items.FilterByPredicate([&NodeId](const TSharedPtr<FJsonValue>& Item) {
            FString Id;
            return Item->AsObject()->TryGetStringField(TEXT("nodeId"), Id) && Id == NodeId;
        });
        const auto& Edges = Snapshot->GetArrayField(TEXT("edges"));
        const int32 EdgeTotal = bPaged ? static_cast<int32>(Snapshot->GetNumberField(TEXT("edgeTotal"))) : Edges.Num();
        Result->SetArrayField(TEXT("edges"), bPaged ? Edges : Page(Edges, EdgeOffset, EdgeLimit));
        Result->SetNumberField(TEXT("edgeTotal"), EdgeTotal);
        Result->SetNumberField(TEXT("edgeOffset"), EdgeOffset);
        Result->SetBoolField(TEXT("edgesTruncated"), EdgeOffset < EdgeTotal && EdgeLimit < EdgeTotal - EdgeOffset);
        if (SubAction == TEXT("get_pcg_editor_graph") || SubAction == TEXT("validate_pcg_graph"))
        {
            auto Check = CheckPCGEditorGraph(Graph);
            const auto& Issues = Check->GetArrayField(TEXT("diagnostics"));
            Result->SetBoolField(TEXT("editorConsistent"), Check->GetBoolField(TEXT("consistent")));
            Check->SetArrayField(TEXT("diagnostics"), Page(Issues, Offset, Limit));
            Result->SetObjectField(TEXT("editorGraph"), Check);
        }
        if (Snapshot->HasField(TEXT("available"))) Result->SetBoolField(TEXT("available"), Snapshot->GetBoolField(TEXT("available")));
        if (Snapshot->HasField(TEXT("ambiguous"))) Result->SetBoolField(TEXT("ambiguous"), Snapshot->GetBoolField(TEXT("ambiguous")));
        if (SubAction == TEXT("validate_pcg_graph"))
        {
            const auto Diagnostics = ValidatePCGEdges(Graph);
            Result->SetBoolField(TEXT("valid"), Diagnostics.IsEmpty());
            Result->SetArrayField(TEXT("diagnostics"), Page(Diagnostics, Offset, Limit));
            Result->SetNumberField(TEXT("diagnosticTotal"), Diagnostics.Num());
            Result->SetStringField(TEXT("validationScope"), TEXT("native_pin_compatibility"));
        }
    }
    if (!bPaged) ItemTotal = Items.Num();
    Result->SetArrayField(TEXT("items"), bPaged ? Items : Page(Items, Offset, Limit));
    Result->SetNumberField(TEXT("total"), ItemTotal);
    Result->SetNumberField(TEXT("offset"), Offset);
    Result->SetNumberField(TEXT("limit"), Limit);
    Result->SetBoolField(TEXT("truncated"), Offset < ItemTotal && Limit < ItemTotal - Offset);
    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG state read."), Result);
    return true;
}
}
#endif
