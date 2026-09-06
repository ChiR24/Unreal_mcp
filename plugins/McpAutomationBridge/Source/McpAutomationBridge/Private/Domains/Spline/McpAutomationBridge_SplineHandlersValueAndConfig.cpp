#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpAutomationBridge_SplineHandlersPrivate.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "Components/SplineMeshComponent.h"

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

USplineMeshComponent* FindSplineMeshComponent(AActor* Actor, const FString& ComponentName)
{
    TArray<USplineMeshComponent*> MeshComponents;
    Actor->GetComponents<USplineMeshComponent>(MeshComponents);
    if (!ComponentName.IsEmpty())
    {
        for (USplineMeshComponent* Comp : MeshComponents)
        {
            if (Comp && Comp->GetName() == ComponentName)
            {
                return Comp;
            }
        }
        return nullptr;
    }
    return MeshComponents.Num() > 0 ? MeshComponents[0] : nullptr;
}

ESplineMeshAxis::Type ParseSplineMeshAxis(const FString& ForwardAxis)
{
    if (ForwardAxis == TEXT("Y")) return ESplineMeshAxis::Y;
    if (ForwardAxis == TEXT("Z")) return ESplineMeshAxis::Z;
    return ESplineMeshAxis::X;
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
