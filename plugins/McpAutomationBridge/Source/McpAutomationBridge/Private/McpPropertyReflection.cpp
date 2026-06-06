#include "McpPropertyReflectionPrivate.h"

namespace McpPropertyReflection
{
TSharedPtr<FJsonValue> ExportPropertyToJsonValue(void* TargetContainer, FProperty* Property)
{
    if (!TargetContainer || !Property) return nullptr;

    if (FStrProperty* Str = CastField<FStrProperty>(Property))
    {
        return MakeShared<FJsonValueString>(Str->GetPropertyValue_InContainer(TargetContainer));
    }
    if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        return MakeShared<FJsonValueString>(NameProp->GetPropertyValue_InContainer(TargetContainer).ToString());
    }
    if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
    {
        return MakeShared<FJsonValueString>(Private::ExportTextToJsonString(TextProp->GetPropertyValue_InContainer(TargetContainer)));
    }
    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue_InContainer(TargetContainer));
    }
    if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
    {
        return MakeShared<FJsonValueNumber>(static_cast<double>(FloatProp->GetPropertyValue_InContainer(TargetContainer)));
    }
    if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
    {
        return MakeShared<FJsonValueNumber>(DoubleProp->GetPropertyValue_InContainer(TargetContainer));
    }
    if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
    {
        return MakeShared<FJsonValueNumber>(static_cast<double>(IntProp->GetPropertyValue_InContainer(TargetContainer)));
    }
    if (FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
    {
        return MakeShared<FJsonValueNumber>(static_cast<double>(Int64Prop->GetPropertyValue_InContainer(TargetContainer)));
    }

    if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        const uint8 ByteVal = ByteProp->GetPropertyValue_InContainer(TargetContainer);
        if (UEnum* Enum = ByteProp->Enum)
        {
            const FString EnumName = Enum->GetNameStringByValue(ByteVal);
            if (!EnumName.IsEmpty()) return MakeShared<FJsonValueString>(EnumName);
        }
        return MakeShared<FJsonValueNumber>(static_cast<double>(ByteVal));
    }

    if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        if (UEnum* Enum = EnumProp->GetEnum())
        {
            void* ValuePtr = EnumProp->ContainerPtrToValuePtr<void>(TargetContainer);
            if (FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty())
            {
                const int64 EnumVal = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
                const FString EnumName = Enum->GetNameStringByValue(EnumVal);
                if (!EnumName.IsEmpty())
                {
                    return MakeShared<FJsonValueString>(EnumName);
                }
                return MakeShared<FJsonValueNumber>(static_cast<double>(EnumVal));
            }
        }
        return MakeShared<FJsonValueNumber>(0.0);
    }

    if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
    {
        UObject* Object = ObjectProp->GetObjectPropertyValue_InContainer(TargetContainer);
        if (Object)
        {
            return MakeShared<FJsonValueString>(Object->GetPathName());
        }
        return MakeShared<FJsonValueNull>();
    }
    if (FSoftObjectProperty* SoftObjectProp = CastField<FSoftObjectProperty>(Property))
    {
        const FSoftObjectPtr* SoftObject = static_cast<const FSoftObjectPtr*>(SoftObjectProp->ContainerPtrToValuePtr<void>(TargetContainer));
        if (SoftObject && !SoftObject->IsNull())
        {
            return MakeShared<FJsonValueString>(SoftObject->ToSoftObjectPath().ToString());
        }
        return MakeShared<FJsonValueNull>();
    }
    if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
    {
        const FSoftObjectPtr* SoftClass = static_cast<const FSoftObjectPtr*>(SoftClassProp->ContainerPtrToValuePtr<void>(TargetContainer));
        if (SoftClass && !SoftClass->IsNull())
        {
            return MakeShared<FJsonValueString>(SoftClass->ToSoftObjectPath().ToString());
        }
        return MakeShared<FJsonValueNull>();
    }

    if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        const FString TypeName = StructProp->Struct ? StructProp->Struct->GetName() : FString();
        if (TypeName.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
        {
            return VectorToJsonValue(*StructProp->ContainerPtrToValuePtr<FVector>(TargetContainer));
        }
        if (TypeName.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase))
        {
            return RotatorToJsonValue(*StructProp->ContainerPtrToValuePtr<FRotator>(TargetContainer));
        }

        FString Exported;
        StructProp->Struct->ExportText(
            Exported,
            StructProp->ContainerPtrToValuePtr<void>(TargetContainer),
            nullptr, nullptr, 0, nullptr, true);
        return MakeShared<FJsonValueString>(Exported);
    }

    if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
    {
        return MakeShared<FJsonValueArray>(ExportArrayToJson(TargetContainer, ArrayProp));
    }
    if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
    {
        return Private::ExportMapToJsonValue(TargetContainer, MapProp);
    }
    if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
    {
        return Private::ExportSetToJsonValue(TargetContainer, SetProp);
    }

    return nullptr;
}
}
