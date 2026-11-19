// McpNestedPropElements.cpp — see the header. The walk mirrors ResolveNestedPropertyPath (McpNestedPropPath.h): a bool
// named without its b on the last segment, a deprecated property refused, an actor's instance component by name, and
// every object a reference leads into checked; a bracket after a name then picks one element of that list, map or set.

#include "Foundation/BridgeHelpers/Properties/McpNestedPropElements.h"
#include "Foundation/BridgeHelpers/Properties/McpNestedPropElementsPick.h"
#include "Foundation/BridgeHelpers/Properties/McpNestedPropPath.h"

bool McpResolveElementPath(UObject* Root, const FString& Path, bool bWrite, FMcpElementPath& Out)
{
    Out = FMcpElementPath();
    TArray<McpPropElems::FSegment> Segments;
    if (!Root || !McpPropElems::Split(Path, Segments, Out.Error))
    {
        Out.Code = TEXT("PROPERTY_NOT_FOUND");
        Out.Error = Root ? Out.Error : TEXT("Root object is null");
        return false;
    }
    UStruct* Scope = Root->GetClass();
    void* Container = Root;
    UObject* Object = Root; // null while the walk is inside a struct
    FProperty* Property = nullptr;
    bool bReadOnly = false;
    Out.Owner = Root;
    for (int32 Index = 0; Index < Segments.Num(); ++Index)
    {
        const McpPropElems::FSegment& Segment = Segments[Index];
        const bool bLast = Index == Segments.Num() - 1;
        Property = FindFProperty<FProperty>(Scope, FName(*Segment.Name));
        if (!Property && bLast && !Segment.bSelector)
        {
            Property = CastField<FBoolProperty>(FindFProperty<FProperty>(Scope, FName(*(TEXT("b") + Segment.Name))));
        }
        if (Property && Property->HasAnyPropertyFlags(CPF_Deprecated))
        {
            return McpPropElems::Fail(Out, TEXT("PROPERTY_NOT_FOUND"), McpDeprecatedPropertyError(Scope, Property));
        }
        // An actor's instance component has no property behind it: a middle part may name it, as the plain walk allows.
        AActor* Actor = !Property && !bLast && !Segment.bSelector ? Cast<AActor>(Object) : nullptr;
        TArray<UActorComponent*> Components;
        if (Actor) Actor->GetComponents(Components);
        UActorComponent* const* Named = Components.FindByPredicate([&Segment](const UActorComponent* Component)
            { return Component && Component->GetName().Equals(Segment.Name, ESearchCase::IgnoreCase); });
        if (Named)
        {
            Object = *Named;
            Container = *Named;
            Scope = (*Named)->GetClass();
            Out.Owner = *Named;
            Out.Chain.Reset();
            continue;
        }
        if (!Property)
        {
            const FString Similar = McpSimilarPropertyNames(Scope, Segment.Name);
            return McpPropElems::Fail(Out, TEXT("PROPERTY_NOT_FOUND"), FString::Printf(TEXT("Property '%s' not found in scope '%s' (part %d "
                "of %d)%s"), *Segment.Name, *Scope->GetName(), Index + 1, Segments.Num(), Similar.IsEmpty() ? TEXT("") : *(TEXT("; similar: ") + Similar)));
        }
        Out.Chain.Add(Property);
        if (Segment.bSelector)
        {
            void* ElementContainer = nullptr;
            if (!McpPropElems::Pick(Property, Container, Segment, Property, ElementContainer, bReadOnly, Out))
            {
                return false;
            }
            Container = ElementContainer;
            Out.Chain.Add(Property);
        }
        if (bLast)
        {
            break;
        }
        if (FObjectProperty* Reference = CastField<FObjectProperty>(Property))
        {
            UObject* Next = Reference->GetObjectPropertyValue_InContainer(Container);
            if (!Next || !McpSafeReflectionTarget::IsAddressable(Next))
            {
                return McpPropElems::Fail(Out, TEXT("PROPERTY_NOT_FOUND"), FString::Printf(TEXT("'%s' %s (part %d of %d)"), *Segment.Name,
                    Next ? *(TEXT("references ") + Next->GetPathName() + TEXT(", which the property walk may not enter")) : TEXT("is empty"),
                    Index + 1, Segments.Num()));
            }
            Object = Next;
            Container = Next;
            Scope = Next->GetClass();
            Out.Owner = Next;
            Out.Chain.Reset();
        }
        else if (FStructProperty* Struct = CastField<FStructProperty>(Property))
        {
            Container = Struct->ContainerPtrToValuePtr<void>(Container);
            Scope = Struct->Struct;
            Object = nullptr;
        }
        else
        {
            return McpPropElems::Fail(Out, TEXT("PROPERTY_NOT_FOUND"), FString::Printf(TEXT("Cannot go into '%s' of type '%s'"), *Segment.Name,
                *Property->GetClass()->GetName()));
        }
    }
    if (bWrite && bReadOnly)
    {
        return McpPropElems::Fail(Out, TEXT("ELEMENT_KEY_READ_ONLY"), TEXT("A set's element is its own key: changed in place it would break "
            "the set. Write the whole set instead."));
    }
    Out.Property = Property;
    Out.Container = Container;
    return true;
}

void McpNotifyElementWrite(const FMcpElementPath& Path)
{
    if (!Path.Owner || Path.Chain.Num() == 0)
    {
        return;
    }
    FEditPropertyChain Chain;
    for (FProperty* Each : Path.Chain)
    {
        Chain.AddTail(Each);
    }
    Chain.SetActiveMemberPropertyNode(Path.Chain[0]);
    Chain.SetActivePropertyNode(Path.Chain.Last());
    FPropertyChangedEvent Event(Path.Chain.Last(), EPropertyChangeType::ValueSet);
    Event.SetActiveMemberProperty(Path.Chain[0]);
    FPropertyChangedChainEvent ChainEvent(Chain, Event);
    Path.Owner->PostEditChangeChainProperty(ChainEvent);
}

FString McpPathHead(const FString& Path)
{
    int32 End = Path.Len();
    for (const TCHAR Stop : {TEXT('.'), TEXT('[')})
    {
        int32 At = INDEX_NONE;
        if (Path.FindChar(Stop, At)) End = FMath::Min(End, At);
    }
    return Path.Left(End);
}
