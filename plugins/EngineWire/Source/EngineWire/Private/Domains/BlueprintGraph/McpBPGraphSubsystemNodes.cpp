#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

#include "Engine/LevelScriptBlueprint.h"
#include "K2Node_ActorBoundEvent.h"
#include "K2Node_GetSubsystem.h"
#include "K2Node_Literal.h"
#include "K2Node_Select.h"
#include "Misc/PackageName.h"
#include "Subsystems/Subsystem.h"

namespace McpBlueprintGraphHandlers
{
namespace
{
// FGraphNodeCreator<T>::CreateNode(bSelect, Class) arrives in 5.3, and the GetSubsystem subclasses are not exported, so
// they cannot be its T. This does what UEdGraph::CreateNode and FGraphNodeCreator::Finalize do, on every engine.
template <typename TNode>
struct TClassNodeCreator
{
    UEdGraph& Graph;
    TNode* Node = nullptr;

    TNode* CreateNode(UClass* NodeClass)
    {
        Node = NewObject<TNode>(&Graph, NodeClass, NAME_None, RF_Transactional);
        Graph.AddNode(Node, false, false);
        return Node;
    }

    void Finalize()
    {
        Node->CreateNewGuid();
        Node->PostPlacedNewNode();
        if (Node->Pins.Num() == 0)
        {
            Node->AllocateDefaultPins();
        }
    }
};
}

bool TryCreateSubsystemNode(
    FActionContext& Context,
    UClass* NodeClass,
    float X,
    float Y)
{
    // Covers the whole UK2Node_GetSubsystem family: GetSubsystem,
    // GetSubsystemFromPC, GetEngineSubsystem, GetEditorSubsystem.
    if (!NodeClass->IsChildOf(UK2Node_GetSubsystem::StaticClass()))
    {
        return false;
    }

    // These nodes carry the subsystem type in a CustomClass UPROPERTY, not on a
    // pin — the palette action calls Initialize() before the node is placed.
    // Spawned generically the property stays null, AllocateDefaultPins() leaves
    // the result pin as the untyped base, and the blueprint stops compiling with
    // "Node Invalid Subsystem Type must have a class specified". That state is
    // unrepairable through this tool: the visible Class pin's default is read
    // only by pin-based nodes, and set_node_property does not expose CustomClass.
    // So require the class here rather than handing back a node that can never
    // compile.
    FString RequestedClassName;
    Context.Payload->TryGetStringField(TEXT("targetClass"), RequestedClassName);
    if (RequestedClassName.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("memberClass"), RequestedClassName);
    }
    if (RequestedClassName.IsEmpty())
    {
        Context.SendError(
            FString::Printf(
                TEXT("'%s' requires 'targetClass' naming the subsystem to fetch "
                     "(e.g. /Script/EnhancedInput.EnhancedInputLocalPlayerSubsystem). "
                     "The type lives on the node, not on a pin, so a node created "
                     "without it can never compile."),
                *NodeClass->GetName()),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UClass* ResolvedClass = ResolveTargetClassFromString(RequestedClassName);
    if (!ResolvedClass)
    {
        Context.SendError(
            FString::Printf(
                TEXT("Could not resolve targetClass '%s' for '%s'."),
                *RequestedClassName,
                *NodeClass->GetName()),
            TEXT("CLASS_NOT_FOUND"));
        return true;
    }
    if (!ResolvedClass->IsChildOf(USubsystem::StaticClass()))
    {
        Context.SendError(
            FString::Printf(
                TEXT("targetClass '%s' is not a USubsystem, so '%s' cannot return it."),
                *ResolvedClass->GetPathName(),
                *NodeClass->GetName()),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TClassNodeCreator<UK2Node_GetSubsystem> Creator{*Context.TargetGraph};
    UK2Node_GetSubsystem* Node = Creator.CreateNode(NodeClass);
    if (!Node)
    {
        Context.SendError(
            TEXT("Failed to instantiate subsystem node."),
            TEXT("CREATE_FAILED"));
        return true;
    }
    // Seed the type BEFORE Finalize(): Finalize() is what allocates the pins,
    // and AllocateDefaultPins() reads CustomClass to type the result pin. Setting
    // it afterwards would need a reconstruct and leaves the node briefly invalid.
    Node->Initialize(ResolvedClass);
    Context.FinalizeNode(Creator, Node, X, Y);
    return true;
}

// Switch on Enum, ForEach<Enum>, Get Number of Entries, Literal Enum and Byte to Enum build their pins from an Enum
// UPROPERTY too. Spawned by class name it stayed null: a Switch on Enum had no case outputs, compiled, and the call
// answered success. Select also holds one, but takes it from the index wired to it.
bool TryCreateEnumNode(FActionContext& Context, UClass* NodeClass, float X, float Y)
{
    FObjectPropertyBase* EnumProperty = CastField<FObjectPropertyBase>(NodeClass->FindPropertyByName(TEXT("Enum")));
    if (!EnumProperty || EnumProperty->PropertyClass != UEnum::StaticClass() ||
        NodeClass->IsChildOf(UK2Node_Select::StaticClass()) || !NodeClass->IsChildOf(UK2Node::StaticClass()))
    {
        return false;
    }
    FString EnumPath;
    Context.Payload->TryGetStringField(TEXT("enumPath"), EnumPath);
    const uint32 Quiet = LOAD_NoWarn | LOAD_Quiet;
    UEnum* Enum = EnumPath.StartsWith(TEXT("/")) ? LoadObject<UEnum>(nullptr, *EnumPath, nullptr, Quiet) : nullptr;
    if (!Enum && EnumPath.StartsWith(TEXT("/")) && !EnumPath.Contains(TEXT(".")))
    {
        Enum = LoadObject<UEnum>(nullptr, *(EnumPath + TEXT(".") + FPackageName::GetShortName(EnumPath)), nullptr, Quiet);
    }
    // A bare name (ECollisionChannel) of an enum already loaded.
    for (TObjectIterator<UEnum> It; It && !Enum && !EnumPath.IsEmpty(); ++It)
    {
        Enum = It->GetName().Equals(EnumPath, ESearchCase::IgnoreCase) ? *It : nullptr;
    }
    if (!Enum)
    {
        Context.SendError(
            FString::Printf(TEXT("%s needs enumPath: the enum its pins come from, a Blueprint enum asset "
                                 "(/Game/Data/E_Phase) or a native one (/Script/Engine.ECollisionChannel)%s."),
                            *NodeClass->GetName(),
                            EnumPath.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("; '%s' was not found"), *EnumPath)),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    TClassNodeCreator<UK2Node> Creator{*Context.TargetGraph};
    UK2Node* Node = Creator.CreateNode(NodeClass);
    // Before FinalizeNode, which allocates the pins from it.
    EnumProperty->SetObjectPropertyValue_InContainer(Node, Enum);
    Context.FinalizeNode(Creator, Node, X, Y);
    return true;
}

// An actor-bound event and an actor reference are about one placed actor of the level Blueprint's level. Spawned by
// class name they came out as "None (None)" and "Unknown", reported success, and the bound event broke the compile.
// Bound before the node joins the graph, so a refusal leaves nothing behind.
bool TryCreateLevelActorNode(FActionContext& Context, UClass* NodeClass, float X, float Y)
{
    if (!NodeClass->IsChildOf(UK2Node_ActorBoundEvent::StaticClass()) && !NodeClass->IsChildOf(UK2Node_Literal::StaticClass()))
    {
        return false;
    }
    FString ActorName;
    FString EventName;
    Context.Payload->TryGetStringField(TEXT("actorName"), ActorName);
    Context.Payload->TryGetStringField(TEXT("eventName"), EventName);
    const ULevelScriptBlueprint* LevelBlueprint = Cast<ULevelScriptBlueprint>(Context.Blueprint);
    UK2Node* Node = NewObject<UK2Node>(Context.TargetGraph, NodeClass, NAME_None, RF_Transactional);
    FString Error;
    FString Code;
    if (!McpBindLevelActorNode(Node, LevelBlueprint ? LevelBlueprint->GetLevel() : nullptr, ActorName, EventName, Error, Code))
    {
        Node->MarkAsGarbage();
        Context.SendError(Error, Code);
        return true;
    }
    Context.TargetGraph->AddNode(Node, false, false);
    TClassNodeCreator<UK2Node> Creator{*Context.TargetGraph, Node};
    Context.FinalizeNode(Creator, Node, X, Y);
    return true;
}
}
