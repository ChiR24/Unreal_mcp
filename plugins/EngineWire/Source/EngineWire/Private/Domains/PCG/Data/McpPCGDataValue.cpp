#include "McpPCGDataValue.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#if MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/PCGMetadataAttributeTraits.h"

namespace McpPCGDataValue
{
namespace
{
constexpr int32 MaxStringChars = 1024;
constexpr int64 MaxSafeJsonInteger = 9007199254740991LL;

bool Fail(FString& OutError, const TCHAR* Message)
{
    OutError = Message;
    return false;
}

bool Number(double Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
    if (!FMath::IsFinite(Value)) return Fail(OutError, TEXT("PCG metadata contains a non-finite number"));
    OutValue = MakeShared<FJsonValueNumber>(Value);
    return true;
}

bool String(const FString& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
    if (Value.Len() > MaxStringChars) return Fail(OutError, TEXT("PCG metadata string exceeds 1024 characters"));
    OutValue = MakeShared<FJsonValueString>(Value);
    return true;
}

bool Vector2(const FVector2D& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
    if (!FMath::IsFinite(Value.X) || !FMath::IsFinite(Value.Y)) return Fail(OutError, TEXT("PCG vector contains a non-finite number"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("x"), Value.X);
    Object->SetNumberField(TEXT("y"), Value.Y);
    OutValue = MakeShared<FJsonValueObject>(Object);
    return true;
}

TSharedPtr<FJsonObject> VectorObject(const FVector& Value)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("x"), Value.X);
    Object->SetNumberField(TEXT("y"), Value.Y);
    Object->SetNumberField(TEXT("z"), Value.Z);
    return Object;
}

TSharedPtr<FJsonObject> RotatorObject(const FRotator& Value)
{
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("pitch"), Value.Pitch);
    Object->SetNumberField(TEXT("yaw"), Value.Yaw);
    Object->SetNumberField(TEXT("roll"), Value.Roll);
    return Object;
}

bool Vector(const FVector& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
    if (!FMath::IsFinite(Value.X) || !FMath::IsFinite(Value.Y) || !FMath::IsFinite(Value.Z))
        return Fail(OutError, TEXT("PCG vector contains a non-finite number"));
    OutValue = MakeShared<FJsonValueObject>(VectorObject(Value));
    return true;
}

