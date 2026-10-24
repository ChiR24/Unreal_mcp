// Copyright (c) 2024 EngineWire Contributors
//
// control_actor describe_relations (McpRelations.h). The scene judged is every visible actor with bounds in the editor
// world, or in the running Play world: within radius of center when given, at most maxActors of them (a named actor
// first, then the nearest the center, else the largest). Lights, volumes and other bounds-only actors are no items; the
// roles read them. actorNames and nameFilter narrow what is reported, never the scene judged, so a lamp still rests on
// a table nobody named. What rests on the ground is reported only for an actor asked about; roles.ground counts it.

#include "Domains/ControlActor/Relations/McpRelations.h"

#include "Components/StaticMeshComponent.h"
#include "Domains/ControlActor/McpActorSupport.h"
#include "Domains/ControlActor/Placement/McpPlacementMount.h"
#include "Engine/StaticMesh.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

namespace McpRelations
{
namespace
{
constexpr int32 MaxRelations = 200;
constexpr int32 MaxMembers = 24;
const TCHAR* const AllKinds[] = {TEXT("on"),     TEXT("mounted"), TEXT("inside"), TEXT("nextTo"), TEXT("faces"),
                                 TEXT("groups"), TEXT("rows"),    TEXT("rings"),  TEXT("roles")};

FString ShapeOf(const AActor* Actor, const FBox& Box)
{
    const UStaticMeshComponent* Root = Cast<UStaticMeshComponent>(Actor->GetRootComponent());
    const FVector Size = Box.GetSize() / 5.0;
    return FString::Printf(TEXT("%s %s %.0f %.0f %.0f"), *Actor->GetClass()->GetName(),
                           Root && Root->GetStaticMesh() ? *Root->GetStaticMesh()->GetName() : TEXT("-"),
                           FMath::Max(Size.X, Size.Y), FMath::Min(Size.X, Size.Y), Size.Z);
}
} // namespace

// The items, named as replies name actors (the label, or the object name when another actor shares the label).
TArray<FItem> CollectItems(UWorld* World, const TSet<AActor*>& Named, bool bArea, const FVector& Center, double Radius,
                           int32 MaxActors, int32& OutInScope)
{
    TMap<FString, int32> Labels;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        ++Labels.FindOrAdd(It->GetActorLabel().ToLower());
    }
    TArray<FItem> Items;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        const FBox Box = Actor->IsHidden() || McpPlacement::IsBoundsOnlyActor(Actor) ? FBox(ForceInit) : McpPlacement::DrawnBounds(*Actor);
        if (!Box.IsValid || Box.GetExtent().GetMax() < 1.0 ||
            (bArea && !Named.Contains(Actor) && Box.ComputeSquaredDistanceToPoint(Center) > Radius * Radius))
        {
            continue;
        }
        FItem& Item = Items.AddDefaulted_GetRef();
        Item.Actor = Actor;
        Item.Name = Labels.FindRef(Actor->GetActorLabel().ToLower()) > 1 ? Actor->GetName() : Actor->GetActorLabel();
        Item.Box = Box;
        Item.bGround = McpPlacement::McpIsGroundLike(Actor);
        Item.Shape = ShapeOf(Actor, Box);
        Item.Print = DrawnFootprint(*Actor);
    }
    OutInScope = Items.Num();
    if (Items.Num() > MaxActors)
    {
        Items.StableSort([&Named, bArea, &Center](const FItem& A, const FItem& B) {
            const bool bNamedA = Named.Contains(A.Actor);
            if (bNamedA != Named.Contains(B.Actor))
            {
                return bNamedA;
            }
            return bArea ? A.Box.ComputeSquaredDistanceToPoint(Center) < B.Box.ComputeSquaredDistanceToPoint(Center)
                         : A.Box.GetVolume() > B.Box.GetVolume();
        });
        Items.SetNum(MaxActors);
    }
    return Items;
}

