#include "McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "EdGraphSchema_K2.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintGraphHandlers
{
FProperty* CreateCustomEventParameter(
    UFunction* Function,
    const FString& ParameterName,
    const FString& ParameterType)
{
    const FEdGraphPinType PinType =
        ResolveCustomEventPinType(ParameterType);
    const FName PropertyName(*ParameterName);
    FProperty* Property = nullptr;

    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Float)
    {
        Property = new FFloatProperty(Function, PropertyName, RF_Public);
    }
    else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
    {
        Property = new FDoubleProperty(Function, PropertyName, RF_Public);
    }
    else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
    {
        Property = new FIntProperty(Function, PropertyName, RF_Public);
    }
    else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int64)
    {
        Property = new FInt64Property(Function, PropertyName, RF_Public);
    }
    else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
    {
        FBoolProperty* BoolProperty =
            new FBoolProperty(Function, PropertyName, RF_Public);
        BoolProperty->SetBoolSize(0, true);
        Property = BoolProperty;
    }
    else if (PinType.PinCategory == UEdGraphSchema_K2::PC_String)
    {
        Property = new FStrProperty(Function, PropertyName, RF_Public);
    }
    else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
    {
        Property = new FNameProperty(Function, PropertyName, RF_Public);
    }
    else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
    {
        Property = new FTextProperty(Function, PropertyName, RF_Public);
    }
    else if (
        PinType.PinCategory == UEdGraphSchema_K2::PC_Byte &&
        PinType.PinSubCategoryObject.IsValid())
    {
        FByteProperty* ByteProperty =
            new FByteProperty(Function, PropertyName, RF_Public);
        ByteProperty->Enum =
            Cast<UEnum>(PinType.PinSubCategoryObject);
        Property = ByteProperty;
    }
    else if (
        PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
        PinType.PinSubCategoryObject.IsValid())
    {
        FStructProperty* StructProperty =
            new FStructProperty(Function, PropertyName, RF_Public);
        StructProperty->Struct =
            Cast<UScriptStruct>(PinType.PinSubCategoryObject);
        Property = StructProperty;
    }
    else if (
        PinType.PinCategory == UEdGraphSchema_K2::PC_Object &&
        PinType.PinSubCategoryObject.IsValid())
    {
        FObjectProperty* ObjectProperty =
            new FObjectProperty(Function, PropertyName, RF_Public);
        ObjectProperty->PropertyClass =
            Cast<UClass>(PinType.PinSubCategoryObject);
        Property = ObjectProperty;
    }
    else if (
        PinType.PinCategory == UEdGraphSchema_K2::PC_Class &&
        PinType.PinSubCategoryObject.IsValid())
    {
        FClassProperty* ClassProperty =
            new FClassProperty(Function, PropertyName, RF_Public);
        ClassProperty->PropertyClass = UClass::StaticClass();
        ClassProperty->MetaClass =
            Cast<UClass>(PinType.PinSubCategoryObject);
        Property = ClassProperty;
    }
    else if (
        PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject &&
        PinType.PinSubCategoryObject.IsValid())
    {
        FSoftObjectProperty* SoftProperty =
            new FSoftObjectProperty(Function, PropertyName, RF_Public);
        SoftProperty->PropertyClass =
            Cast<UClass>(PinType.PinSubCategoryObject);
        Property = SoftProperty;
    }

    if (Property)
    {
        Property->SetFlags(RF_Public);
        Property->PropertyFlags |= CPF_Parm;
    }
    else
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Unsupported parameter type '%s' for parameter '%s' - skipping"),
            *ParameterType,
            *ParameterName);
    }
    return Property;
}
}
#endif
