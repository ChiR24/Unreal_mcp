// control_actor learn_layout_style and suggest_companions (McpLayoutStyle.h). learn reads the open level only: loading
// another would replace the level the user has open, so each level is learned while it is open and kept under its own
// name in Saved/EngineWire/LayoutStats.json (learning it again replaces its entry). suggest reads every level kept.

#include "Domains/ControlActor/Layout/McpLayoutStyle.h"

#include "Components/StaticMeshComponent.h"
#include "Domains/ControlActor/Placement/McpPlacementMount.h"
#include "Domains/ControlActor/Relations/McpRelations.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace McpLayoutStyle
{
namespace HandlerDetail
{
constexpr double NearCm = 300.0;

FString StatsFile()
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EngineWire"), TEXT("LayoutStats.json"));
}

TSharedPtr<FJsonObject> LoadLevels()
{
    FString Text;
    TSharedPtr<FJsonObject> Root;
    if (FFileHelper::LoadFileToString(Text, *StatsFile()))
    {
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root);
    }
    const TSharedPtr<FJsonObject>* Levels = nullptr;
    return Root.IsValid() && Root->TryGetObjectField(TEXT("levels"), Levels) && Levels ? *Levels : TSharedPtr<FJsonObject>(MakeShared<FJsonObject>());
}

FString MeshOf(const AActor* Actor)
{
    const UStaticMeshComponent* Mesh = Actor ? Cast<UStaticMeshComponent>(Actor->GetRootComponent()) : nullptr;
    return Mesh && Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetPathName() : FString();
}

// Other seen from Anchor: the flat distance between box centres, where Other stands in the anchor's turned frame, and
// its turn relative to the anchor's.
FObservation Observe(const McpRelations::FItem& Anchor, const McpRelations::FItem& Other, const FString& AnchorMesh,
                     const FString& OtherMesh, const TCHAR* Kind)
{
    const FVector Delta = Other.Box.GetCenter() - Anchor.Box.GetCenter();
    const double AnchorYaw = Anchor.Actor->GetActorRotation().Yaw;
    FObservation Seen;
    Seen.Anchor = AnchorMesh;
    Seen.Other = OtherMesh;
    Seen.Kind = Kind;
    Seen.Distance = FVector2D(Delta.X, Delta.Y).Size();
    Seen.Bearing = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)) - AnchorYaw;
    Seen.Yaw = Other.Actor->GetActorRotation().Yaw - AnchorYaw;
    return Seen;
}
} // namespace HandlerDetail

bool HandleLearn(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                 TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace HandlerDetail;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("No editor world is open."), TEXT("NO_WORLD"));
        return true;
    }
    int32 InScope = 0;
    const TArray<McpRelations::FItem> Items = McpRelations::CollectItems(World, TSet<AActor*>(), false, FVector::ZeroVector, 0.0, 400, InScope);
    TArray<FString> Meshes;
    FLevelStats Stats;
    for (const McpRelations::FItem& Item : Items)
    {
        Meshes.Add(Item.bGround ? FString() : MeshOf(Item.Actor));
        if (!Meshes.Last().IsEmpty()) ++Stats.Copies.FindOrAdd(Meshes.Last());
    }
    TArray<McpRelations::FRelation> Relations;
    TSet<FString> Kinds;
    Kinds.Add(TEXT("on"));
    Kinds.Add(TEXT("nextTo"));
    McpRelations::FindPairRelations(World, Items, Kinds, Relations);
    TSet<uint64> Joined;
    const auto Pair = [](int32 A, int32 B) { return (uint64(uint32(FMath::Min(A, B))) << 32) | uint32(FMath::Max(A, B)); };
    for (const McpRelations::FRelation& Relation : Relations)
    {
        const int32 A = Relation.Subject, B = Relation.Object;
        if (Meshes[A].IsEmpty() || Meshes[B].IsEmpty()) continue;
        Joined.Add(Pair(A, B));
        Add(Stats, Observe(Items[B], Items[A], Meshes[B], Meshes[A], Relation.Kind)); // what rests on the anchor, or beside it
        if (FCString::Strcmp(Relation.Kind, TEXT("nextTo")) == 0) Add(Stats, Observe(Items[A], Items[B], Meshes[A], Meshes[B], Relation.Kind));
    }
    for (int32 A = 0; A < Items.Num(); ++A)
    {
        for (int32 B = A + 1; B < Items.Num() && !Meshes[A].IsEmpty(); ++B)
        {
            if (Meshes[B].IsEmpty() || Joined.Contains(Pair(A, B)) ||
                FVector2D(Items[A].Box.GetCenter() - Items[B].Box.GetCenter()).Size() > NearCm)
            {
                continue;
            }
            Add(Stats, Observe(Items[A], Items[B], Meshes[A], Meshes[B], TEXT("near")));
            Add(Stats, Observe(Items[B], Items[A], Meshes[B], Meshes[A], TEXT("near")));
        }
    }
    const FString Level = World->GetOutermost()->GetName();
    TSharedPtr<FJsonObject> Levels = LoadLevels();
    Levels->SetObjectField(Level, ToJson(Stats));
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("version"), 1);
    Root->SetObjectField(TEXT("levels"), Levels);
    FString Text;
    FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(StatsFile()), true);
    if (!FFileHelper::SaveStringToFile(Text, *StatsFile()))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not write Saved/EngineWire/LayoutStats.json."), TEXT("WRITE_FAILED"));
        return true;
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("level"), Level);
    Result->SetNumberField(TEXT("actorsRead"), Items.Num());
    Result->SetNumberField(TEXT("meshes"), Stats.Copies.Num());
    Result->SetNumberField(TEXT("pairs"), Stats.Pairs.Num());
    Result->SetNumberField(TEXT("levelsLearned"), Levels->Values.Num());
    if (InScope > Items.Num()) Result->SetNumberField(TEXT("actorsInLevel"), InScope);
    Bridge->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Learned %d mesh pairing(s) among %d mesh(es) of %s; %d level(s) learned in all."),
                        Stats.Pairs.Num(), Stats.Copies.Num(), *Level, Levels->Values.Num()), Result);
    return true;
}

