#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

#include "EdGraphSchema_K2.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"

#if WITH_EDITOR


FEdGraphPinType ParseMemberType(const FString& TypeStr)
{
    FEdGraphPinType Pin;
    Pin.ContainerType = EPinContainerType::None;

    FString Base = TypeStr;
    if (Base.StartsWith(TEXT("Array:")))
    {
        Pin.ContainerType = EPinContainerType::Array;
        Base = Base.Mid(6);
    }
    else if (Base.StartsWith(TEXT("Set:")))
    {
        // Like Array, a Set's element type lives in PinCategory; only the
        // container flag differs. See DescribePinType() in the Blueprint utils.
        Pin.ContainerType = EPinContainerType::Set;
        Base = Base.Mid(4);
    }
    else if (Base.StartsWith(TEXT("Map:")))
    {
        Pin.ContainerType = EPinContainerType::Map;
        // For a Map container, PinCategory carries the KEY type and PinValueType
        // carries the VALUE type. The schema form is "Map:<Key>,<Value>". With no
        // comma, the token is treated as the value type and the key defaults to
        // Int (instead of silently degrading the whole pin to Int).
        FString MapInner = Base.Mid(4);
        FString KeyType, ValueType;
        if (MapInner.Split(TEXT(","), &KeyType, &ValueType))
        {
            KeyType.TrimStartAndEndInline();
            ValueType.TrimStartAndEndInline();
            FEdGraphPinType KeyPin = ParseMemberType(KeyType);
            FEdGraphPinType ValuePin = ParseMemberType(ValueType);
            Pin.PinCategory = KeyPin.PinCategory;
            Pin.PinSubCategory = KeyPin.PinSubCategory;
            Pin.PinSubCategoryObject = KeyPin.PinSubCategoryObject;
            Pin.PinValueType.TerminalCategory = ValuePin.PinCategory;
            Pin.PinValueType.TerminalSubCategory = ValuePin.PinSubCategory;
            Pin.PinValueType.TerminalSubCategoryObject = ValuePin.PinSubCategoryObject;
            return Pin;
        }
        Base = MapInner;
    }

    if (Base == TEXT("Bool"))
    {
        Pin.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    }
    else if (Base == TEXT("Int"))
    {
        Pin.PinCategory = UEdGraphSchema_K2::PC_Int;
    }
    else if (Base == TEXT("Float"))
    {
        Pin.PinCategory = UEdGraphSchema_K2::PC_Real;
        Pin.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    }
    else if (Base == TEXT("Double"))
    {
        Pin.PinCategory = UEdGraphSchema_K2::PC_Real;
        Pin.PinSubCategory = UEdGraphSchema_K2::PC_Double;
    }
    else if (Base == TEXT("String"))
    {
        Pin.PinCategory = UEdGraphSchema_K2::PC_String;
    }
    else if (Base == TEXT("Name"))
    {
        Pin.PinCategory = UEdGraphSchema_K2::PC_Name;
    }
    else if (Base == TEXT("Text"))
    {
        Pin.PinCategory = UEdGraphSchema_K2::PC_Text;
    }
    else if (Base == TEXT("Vector"))
    {
        Pin.PinCategory = UEdGraphSchema_K2::PC_Struct;
        Pin.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }
    else if (Base == TEXT("Rotator"))
    {
        Pin.PinCategory = UEdGraphSchema_K2::PC_Struct;
        Pin.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
    }
    else if (Base == TEXT("Transform"))
    {
        Pin.PinCategory = UEdGraphSchema_K2::PC_Struct;
        Pin.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
    }
    else if (Base == TEXT("Object"))
    {
        Pin.PinCategory = UEdGraphSchema_K2::PC_Object;
    }
    else if (Base == TEXT("Class"))
    {
        Pin.PinCategory = UEdGraphSchema_K2::PC_Class;
    }
    else if (Base.StartsWith(TEXT("Enum:")))
    {
        Pin.PinCategory = UEdGraphSchema_K2::PC_Enum;
        FString EnumName = Base.Mid(5);
        if (UEnum* Enum = FindObject<UEnum>(nullptr, *EnumName))
        {
            Pin.PinSubCategoryObject = Enum;
        }
        else if (UEnum* LoadedEnum = LoadObject<UEnum>(nullptr, *EnumName))
        {
            Pin.PinSubCategoryObject = LoadedEnum;
        }
    }
    else if (Base.StartsWith(TEXT("Struct:")))
    {
        Pin.PinCategory = UEdGraphSchema_K2::PC_Struct;
        FString StructPath = Base.Mid(7);
        // A bare package path (/Game/Structs/X) must be promoted to the object
        // path (/Game/Structs/X.X) so FindObject/LoadObject resolves the
        // UserDefinedStruct rather than the package.
        if (!StructPath.Contains(TEXT(".")))
        {
            int32 LastSlash;
            if (StructPath.FindLastChar(TEXT('/'), LastSlash))
            {
                StructPath = StructPath + TEXT(".") + StructPath.Mid(LastSlash + 1);
            }
        }
        if (UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *StructPath))
        {
            Pin.PinSubCategoryObject = Struct;
        }
        else if (UScriptStruct* LoadedStruct = LoadObject<UScriptStruct>(nullptr, *StructPath))
        {
            Pin.PinSubCategoryObject = LoadedStruct;
        }
    }
    else
    {
        Pin.PinCategory = UEdGraphSchema_K2::PC_Int;
    }

    return Pin;
}

