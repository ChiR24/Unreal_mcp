#include "McpPropertyReflectionPrivate.h"

namespace McpPropertyReflection
{
namespace
{
bool JsonNumberOrStringToDouble(const TSharedPtr<FJsonValue>& ValueField, double& OutValue)
{
    if (ValueField->Type == EJson::Number)
    {
        OutValue = ValueField->AsNumber();
        return true;
    }
    if (ValueField->Type == EJson::String)
    {
        OutValue = FCString::Atod(*ValueField->AsString());
        return true;
    }
    return false;
}

bool JsonNumberOrStringToInt64(const TSharedPtr<FJsonValue>& ValueField, int64& OutValue)
{
    if (ValueField->Type == EJson::Number)
    {
        OutValue = static_cast<int64>(ValueField->AsNumber());
        return true;
    }
    if (ValueField->Type == EJson::String)
    {
        OutValue = FCString::Atoi64(*ValueField->AsString());
        return true;
    }
    return false;
}
}

bool ApplyJsonValueToProperty(void* TargetContainer, FProperty* Property, const TSharedPtr<FJsonValue>& ValueField, FString& OutError)
{
    OutError.Empty();
    if (!TargetContainer || !Property || !ValueField.IsValid())
    {
        OutError = TEXT("Invalid target/property/value");
        return false;
    }

    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        if (ValueField->Type == EJson::Boolean) BoolProp->SetPropertyValue_InContainer(TargetContainer, ValueField->AsBool());
        else if (ValueField->Type == EJson::Number) BoolProp->SetPropertyValue_InContainer(TargetContainer, ValueField->AsNumber() != 0.0);
        else if (ValueField->Type == EJson::String) BoolProp->SetPropertyValue_InContainer(TargetContainer, ValueField->AsString().Equals(TEXT("true"), ESearchCase::IgnoreCase));
        else { OutError = TEXT("Unsupported JSON type for bool property"); return false; }
        return true;
    }

    if (FStrProperty* StringProp = CastField<FStrProperty>(Property))
    {
        if (ValueField->Type != EJson::String) { OutError = TEXT("Expected string for string property"); return false; }
        StringProp->SetPropertyValue_InContainer(TargetContainer, ValueField->AsString());
        return true;
    }
    if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        if (ValueField->Type != EJson::String) { OutError = TEXT("Expected string for name property"); return false; }
        NameProp->SetPropertyValue_InContainer(TargetContainer, FName(*ValueField->AsString()));
        return true;
    }
    if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
    {
        if (ValueField->Type != EJson::String) { OutError = TEXT("Expected string for text property"); return false; }
        TextProp->SetPropertyValue_InContainer(TargetContainer, Private::ImportTextFromJsonString(ValueField->AsString()));
        return true;
    }

    double NumberValue = 0.0;
    if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
    {
        if (!JsonNumberOrStringToDouble(ValueField, NumberValue)) { OutError = TEXT("Unsupported JSON type for float property"); return false; }
        FloatProp->SetPropertyValue_InContainer(TargetContainer, static_cast<float>(NumberValue));
        return true;
    }
    if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
    {
        if (!JsonNumberOrStringToDouble(ValueField, NumberValue)) { OutError = TEXT("Unsupported JSON type for double property"); return false; }
        DoubleProp->SetPropertyValue_InContainer(TargetContainer, NumberValue);
        return true;
    }

    int64 IntValue = 0;
    if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
    {
        if (!JsonNumberOrStringToInt64(ValueField, IntValue)) { OutError = TEXT("Unsupported JSON type for int property"); return false; }
        IntProp->SetPropertyValue_InContainer(TargetContainer, static_cast<int32>(IntValue));
        return true;
    }
    if (FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
    {
        if (!JsonNumberOrStringToInt64(ValueField, IntValue)) { OutError = TEXT("Unsupported JSON type for int64 property"); return false; }
        Int64Prop->SetPropertyValue_InContainer(TargetContainer, IntValue);
        return true;
    }

    OutError = FString::Printf(TEXT("Unsupported property type: %s"), *Property->GetClass()->GetName());
    return false;
}
}
