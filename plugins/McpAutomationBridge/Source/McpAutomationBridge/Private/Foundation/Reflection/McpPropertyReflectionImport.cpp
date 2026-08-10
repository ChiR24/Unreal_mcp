#include "Foundation/Reflection/McpPropertyReflectionPrivate.h"
#include "JsonObjectConverter.h"

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

bool DoubleToInt64(const double Value, int64& OutValue)
{
    constexpr double Int64UpperBound = 9223372036854775808.0;
    if (!FMath::IsFinite(Value) || FMath::Frac(Value) != 0.0 ||
        Value < static_cast<double>(MIN_int64) || Value >= Int64UpperBound)
    {
        return false;
    }
    OutValue = static_cast<int64>(Value);
    return true;
}

bool StringToInt64(const FString& Input, int64& OutValue)
{
    const FString Value = Input.TrimStartAndEnd();
    if (Value.IsEmpty() || Value.Len() != FCString::Strlen(*Value)) return false;

    int32 Index = 0;
    const bool bNegative = Value[0] == TEXT('-');
    if (bNegative || Value[0] == TEXT('+')) ++Index;
    if (Index == Value.Len()) return false;

    const uint64 Limit = bNegative
        ? static_cast<uint64>(MAX_int64) + 1u
        : static_cast<uint64>(MAX_int64);
    uint64 Magnitude = 0;
    for (; Index < Value.Len(); ++Index)
    {
        const TCHAR Character = Value[Index];
        if (Character < TEXT('0') || Character > TEXT('9')) return false;
        const uint64 Digit = static_cast<uint64>(Character - TEXT('0'));
        if (Magnitude > (Limit - Digit) / 10u) return false;
        Magnitude = Magnitude * 10u + Digit;
    }
    OutValue = bNegative
        ? (Magnitude == Limit ? MIN_int64 : -static_cast<int64>(Magnitude))
        : static_cast<int64>(Magnitude);
    return true;
}

bool JsonNumberOrStringToInt64(const TSharedPtr<FJsonValue>& ValueField, int64& OutValue)
{
    if (ValueField->Type == EJson::Number)
    {
        return DoubleToInt64(ValueField->AsNumber(), OutValue);
    }
    return ValueField->Type == EJson::String &&
        StringToInt64(ValueField->AsString(), OutValue);
}

