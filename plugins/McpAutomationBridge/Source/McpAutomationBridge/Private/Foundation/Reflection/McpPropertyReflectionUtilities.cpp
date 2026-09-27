#include "Foundation/Reflection/McpPropertyReflectionPrivate.h"

namespace McpPropertyReflection
{
FString GetPropertyTypeName(FProperty* Property)
{
    if (!Property) return TEXT("Unknown");
    if (Property->IsA<FStrProperty>()) return TEXT("String");
    if (Property->IsA<FNameProperty>()) return TEXT("Name");
    if (Property->IsA<FBoolProperty>()) return TEXT("Bool");
    if (Property->IsA<FFloatProperty>()) return TEXT("Float");
    if (Property->IsA<FDoubleProperty>()) return TEXT("Double");
    if (Property->IsA<FIntProperty>()) return TEXT("Int");
    if (Property->IsA<FInt64Property>()) return TEXT("Int64");
    if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        return ByteProp->Enum ? FString::Printf(TEXT("Enum(%s)"), *ByteProp->Enum->GetName()) : TEXT("Byte");
    }
    if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        return EnumProp->GetEnum() ? FString::Printf(TEXT("Enum(%s)"), *EnumProp->GetEnum()->GetName()) : TEXT("Enum");
    }
    if (Property->IsA<FObjectProperty>()) return TEXT("Object");
    if (Property->IsA<FSoftObjectProperty>()) return TEXT("SoftObject");
    if (Property->IsA<FSoftClassProperty>()) return TEXT("SoftClass");
    if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        return StructProp->Struct ? FString::Printf(TEXT("Struct(%s)"), *StructProp->Struct->GetName()) : TEXT("Struct");
    }
    if (Property->IsA<FArrayProperty>()) return TEXT("Array");
    if (Property->IsA<FMapProperty>()) return TEXT("Map");
    if (Property->IsA<FSetProperty>()) return TEXT("Set");
    if (Property->IsA<FTextProperty>()) return TEXT("Text");
    return Property->GetClass()->GetName();
}

FString GetPropertyValueAsString(UObject* Object, FProperty* Property)
{
    if (!Object || !Property) return FString();

    FString Result;
    MCP_PROPERTY_EXPORT_TEXT(Property, Result, Property->ContainerPtrToValuePtr<void>(Object), nullptr, nullptr, PPF_None);
    return Result;
}
}
