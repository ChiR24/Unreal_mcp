#pragma once

#include "CoreMinimal.h"
#include "McpAutomationBridgeHelpers.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"

#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif

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

    if (UEditorAssetLibrary::DoesAssetExist(FullPath))
    {
        OutError = FString::Printf(TEXT("Asset already exists at path: %s"), *FullPath);
        return nullptr;
    }

    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
        return nullptr;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = ParentClass;

    UBlueprint* Blueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name),
                                  RF_Public | RF_Standalone, nullptr, GWarn));

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
    USCS_Node* NewNode = SCS->CreateNode(T::StaticClass(), FName(*ComponentName));
    if (!NewNode || !NewNode->ComponentTemplate)
    {
        return nullptr;
    }

    T* NewComp = Cast<T>(NewNode->ComponentTemplate);
    if (!NewComp)
    {
        return nullptr;
    }

    if (!AttachTo.IsEmpty())
    {
        for (USCS_Node* ParentNode : SCS->GetAllNodes())
        {
            if (ParentNode && ParentNode->GetVariableName().ToString() == AttachTo)
            {
                NewNode->SetParent(ParentNode);
                break;
            }
        }
    }

    SCS->AddNode(NewNode);
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

inline FEdGraphPinType MakeIntPinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    return PinType;
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

inline FEdGraphPinType MakeStringPinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    return PinType;
}

inline FEdGraphPinType MakeNamePinType()
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
    return PinType;
}

inline FEdGraphPinType MakeObjectPinType(UClass* ObjectClass)
{
    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
    PinType.PinSubCategoryObject = ObjectClass;
    return PinType;
}
}
