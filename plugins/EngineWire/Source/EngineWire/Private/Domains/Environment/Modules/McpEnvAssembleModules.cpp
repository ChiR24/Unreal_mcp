// Copyright (c) 2024 EngineWire Contributors
//
// build_environment assemble_modules: a modular kit of static meshes laid out over a grid of cells so that every face
// where two pieces meet fits, by wave function collapse (McpModuleKit). The faces are read from each piece's source
// geometry (McpModuleRequest.cpp); the layout is spawned as one actor holding an instanced static mesh component per
// piece, one undo step. Epic's WaveFunctionCollapse plugin is not used: enabling a plugin edits the project, and its
// adjacencies are authored by hand where these are read from the meshes.

#include "Domains/Environment/Modules/McpModuleKit.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "HAL/PlatformTime.h"

namespace McpModuleAssembly
{
using namespace McpModuleKit;

namespace
{
// The layout as one actor: a root at the grid's corner and an instanced component per piece, every instance placed by
// the same transform its faces were read through.
AActor* SpawnLayout(UWorld* World, const FString& Name, const FVector& Location, const FModuleRequest& Request, const TArray<int32>& Placed)
{
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Location), Params);
    if (!Actor)
    {
        return nullptr;
    }
    Actor->SetActorLabel(Name);
    USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"), RF_Transactional);
    Root->SetMobility(EComponentMobility::Static);
    Actor->SetRootComponent(Root);
    Root->RegisterComponent();
    Actor->AddInstanceComponent(Root);
    Actor->SetActorLocation(Location);
    const FIntVector& Size = Request.In.Size;
    TArray<TArray<FTransform>> Instances;
    Instances.SetNum(Request.Pieces.Num());
    for (int32 Index = 0; Index < Placed.Num(); ++Index)
    {
        const FModuleVariant& Variant = Request.In.Variants[Placed[Index]];
        if (Variant.Module != INDEX_NONE)
        {
            const FVector At((Index % Size.X + 0.5) * Request.Cell.X, ((Index / Size.X) % Size.Y + 0.5) * Request.Cell.Y,
                             (Index / (Size.X * Size.Y) + 0.5) * Request.Cell.Z);
            Instances[Variant.Module].Add(CellFromMesh(Request.Centre, Variant.Turns) * FTransform(At));
        }
    }
    for (int32 Piece = 0; Piece < Request.Pieces.Num(); ++Piece)
    {
        if (Instances[Piece].Num() == 0)
        {
            continue;
        }
        UStaticMesh* Mesh = Request.Pieces[Piece].Mesh;
        UInstancedStaticMeshComponent* Part = NewObject<UInstancedStaticMeshComponent>(
            Actor, MakeUniqueObjectName(Actor, UInstancedStaticMeshComponent::StaticClass(), Mesh->GetFName()), RF_Transactional);
        Part->SetMobility(EComponentMobility::Static);
        Part->SetStaticMesh(Mesh);
        Part->SetupAttachment(Root);
        Part->RegisterComponent();
        Actor->AddInstanceComponent(Part);
        Part->AddInstances(Instances[Piece], false);
    }
    return Actor;
}
} // namespace

