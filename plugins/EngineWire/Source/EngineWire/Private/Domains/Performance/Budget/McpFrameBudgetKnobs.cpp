// Copyright (c) 2024 EngineWire Contributors
//
// fit_frame_budget's knobs (McpFrameBudget.h): found in the view, switched for a measure, kept as one undo step.

#include "Domains/Performance/Budget/McpFrameBudget.h"

#include "Algo/Sort.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EditorViewportClient.h"
#include "EngineUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "HAL/IConsoleManager.h"

namespace McpFrameBudget
{
namespace
{
constexpr int32 MaxLights = 12;
constexpr int32 BandCount = 3;
constexpr float Bands[BandCount] = {2000.f, 4000.f, 8000.f};
// A mesh no wider than this share of the view's width at its distance is small: culling it far away is rarely missed.
constexpr double SmallShare = 0.02;

struct FCone
{
    FVector Eye = FVector::ZeroVector;
    FVector Forward = FVector::ForwardVector;
    double Angle = 0.0;   // half the angle across the view's corners
    double TanHalf = 1.0; // tan of half the horizontal field of view
};

FCone ViewCone(const FEditorViewportClient& Client)
{
    const FIntPoint Size = Client.Viewport->GetSizeXY();
    const double TanX = FMath::Tan(FMath::DegreesToRadians(Client.ViewFOV * 0.5));
    const double TanY = TanX * Size.Y / FMath::Max(Size.X, 1);
    FCone Cone;
    Cone.Eye = Client.GetViewLocation();
    Cone.Forward = Client.GetViewRotation().Vector();
    Cone.Angle = FMath::Atan(FMath::Sqrt(TanX * TanX + TanY * TanY));
    Cone.TanHalf = TanX;
    return Cone;
}

// A sphere reaches the view when its edge comes inside the cone through the view's corners.
bool Reaches(const FCone& Cone, const FVector& Centre, double Radius)
{
    const FVector To = Centre - Cone.Eye;
    const double Distance = To.Size();
    if (Distance <= Radius)
    {
        return true;
    }
    const double Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(To / Distance, Cone.Forward), -1.0, 1.0));
    return Angle <= Cone.Angle + FMath::Asin(FMath::Min(1.0, Radius / Distance));
}

bool CastsMovableShadows(const ULightComponent* Light, const FCone& Cone)
{
    if (!Light || !Light->IsRegistered() || !Light->IsVisible() || !Light->bAffectsWorld || !Light->CastShadows ||
        Light->Mobility != EComponentMobility::Movable)
    {
        return false; // a static or stationary light's shadows are baked (and SetCastShadows leaves it alone)
    }
    const ULocalLightComponent* Local = Cast<ULocalLightComponent>(Light);
    return Light->IsA<UDirectionalLightComponent>() || (Local && Reaches(Cone, Local->GetComponentLocation(), Local->AttenuationRadius));
}

// The radius a light's shadows can reach; a directional light reaches everything.
double LightReach(const ULightComponent* Light)
{
    const ULocalLightComponent* Local = Cast<ULocalLightComponent>(Light);
    return Local ? Local->AttenuationRadius : TNumericLimits<double>::Max();
}

FString OwnerLabel(const UActorComponent* Component)
{
    const AActor* Owner = Component->GetOwner();
    return Owner ? Owner->GetActorLabel() : Component->GetName();
}
} // namespace

