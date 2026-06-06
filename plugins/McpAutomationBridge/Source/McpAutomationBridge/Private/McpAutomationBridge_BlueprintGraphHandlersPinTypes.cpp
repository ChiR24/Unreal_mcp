#include "McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "EdGraphSchema_K2.h"

namespace McpBlueprintGraphHandlers
{
FEdGraphPinType ResolveCustomEventPinType(const FString& TypeName)
{
    FEdGraphPinType PinType;
    FString CleanType = TypeName;
    CleanType.TrimStartAndEndInline();
    CleanType.RemoveFromEnd(TEXT("*"));

    if (CleanType.Equals(TEXT("float"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Float;
        return PinType;
    }
    if (CleanType.Equals(TEXT("double"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
        PinType.PinSubCategory = NAME_Double;
        return PinType;
    }
    if (CleanType.Equals(TEXT("int"), ESearchCase::IgnoreCase) ||
        CleanType.Equals(TEXT("int32"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        return PinType;
    }
    if (CleanType.Equals(TEXT("int64"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
        return PinType;
    }
    if (CleanType.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        return PinType;
    }
    if (CleanType.Equals(TEXT("string"), ESearchCase::IgnoreCase) ||
        CleanType.Equals(TEXT("FString"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
        return PinType;
    }
    if (CleanType.Equals(TEXT("name"), ESearchCase::IgnoreCase) ||
        CleanType.Equals(TEXT("FName"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        return PinType;
    }
    if (CleanType.Equals(TEXT("text"), ESearchCase::IgnoreCase) ||
        CleanType.Equals(TEXT("FText"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
        return PinType;
    }
    if (CleanType.Equals(TEXT("byte"), ESearchCase::IgnoreCase) ||
        CleanType.Equals(TEXT("uint8"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
        return PinType;
    }
    if (CleanType.Equals(TEXT("object"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        return PinType;
    }
    if (CleanType.Equals(TEXT("class"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Class;
        return PinType;
    }
    if (CleanType.Equals(TEXT("softobject"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
        return PinType;
    }
    if (CleanType.Equals(TEXT("softclass"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
        return PinType;
    }
    if (CleanType.Equals(TEXT("interface"), ESearchCase::IgnoreCase))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Interface;
        return PinType;
    }

    UScriptStruct* Struct = nullptr;
    if (CleanType.Contains(TEXT("/Script/")))
    {
        Struct = LoadObject<UScriptStruct>(nullptr, *CleanType);
    }
    if (!Struct)
    {
        FString StructName = CleanType;
        if (StructName.StartsWith(TEXT("F")))
        {
            StructName = StructName.Mid(1);
        }
        Struct = FindObject<UScriptStruct>(nullptr, *StructName);
        if (!Struct)
        {
            Struct = FindObject<UScriptStruct>(nullptr, *CleanType);
        }
    }
    if (!Struct)
    {
        for (TObjectIterator<UScriptStruct> It; It; ++It)
        {
            if (It->GetName().Equals(
                    CleanType,
                    ESearchCase::IgnoreCase))
            {
                Struct = *It;
                break;
            }
        }
    }
    if (Struct)
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = Struct;
        return PinType;
    }

    if (UEnum* Enum = FindObject<UEnum>(nullptr, *CleanType))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
        PinType.PinSubCategoryObject = Enum;
        return PinType;
    }
    if (UClass* Class = ResolveUClass(CleanType))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        PinType.PinSubCategoryObject = Class;
        return PinType;
    }

    PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
    if (CleanType.StartsWith(TEXT("array<")))
    {
        PinType.ContainerType = EPinContainerType::Array;
    }
    else if (CleanType.StartsWith(TEXT("set<")))
    {
        PinType.ContainerType = EPinContainerType::Set;
    }
    else if (CleanType.StartsWith(TEXT("map<")))
    {
        PinType.ContainerType = EPinContainerType::Map;
    }
    return PinType;
}
}
#endif
