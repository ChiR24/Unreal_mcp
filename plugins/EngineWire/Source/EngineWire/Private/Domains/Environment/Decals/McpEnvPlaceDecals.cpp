// Copyright (c) 2024 EngineWire Contributors
//
// build_environment place_decals: deferred decals laid onto surfaces, as a level artist drops stains, cracks, moss or
// posters. Each point (given, or scattered over a disc) is traced along a direction to the first surface drawn there; a
// decal actor is placed on it, its +X into the surface (McpDecalPlacement::SurfaceRotation), sized and given the material.
// Every decal of the call is one undo step; points that find no surface (or not one of onActors) are listed, not placed.

#include "Domains/Environment/Decals/McpDecalPlacement.h"

#include "Components/DecalComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/DecalActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Materials/Material.h"
#include "Math/RandomStream.h"
#if __has_include("MaterialDomain.h")
#include "MaterialDomain.h"
#endif
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
#include "Engine/HitResult.h" // 5.0 declares FHitResult in EngineTypes.h
#endif

namespace McpDecalPlacement
{
namespace
{
constexpr int32 MaxDecals = 200;

struct FDecalSpot
{
    FVector Point = FVector::ZeroVector;
    FVector Location = FVector::ZeroVector;
    FVector Normal = FVector::UpVector;
    FString HitActor;
    FString Missed; // empty when a surface was found: no_surface, not_listed
};

// The first drawn surface along Direction through the spot's point, from 200 cm before it to TraceCm past it: where the
// line meets the hit mesh's own triangles (a box used as collision can stand 20 cm off a recessed face, past a decal's
// reach), and on past a mesh that draws nothing along the line, as through a grate's holes.
void FindSurface(UWorld* World, const FVector& Direction, double TraceCm, const TSet<FString>& OnActors, FDrawnMeshes& Meshes,
                 FDecalSpot& Spot)
{
    const FVector Start = Spot.Point - Direction * 200.0;
    const FVector End = Spot.Point + Direction * TraceCm;
    FCollisionQueryParams Params(FName(TEXT("McpPlaceDecals")), true);
    FHitResult Hit;
    bool bFound = false;
    for (int32 Tries = 0; Tries < 8 && !bFound; ++Tries)
    {
        if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) break;
        Spot.Location = Hit.ImpactPoint;
        bFound = DrawnHit(Hit, Start, End, Meshes, Spot.Location) != EDrawnHit::Missed;
        if (!bFound) Params.AddIgnoredComponent(Hit.GetComponent());
    }
    if (!bFound)
    {
        Spot.Missed = TEXT("no_surface");
        return;
    }
    const AActor* Actor = Hit.GetActor();
    Spot.HitActor = Actor ? Actor->GetActorLabel() : FString();
    if (OnActors.Num() > 0 && !(Actor && (OnActors.Contains(Spot.HitActor) || OnActors.Contains(Actor->GetName()))))
    {
        Spot.Missed = TEXT("not_listed");
        return;
    }
    Spot.Normal = Hit.ImpactNormal;
}

TSharedPtr<FJsonValue> DecalVector(const FVector& Value)
{
    TArray<TSharedPtr<FJsonValue>> Parts;
    for (const double Part : {Value.X, Value.Y, Value.Z})
    {
        Parts.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Part * 10.0) / 10.0));
    }
    return MakeShared<FJsonValueArray>(Parts);
}

// Label_01, Label_02 ... skipping every label the level already holds, so each decal can be named in a later call.
FString NextDecalLabel(const FString& Label, TSet<FString>& Taken, int32& Counter)
{
    FString Name;
    do
    {
        Name = FString::Printf(TEXT("%s_%02d"), *Label, ++Counter);
    } while (Taken.Contains(Name));
    Taken.Add(Name);
    return Name;
}

