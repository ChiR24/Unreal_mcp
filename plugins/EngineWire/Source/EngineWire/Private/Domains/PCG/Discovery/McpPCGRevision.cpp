#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "Domains/PCG/Discovery/McpPCGBudget.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "MCP/Gateway/McpNativeGatewayCanonicalJson.h"
#include "Misc/SecureHash.h"
#include "UObject/UObjectHash.h"
#include "EdGraph/EdGraph.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
const TArray<FName>& PCGRevisionExtraProperties()
{
    static const TArray<FName> Properties = { FName(TEXT("PositionX")), FName(TEXT("PositionY")), FName(TEXT("NodeTitle")),
        FName(TEXT("NodeTitleColor")), FName(TEXT("NodeComment")), FName(TEXT("CommentNodes")), FName(TEXT("bHidden")),
        FName(TEXT("bCommentBubblePinned")), FName(TEXT("bCommentBubbleVisible")), FName(TEXT("UserParameters")) };
    return Properties;
}
namespace
{
TMap<FName, FString> PinPropertyTextOverrides(UPCGPin* Pin)
{
    if (!Pin) return {};
    FPCGPinProperties Properties = Pin->Properties;
    FString Text, TextKind;
#if WITH_EDITORONLY_DATA
    // Saving assigns localization IDs to generated tooltips, including their format arguments.
    // Hash source text instead of those IDs or the current culture's translated display string.
    TextKind = Properties.Tooltip.IsCultureInvariant() ? TEXT("invariant") : TEXT("source");
    if (!Properties.Tooltip.IsFromStringTable())
        Properties.Tooltip = FText::AsCultureInvariant(Properties.Tooltip.BuildSourceString());
    else TextKind = TEXT("stringTable");
#endif
    FPCGPinProperties::StaticStruct()->ExportText(Text, &Properties, nullptr, Pin, PPF_None, nullptr);
    Text += TEXT("\n$tooltip-kind:") + TextKind;
    return {{FName(TEXT("Properties")), MoveTemp(Text)}};
}

TSharedPtr<FJsonObject> CaptureLocalRevision(UPCGGraph* Graph)
{
    auto State = ReadPCGGraph(Graph, false);
    // Public reads retain their native export. Only this private hash projection is normalized.
    // Explicit NodeTitle and complete pin properties remain covered by reflected state below.
    for (const auto& Value : State->GetArrayField(TEXT("nodes")))
    {
        const auto Node = Value->AsObject();
        Node->RemoveField(TEXT("title"));
        for (const TCHAR* Direction : {TEXT("inputPins"), TEXT("outputPins")})
            for (const auto& Pin : Node->GetArrayField(Direction)) Pin->AsObject()->RemoveField(TEXT("properties"));
    }
    auto Exact = MakeShared<FJsonObject>();
    TArray<UObject*> Objects;
    GetObjectsWithOuter(Graph, Objects, true);
    Objects.Insert(Graph, 0);
    bool bAmbiguous = false;
    UEdGraph* Editor = FindExistingPCGEditorGraph(Graph, bAmbiguous);
    if (bAmbiguous) return nullptr;
    for (UObject* Object : Objects)
    {
        if (!IsPCGAuthoredObject(Object, Graph) || (Editor && (Object == Editor || Object->IsIn(Editor)))) continue;
        const FString Hash = McpPropertyReflection::FingerprintEditableProperties(Object, PCGRevisionExtraProperties(),
            PinPropertyTextOverrides(Cast<UPCGPin>(Object)));
        if (Hash.IsEmpty()) return nullptr;
        // Reinstancing a Blueprint class changes executable inputs even when its asset path stays the same.
        const FString Class = Object->GetClass()->GetPathName() + TEXT(":") + FString::FromInt(Object->GetClass()->GetUniqueID());
        Exact->SetStringField(Object == Graph ? TEXT("$graph") : Object->GetPathName(Graph), Class + TEXT(":") + Hash);
    }
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    const FInstancedPropertyBag* Bag = Graph->GetUserParametersStruct();
    if (Bag && Bag->GetPropertyBagStruct())
    {
        for (const FPropertyBagPropertyDesc& Desc : Bag->GetPropertyBagStruct()->GetPropertyDescs())
        {
            if (!Desc.CachedProperty) return nullptr;
            const FString Hash = McpPropertyReflection::FingerprintReflectedValue(Desc.CachedProperty,
                const_cast<uint8*>(Bag->GetValue().GetMemory()));
            if (Hash.IsEmpty()) return nullptr;
            Exact->SetStringField(TEXT("$parameter:") + Desc.ID.ToString(), Hash);
        }
    }
#endif
    State->SetObjectField(TEXT("exactAuthoredState"), Exact);
    return State;
}

FString CaptureRevision(UPCGGraph* Graph, TMap<UPCGGraph*, FString>& Captured, TSet<UPCGGraph*>& Visiting, FPCGReadBudget& Budget, int32 Depth)
{
    if (!IsValid(Graph) || Depth > 32 || Visiting.Contains(Graph)) return FString();
    if (const FString* Existing = Captured.Find(Graph)) return *Existing;
    if (Captured.Num() + Visiting.Num() >= 256) return FString();
    if (!Budget.CheckGraph(Graph)) return FString();
    Visiting.Add(Graph);
    auto Dependencies = MakeShared<FJsonObject>();
    for (UPCGNode* Node : Graph->GetNodes())
    {
        const UPCGBaseSubgraphSettings* Settings = Node ? Cast<UPCGBaseSubgraphSettings>(Node->GetSettings()) : nullptr;
        UPCGGraph* Subgraph = Settings ? Settings->GetSubgraph() : nullptr;
        if (!Subgraph) continue;
        const FString Revision = CaptureRevision(Subgraph, Captured, Visiting, Budget, Depth + 1);
        if (Revision.IsEmpty()) return FString();
        Dependencies->SetStringField(Subgraph->GetPathName(), Revision);
    }
    auto State = CaptureLocalRevision(Graph);
    if (!State.IsValid() || !Budget.IsWithinTime()) return FString();
    State->SetObjectField(TEXT("subgraphRevisions"), Dependencies);
    FString Json;
    if (!McpCanonicalJsonObject(State, Json, true) || !Budget.IsWithinTime()) return FString();
    const FString Revision = FMD5::HashAnsiString(*Json);
    Visiting.Remove(Graph);
    Captured.Add(Graph, Revision);
    return Revision;
}
}

FString GetPCGGraphRevision(UPCGGraph* Graph)
{
    TMap<UPCGGraph*, FString> Captured;
    TSet<UPCGGraph*> Visiting;
    FPCGReadBudget Budget;
    return CaptureRevision(Graph, Captured, Visiting, Budget, 0);
}
}
#endif
