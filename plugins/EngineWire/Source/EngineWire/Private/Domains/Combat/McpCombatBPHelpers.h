#pragma once

#include "CoreMinimal.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/BridgeHelpers/Blueprints/McpNewBlueprint.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "EditorAssetLibrary.h"

namespace McpCombatHandlers
{
inline UBlueprint* CreateActorBlueprint(
    UClass* ParentClass,
    const FString& Path,
    const FString& Name,
    FString& OutError)
{
    FString FullPath = Path / Name;

    if (!IsValidAssetPath(FullPath))
    {
        OutError = FString::Printf(TEXT("Invalid asset path: '%s'. Path must start with '/', cannot contain '..' or '//'."), *FullPath);
        return nullptr;
    }

    if (McpAssetExists(FullPath))
    {
        OutError = FString::Printf(TEXT("Asset already exists at path: %s"), *FullPath);
        return nullptr;
    }

    UPackage* Package = McpCreatePackageOrNull(FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlueprint* Blueprint = McpNewBlueprint(Package, FName(*Name), ParentClass);
    if (!Blueprint)
    {
        OutError = TEXT("Failed to create blueprint");
        return nullptr;
    }

    McpSafeAssetSave(Blueprint);
    return Blueprint;
}

template<typename T>
T* GetOrCreateSCSComponent(
    UBlueprint* Blueprint,
    const FString& ComponentName,
    const FString& AttachTo = TEXT(""))
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return nullptr;
    }

    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<T>())
        {
            if (ComponentName.IsEmpty() || Node->GetVariableName().ToString() == ComponentName)
            {
                return Cast<T>(Node->ComponentTemplate);
            }
        }
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    USCS_Node* Parent = nullptr;
    if (!AttachTo.IsEmpty())
    {
        for (USCS_Node* ParentNode : SCS->GetAllNodes())
        {
            if (ParentNode && ParentNode->GetVariableName().ToString() == AttachTo)
            {
                Parent = ParentNode;
                break;
            }
        }
    }
    USCS_Node* NewNode = McpAddScsNode(SCS, T::StaticClass(), FName(*ComponentName), Parent);
    T* NewComp = NewNode ? Cast<T>(NewNode->ComponentTemplate) : nullptr;
    if (!NewComp)
    {
        return nullptr;
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    return NewComp;
}

inline bool AddBlueprintVariableCombat(
    UBlueprint* Blueprint,
    const FName& VarName,
    const FEdGraphPinType& PinType)
{
    if (!Blueprint)
    {
        return false;
    }

    for (const FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        if (Var.VarName == VarName)
        {
            return true;
        }
    }

    FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, PinType);
    return true;
}

// Value, on the class default object, for a variable the last compile put on Blueprint's generated class (a later
// compile keeps it). Nothing when the class or the variable is not there.
template <typename TProperty, typename TValue>
void SetVariableDefaultCombat(UBlueprint* Blueprint, const TCHAR* VarName, const TValue& Value)
{
    UBlueprintGeneratedClass* Class = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
    UObject* Defaults = Class ? Class->GetDefaultObject() : nullptr;
    if (TProperty* Property = Defaults ? FindFProperty<TProperty>(Class, VarName) : nullptr)
    {
        Property->SetPropertyValue_InContainer(Defaults, Value);
    }
}

inline FEdGraphPinType MakeFloatPinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
    PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
    return PinType;
}

inline FEdGraphPinType MakeBoolPinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    return PinType;
}

}