namespace
{
FString ReadKinds(const TSharedPtr<FJsonObject>& Payload, TSet<FString>& OutKinds)
{
    for (const FString& Kind : McpHandlerUtils::GetStringArrayField(Payload, TEXT("kinds")))
    {
        const TCHAR* Known = nullptr;
        for (const TCHAR* Name : AllKinds)
        {
            Known = Kind.Equals(Name, ESearchCase::IgnoreCase) ? Name : Known;
        }
        if (!Known)
        {
            return FString::Printf(TEXT("kinds takes on, mounted, inside, nextTo, faces, groups, rows, rings and roles; '%s' is "
                                        "none of them."), *Kind);
        }
        OutKinds.Add(Known);
    }
    return FString();
}

// The patterns with a member asked about, each naming at most MaxMembers of them: a grid of 400 copies listed all 400.
TArray<TSharedPtr<FJsonValue>> Asked(const TArray<TSharedPtr<FJsonValue>>& Patterns, const TSet<FString>* Names)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    for (const TSharedPtr<FJsonValue>& Pattern : Patterns)
    {
        const TSharedPtr<FJsonObject> Row = Pattern->AsObject();
        const TArray<TSharedPtr<FJsonValue>>* Listed = nullptr;
        if (!Row->TryGetArrayField(TEXT("members"), Listed) ||
            (Names && !Listed->ContainsByPredicate([Names](const TSharedPtr<FJsonValue>& Member) { return Names->Contains(Member->AsString()); })))
        {
            continue;
        }
        if (Listed->Num() > MaxMembers)
        {
            const TArray<TSharedPtr<FJsonValue>> First(Listed->GetData(), MaxMembers);
            Row->SetNumberField(TEXT("memberCount"), Listed->Num());
            Row->SetArrayField(TEXT("members"), First);
        }
        Out.Add(Pattern);
    }
    return Out;
}
} // namespace

