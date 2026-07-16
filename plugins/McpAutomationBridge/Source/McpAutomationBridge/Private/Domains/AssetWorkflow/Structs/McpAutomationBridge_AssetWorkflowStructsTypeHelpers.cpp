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
        // Map the value's TerminalCategory to a user-facing string that
        // round-trips through ParseMemberType.  Raw .ToString() produces UE
        // internal names ("Boolean", "Real") instead of the MCP names
        // ("Bool", "Float" / "Double").
        auto ValueTypeToSummary = [](const FEdGraphTerminalType& Vt) -> FString
        {
            if (Vt.TerminalSubCategoryObject.Get())
            {
                if (Vt.TerminalCategory == UEdGraphSchema_K2::PC_Struct)
                {
                    return TEXT("Struct:") + Cast<UScriptStruct>(Vt.TerminalSubCategoryObject.Get())->GetPathName();
                }
                if (Vt.TerminalCategory == UEdGraphSchema_K2::PC_Enum)
                {
                    return TEXT("Enum:") + Cast<UEnum>(Vt.TerminalSubCategoryObject.Get())->GetPathName();
                }
            }
            // Primitive value types — map UE internal category names to the
            // same user-facing tokens the key-type switch produces above.
            if (Vt.TerminalCategory == UEdGraphSchema_K2::PC_Boolean) return TEXT("Bool");
            if (Vt.TerminalCategory == UEdGraphSchema_K2::PC_Int)     return TEXT("Int");
            if (Vt.TerminalCategory == UEdGraphSchema_K2::PC_Float)   return TEXT("Float");
            if (Vt.TerminalCategory == UEdGraphSchema_K2::PC_Real)    return (Vt.TerminalSubCategory == UEdGraphSchema_K2::PC_Double) ? TEXT("Double") : TEXT("Float");
            if (Vt.TerminalCategory == UEdGraphSchema_K2::PC_String)  return TEXT("String");
            if (Vt.TerminalCategory == UEdGraphSchema_K2::PC_Name)   return TEXT("Name");
            if (Vt.TerminalCategory == UEdGraphSchema_K2::PC_Text)   return TEXT("Text");
            if (Vt.TerminalCategory == UEdGraphSchema_K2::PC_Object) return TEXT("Object");
            if (Vt.TerminalCategory == UEdGraphSchema_K2::PC_Class)  return TEXT("Class");
            if (Vt.TerminalCategory == UEdGraphSchema_K2::PC_SoftObject) return TEXT("SoftObject");
            if (Vt.TerminalCategory == UEdGraphSchema_K2::PC_SoftClass)  return TEXT("SoftClass");
            return Vt.TerminalCategory.ToString();
        };
        const FString ValueBase = ValueTypeToSummary(Pin.PinValueType);
        return TEXT("Map:") + Base + TEXT(",") + ValueBase;
    }
    return Base;
}

#endif // WITH_EDITOR