FString PinTypeToSummary(const FEdGraphPinType& Pin)
{
    FString Base;
    if (Pin.PinCategory == UEdGraphSchema_K2::PC_Boolean)
    {
        Base = TEXT("Bool");
    }
    else if (Pin.PinCategory == UEdGraphSchema_K2::PC_Int)
    {
        Base = TEXT("Int");
    }
    else if (Pin.PinCategory == UEdGraphSchema_K2::PC_Float)
    {
        Base = TEXT("Float");
    }
    else if (Pin.PinCategory == UEdGraphSchema_K2::PC_String)
    {
        Base = TEXT("String");
    }
    else if (Pin.PinCategory == UEdGraphSchema_K2::PC_Name)
    {
        Base = TEXT("Name");
    }
    else if (Pin.PinCategory == UEdGraphSchema_K2::PC_Text)
    {
        Base = TEXT("Text");
    }
    else if (Pin.PinCategory == UEdGraphSchema_K2::PC_Real)
    {
        Base = (Pin.PinSubCategory == UEdGraphSchema_K2::PC_Double) ? TEXT("Double") : TEXT("Float");
    }
    else if (Pin.PinCategory == UEdGraphSchema_K2::PC_Object)
    {
        Base = TEXT("Object");
        if (UObject* Sub = Pin.PinSubCategoryObject.Get())
        {
            Base += TEXT(":") + Sub->GetPathName();
        }
    }
    else if (Pin.PinCategory == UEdGraphSchema_K2::PC_Class)
    {
        Base = TEXT("Class");
        if (UObject* Sub = Pin.PinSubCategoryObject.Get())
        {
            Base += TEXT(":") + Sub->GetPathName();
        }
    }
    else if (Pin.PinCategory == UEdGraphSchema_K2::PC_Enum)
    {
        Base = TEXT("Enum:");
        if (UEnum* Enum = Cast<UEnum>(Pin.PinSubCategoryObject.Get()))
        {
            Base += Enum->GetPathName();
        }
    }
    else if (Pin.PinCategory == UEdGraphSchema_K2::PC_Struct)
    {
        Base = TEXT("Struct:");
        if (UScriptStruct* Struct = Cast<UScriptStruct>(Pin.PinSubCategoryObject.Get()))
        {
            Base += Struct->GetPathName();
        }
        else
        {
            Base += TEXT("Struct");
        }
    }
    else
    {
        Base = Pin.PinCategory.ToString();
    }

    if (Pin.ContainerType == EPinContainerType::Array)
    {
        return TEXT("Array:") + Base;
    }
    if (Pin.ContainerType == EPinContainerType::Set)
    {
        return TEXT("Set:") + Base;
    }
    if (Pin.ContainerType == EPinContainerType::Map)
    {
        // For a Map pin, PinCategory is the KEY and PinValueType the VALUE.
        const FString ValueBase = Pin.PinValueType.TerminalSubCategoryObject.Get()
            ? (Pin.PinValueType.TerminalCategory == UEdGraphSchema_K2::PC_Struct
                   ? TEXT("Struct:") + Cast<UScriptStruct>(Pin.PinValueType.TerminalSubCategoryObject.Get())->GetPathName()
               : Pin.PinValueType.TerminalCategory == UEdGraphSchema_K2::PC_Enum
                   ? TEXT("Enum:") + Cast<UEnum>(Pin.PinValueType.TerminalSubCategoryObject.Get())->GetPathName()
                   : Pin.PinValueType.TerminalCategory.ToString())
            : Pin.PinValueType.TerminalCategory.ToString();
        return TEXT("Map:") + Base + TEXT(",") + ValueBase;
    }
    return Base;
}

#endif // WITH_EDITOR