// The points of the call: points as given, or area scattered; false with the refusal in OutWhy.
bool ReadSpots(const TSharedPtr<FJsonObject>& Payload, const FVector& Direction, double Spacing, TArray<FDecalSpot>& Out, int32& OutSeed,
               FString& OutWhy)
{
    const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
    const TSharedPtr<FJsonObject>* Area = nullptr;
    if (Payload->TryGetArrayField(TEXT("points"), Points) && Points && Points->Num() > 0)
    {
        if (Points->Num() > MaxDecals)
        {
            OutWhy = FString::Printf(TEXT("points holds %d: at most %d."), Points->Num(), MaxDecals);
            return false;
        }
        static const TCHAR* const Keys[3] = {TEXT("x"), TEXT("y"), TEXT("z")};
        for (int32 Index = 0; Index < Points->Num(); ++Index)
        {
            double V[3] = {0.0, 0.0, 0.0};
            if (!ReadJsonTriple((*Points)[Index], Keys, V))
            {
                OutWhy = FString::Printf(TEXT("points[%d] is not a point {x, y, z}."), Index);
                return false;
            }
            Out.AddDefaulted_GetRef().Point = FVector(V[0], V[1], V[2]);
        }
        return true;
    }
    if (!Payload->TryGetObjectField(TEXT("area"), Area) || !Area || !(*Area)->HasField(TEXT("center")))
    {
        OutWhy = TEXT("points (world points to trace from) or area {center, radius, count} is required.");
        return false;
    }
    const double Radius = GetJsonNumberField(*Area, TEXT("radius"), 0.0);
    const int32 Count = static_cast<int32>(GetJsonNumberField(*Area, TEXT("count"), 0.0));
    OutSeed = static_cast<int32>(GetJsonNumberField(*Area, TEXT("seed"), 1.0));
    if (Radius <= 0.0 || Count < 1 || Count > MaxDecals)
    {
        OutWhy = FString::Printf(TEXT("area needs radius over 0 and count 1-%d."), MaxDecals);
        return false;
    }
    const FVector Center = ReadJsonVector((*Area)->TryGetField(TEXT("center")), FVector::ZeroVector);
    for (const FVector& Point : ScatterInDisc(Center, Direction, Radius, Count, OutSeed, Spacing))
    {
        Out.AddDefaulted_GetRef().Point = Point;
    }
    return true;
}
} // namespace