void FindKnobs(FRun& Run, bool bShadows, bool bDistance)
{
    // At shadow quality 0 (scalability Low) the renderer draws no dynamic shadows: turning a light's off saves nothing.
    const IConsoleVariable* ShadowQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShadowQuality"));
    Run.bNoDynamicShadows = bShadows && ShadowQuality && ShadowQuality->GetInt() <= 0;
    bShadows = bShadows && !Run.bNoDynamicShadows;
    const FCone Cone = ViewCone(*Run.Client);
    TArray<ULightComponent*> Lights;
    TArray<UStaticMeshComponent*> Banded[BandCount];
    for (TActorIterator<AActor> It(Run.Client->GetWorld()); It; ++It)
    {
        for (UActorComponent* Component : It->GetComponents())
        {
            ULightComponent* Light = bShadows ? Cast<ULightComponent>(Component) : nullptr;
            if (CastsMovableShadows(Light, Cone))
            {
                Lights.Add(Light);
            }
            UStaticMeshComponent* Mesh = bDistance ? Cast<UStaticMeshComponent>(Component) : nullptr;
            if (!Mesh || !Mesh->IsRegistered() || !Mesh->IsVisible() || Mesh->bNeverDistanceCull || Mesh->GetLODParentPrimitive())
            {
                continue;
            }
            const FBoxSphereBounds& Bounds = Mesh->Bounds;
            const double Distance = FVector::Dist(Cone.Eye, Bounds.Origin);
            if (!Reaches(Cone, Bounds.Origin, Bounds.SphereRadius) || Bounds.SphereRadius > SmallShare * 2.0 * Distance * Cone.TanHalf)
            {
                continue;
            }
            for (int32 Band = 0; Band < BandCount; ++Band)
            {
                const float Drawn = Mesh->CachedMaxDrawDistance; // 0: drawn at any distance
                if (Distance - Bounds.SphereRadius > Bands[Band] && (Drawn == 0.f || Drawn > Bands[Band]))
                {
                    Banded[Band].Add(Mesh);
                }
            }
        }
    }
    Algo::Sort(Lights, [](const ULightComponent* A, const ULightComponent* B) { return LightReach(A) > LightReach(B); });
    for (int32 Index = 0; Index < FMath::Min(Lights.Num(), MaxLights); ++Index)
    {
        FKnob& Knob = Run.Knobs.AddDefaulted_GetRef();
        Knob.Kind = TEXT("shadows");
        Knob.What = FString::Printf(TEXT("%s casts no shadows"), *OwnerLabel(Lights[Index]));
        Knob.Group = Index;
        Knob.Lights.Add(Lights[Index]);
    }
    for (int32 Band = 0; Band < BandCount; ++Band)
    {
        if (Banded[Band].Num() == 0)
        {
            continue;
        }
        FKnob& Knob = Run.Knobs.AddDefaulted_GetRef();
        Knob.Kind = TEXT("draw_distance");
        Knob.What = FString::Printf(TEXT("%d small mesh(es) not drawn beyond %.0f m"), Banded[Band].Num(), Bands[Band] / 100.f);
        Knob.Group = MaxLights; // the bands exclude each other
        Knob.Distance = Bands[Band];
        for (UStaticMeshComponent* Mesh : Banded[Band])
        {
            Knob.Meshes.Add(Mesh);
            Knob.OldDistance.Add(Mesh->CachedMaxDrawDistance);
        }
    }
}

void SetKnob(FKnob& Knob, bool bOn)
{
    Knob.bOn = bOn;
    for (const TWeakObjectPtr<ULightComponent>& Light : Knob.Lights)
    {
        if (Light.IsValid())
        {
            Light->SetCastShadows(!bOn);
        }
    }
    // The distance the renderer culls at, not the saved one (LDMaxDrawDistance): a measure leaves nothing to save.
    for (int32 Index = 0; Index < Knob.Meshes.Num(); ++Index)
    {
        if (UPrimitiveComponent* Mesh = Knob.Meshes[Index].Get())
        {
            Mesh->SetCachedMaxDrawDistance(bOn ? Knob.Distance : Knob.OldDistance[Index]);
        }
    }
}

int32 KeepKnobs(const FRun& Run, FString& OutCode, FString& OutDetail)
{
    TArray<UObject*> Objects;
    for (const int32 Index : Run.Chosen)
    {
        for (const TWeakObjectPtr<ULightComponent>& Light : Run.Knobs[Index].Lights)
        {
            if (Light.IsValid())
            {
                Objects.Add(Light.Get());
            }
        }
        for (const TWeakObjectPtr<UPrimitiveComponent>& Mesh : Run.Knobs[Index].Meshes)
        {
            if (Mesh.IsValid())
            {
                Objects.Add(Mesh.Get());
            }
        }
    }
    const FMcpScopedEditorTransaction Transaction(NSLOCTEXT("McpFrameBudget", "FitFrameBudget", "Fit Frame Budget"),
                                                  EMcpMutationDurability::EditorStateOnly, Objects);
    if (!Transaction.IsRecordingUndo()) // kept only as an undo step: nothing changes without one
    {
        OutCode = Transaction.GetRefusalCode();
        OutDetail = Transaction.GetRefusalDetail();
        return INDEX_NONE;
    }
    for (const int32 Index : Run.Chosen)
    {
        const FKnob& Knob = Run.Knobs[Index];
        for (const TWeakObjectPtr<ULightComponent>& Light : Knob.Lights)
        {
            if (Light.IsValid())
            {
                Light->SetCastShadows(false);
            }
        }
        for (const TWeakObjectPtr<UPrimitiveComponent>& Mesh : Knob.Meshes)
        {
            if (Mesh.IsValid())
            {
                Mesh->SetCullDistance(Knob.Distance); // the saved Desired Max Draw Distance, and the drawn one with it
            }
        }
    }
    return Run.Chosen.Num();
}
} // namespace McpFrameBudget
