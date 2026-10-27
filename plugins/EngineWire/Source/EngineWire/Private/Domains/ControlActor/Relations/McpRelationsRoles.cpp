// Copyright (c) 2024 EngineWire Contributors
//
// describe_relations' scene roles. ground: the ground-like item the most others rest on, else the widest; sun: the
// brightest directional light, as the sun's elevation above the horizon and its azimuth (degrees from +X toward +Y);
// sky: the actors that make the sky (atmosphere, sky light, height fog, clouds, a sky sphere); playerStarts and
// cameras with their locations, forPlayer on a camera a player views through from the start; playArea: the box the
// navigation mesh is built in.

#include "Domains/ControlActor/Relations/McpRelations.h"

#include "Camera/CameraActor.h"
#include "Components/LightComponent.h"
#include "Dom/JsonValue.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "GameFramework/PlayerStart.h"

namespace McpRelations
{
namespace
{
double Area(const FItem& Item)
{
    const FVector Size = Item.Box.GetSize();
    return Size.X * Size.Y;
}

TSharedPtr<FJsonObject> Placed(const AActor* Actor)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("name"), McpActorRef(Actor));
    Row->SetArrayField(TEXT("location"), McpHandlerUtils::VectorToJsonArray(Actor->GetActorLocation()));
    return Row;
}

bool IsSky(const FString& Class)
{
    static const TCHAR* const Parts[] = {TEXT("SkyAtmosphere"), TEXT("SkyLight"), TEXT("HeightFog"), TEXT("VolumetricCloud"),
                                         TEXT("SkySphere"), TEXT("Sky_Sphere")};
    for (const TCHAR* Part : Parts)
    {
        if (Class.Contains(Part))
        {
            return true;
        }
    }
    return false;
}
} // namespace

TSharedPtr<FJsonObject> FindRoles(UWorld* World, const TArray<FItem>& Items, const TArray<FRelation>& Relations)
{
    TSharedPtr<FJsonObject> Roles = MakeShared<FJsonObject>();
    TMap<int32, int32> Supports;
    for (const FRelation& Relation : Relations)
    {
        if (FCString::Strcmp(Relation.Kind, TEXT("on")) == 0 && Items[Relation.Object].bGround)
        {
            ++Supports.FindOrAdd(Relation.Object);
        }
    }
    int32 Ground = INDEX_NONE;
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        const int32 Here = Supports.FindRef(Index);
        const int32 Best = Ground == INDEX_NONE ? -1 : Supports.FindRef(Ground);
        if (Items[Index].bGround && (Here > Best || (Here == Best && Area(Items[Index]) > Area(Items[Ground]))))
        {
            Ground = Index;
        }
    }
    if (Ground != INDEX_NONE)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Items[Ground].Name);
        Row->SetNumberField(TEXT("supports"), Supports.FindRef(Ground));
        Row->SetNumberField(TEXT("top"), McpHandlerUtils::ReadableDouble(Items[Ground].Box.Max.Z));
        Roles->SetObjectField(TEXT("ground"), Row);
    }
    const ADirectionalLight* Sun = nullptr;
    for (TActorIterator<ADirectionalLight> It(World); It; ++It)
    {
        const ULightComponent* Light = It->GetLightComponent();
        if (Light && (!Sun || Light->Intensity > Sun->GetLightComponent()->Intensity))
        {
            Sun = *It;
        }
    }
    if (Sun)
    {
        // The light travels along its forward axis, so the sun stands the opposite way.
        const FVector Toward = -Sun->GetLightComponent()->GetForwardVector();
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), McpActorRef(Sun));
        Row->SetNumberField(TEXT("elevation"), FMath::RoundToDouble(FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Toward.Z, -1.0, 1.0)))));
        Row->SetNumberField(TEXT("azimuth"), FMath::RoundToDouble(FMath::RadiansToDegrees(FMath::Atan2(Toward.Y, Toward.X))));
        Roles->SetObjectField(TEXT("sun"), Row);
    }
    TArray<TSharedPtr<FJsonValue>> Sky;
    TArray<TSharedPtr<FJsonValue>> Starts;
    TArray<TSharedPtr<FJsonValue>> Cameras;
    FBox PlayArea(ForceInit);
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        const FString Class = Actor->GetClass()->GetName();
        if (IsSky(Class))
        {
            Sky.Add(MakeShared<FJsonValueString>(McpActorRef(Actor)));
        }
        else if (Actor->IsA<APlayerStart>())
        {
            Starts.Add(MakeShared<FJsonValueObject>(Placed(Actor)));
        }
        else if (const ACameraActor* Camera = Cast<ACameraActor>(Actor))
        {
            TSharedPtr<FJsonObject> Row = Placed(Camera);
            if (Camera->GetAutoActivatePlayerIndex() >= 0)
            {
                Row->SetNumberField(TEXT("forPlayer"), Camera->GetAutoActivatePlayerIndex());
            }
            Cameras.Add(MakeShared<FJsonValueObject>(Row));
        }
        else if (Class == TEXT("NavMeshBoundsVolume"))
        {
            FVector Origin = FVector::ZeroVector;
            FVector Extent = FVector::ZeroVector;
            Actor->GetActorBounds(false, Origin, Extent);
            PlayArea += FBox::BuildAABB(Origin, Extent);
        }
    }
    const auto SetList = [&Roles](const TCHAR* Field, const TArray<TSharedPtr<FJsonValue>>& List) {
        if (List.Num() > 0)
        {
            Roles->SetArrayField(Field, List);
        }
    };
    SetList(TEXT("sky"), Sky);
    SetList(TEXT("playerStarts"), Starts);
    SetList(TEXT("cameras"), Cameras);
    if (PlayArea.IsValid)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetArrayField(TEXT("center"), McpHandlerUtils::VectorToJsonArray(PlayArea.GetCenter()));
        Row->SetArrayField(TEXT("size"), McpHandlerUtils::VectorToJsonArray(PlayArea.GetSize()));
        Roles->SetObjectField(TEXT("playArea"), Row);
    }
    return Roles;
}
} // namespace McpRelations