bool HandleAssembleModules(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                           TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const double Start = FPlatformTime::Seconds();
    FModuleRequest Request;
    FString Why, Code;
    // The faces are read inside 10 s of the call and the layout found inside 20, well under the client's 30.
    if (!ReadRequest(Payload, Request, Why, Code) || !BuildVariants(Request, Start + 10.0, Why))
    {
        Bridge.SendAutomationError(Socket, RequestId, Why + TEXT("; nothing was placed."), Code);
        return true;
    }
    FModuleProblem& In = Request.In;
    In.Deadline = Start + 20.0;
    // Faces nothing can meet, on a side the outside does not take: those pieces can never be placed. One row per piece:
    // the turns that never fit (none named when no turn does) and why, for the first of them.
    const TArray<FIntPoint> NoMatch = UnmatchedFaces(In);
    TArray<TSharedPtr<FJsonValue>> Unmatched;
    for (int32 Piece = 0; Piece < Request.Pieces.Num(); ++Piece)
    {
        TArray<FString> Turns, Reasons;
        int32 First = INDEX_NONE;
        for (const FIntPoint& Face : NoMatch)
        {
            const FModuleVariant& Variant = In.Variants[Face.X];
            if (Variant.Module != Piece)
            {
                continue;
            }
            Turns.AddUnique(FString::FromInt(Variant.Turns * 90));
            First = First == INDEX_NONE ? Face.X : First;
            if (Face.X == First)
            {
                const int32 Along = Face.Y < 2 ? In.Size.X : Face.Y < 4 ? In.Size.Y : In.Size.Z;
                Reasons.Add(FString::Printf(TEXT("its %s face %s"), SideName(Face.Y),
                                        Along > 1 ? TEXT("meets no piece's face") : TEXT("always meets the outside (the grid is one cell thick there)")));
            }
        }
        const int32 Variants = In.Variants.FilterByPredicate([Piece](const FModuleVariant& Variant) { return Variant.Module == Piece; }).Num();
        if (First != INDEX_NONE)
        {
            const FString Which = Turns.Num() < Variants ? FString::Printf(TEXT(" turned %s"), *FString::Join(Turns, TEXT(" or "))) : FString();
            const FString Where = Which.IsEmpty() ? FString() : FString::Printf(TEXT("turned %d, "), In.Variants[First].Turns * 90);
            Unmatched.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("'%s'%s is never placed: %s%s, and openSides does not name %s"),
                *Request.Pieces[Piece].Mesh->GetName(), *Which, *Where, *FString::Join(Reasons, TEXT("; ")), Reasons.Num() > 1 ? TEXT("those sides") : TEXT("that side"))));
        }
    }
    const FModuleLayout Layout = Solve(In);
    if (!Layout.bSolved)
    {
        FString Faces;
        for (int32 Index = 0; Index < Unmatched.Num() && Index < 6; ++Index)
        {
            Faces += TEXT(" ") + Unmatched[Index]->AsString() + TEXT(".");
        }
        Bridge.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No layout: %s.%s"), *Layout.Why, *Faces), TEXT("NO_SOLUTION"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    bool bDryRun = false;
    Payload->TryGetBoolField(TEXT("dryRun"), bDryRun);
    if (!bDryRun)
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Assemble Modules")), EMcpMutationDurability::EditorStateOnly, TArray<UObject*>());
        AActor* Actor = World ? SpawnLayout(World, GetJsonStringField(Payload, TEXT("name"), TEXT("ModuleAssembly")),
                                            ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector), Request, Layout.Placed)
                              : nullptr;
        if (!Actor)
        {
            Bridge.SendAutomationError(Socket, RequestId, TEXT("The layout was found but its actor could not be spawned."), TEXT("SPAWN_FAILED"));
            return true;
        }
        Result->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
        McpHandlerUtils::AddVerification(Result, Actor);
        Transaction.DescribeInto(Result);
    }
    TArray<int32> Counts;
    Counts.Init(0, Request.Pieces.Num());
    for (const int32 Placed : Layout.Placed)
    {
        const int32 Module = In.Variants[Placed].Module;
        if (Module != INDEX_NONE)
        {
            ++Counts[Module];
        }
    }
    TArray<TSharedPtr<FJsonValue>> Rows;
    int32 Filled = 0, Used = 0;
    for (int32 Index = 0; Index < Request.Pieces.Num(); ++Index)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("meshPath"), Request.Pieces[Index].Mesh->GetPathName());
        Row->SetNumberField(TEXT("count"), Counts[Index]);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
        Filled += Counts[Index];
        Used += Counts[Index] > 0 ? 1 : 0;
    }
    Result->SetArrayField(TEXT("placed"), Rows);
    Result->SetNumberField(TEXT("cells"), Layout.Placed.Num());
    Result->SetNumberField(TEXT("emptyCells"), Layout.Placed.Num() - Filled);
    Result->SetNumberField(TEXT("variants"), In.Variants.Num());
    Result->SetNumberField(TEXT("openFaces"), OpenFaces(In, Layout.Placed));
    Result->SetNumberField(TEXT("backtracks"), Layout.Backtracks);
    Result->SetNumberField(TEXT("seedsTried"), Layout.Attempt + 1);
    Result->SetNumberField(TEXT("seconds"), FMath::RoundToDouble((FPlatformTime::Seconds() - Start) * 100.0) / 100.0);
    if (Unmatched.Num() > 0)
    {
        Result->SetArrayField(TEXT("unmatchedFaces"), Unmatched);
    }
    Bridge.SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("%s %d of %d cells with %d of %d piece(s); every face where two meet matches."), bDryRun ? TEXT("Would fill") : TEXT("Filled"),
                        Filled, Layout.Placed.Num(), Used, Request.Pieces.Num()), Result);
    return true;
}
} // namespace McpModuleAssembly