bool JsonNumberOrStringToEnumValue(
    const TSharedPtr<FJsonValue>& ValueField, UEnum* Enum, int64& OutValue, FString& OutError)
{
    if (!Enum)
    {
        OutError = TEXT("Enum property has no valid enum definition");
        return false;
    }

    int32 EnumIndex = INDEX_NONE;
    if (ValueField->Type == EJson::String)
    {
        const FString Name = ValueField->AsString();
        bool bContainsEmbeddedNull = false;
        for (int32 Index = 0; Index < Name.Len(); ++Index)
        {
            if (Name[Index] == TEXT('\0'))
            {
                bContainsEmbeddedNull = true;
                break;
            }
        }
        if (bContainsEmbeddedNull)
        {
            OutError = TEXT("Enum string value must not contain embedded NUL characters");
            return false;
        }

        EnumIndex = Enum->GetIndexByNameString(Name);
        for (int32 Index = 0; EnumIndex == INDEX_NONE && Index < Enum->NumEnums(); ++Index)
        {
            if (Enum->GetDisplayNameTextByIndex(Index).ToString().Equals(
                    Name, ESearchCase::CaseSensitive))
            {
                EnumIndex = Index;
            }
        }
        if (EnumIndex != INDEX_NONE)
        {
            OutValue = Enum->GetValueByIndex(EnumIndex);
        }
    }
    else if (ValueField->Type == EJson::Number)
    {
        if (!DoubleToInt64(ValueField->AsNumber(), OutValue))
        {
            OutError = TEXT("Enum numeric value must be a finite integer in the int64 range");
            return false;
        }
        EnumIndex = Enum->GetIndexByValue(OutValue);
    }
    else
    {
        OutError = TEXT("Enum property requires string or number");
        return false;
    }

    if (EnumIndex == INDEX_NONE)
    {
        OutError = FString::Printf(TEXT("Invalid enum value for enum '%s'"), *Enum->GetName());
        return false;
    }

    const FString EnumName = Enum->GetNameStringByIndex(EnumIndex);
    const bool bIsGeneratedMax =
        EnumIndex == Enum->NumEnums() - 1 &&
        (EnumName.Equals(TEXT("MAX"), ESearchCase::CaseSensitive) ||
         EnumName.EndsWith(TEXT("_MAX"), ESearchCase::CaseSensitive));
    if (Enum->HasMetaData(TEXT("Hidden"), EnumIndex) || bIsGeneratedMax)
    {
        OutError = FString::Printf(
            TEXT("Enum value '%s' is hidden or reserved for enum '%s'"),
            *EnumName, *Enum->GetName());
        return false;
    }
    return true;
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
        if (!JsonNumberOrStringToInt64(ValueField, IntValue) ||
            IntValue < MIN_int32 || IntValue > MAX_int32)
        {
            OutError = TEXT("Int property requires a 32-bit signed integer");
            return false;
        }
        IntProp->SetPropertyValue_InContainer(TargetContainer, static_cast<int32>(IntValue));
        return true;
    }
    if (FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
    {
        if (!JsonNumberOrStringToInt64(ValueField, IntValue)) { OutError = TEXT("Unsupported JSON type for int64 property"); return false; }
        Int64Prop->SetPropertyValue_InContainer(TargetContainer, IntValue);
        return true;
    }
    if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        if (ByteProp->Enum)
        {
            if (!JsonNumberOrStringToEnumValue(ValueField, ByteProp->Enum, IntValue, OutError)) return false;
            if (IntValue < 0 || IntValue > MAX_uint8)
            {
                OutError = TEXT("Byte-backed enum requires a value from 0 to 255");
                return false;
            }
        }
        else if (!JsonNumberOrStringToInt64(ValueField, IntValue) || IntValue < 0 || IntValue > MAX_uint8)
        {
            OutError = TEXT("Byte property requires a value from 0 to 255");
            return false;
        }
        ByteProp->SetPropertyValue_InContainer(TargetContainer, static_cast<uint8>(IntValue));
        return true;
    }
    if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        FNumericProperty* UnderlyingProperty = EnumProp->GetUnderlyingProperty();
        if (!UnderlyingProperty ||
            !JsonNumberOrStringToEnumValue(ValueField, EnumProp->GetEnum(), IntValue, OutError))
        {
            if (OutError.IsEmpty()) OutError = TEXT("Enum property has no numeric storage");
            return false;
        }
        UnderlyingProperty->SetIntPropertyValue(
            EnumProp->ContainerPtrToValuePtr<void>(TargetContainer), IntValue);
        return true;
    }

    if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        if (ValueField->Type != EJson::Object) { OutError = TEXT("Struct property requires a JSON object value"); return false; }
        UScriptStruct* ScriptStruct = StructProp->Struct;
        if (!ScriptStruct) { OutError = TEXT("Struct property has no valid UScriptStruct"); return false; }
        void* StructMemory = StructProp->ContainerPtrToValuePtr<void>(TargetContainer);
        if (!FJsonObjectConverter::JsonObjectToUStruct(ValueField->AsObject().ToSharedRef(), ScriptStruct, StructMemory, 0, 0)) { OutError = FString::Printf(TEXT("Failed to convert JSON object to struct '%s'"), *ScriptStruct->GetName()); return false; }
        return true;
    }

    OutError = FString::Printf(TEXT("Unsupported property type: %s"), *Property->GetClass()->GetName());
    return false;
}
}