bool Vector4(const FVector4& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
    if (!FMath::IsFinite(Value.X) || !FMath::IsFinite(Value.Y) || !FMath::IsFinite(Value.Z) || !FMath::IsFinite(Value.W))
        return Fail(OutError, TEXT("PCG vector contains a non-finite number"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("x"), Value.X);
    Object->SetNumberField(TEXT("y"), Value.Y);
    Object->SetNumberField(TEXT("z"), Value.Z);
    Object->SetNumberField(TEXT("w"), Value.W);
    OutValue = MakeShared<FJsonValueObject>(Object);
    return true;
}

bool Quaternion(const FQuat& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
    if (!FMath::IsFinite(Value.X) || !FMath::IsFinite(Value.Y) || !FMath::IsFinite(Value.Z) || !FMath::IsFinite(Value.W))
        return Fail(OutError, TEXT("PCG quaternion contains a non-finite number"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("x"), Value.X);
    Object->SetNumberField(TEXT("y"), Value.Y);
    Object->SetNumberField(TEXT("z"), Value.Z);
    Object->SetNumberField(TEXT("w"), Value.W);
    OutValue = MakeShared<FJsonValueObject>(Object);
    return true;
}

bool Rotator(const FRotator& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
    if (!FMath::IsFinite(Value.Pitch) || !FMath::IsFinite(Value.Yaw) || !FMath::IsFinite(Value.Roll))
        return Fail(OutError, TEXT("PCG rotator contains a non-finite number"));
    OutValue = MakeShared<FJsonValueObject>(RotatorObject(Value));
    return true;
}

bool Transform(const FTransform& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
    const FVector Translation = Value.GetLocation();
    const FQuat RotationValue = Value.GetRotation();
    const FVector Scale = Value.GetScale3D();
    if (!FMath::IsFinite(Translation.X) || !FMath::IsFinite(Translation.Y) || !FMath::IsFinite(Translation.Z) ||
        !FMath::IsFinite(RotationValue.X) || !FMath::IsFinite(RotationValue.Y) || !FMath::IsFinite(RotationValue.Z) || !FMath::IsFinite(RotationValue.W) ||
        !FMath::IsFinite(Scale.X) || !FMath::IsFinite(Scale.Y) || !FMath::IsFinite(Scale.Z))
        return Fail(OutError, TEXT("PCG transform contains a non-finite number"));
    const FRotator Rotation = RotationValue.Rotator();
    if (!FMath::IsFinite(Rotation.Pitch) || !FMath::IsFinite(Rotation.Yaw) || !FMath::IsFinite(Rotation.Roll))
        return Fail(OutError, TEXT("PCG transform rotation is non-finite"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetObjectField(TEXT("location"), VectorObject(Translation));
    Object->SetObjectField(TEXT("rotation"), RotatorObject(Rotation));
    Object->SetObjectField(TEXT("scale"), VectorObject(Scale));
    OutValue = MakeShared<FJsonValueObject>(Object);
    return true;
}

bool Encode(float Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
bool Encode(double Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
bool Encode(int32 Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
bool Encode(int64 Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
bool Encode(bool Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
bool Encode(const FVector2D& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
bool Encode(const FVector& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
bool Encode(const FVector4& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
bool Encode(const FQuat& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
bool Encode(const FTransform& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
bool Encode(const FRotator& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
bool Encode(const FString& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
bool Encode(const FName& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
bool Encode(const FSoftObjectPath& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
bool Encode(const FSoftClassPath& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError);

template<typename T>
bool ReadTyped(const FPCGMetadataAttributeBase* Attribute, PCGMetadataEntryKey EntryKey,
    TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
    // A legacy value type ID alone does not prove a scalar descriptor on UE 5.8.
    if (!Attribute->IsOfType<T>()) return Fail(OutError, TEXT("Unsupported PCG metadata descriptor or container"));
    const T Value = Attribute->GetValueFromItemKey<T>(EntryKey);
#else
    const T Value = static_cast<const FPCGMetadataAttribute<T>*>(Attribute)->GetValueFromItemKey(EntryKey);
#endif
    return Encode(Value, OutValue, OutError);
}

bool Encode(float Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError) { return Number(Value, OutValue, OutError); }
bool Encode(double Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError) { return Number(Value, OutValue, OutError); }
bool Encode(int32 Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError) { OutValue = MakeShared<FJsonValueNumber>(Value); return true; }
bool Encode(int64 Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
    if (Value >= -MaxSafeJsonInteger && Value <= MaxSafeJsonInteger)
    {
        OutValue = MakeShared<FJsonValueNumber>(static_cast<double>(Value));
        return true;
    }
    OutValue = MakeShared<FJsonValueString>(LexToString(Value));
    return true;
}
bool Encode(bool Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError) { OutValue = MakeShared<FJsonValueBoolean>(Value); return true; }
bool Encode(const FVector2D& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError) { return Vector2(Value, OutValue, OutError); }
bool Encode(const FVector& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError) { return Vector(Value, OutValue, OutError); }
bool Encode(const FVector4& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError) { return Vector4(Value, OutValue, OutError); }
bool Encode(const FQuat& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError) { return Quaternion(Value, OutValue, OutError); }
bool Encode(const FTransform& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError) { return Transform(Value, OutValue, OutError); }
bool Encode(const FRotator& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError) { return Rotator(Value, OutValue, OutError); }
bool Encode(const FString& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError) { return String(Value, OutValue, OutError); }
bool Encode(const FName& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError) { return String(Value.ToString(), OutValue, OutError); }
bool Encode(const FSoftObjectPath& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError) { return String(Value.ToString(), OutValue, OutError); }
bool Encode(const FSoftClassPath& Value, TSharedPtr<FJsonValue>& OutValue, FString& OutError) { return String(Value.ToString(), OutValue, OutError); }
}

bool ExportPCGDataValue(const FPCGMetadataAttributeBase* Attribute, PCGMetadataEntryKey EntryKey,
    TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
    OutValue.Reset();
    OutError.Reset();
    if (!Attribute) return Fail(OutError, TEXT("PCG metadata attribute is null"));
    const int16 TypeId = Attribute->GetTypeId();
    if (TypeId < static_cast<int16>(EPCGMetadataTypes::Float) || TypeId > static_cast<int16>(EPCGMetadataTypes::SoftClassPath))
    {
        OutError = FString::Printf(TEXT("Unsupported PCG metadata type id: %d"), TypeId);
        return false;
    }

    switch (static_cast<EPCGMetadataTypes>(TypeId))
    {
    case EPCGMetadataTypes::Float: return ReadTyped<float>(Attribute, EntryKey, OutValue, OutError);
    case EPCGMetadataTypes::Double: return ReadTyped<double>(Attribute, EntryKey, OutValue, OutError);
    case EPCGMetadataTypes::Integer32: return ReadTyped<int32>(Attribute, EntryKey, OutValue, OutError);
    case EPCGMetadataTypes::Integer64: return ReadTyped<int64>(Attribute, EntryKey, OutValue, OutError);
    case EPCGMetadataTypes::Vector2: return ReadTyped<FVector2D>(Attribute, EntryKey, OutValue, OutError);
    case EPCGMetadataTypes::Vector: return ReadTyped<FVector>(Attribute, EntryKey, OutValue, OutError);
    case EPCGMetadataTypes::Vector4: return ReadTyped<FVector4>(Attribute, EntryKey, OutValue, OutError);
    case EPCGMetadataTypes::Quaternion: return ReadTyped<FQuat>(Attribute, EntryKey, OutValue, OutError);
    case EPCGMetadataTypes::Transform: return ReadTyped<FTransform>(Attribute, EntryKey, OutValue, OutError);
    case EPCGMetadataTypes::String: return ReadTyped<FString>(Attribute, EntryKey, OutValue, OutError);
    case EPCGMetadataTypes::Boolean: return ReadTyped<bool>(Attribute, EntryKey, OutValue, OutError);
    case EPCGMetadataTypes::Rotator: return ReadTyped<FRotator>(Attribute, EntryKey, OutValue, OutError);
    case EPCGMetadataTypes::Name: return ReadTyped<FName>(Attribute, EntryKey, OutValue, OutError);
    case EPCGMetadataTypes::SoftObjectPath: return ReadTyped<FSoftObjectPath>(Attribute, EntryKey, OutValue, OutError);
    case EPCGMetadataTypes::SoftClassPath: return ReadTyped<FSoftClassPath>(Attribute, EntryKey, OutValue, OutError);
    default:
        OutError = FString::Printf(TEXT("Unsupported PCG metadata type id: %d"), Attribute->GetTypeId());
        return false;
    }
}
}
#endif