bool HandleSuggest(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                   TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find)
{
    using namespace HandlerDetail;
    const FString Name = GetJsonStringField(Payload, TEXT("anchor"));
    AActor* Anchor = Name.IsEmpty() ? nullptr : Find(Name);
    const FString Mesh = MeshOf(Anchor);
    if (Mesh.IsEmpty())
    {
        Bridge->SendAutomationError(Socket, RequestId, Anchor ? FString::Printf(TEXT("'%s' is not a static mesh actor."), *Name)
            : FString::Printf(TEXT("No actor '%s' in the open level."), *Name), Anchor ? TEXT("INVALID_ARGUMENT") : TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    TArray<FLevelStats> Levels;
    const TSharedPtr<FJsonObject> Kept = LoadLevels(); // held: the loop must not outlive the object it walks
    for (const auto& Each : Kept->Values) Levels.Add(FromJson(Each.Value->AsObject()));
    if (Levels.Num() == 0)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("No level has been learned yet: open a dressed level and call "
            "control_actor learn_layout_style first."), TEXT("NO_LAYOUT_STATS"));
        return true;
    }
    const FBox Box = McpPlacement::DrawnBounds(*Anchor);
    const double AnchorYaw = Anchor->GetActorRotation().Yaw;
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const FSuggestion& Each : Suggest(Levels, Mesh, FMath::Clamp(GetJsonIntField(Payload, TEXT("count"), 3), 1, 10)))
    {
        const FVector Direction = FRotator(0.0, AnchorYaw + Each.Bearing, 0.0).Vector();
        const bool bOn = Each.Kind == TEXT("on");
        const FVector At(Box.GetCenter().X + Direction.X * (bOn ? 0.0 : Each.Distance),
                         Box.GetCenter().Y + Direction.Y * (bOn ? 0.0 : Each.Distance), bOn ? Box.Max.Z : Box.Min.Z);
        TSharedPtr<FJsonObject> Spawn = MakeShared<FJsonObject>();
        Spawn->SetStringField(TEXT("meshPath"), Each.Mesh);
        Spawn->SetArrayField(TEXT("location"), {MakeShared<FJsonValueNumber>(FMath::RoundToDouble(At.X)),
            MakeShared<FJsonValueNumber>(FMath::RoundToDouble(At.Y)), MakeShared<FJsonValueNumber>(FMath::RoundToDouble(At.Z))});
        Spawn->SetArrayField(TEXT("rotation"), {MakeShared<FJsonValueNumber>(0.0),
            MakeShared<FJsonValueNumber>(FMath::RoundToDouble(AnchorYaw + Each.Yaw)), MakeShared<FJsonValueNumber>(0.0)});
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("mesh"), Each.Mesh);
        Row->SetStringField(TEXT("kind"), Each.Kind);
        Row->SetNumberField(TEXT("perAnchor"), FMath::RoundToDouble(Each.PerAnchor * 100.0) / 100.0);
        Row->SetNumberField(TEXT("distance"), Each.Distance);
        Row->SetNumberField(TEXT("bearing"), Each.Bearing);
        Row->SetNumberField(TEXT("yaw"), Each.Yaw);
        Row->SetObjectField(TEXT("nextCall"), McpHandlerUtils::MakeExecuteCall(TEXT("control_actor"), TEXT("spawn"), Spawn));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("anchorMesh"), Mesh);
    Result->SetArrayField(TEXT("suggestions"), Rows);
    Result->SetNumberField(TEXT("levelsLearned"), Levels.Num());
    Bridge->SendAutomationResponse(Socket, RequestId, true, Rows.Num() == 0
        ? FString::Printf(TEXT("Nothing goes with %s in the %d level(s) learned."), *Mesh, Levels.Num())
        : FString::Printf(TEXT("%d mesh(es) go with %s in the levels learned; each row's nextCall places one."), Rows.Num(), *Mesh), Result);
    return true;
}
} // namespace McpLayoutStyle