bool HandleDescribeRelations(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                             TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find)
{
    UWorld* World = GEditor ? (GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World()) : nullptr;
    if (!World)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("No editor world is open."), TEXT("NO_WORLD"));
        return true;
    }
    TSet<FString> Kinds;
    FString Refusal = ReadKinds(Payload, Kinds);
    double Radius = 0.0;
    const bool bArea = Payload->TryGetNumberField(TEXT("radius"), Radius);
    if (bArea != Payload->HasField(TEXT("center")) || (bArea && Radius <= 0.0))
    {
        Refusal = TEXT("center and radius go together: the area within radius cm (above 0) of center.");
    }
    if (!Refusal.IsEmpty())
    {
        Bridge->SendAutomationError(Socket, RequestId, Refusal, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    TArray<AActor*> NamedList;
    TArray<FString> Missing;
    const bool bNamed = McpResolveActorNames(Payload, Find, NamedList, Missing);
    if (bNamed && NamedList.Num() == 0)
    {
        McpSendNoActorNamesFound(Bridge, Socket, RequestId, Missing);
        return true;
    }
    FString NameFilter;
    Payload->TryGetStringField(TEXT("nameFilter"), NameFilter);
    double MaxActors = 300.0;
    Payload->TryGetNumberField(TEXT("maxActors"), MaxActors);
    const TSet<AActor*> Named(NamedList);
    int32 InScope = 0;
    const TArray<FItem> Items = CollectItems(World, Named, bArea, ExtractVectorField(Payload, TEXT("center"), FVector::ZeroVector),
                                             Radius, FMath::Clamp(static_cast<int32>(MaxActors), 1, 400), InScope);
    const bool bFocused = bNamed || !NameFilter.IsEmpty();
    TSet<FString> AskedNames;
    TArray<bool> IsAsked;
    for (const FItem& Item : Items)
    {
        const bool bAsked = (!bNamed || Named.Contains(Item.Actor)) &&
            (NameFilter.IsEmpty() || Item.Actor->GetActorLabel().Contains(NameFilter) || Item.Actor->GetName().Contains(NameFilter));
        IsAsked.Add(bAsked);
        if (bAsked)
        {
            AskedNames.Add(Item.Name);
        }
    }
    // faces would name a front for most props of a level (a crate faces whatever lies along its X axis), burying the
    // rest: it is listed when kinds names it or the report is narrowed to the actors asked about.
    if (Kinds.Num() == 0)
    {
        for (const TCHAR* Kind : AllKinds)
        {
            if (bFocused || FCString::Strcmp(Kind, TEXT("faces")) != 0)
            {
                Kinds.Add(Kind);
            }
        }
    }
    const auto Wants = [&Kinds](const TCHAR* Kind) { return Kinds.Contains(Kind); };
    TSet<FString> PairKinds = Kinds;
    PairKinds.Add(TEXT("on")); // the ground's role counts what rests on it
    TArray<FRelation> Relations;
    FindPairRelations(World, Items, PairKinds, Relations);
    Relations.StableSort([](const FRelation& A, const FRelation& B) { return A.Subject < B.Subject; });
    TArray<TSharedPtr<FJsonValue>> Rows;
    TMap<FString, int32> Counts;
    int32 Shown = 0;
    for (const FRelation& Relation : Relations)
    {
        if (!Wants(Relation.Kind) || !(IsAsked[Relation.Subject] || IsAsked[Relation.Object]) ||
            (Items[Relation.Object].bGround && !(bFocused && IsAsked[Relation.Subject])))
        {
            continue;
        }
        ++Shown;
        ++Counts.FindOrAdd(FString(Relation.Kind));
        TArray<TSharedPtr<FJsonValue>> Row = {MakeShared<FJsonValueString>(Items[Relation.Subject].Name),
                                              MakeShared<FJsonValueString>(Relation.Kind),
                                              MakeShared<FJsonValueString>(Items[Relation.Object].Name)};
        if (!Relation.Detail.IsEmpty())
        {
            Row.Add(MakeShared<FJsonValueString>(Relation.Detail));
        }
        if (Rows.Num() < MaxRelations)
        {
            Rows.Add(MakeShared<FJsonValueArray>(Row));
        }
    }
    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    TArray<FString> Said;
    const auto Put = [&Data, &Said](const TCHAR* Field, const TArray<TSharedPtr<FJsonValue>>& List, int32 Count) {
        if (List.Num() > 0)
        {
            Data->SetArrayField(Field, List);
            Said.Add(FString::Printf(TEXT("%s %d"), Field, Count));
        }
    };
    Put(TEXT("relations"), Rows, Shown);
    TArray<FString> ByKind;
    for (const TCHAR* Kind : AllKinds)
    {
        if (const int32* Count = Counts.Find(Kind))
        {
            ByKind.Add(FString::Printf(TEXT("%s %d"), Kind, *Count));
        }
    }
    if (ByKind.Num() > 0)
    {
        Said.Last() += FString::Printf(TEXT(" (%s)"), *FString::Join(ByKind, TEXT(", ")));
    }
    const FPatterns Patterns = FindPatterns(Items, Kinds);
    const TSet<FString>* Focus = bFocused ? &AskedNames : nullptr;
    const auto PutAsked = [&Put, Focus](const TCHAR* Field, const TArray<TSharedPtr<FJsonValue>>& List) {
        const TArray<TSharedPtr<FJsonValue>> Kept = Asked(List, Focus);
        Put(Field, Kept, Kept.Num());
    };
    PutAsked(TEXT("groups"), Patterns.Groups);
    PutAsked(TEXT("rows"), Patterns.Rows);
    PutAsked(TEXT("grids"), Patterns.Grids);
    PutAsked(TEXT("rings"), Patterns.Rings);
    if (Wants(TEXT("roles")))
    {
        Data->SetObjectField(TEXT("roles"), FindRoles(World, Items, Relations));
    }
    Data->SetNumberField(TEXT("actorsExamined"), Items.Num());
    if (InScope > Items.Num())
    {
        Data->SetBoolField(TEXT("truncated"), true);
        Data->SetNumberField(TEXT("actorsInScope"), InScope);
    }
    if (Shown > Rows.Num())
    {
        Data->SetBoolField(TEXT("truncated"), true);
        Data->SetNumberField(TEXT("relationCount"), Shown);
    }
    if (Missing.Num() > 0)
    {
        Data->SetArrayField(TEXT("missing"), McpHandlerUtils::ToJsonStringArray(Missing));
    }
    const FString Found = Said.Num() > 0 ? FString::Join(Said, TEXT(", ")) : FString(TEXT("no relation or pattern"));
    Bridge->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Among %d actors: %s."), Items.Num(), *Found),
                                   Data, FString());
    return true;
}
} // namespace McpRelations
