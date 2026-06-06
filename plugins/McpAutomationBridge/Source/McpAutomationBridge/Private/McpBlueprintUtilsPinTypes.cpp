#include "McpHandlerUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpSafeOperations.h"
#include "EngineUtils.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#endif

#if WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2

namespace McpBlueprintUtils
{

FEdGraphPinType MakePinType(const FString& InType)
{
    FEdGraphPinType PinType;
    const FString Lower = InType.ToLower();
    const FString CleanType = InType.TrimStartAndEnd();

    if (Lower == TEXT("float") || Lower == TEXT("double"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Float;
    }
    else if (Lower == TEXT("int") || Lower == TEXT("integer"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    }
    else if (Lower == TEXT("int64"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
    }
    else if (Lower == TEXT("bool") || Lower == TEXT("boolean"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    }
    else if (Lower == TEXT("string"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    }
    else if (Lower == TEXT("name"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
    }
    else if (Lower == TEXT("text"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
    }
    else if (Lower == TEXT("byte"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
    }
    else if (Lower == TEXT("vector"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }
    else if (Lower == TEXT("rotator"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
    }
    else if (Lower == TEXT("transform"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
    }
    else if (Lower == TEXT("object"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        PinType.PinSubCategoryObject = UObject::StaticClass();
    }
    else if (Lower == TEXT("class"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Class;
        PinType.PinSubCategoryObject = UObject::StaticClass();
    }
    else
    {
        // Fallback: try to resolve as specific type
        if (UClass* ClassResolve = ResolveClassByName(CleanType))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
            PinType.PinSubCategoryObject = ClassResolve;
        }
        else if (UScriptStruct* StructResolve = FindObject<UScriptStruct>(nullptr, *CleanType))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            PinType.PinSubCategoryObject = StructResolve;
        }
        else if (UScriptStruct* LoadedStruct = LoadObject<UScriptStruct>(nullptr, *CleanType))
        {
            PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            PinType.PinSubCategoryObject = LoadedStruct;
        }
        else
        {
            // Try short name loop for structs
            bool bFoundStruct = false;
            if (!CleanType.Contains(TEXT("/")) && !CleanType.Contains(TEXT(".")))
            {
                for (TObjectIterator<UScriptStruct> It; It; ++It)
                {
                    if (It->GetName().Equals(CleanType, ESearchCase::IgnoreCase))
                    {
                        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
                        PinType.PinSubCategoryObject = *It;
                        bFoundStruct = true;
                        break;
                    }
                }
            }

            if (!bFoundStruct)
            {
                // Try Enum
                if (UEnum* EnumResolve = FindObject<UEnum>(nullptr, *CleanType))
                {
                    PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
                    PinType.PinSubCategoryObject = EnumResolve;
                }
                else if (UEnum* LoadedEnum = LoadObject<UEnum>(nullptr, *CleanType))
                {
                    PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
                    PinType.PinSubCategoryObject = LoadedEnum;
                }
                else
                {
                    // Default to wildcard
                    PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
                }
            }
        }
    }

    return PinType;
}

FString DescribePinType(const FEdGraphPinType& PinType)
{
    FString BaseType = PinType.PinCategory.ToString();

    if (PinType.PinSubCategoryObject.IsValid())
    {
        if (const UObject* SubObj = PinType.PinSubCategoryObject.Get())
        {
            BaseType = SubObj->GetName();
        }
    }
    else if (PinType.PinSubCategory != NAME_None)
    {
        BaseType = PinType.PinSubCategory.ToString();
    }

    FString ContainerWrappedType = BaseType;
    switch (PinType.ContainerType)
    {
    case EPinContainerType::Array:
        ContainerWrappedType = FString::Printf(TEXT("Array<%s>"), *BaseType);
        break;
    case EPinContainerType::Set:
        ContainerWrappedType = FString::Printf(TEXT("Set<%s>"), *BaseType);
        break;
    case EPinContainerType::Map:
    {
        FString ValueType = PinType.PinValueType.TerminalCategory.ToString();
        if (PinType.PinValueType.TerminalSubCategoryObject.IsValid())
        {
            if (const UObject* ValueObj = PinType.PinValueType.TerminalSubCategoryObject.Get())
            {
                ValueType = ValueObj->GetName();
            }
        }
        else if (PinType.PinValueType.TerminalSubCategory != NAME_None)
        {
            ValueType = PinType.PinValueType.TerminalSubCategory.ToString();
        }
        ContainerWrappedType = FString::Printf(TEXT("Map<%s,%s>"), *BaseType, *ValueType);
        break;
    }
    default:
        break;
    }

    return ContainerWrappedType;
}

UK2Node_VariableGet* CreateVariableGetter(UEdGraph* Graph, const FMemberReference& VarRef, float NodePosX, float NodePosY)
{
    if (!Graph)
    {
        return nullptr;
    }

    UK2Node_VariableGet* NewGet = NewObject<UK2Node_VariableGet>(Graph);
    if (!NewGet)
    {
        return nullptr;
    }

    Graph->Modify();
    NewGet->SetFlags(RF_Transactional);
    NewGet->VariableReference = VarRef;
    Graph->AddNode(NewGet, true, false);
    NewGet->CreateNewGuid();
    NewGet->NodePosX = NodePosX;
    NewGet->NodePosY = NodePosY;
    NewGet->AllocateDefaultPins();
    NewGet->Modify();

    return NewGet;
}

void LogConnectionFailure(const TCHAR* Context, UEdGraphPin* SourcePin, UEdGraphPin* TargetPin, const FPinConnectionResponse& Response)
{
    if (!SourcePin || !TargetPin)
    {
        UE_LOG(LogTemp, Verbose, TEXT("%s: connection skipped due to null pins (source=%p target=%p)"),
            Context, SourcePin, TargetPin);
        return;
    }

    FString SourceNodeName = SourcePin->GetOwningNode() ? SourcePin->GetOwningNode()->GetName() : TEXT("<null>");
    FString TargetNodeName = TargetPin->GetOwningNode() ? TargetPin->GetOwningNode()->GetName() : TEXT("<null>");

    UE_LOG(LogTemp, Verbose, TEXT("%s: schema rejected connection %s (%s) -> %s (%s) reason=%d"),
        Context, *SourceNodeName, *SourcePin->PinName.ToString(),
        *TargetNodeName, *TargetPin->PinName.ToString(),
        static_cast<int32>(Response.Response));
}
}

#endif
