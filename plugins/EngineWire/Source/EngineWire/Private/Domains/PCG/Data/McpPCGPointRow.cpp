#include "Domains/PCG/Data/McpPCGPointRow.h"

#include "Runtime/Launch/Resources/Version.h"

#if MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Data/PCGBasePointData.h"
#include "Data/PCGPointData.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/ThreadingBase.h"

namespace McpPCGHandlers
{
namespace
{
constexpr int32 MaxFields = 8;
constexpr int64 MaxSafeJsonInteger = 9007199254740991LL;

bool Fail(FString& Error, const TCHAR* Message)
{
    Error = Message;
    return false;
}

bool IsFiniteVector(const FVector& Value)
{
    return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
}

bool AddNumber(FJsonObject& Row, const TCHAR* Field, double Value, FString& Error)
{
    if (!FMath::IsFinite(Value)) return Fail(Error, TEXT("PCG point contains a non-finite number"));
    Row.SetNumberField(Field, Value);
    return true;
}

template<typename RangeType>
bool AddVectorRange(const RangeType& Values, int32 Index, FJsonObject& Row, const TCHAR* Field, FString& Error)
{
    if (Values.Num() <= Index) return Fail(Error, TEXT("PCG point vector range is unavailable"));
    const FVector& Value = Values[Index];
    if (!IsFiniteVector(Value)) return Fail(Error, TEXT("PCG point vector contains a non-finite number"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("x"), Value.X);
    Object->SetNumberField(TEXT("y"), Value.Y);
    Object->SetNumberField(TEXT("z"), Value.Z);
    Row.SetObjectField(Field, Object);
    return true;
}

template<typename RangeType>
bool AddFloatRange(const RangeType& Values, int32 Index, FJsonObject& Row, const TCHAR* Field, FString& Error)
{
    if (Values.Num() <= Index) return Fail(Error, TEXT("PCG point scalar range is unavailable"));
    return AddNumber(Row, Field, static_cast<double>(Values[Index]), Error);
}

template<typename RangeType>
bool AddSeedRange(const RangeType& Values, int32 Index, FJsonObject& Row, FString& Error)
{
    if (Values.Num() <= Index) return Fail(Error, TEXT("PCG point seed range is unavailable"));
    Row.SetNumberField(TEXT("seed"), static_cast<double>(Values[Index]));
    return true;
}

template<typename RangeType>
bool AddMetadataEntryRange(const RangeType& Values, int32 Index, FJsonObject& Row, FString& Error)
{
    if (Values.Num() <= Index) return Fail(Error, TEXT("PCG point metadata-entry range is unavailable"));
    const int64 Value = static_cast<int64>(Values[Index]);
    if (Value >= -MaxSafeJsonInteger && Value <= MaxSafeJsonInteger)
        Row.SetNumberField(TEXT("metadataEntry"), static_cast<double>(Value));
    else
        Row.SetStringField(TEXT("metadataEntry"), LexToString(Value));
    return true;
}

bool AddTransformRange(const UPCGBasePointData* Data, int32 Index, FJsonObject& Row, FString& Error)
{
    const auto Values = Data->GetConstTransformValueRange();
    if (Values.Num() <= Index) return Fail(Error, TEXT("PCG point transform range is unavailable"));
    const FTransform& Value = Values[Index];
    const FVector Location = Value.GetLocation();
    const FQuat Rotation = Value.GetRotation();
    const FVector Scale = Value.GetScale3D();
    if (!IsFiniteVector(Location) || !FMath::IsFinite(Rotation.X) || !FMath::IsFinite(Rotation.Y) ||
        !FMath::IsFinite(Rotation.Z) || !FMath::IsFinite(Rotation.W) || !IsFiniteVector(Scale))
        return Fail(Error, TEXT("PCG point transform contains a non-finite number"));
    TSharedPtr<FJsonObject> LocationObject = MakeShared<FJsonObject>();
    LocationObject->SetNumberField(TEXT("x"), Location.X);
    LocationObject->SetNumberField(TEXT("y"), Location.Y);
    LocationObject->SetNumberField(TEXT("z"), Location.Z);
    TSharedPtr<FJsonObject> RotationObject = MakeShared<FJsonObject>();
    RotationObject->SetNumberField(TEXT("x"), Rotation.X);
    RotationObject->SetNumberField(TEXT("y"), Rotation.Y);
    RotationObject->SetNumberField(TEXT("z"), Rotation.Z);
    RotationObject->SetNumberField(TEXT("w"), Rotation.W);
    TSharedPtr<FJsonObject> ScaleObject = MakeShared<FJsonObject>();
    ScaleObject->SetNumberField(TEXT("x"), Scale.X);
    ScaleObject->SetNumberField(TEXT("y"), Scale.Y);
    ScaleObject->SetNumberField(TEXT("z"), Scale.Z);
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetObjectField(TEXT("location"), LocationObject);
    Object->SetObjectField(TEXT("rotation"), RotationObject);
    Object->SetObjectField(TEXT("scale"), ScaleObject);
    Row.SetObjectField(TEXT("transform"), Object);
    return true;
}

bool AddColorRange(const UPCGBasePointData* Data, int32 Index, FJsonObject& Row, FString& Error)
{
    const auto Values = Data->GetConstColorValueRange();
    if (Values.Num() <= Index) return Fail(Error, TEXT("PCG point color range is unavailable"));
    const FVector4& Value = Values[Index];
    if (!FMath::IsFinite(Value.X) || !FMath::IsFinite(Value.Y) || !FMath::IsFinite(Value.Z) || !FMath::IsFinite(Value.W))
        return Fail(Error, TEXT("PCG point color contains a non-finite number"));
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("r"), Value.X);
    Object->SetNumberField(TEXT("g"), Value.Y);
    Object->SetNumberField(TEXT("b"), Value.Z);
    Object->SetNumberField(TEXT("a"), Value.W);
    Row.SetObjectField(TEXT("color"), Object);
    return true;
}
}

bool ValidatePCGPointFields(const TArray<FString>& Fields, FString& OutError)
{
    OutError.Reset();
    if (Fields.IsEmpty()) return Fail(OutError, TEXT("At least one PCG point field is required"));
    if (Fields.Num() > MaxFields) return Fail(OutError, TEXT("Too many PCG point fields were requested"));
    TSet<FString> Seen;
    for (const FString& Field : Fields)
    {
        if (Seen.Contains(Field)) return Fail(OutError, TEXT("Duplicate PCG point fields are not allowed"));
        Seen.Add(Field);
        if (Field != TEXT("transform") && Field != TEXT("density") && Field != TEXT("seed") && Field != TEXT("boundsMin") &&
            Field != TEXT("boundsMax") && Field != TEXT("color") && Field != TEXT("steepness") && Field != TEXT("metadataEntry"))
            return Fail(OutError, TEXT("Unknown PCG point field"));
    }
    return true;
}

bool ReadPCGPointRow(const UPCGBasePointData* Data, int32 Index, const TArray<FString>& Fields,
    TSharedPtr<FJsonObject>& OutRow, FString& OutError)
{
    OutRow.Reset();
    OutError.Reset();
    if (!ValidatePCGPointFields(Fields, OutError)) return false;
    if (!IsInGameThread()) return Fail(OutError, TEXT("PCG point rows may only be read on the game thread"));
    if (!IsValid(Data)) return Fail(OutError, TEXT("PCG point data is unavailable"));
    if (Index < 0 || Index >= Data->GetNumPoints()) return Fail(OutError, TEXT("PCG point index is out of range"));

    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    for (const FString& Field : Fields)
    {
        if (Field == TEXT("transform"))
        {
            if (!AddTransformRange(Data, Index, *Row, OutError)) return false;
        }
        else if (Field == TEXT("density"))
        {
            const auto Values = Data->GetConstDensityValueRange();
            if (!AddFloatRange(Values, Index, *Row, TEXT("density"), OutError)) return false;
        }
        else if (Field == TEXT("seed"))
        {
            const auto Values = Data->GetConstSeedValueRange();
            if (!AddSeedRange(Values, Index, *Row, OutError)) return false;
        }
        else if (Field == TEXT("boundsMin"))
        {
            const auto Values = Data->GetConstBoundsMinValueRange();
            if (!AddVectorRange(Values, Index, *Row, TEXT("boundsMin"), OutError)) return false;
        }
        else if (Field == TEXT("boundsMax"))
        {
            const auto Values = Data->GetConstBoundsMaxValueRange();
            if (!AddVectorRange(Values, Index, *Row, TEXT("boundsMax"), OutError)) return false;
        }
        else if (Field == TEXT("color"))
        {
            if (!AddColorRange(Data, Index, *Row, OutError)) return false;
        }
        else if (Field == TEXT("steepness"))
        {
            const auto Values = Data->GetConstSteepnessValueRange();
            if (!AddFloatRange(Values, Index, *Row, TEXT("steepness"), OutError)) return false;
        }
        else
        {
            const auto Values = Data->GetConstMetadataEntryValueRange();
            if (!AddMetadataEntryRange(Values, Index, *Row, OutError)) return false;
        }
    }
    OutRow = MoveTemp(Row);
    return true;
}
}
#endif
