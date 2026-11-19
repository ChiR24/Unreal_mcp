// McpMissingReferences.cpp — see the header. Reflection only: no Asset Registry query, so a compile is judged on the
// Blueprint in memory, before any save has updated the registry's dependency lists.

#include "Safety/McpMissingReferences.h"
#include "Safety/McpSafeOperationsAssetSave.h"
#include "Safety/McpSafeOperationsLog.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

namespace McpMissingRefs
{
constexpr int32 MaxWarnings = 10;
constexpr int32 MaxDepth = 6;
constexpr int32 MaxItems = 256;

// Content a project or plugin mounts: not native classes, scratch memory or the transient package.
bool Judged(const FString& Package)
{
    return !Package.IsEmpty() && !Package.StartsWith(TEXT("/Script/")) && !Package.StartsWith(TEXT("/Temp/")) &&
           !Package.StartsWith(TEXT("/Memory/")) && !Package.StartsWith(TEXT("/Engine/Transient"));
}

struct FWalk
{
    const UPackage* Self = nullptr;
    EMcpMissingReferenceCheck Check = EMcpMissingReferenceCheck::Compile;
    TArray<TPair<FString, FString>> Found; // what is wrong with the path, what holds it

    void Soft(const FSoftObjectPath& Path, const FString& Holder)
    {
        const FString Package = Path.GetLongPackageName();
        if (Path.IsNull() || !Judged(Package) || FPackageName::DoesPackageExist(Package))
        {
            return;
        }
        if (!FindPackage(nullptr, *Package))
        {
            Found.AddUnique(TPair<FString, FString>(FString::Printf(TEXT("%s, which is not an asset in this project"), *Path.ToString()), Holder));
        }
        else if (Check == EMcpMissingReferenceCheck::Save)
        {
            Found.AddUnique(TPair<FString, FString>(FString::Printf(TEXT("%s, which exists only in memory (never saved)"), *Path.ToString()), Holder));
        }
    }

    // A hard reference resolves while its object is loaded; one into a package never saved is gone after a restart.
    void Hard(const UObject* Object, const FString& Holder)
    {
        const UPackage* Package = Object ? Object->GetOutermost() : nullptr;
        if (Check != EMcpMissingReferenceCheck::Save || !Package || Package == Self || Package == GetTransientPackage() ||
            Package->HasAnyPackageFlags(PKG_PlayInEditor | PKG_CompiledIn) || !Judged(Package->GetName()) ||
            McpSafeOperations::McpPackageHasBackingFile(Package->GetName()))
        {
            return;
        }
        Found.AddUnique(TPair<FString, FString>(FString::Printf(TEXT("%s, which exists only in memory (never saved)"), *Object->GetPathName()), Holder));
    }

    void Walk(const FProperty* Property, const void* Memory, const FString& Holder, int32 Depth)
    {
        if (Depth > MaxDepth || Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
        {
            return;
        }
        if (CastField<FSoftObjectProperty>(Property))
        {
            Soft(static_cast<const FSoftObjectPtr*>(Memory)->ToSoftObjectPath(), Holder);
        }
        else if (const FObjectProperty* Reference = CastField<FObjectProperty>(Property))
        {
            Hard(Reference->GetObjectPropertyValue(Memory), Holder);
        }
        else if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
        {
            if (Struct->Struct->IsChildOf(TBaseStructure<FSoftObjectPath>::Get()))
            {
                Soft(*static_cast<const FSoftObjectPath*>(Memory), Holder);
                return;
            }
            for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
            {
                Walk(*It, It->ContainerPtrToValuePtr<void>(Memory), Holder + TEXT(".") + It->GetName(), Depth + 1);
            }
        }
        else if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
        {
            FScriptArrayHelper Helper(Array, Memory);
            for (int32 Index = 0; Index < FMath::Min(Helper.Num(), MaxItems); ++Index)
            {
                Walk(Array->Inner, Helper.GetRawPtr(Index), FString::Printf(TEXT("%s[%d]"), *Holder, Index), Depth + 1);
            }
        }
    }

    // An object or class pin's default: a hard object, else a path the node resolves when it runs.
    void Pins(const UEdGraphNode* Node)
    {
        const UEdGraph* Graph = Node->GetGraph();
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            const FName Category = Pin ? Pin->PinType.PinCategory : NAME_None;
            if (Category != UEdGraphSchema_K2::PC_Object && Category != UEdGraphSchema_K2::PC_Class &&
                Category != UEdGraphSchema_K2::PC_SoftObject && Category != UEdGraphSchema_K2::PC_SoftClass)
            {
                continue;
            }
            const FString Holder = FString::Printf(TEXT("%s graph, node %s, pin %s"), Graph ? *Graph->GetName() : TEXT("?"),
                *Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Pin->PinName.ToString());
            if (Pin->DefaultObject)
            {
                Hard(Pin->DefaultObject, Holder);
            }
            else if (Pin->DefaultValue.StartsWith(TEXT("/")) && FPackageName::IsValidObjectPath(Pin->DefaultValue))
            {
                Soft(FSoftObjectPath(Pin->DefaultValue), Holder);
            }
        }
    }
};
} // namespace McpMissingRefs

void McpWarnMissingReferences(UObject* Asset, EMcpMissingReferenceCheck Check)
{
    UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
    if (!Package || !McpMissingRefs::Judged(Package->GetName()))
    {
        return;
    }
    McpMissingRefs::FWalk Walk;
    Walk.Self = Package;
    Walk.Check = Check;
    ForEachObjectWithPackage(Package, [&Walk](UObject* Object)
    {
        for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
        {
            Walk.Walk(*It, It->ContainerPtrToValuePtr<void>(Object), Object->GetName() + TEXT(".") + It->GetName(), 0);
        }
        if (const UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
        {
            Walk.Pins(Node);
        }
        return true;
    });
    const FString Name = Package->GetName();
    for (int32 Index = 0; Index < FMath::Min(Walk.Found.Num(), McpMissingRefs::MaxWarnings); ++Index)
    {
        UE_LOG(LogMcpSafeOperations, Warning, TEXT("%s refers to %s (held by %s): it loads as nothing at runtime. Point it "
            "at an asset that exists (search for it by name) or clear it, and save what it should point at."),
            *Name, *Walk.Found[Index].Key, *Walk.Found[Index].Value);
    }
    if (Walk.Found.Num() > McpMissingRefs::MaxWarnings)
    {
        UE_LOG(LogMcpSafeOperations, Warning, TEXT("%s holds %d more references like these."), *Name,
            Walk.Found.Num() - McpMissingRefs::MaxWarnings);
    }
}