bool HandlePlaceDecals(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                       TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    auto Fail = [&](const FString& Why, const TCHAR* Code) {
        Bridge.SendAutomationError(Socket, RequestId, Why, Code);
        return true;
    };
    const FString MaterialText = GetJsonStringField(Payload, TEXT("materialPath"));
    const FString MaterialPath = SanitizeProjectRelativePath(MaterialText);
    if (MaterialText.IsEmpty()) return Fail(TEXT("materialPath is required: a decal material or an instance of one."), TEXT("INVALID_ARGUMENT"));
    if (MaterialPath.IsEmpty()) return Fail(McpPathRefusalMessage(TEXT("materialPath"), MaterialText), TEXT("SECURITY_VIOLATION"));
    UMaterialInterface* Material = Cast<UMaterialInterface>(McpLoadAsset(MaterialPath));
    if (!Material) return Fail(FString::Printf(TEXT("No material at %s."), *MaterialText), TEXT("MATERIAL_NOT_FOUND"));
    const UMaterial* Base = Material->GetMaterial();
    if (!Base || Base->MaterialDomain != MD_DeferredDecal)
    {
        return Fail(FString::Printf(TEXT("%s is not a decal material (its material domain is not Deferred Decal): make one with "
                                         "material create_material kind decal."), *MaterialText), TEXT("DECAL_MATERIAL_REQUIRED"));
    }
    const double Width = GetJsonNumberField(Payload, TEXT("width"), 200.0);
    const double Height = GetJsonNumberField(Payload, TEXT("height"), 200.0);
    const double Depth = GetJsonNumberField(Payload, TEXT("depth"), FMath::Max(20.0, FMath::Min(Width, Height) * 0.5));
    const double Jitter = GetJsonNumberField(Payload, TEXT("scaleJitter"), 0.0);
    const FVector Direction = ExtractVectorField(Payload, TEXT("direction"), FVector(0.0, 0.0, -1.0)).GetSafeNormal();
    if (Width <= 0.0 || Height <= 0.0 || Depth <= 0.0 || Width > 100000.0 || Height > 100000.0 || Jitter < 0.0 || Jitter > 0.5 || Direction.IsNearlyZero())
    {
        return Fail(TEXT("width, height and depth must be over 0 (at most 100000 cm), scaleJitter 0-0.5, and direction not zero."), TEXT("INVALID_ARGUMENT"));
    }
    TArray<FDecalSpot> Spots;
    int32 Seed = 1;
    FString Why;
    if (!ReadSpots(Payload, Direction, FMath::Min(Width, Height) * 0.5, Spots, Seed, Why)) return Fail(Why, TEXT("INVALID_ARGUMENT"));
    TSet<FString> OnActors;
    const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
    if (Payload->TryGetArrayField(TEXT("onActors"), Names) && Names)
    {
        for (const TSharedPtr<FJsonValue>& Name : *Names) OnActors.Add(Name->AsString());
    }
    UWorld* World = McpHandlerUtils::GetEditorWorld();
    if (!World) return Fail(TEXT("No editor world to place decals in."), TEXT("NO_WORLD"));
    const double TraceCm = FMath::Clamp(GetJsonNumberField(Payload, TEXT("traceCm"), 1000.0), 1.0, 100000.0);
    FDrawnMeshes Meshes;
    for (FDecalSpot& Spot : Spots) FindSurface(World, Direction, TraceCm, OnActors, Meshes, Spot);

    double Angle = 0.0;
    const bool bAngleGiven = Payload->TryGetNumberField(TEXT("angle"), Angle);
    // Turns and sizes come from the seed, drawn for every spot in order, so a call made again places the same decals.
    FRandomStream Random(Seed + 7919);
    const FString Label = GetJsonStringField(Payload, TEXT("label"), TEXT("Decal"));
    const FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("Decals"));
    bool bDryRun = false;
    Payload->TryGetBoolField(TEXT("dryRun"), bDryRun);
    TSet<FString> Taken;
    for (TActorIterator<AActor> It(World); It; ++It) Taken.Add(It->GetActorLabel());
    int32 Counter = 0;
    TArray<TSharedPtr<FJsonValue>> Placed, Missed;
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    {
        TOptional<FMcpScopedEditorTransaction> Transaction;
        if (!bDryRun) Transaction.Emplace(FText::FromString(TEXT("Place Decals")), EMcpMutationDurability::EditorStateOnly, TArray<UObject*>());
        for (int32 Index = 0; Index < Spots.Num(); ++Index)
        {
            const FDecalSpot& Spot = Spots[Index];
            const double Turn = bAngleGiven ? Angle : (Payload->HasField(TEXT("area")) ? 360.0 * Random.FRand() : 0.0);
            const double Scale = 1.0 + Jitter * (2.0 * Random.FRand() - 1.0);
            TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetNumberField(TEXT("index"), Index);
            if (!Spot.HitActor.IsEmpty()) Row->SetStringField(TEXT("hitActor"), Spot.HitActor);
            if (!Spot.Missed.IsEmpty())
            {
                Row->SetField(TEXT("point"), DecalVector(Spot.Point));
                Row->SetStringField(TEXT("reason"), Spot.Missed);
                Missed.Add(MakeShared<FJsonValueObject>(Row));
                continue;
            }
            FRotator Rotation = SurfaceRotation(Spot.Normal, Turn);
            FString Name = NextDecalLabel(Label, Taken, Counter);
            if (!bDryRun)
            {
                FActorSpawnParameters Params;
                Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                ADecalActor* Decal = World->SpawnActor<ADecalActor>(ADecalActor::StaticClass(), FTransform(Spot.Location), Params);
                UDecalComponent* Component = Decal ? Decal->GetDecal() : nullptr;
                if (!Component)
                {
                    Row->SetStringField(TEXT("reason"), TEXT("spawn_failed"));
                    Missed.Add(MakeShared<FJsonValueObject>(Row));
                    continue;
                }
                // The decal component, the actor's root, holds a pitch of -90 in the class defaults, which SpawnActor
                // composes with a spawn rotation: the rotation is set once the actor exists, and read back.
                Decal->SetActorLocationAndRotation(Spot.Location, Rotation);
                Rotation = Decal->GetActorRotation();
                Component->DecalSize = FVector(Depth * 0.5, Height * 0.5 * Scale, Width * 0.5 * Scale);
                Component->SetDecalMaterial(Material);
                if (Payload->HasField(TEXT("sortOrder"))) Component->SetSortOrder(static_cast<int32>(GetJsonNumberField(Payload, TEXT("sortOrder"), 0.0)));
                if (Payload->HasField(TEXT("fadeScreenSize"))) Component->SetFadeScreenSize(static_cast<float>(GetJsonNumberField(Payload, TEXT("fadeScreenSize"), 0.01)));
                Component->MarkRenderStateDirty();
                Decal->SetActorLabel(Name);
                Decal->SetFolderPath(FName(*Folder));
                // A decal's box straddles the surface by design: the placement checks are told it is deliberate.
                Decal->Tags.Add(FName(TEXT("mcp.decal")));
                Decal->Tags.Add(FName(TEXT("mcp.placement.ok")));
                Name = Decal->GetActorLabel();
            }
            Row->SetStringField(TEXT("name"), Name);
            Row->SetField(TEXT("location"), DecalVector(Spot.Location));
            Row->SetField(TEXT("normal"), DecalVector(Spot.Normal));
            Row->SetField(TEXT("rotation"), DecalVector(FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll)));
            Placed.Add(MakeShared<FJsonValueObject>(Row));
        }
        if (Transaction.IsSet() && Placed.Num() > 0) Transaction->DescribeInto(Result);
    }
    Result->SetArrayField(TEXT("placed"), Placed);
    Result->SetArrayField(TEXT("missed"), Missed);
    Result->SetNumberField(TEXT("placedCount"), Placed.Num());
    Result->SetBoolField(TEXT("dryRun"), bDryRun);
    if (Placed.Num() == 0)
    {
        Bridge.SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("No decal placed: none of the %d point(s) "
            "found a surface it may go on (see missed)."), Spots.Num()), Result, TEXT("NOTHING_PLACED"));
        return true;
    }
    Bridge.SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%s %d decal(s)%s."), bDryRun ? TEXT("Would place") : TEXT("Placed"),
        Placed.Num(), Missed.Num() > 0 ? *FString::Printf(TEXT("; %d point(s) missed"), Missed.Num()) : TEXT("")), Result);
    return true;
}
} // namespace McpDecalPlacement
