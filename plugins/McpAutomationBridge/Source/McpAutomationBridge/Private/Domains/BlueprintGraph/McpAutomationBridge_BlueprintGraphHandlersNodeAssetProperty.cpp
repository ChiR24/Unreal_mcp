#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"

namespace McpBlueprintGraphHandlers
{
namespace
{
// Assign one reflected property, loading an asset by path for object fields and
// falling back to the property's own text import for everything else.
bool AssignReflectedValue(FProperty* Prop, void* Container, UObject* Owner,
                          const FString& Value)
{
    if (!Prop)
    {
        return false;
    }
    void* Address = Prop->ContainerPtrToValuePtr<void>(Container);
    if (FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Prop))
    {
        UObject* Loaded =
            StaticLoadObject(ObjectProp->PropertyClass, nullptr, *Value);
        if (!Loaded)
        {
            return false;
        }
        ObjectProp->SetObjectPropertyValue(Address, Loaded);
        return true;
    }
    return Prop->ImportText_Direct(*Value, Address, Owner, PPF_None) != nullptr;
}
} // namespace

// set_node_property previously reached only the handful of fields every
// UEdGraphNode shares (comment, position, enabled state), so the thing a caller
// most often wants to set -- WHICH asset a node plays -- was unreachable. An
// AnimGraph sequence or blend space player created through create_node came out
// empty and stayed empty: the Blueprint compiled clean, the character simply
// never animated, and nothing in the reply said why.
//
// Anim nodes keep their payload in a "Node" struct member (FAnimNode_*), so the
// name the caller gives ("Sequence", "BlendSpace") lives one level down. Look on
// the node itself first, then inside that struct.
bool McpTrySetNodeAssetPropertyForMcp(UEdGraphNode* TargetNode,
                                      const FString& PropertyName,
                                      const FString& Value)
{
    if (!TargetNode || PropertyName.IsEmpty())
    {
        return false;
    }
    UClass* NodeClass = TargetNode->GetClass();
    const FName Wanted(*PropertyName);
    if (AssignReflectedValue(NodeClass->FindPropertyByName(Wanted), TargetNode,
                             TargetNode, Value))
    {
        TargetNode->ReconstructNode();
        return true;
    }
    FStructProperty* NodeStruct =
        CastField<FStructProperty>(NodeClass->FindPropertyByName(TEXT("Node")));
    if (!NodeStruct || !NodeStruct->Struct)
    {
        return false;
    }
    void* StructAddress = NodeStruct->ContainerPtrToValuePtr<void>(TargetNode);
    if (!AssignReflectedValue(NodeStruct->Struct->FindPropertyByName(Wanted),
                              StructAddress, TargetNode, Value))
    {
        return false;
    }
    TargetNode->ReconstructNode();
    return true;
}
} // namespace McpBlueprintGraphHandlers
#endif
