#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

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
    // A bare object, class or softobject names no class and takes any object; these were dropped from the event.
    UClass* ReferenceClass = Cast<UClass>(PinType.PinSubCategoryObject.Get());
    ReferenceClass = ReferenceClass ? ReferenceClass : UObject::StaticClass();

    if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
    {
        // UE5 real pins carry their precision in PinSubCategory (PC_Float or
        // PC_Double); a bare PC_Float pin category is invalid and no longer
        // produced by ResolveCustomEventPinType, so dispatch on the
        // subcategory instead.
        if (PinType.PinSubCategory == UEdGraphSchema_K2::PC_Float)
        {
            Property = new FFloatProperty(Function, PropertyName, RF_Public);
        }
        else
        {
            Property = new FDoubleProperty(Function, PropertyName, RF_Public);
        }
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
        // A native bool's size is sizeof(bool); SetBoolSize(0, ...) leaves
        // ElementSize 0 and trips check(GetElementSize() != 0) -> editor crash.
        BoolProperty->SetBoolSize(sizeof(bool), true);
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
    else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
    {
        // A bare byte names no enum.
        FByteProperty* ByteProperty =
            new FByteProperty(Function, PropertyName, RF_Public);
        ByteProperty->Enum =
            Cast<UEnum>(PinType.PinSubCategoryObject.Get());
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
    else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
    {
        FObjectProperty* ObjectProperty =
            new FObjectProperty(Function, PropertyName, RF_Public);
        ObjectProperty->PropertyClass = ReferenceClass;
        Property = ObjectProperty;
    }
    else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
    {
        FClassProperty* ClassProperty =
            new FClassProperty(Function, PropertyName, RF_Public);
        ClassProperty->PropertyClass = UClass::StaticClass();
        ClassProperty->MetaClass = ReferenceClass;
        Property = ClassProperty;
    }
    else if (PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
    {
        FSoftObjectProperty* SoftProperty =
            new FSoftObjectProperty(Function, PropertyName, RF_Public);
        SoftProperty->PropertyClass = ReferenceClass;
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
