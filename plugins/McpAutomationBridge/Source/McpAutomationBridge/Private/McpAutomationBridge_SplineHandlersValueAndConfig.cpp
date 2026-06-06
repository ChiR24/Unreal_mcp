#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SplineHandlersPrivate.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"

FString GetJsonStringFieldSpline(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, const FString& Default)
{
    if (!Payload.IsValid()) return Default;
    FString Value;
    if (Payload->TryGetStringField(FieldName, Value))
    {
        return Value;
    }
    return Default;
}

double GetJsonNumberFieldSpline(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, double Default)
{
    if (!Payload.IsValid()) return Default;
    double Value;
    if (Payload->TryGetNumberField(FieldName, Value))
    {
        return Value;
    }
    return Default;
}

bool GetJsonBoolFieldSpline(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, bool Default)
{
    if (!Payload.IsValid()) return Default;
    bool Value;
    if (Payload->TryGetBoolField(FieldName, Value))
    {
        return Value;
    }
    return Default;
}

int32 GetJsonIntFieldSpline(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, int32 Default)
{
    if (!Payload.IsValid()) return Default;
    double Value;
    if (Payload->TryGetNumberField(FieldName, Value))
    {
        return static_cast<int32>(Value);
    }
    return Default;
}

FVector GetJsonVectorFieldSpline(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, const FVector& Default)
{
    if (!Payload.IsValid()) return Default;
    const TSharedPtr<FJsonObject>* VecObj;
    if (Payload->TryGetObjectField(FieldName, VecObj) && VecObj->IsValid())
    {
        return FVector(
            GetJsonNumberFieldSpline(*VecObj, TEXT("x"), Default.X),
            GetJsonNumberFieldSpline(*VecObj, TEXT("y"), Default.Y),
            GetJsonNumberFieldSpline(*VecObj, TEXT("z"), Default.Z)
        );
    }
    return Default;
}

FRotator GetJsonRotatorFieldSpline(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName, const FRotator& Default)
{
    if (!Payload.IsValid()) return Default;
    const TSharedPtr<FJsonObject>* RotObj;
    if (Payload->TryGetObjectField(FieldName, RotObj) && RotObj->IsValid())
    {
        return FRotator(
            GetJsonNumberFieldSpline(*RotObj, TEXT("pitch"), Default.Pitch),
            GetJsonNumberFieldSpline(*RotObj, TEXT("yaw"), Default.Yaw),
            GetJsonNumberFieldSpline(*RotObj, TEXT("roll"), Default.Roll)
        );
    }
    return Default;
}

AActor* FindActorByName(UWorld* World, const FString& ActorName)
{
    if (!World || ActorName.IsEmpty()) return nullptr;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            return *It;
        }
    }
    return nullptr;
}

USplineComponent* FindSplineComponent(AActor* Actor, const FString& ComponentName)
{
    if (!Actor) return nullptr;

    TArray<USplineComponent*> SplineComponents;
    Actor->GetComponents<USplineComponent>(SplineComponents);

    if (SplineComponents.Num() == 0) return nullptr;

    if (!ComponentName.IsEmpty())
    {
        for (USplineComponent* Comp : SplineComponents)
        {
            if (Comp && Comp->GetName() == ComponentName)
            {
                return Comp;
            }
        }
        return nullptr;
    }

    return SplineComponents[0];
}

ESplinePointType::Type ParseSplinePointType(const FString& TypeStr)
{
    FString LowerStr = TypeStr.ToLower();
    if (LowerStr == TEXT("linear")) return ESplinePointType::Linear;
    if (LowerStr == TEXT("curve")) return ESplinePointType::Curve;
    if (LowerStr == TEXT("constant")) return ESplinePointType::Constant;
    if (LowerStr == TEXT("curveclamped")) return ESplinePointType::CurveClamped;
    if (LowerStr == TEXT("curvecustomtangent")) return ESplinePointType::CurveCustomTangent;
    return ESplinePointType::Curve;
}

FString SplinePointTypeToString(ESplinePointType::Type Type)
{
    switch (Type)
    {
        case ESplinePointType::Linear: return TEXT("Linear");
        case ESplinePointType::Curve: return TEXT("Curve");
        case ESplinePointType::Constant: return TEXT("Constant");
        case ESplinePointType::CurveClamped: return TEXT("CurveClamped");
        case ESplinePointType::CurveCustomTangent: return TEXT("CurveCustomTangent");
        default: return TEXT("Unknown");
    }
}

static FString MakeSplineConfigTagPrefix(const FString& Key)
{
    return FString::Printf(TEXT("MCP.Spline.%s="), *Key);
}

void SetSplineConfigValue(AActor* Target, const FString& Key, const FString& Value)
{
    if (!Target) return;

    const FString Prefix = MakeSplineConfigTagPrefix(Key);
    for (int32 Index = Target->Tags.Num() - 1; Index >= 0; --Index)
    {
        if (Target->Tags[Index].ToString().StartsWith(Prefix))
        {
            Target->Tags.RemoveAt(Index);
        }
    }

    Target->Modify();
    Target->Tags.Add(FName(*(Prefix + Value)));
    Target->MarkPackageDirty();
}

static bool TryGetSplineConfigValue(AActor* Target, const FString& Key, FString& OutValue)
{
    if (!Target) return false;

    const FString Prefix = MakeSplineConfigTagPrefix(Key);
    for (const FName& Tag : Target->Tags)
    {
        const FString TagString = Tag.ToString();
        if (TagString.StartsWith(Prefix))
        {
            OutValue = TagString.RightChop(Prefix.Len());
            return true;
        }
    }

    return false;
}

AActor* ResolveSplineConfigTarget(UWorld* World, const FString& ActorName)
{
    if (!World) return nullptr;

    if (!ActorName.TrimStartAndEnd().IsEmpty())
    {
        return FindActorByName(World, ActorName.TrimStartAndEnd());
    }

    return World->GetWorldSettings();
}

FString GetSplineConfigTargetName(AActor* Target)
{
    if (!Target) return TEXT("");
    return Target->GetActorLabel().IsEmpty() ? Target->GetName() : Target->GetActorLabel();
}

bool GetConfiguredSplineBool(AActor* Actor, UWorld* World, const FString& Key, bool DefaultValue)
{
    FString Value;
    if (TryGetSplineConfigValue(Actor, Key, Value) || TryGetSplineConfigValue(World ? World->GetWorldSettings() : nullptr, Key, Value))
    {
        return Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value == TEXT("1");
    }

    return DefaultValue;
}

double GetConfiguredSplineNumber(AActor* Actor, UWorld* World, const FString& Key, double DefaultValue)
{
    FString Value;
    if (TryGetSplineConfigValue(Actor, Key, Value) || TryGetSplineConfigValue(World ? World->GetWorldSettings() : nullptr, Key, Value))
    {
        return FCString::Atod(*Value);
    }

    return DefaultValue;
}

FString BoolToSplineConfigString(bool bValue)
{
    return bValue ? TEXT("true") : TEXT("false");
}
#endif
