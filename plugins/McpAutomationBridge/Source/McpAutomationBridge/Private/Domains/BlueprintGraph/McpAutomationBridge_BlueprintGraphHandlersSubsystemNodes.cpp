#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#include "K2Node_GetSubsystem.h"
#include "Subsystems/Subsystem.h"

namespace McpBlueprintGraphHandlers
{
namespace
{
// FGraphNodeCreator<T>::CreateNode(bSelect, Class) arrives in 5.3, and the GetSubsystem subclasses are not exported, so
// they cannot be its T. This does what UEdGraph::CreateNode and FGraphNodeCreator::Finalize do, on every engine.
struct FSubsystemNodeCreator
{
    UEdGraph& Graph;
    UK2Node_GetSubsystem* Node = nullptr;

    UK2Node_GetSubsystem* CreateNode(UClass* NodeClass)
    {
        Node = NewObject<UK2Node_GetSubsystem>(&Graph, NodeClass, NAME_None, RF_Transactional);
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

    FSubsystemNodeCreator Creator{*Context.TargetGraph};
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
}
