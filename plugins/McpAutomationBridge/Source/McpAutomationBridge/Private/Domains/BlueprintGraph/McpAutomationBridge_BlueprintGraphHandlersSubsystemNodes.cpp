#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "K2Node_GetSubsystem.h"
#include "Subsystems/Subsystem.h"

namespace McpBlueprintGraphHandlers
{
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

    FGraphNodeCreator<UK2Node_GetSubsystem> Creator(*Context.TargetGraph);
    UK2Node_GetSubsystem* Node = Creator.CreateNode(false, NodeClass);
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
#endif
