#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

#include "EdGraphSchema_K2.h"

#if WITH_EDITOR


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
    else if (Pin.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
    {
        Base = TEXT("SoftObject");
        if (UObject* Sub = Pin.PinSubCategoryObject.Get())
        {
            Base += TEXT(":") + Sub->GetPathName();
        }
    }
    else if (Pin.PinCategory == UEdGraphSchema_K2::PC_SoftClass)
    {
        Base = TEXT("SoftClass");
        if (UObject* Sub = Pin.PinSubCategoryObject.Get())
        {
            Base += TEXT(":") + Sub->GetPathName();
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
