#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Domains/LevelStructure/McpLevelStructureEditorWorld.h"

#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "K2Node.h"
#include "K2Node_ActorBoundEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Literal.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpLevelStructure
{
namespace
{
bool LevelNodeIsA(const UClass* NodeClass, const UClass* Base)
{
    return NodeClass && NodeClass->IsChildOf(Base);
}

// The name the caller asked for when nothing in the graph holds it, else a free one beside it: the edits that follow find the
// node by the name the reply carries.
void RenameLevelBlueprintNode(UEdGraph* EventGraph, UEdGraphNode* NewNode, const FString& NodeName)
{
    if (!NodeName.IsEmpty())
    {
        FString SafeNodeName = NodeName.TrimStartAndEnd();
        for (const TCHAR* Bad : { TEXT(" "), TEXT("/"), TEXT("\\"), TEXT(":"), TEXT("."), TEXT("'"), TEXT("\"") }) { SafeNodeName.ReplaceInline(Bad, TEXT("_")); }
        if (!SafeNodeName.IsEmpty())
        {
            // The name asked for when nothing in the graph holds it (MakeUniqueObjectName numbers even a free name), so the
            // caller's own name finds the node in the edits that follow; a taken name gets a free one beside it.
            const FName WantedName(*SafeNodeName);
            const FName NodeObjectName = FindObject<UObject>(EventGraph, *SafeNodeName)
                ? MakeUniqueObjectName(EventGraph, NewNode->GetClass(), WantedName) : WantedName;
            NewNode->Rename(*NodeObjectName.ToString(), EventGraph, REN_DontCreateRedirectors | REN_NonTransactional);
        }
    }
}
} // namespace

// add_level_blueprint_node: blueprint's create_node on the level Blueprint. What only this door knows stays here: the level and
// its Blueprint (created by the first node), the node name asked for, the level's save rule, the refusals for a parameter the
// node does not take (the graph door ignores those), and the external member a placed actor's reference feeds.
bool HandleAddLevelBlueprintNode(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString NodeType = GetJsonStringField(Payload, TEXT("nodeClass"), TEXT(""));
    if (NodeType.IsEmpty())
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("nodeClass is required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Honor levelPath: edit the level the caller named, not whatever level is
    // open. A transient /Temp/ level is refused because its blueprint cannot be
    // saved (previously such a call reported success and silently discarded the
    // node — see ResolveTargetLevelForBlueprintRequest). Adding the first node
    // creates the level Blueprint.
    ULevel* CurrentLevel = nullptr;
    ULevelScriptBlueprint* LevelBP = nullptr;
    UEdGraph* EventGraph = nullptr;
    if (!ResolveLevelBlueprintGraph(Subsystem, RequestId, Socket, Payload, /*bCreate=*/true, CurrentLevel, LevelBP, EventGraph))
    {
        return true;
    }

    const FString NodeName = GetJsonStringField(Payload, TEXT("nodeName"), TEXT(""));
    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"), TEXT(""));
    const FString MemberName = GetJsonStringField(Payload, TEXT("memberName"), TEXT(""));
    const FString EventName = GetJsonStringField(Payload, TEXT("eventName"), TEXT(""));
    FString MemberClass = GetJsonStringField(Payload, TEXT("memberClass"), TEXT(""));
    FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"), TEXT(""));

    // A class path (/Script/BlueprintGraph.K2Node_IfThenElse) is its last part to the graph door.
    int32 LastDot = INDEX_NONE;
    if (NodeType.FindLastChar(TEXT('.'), LastDot))
    {
        NodeType.RightChopInline(LastDot + 1);
    }
    // Friendly names (EventBeginPlay, PrintString, ...): the node type to send and what to bind. A K2Node_MacroInstance names
    // its macro in functionName; the graph door reads the macro from the node type.
    FString AliasEvent;
    FString AliasFunction;
    FString AliasOwner;
    ResolveLevelBlueprintNodeAlias(NodeType, AliasEvent, AliasFunction, AliasOwner);
    if (!AliasFunction.IsEmpty())
    {
        FunctionName = AliasFunction;
    }
    if (MemberClass.IsEmpty())
    {
        MemberClass = AliasOwner;
    }
    const FString NodeKey = McpPlainKey(NodeType);
    if (!FunctionName.IsEmpty() && (NodeKey == TEXT("k2nodemacroinstance") || NodeKey == TEXT("macroinstance")))
    {
        NodeType = FunctionName;
        FString UnusedEvent;
        FString UnusedFunction;
        FString UnusedOwner;
        ResolveLevelBlueprintNodeAlias(NodeType, UnusedEvent, UnusedFunction, UnusedOwner);
        FunctionName.Reset();
    }

    // Refusals for a parameter the node does not take: the graph door ignores it and builds the node.
    UClass* NodeClassObj = McpBlueprintGraphHandlers::FindNodeClassByName(NodeType);
    const bool bBoundEvent = LevelNodeIsA(NodeClassObj, UK2Node_ActorBoundEvent::StaticClass());
    const bool bLiteral = LevelNodeIsA(NodeClassObj, UK2Node_Literal::StaticClass());
    const bool bCall = LevelNodeIsA(NodeClassObj, UK2Node_CallFunction::StaticClass());
    const bool bCustomEvent = LevelNodeIsA(NodeClassObj, UK2Node_CustomEvent::StaticClass());
    bool bSetter = false;
    const bool bVariable = McpBlueprintGraphHandlers::ParseVariableNodeType(NodeType, bSetter);
    const auto Refuse = [&](const FString& Message, const TCHAR* Code)
    {
        Subsystem->SendAutomationError(Socket, RequestId, Message, Code);
        return true;
    };
    if (!EventName.IsEmpty() && !bBoundEvent)
    {
        return Refuse(TEXT("eventName applies to a K2Node_ActorBoundEvent only; pass BeginPlay, Tick and the "
                           "other events as nodeClass (EventBeginPlay, EventTick)."), TEXT("INVALID_ARGUMENT"));
    }
    if (!ActorName.IsEmpty() && !bBoundEvent && !bLiteral)
    {
        return Refuse(TEXT("actorName applies to an actor reference (K2Node_Literal) or an actor-bound event "
                           "(K2Node_ActorBoundEvent) only."), TEXT("INVALID_ARGUMENT"));
    }
    if (!MemberName.IsEmpty() && !bVariable)
    {
        return Refuse(TEXT("memberName applies to a K2Node_VariableGet or K2Node_VariableSet only; a function goes in functionName."),
                      TEXT("INVALID_ARGUMENT"));
    }
    if (bVariable && MemberName.IsEmpty())
    {
        return Refuse(FString::Printf(TEXT("%s needs memberName: a level Blueprint variable, or with memberClass a property of "
                                           "that class, such as a placed actor's component (PointLightComponent on PointLight)."),
                                      NodeClassObj ? *NodeClassObj->GetName() : *NodeType), TEXT("INVALID_ARGUMENT"));
    }
    if (!FunctionName.IsEmpty() && !bCall)
    {
        return Refuse(FString::Printf(TEXT("Function '%s' could not be bound: functionName applies to a K2Node_CallFunction only "
                                           "(a macro goes in nodeClass, ForEachLoop)."), *FunctionName), TEXT("NOT_SUPPORTED"));
    }

    // What create_node takes: nodeName and nodeClass stay out of the request (it reads them as a custom event's name and as a
    // target class); a custom event takes its name from nodeName; position only when one was sent (else it places the node).
    TSharedPtr<FJsonObject> Request = MakeShared<FJsonObject>();
    const auto Pass = [&Request](const FString& Key, const FString& Value)
    {
        if (!Value.IsEmpty())
        {
            Request->SetStringField(Key, Value);
        }
    };
    Request->SetStringField(TEXT("nodeType"), NodeType);
    Pass(TEXT("functionName"), FunctionName);
    Pass(TEXT("eventName"), AliasEvent.IsEmpty() ? EventName : AliasEvent);
    Pass(TEXT("memberName"), MemberName);
    Pass(TEXT("actorName"), ActorName);
    if (bCall || bVariable)
    {
        Pass(TEXT("memberClass"), MemberClass);
    }
    if (bCustomEvent)
    {
        Pass(TEXT("customEventName"), NodeName);
    }
    const TSharedPtr<FJsonObject>* PositionJson = nullptr;
    if (Payload->TryGetObjectField(TEXT("nodePosition"), PositionJson) && PositionJson)
    {
        Request->SetNumberField(TEXT("posX"), static_cast<int32>(GetJsonNumberField(*PositionJson, TEXT("x"))));
        Request->SetNumberField(TEXT("posY"), static_cast<int32>(GetJsonNumberField(*PositionJson, TEXT("y"))));
    }

    // The nodes the graph holds now: an event it already had (BeginPlay) comes back as that node and is left as it is.
    TSet<FGuid> NodesBefore;
    for (const UEdGraphNode* Existing : EventGraph->Nodes)
    {
        NodesBefore.Add(Existing ? Existing->NodeGuid : FGuid());
    }
    // The level's own save rule answers, not create_node's compile at response: an add compiles nothing, as before.
    McpBlueprintGraphHandlers::FActionContext Context{
        Subsystem, RequestId, Request, Socket, TEXT("create_node"), LevelBP, EventGraph, /*bDeferCompile=*/true};
    Context.bExternalVariableMember = true;
    Context.OnSuccess = [&](const FString& Message, const TSharedPtr<FJsonObject>& Result)
    {
        UEdGraphNode* NewNode = Context.FindNode(GetJsonStringField(Result, TEXT("nodeId"), TEXT("")));
        if (NewNode && !NodesBefore.Contains(NewNode->NodeGuid))
        {
            if (!NodeName.IsEmpty() && !bCustomEvent)
            {
                RenameLevelBlueprintNode(EventGraph, NewNode, NodeName);
                Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
            }
            // An event node adds a function to the class, as the editor's own placement marks it; a plain mark left the
            // skeleton class without it until something else recompiled the level blueprint.
            UK2Node* K2Node = Cast<UK2Node>(NewNode);
            if (K2Node && K2Node->NodeCausesStructuralBlueprintChange())
            {
                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LevelBP);
            }
        }
        SendLevelEditResult(Subsystem, RequestId, Socket, Payload, CurrentLevel, Message, Result);
    };
    return McpBlueprintGraphHandlers::HandleNodeCreationAction(Context);
}

}
