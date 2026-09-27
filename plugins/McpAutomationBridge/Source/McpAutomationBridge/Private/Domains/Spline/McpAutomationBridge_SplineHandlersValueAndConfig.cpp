#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpAutomationBridge_SplineHandlersPrivate.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "Components/SplineMeshComponent.h"
#include "Editor.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

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

AActor* ResolveSplineActor(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& ActorName)
{
    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
        return nullptr;
    }
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return nullptr;
    }
    AActor* Actor = FindActorByNameInWorldForMcp(World, ActorName, true);
    if (!Actor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
    }
    return Actor;
}

USplineComponent* ResolveSplineTarget(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& ActorName, AActor*& OutActor)
{
    OutActor = ResolveSplineActor(Self, RequestId, Socket, ActorName);
    USplineComponent* Spline = OutActor ? FindSplineComponent(OutActor) : nullptr;
    if (OutActor && !Spline)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No spline component found on actor"), nullptr, TEXT("NO_SPLINE"));
    }
    return Spline;
}

USplineMeshComponent* ResolveSplineMeshTarget(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& ActorName, const FString& ComponentName, AActor*& OutActor)
{
    OutActor = ResolveSplineActor(Self, RequestId, Socket, ActorName);
    USplineMeshComponent* Mesh = OutActor ? FindSplineMeshComponent(OutActor, ComponentName) : nullptr;
    if (OutActor && !Mesh)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No SplineMeshComponent found on actor"), nullptr, TEXT("NO_COMPONENT"));
    }
    return Mesh;
}

USplineComponent* SpawnSplineActor(UWorld* World, const FString& Name, const FVector& Location, const FRotator& Rotation)
{
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *Name;
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
    if (!Actor)
    {
        return nullptr;
    }
    Actor->SetActorLabel(*Name);
    USplineComponent* Spline = NewObject<USplineComponent>(Actor, TEXT("SplineComponent"));
    Spline->RegisterComponent();
    Actor->AddInstanceComponent(Spline);
    Actor->SetRootComponent(Spline);
    return Spline;
}

FString RequireSplineProjectPath(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, const TCHAR* Field, const FString& Path)
{
    const FString Safe = SanitizeProjectRelativePath(Path);
    if (Safe.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid or unsafe %s: %s. Path must be relative to project (e.g., /Game/...)"), Field, *Path),
            nullptr, TEXT("SECURITY_VIOLATION"));
    }
    return Safe;
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
        return FindActorByNameInWorldForMcp(World, ActorName.TrimStartAndEnd(), true);
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
