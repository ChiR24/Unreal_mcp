#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Debug/DebugDrawComponent.h"

// control_actor.list, moved out of the lookup file when near and radius made it two passes: the actors that pass
// the filters (and the radius) are collected and, with near, sorted by distance, then paged and described.
namespace {
struct FMcpListedActor {
  AActor *Actor = nullptr;
  double Distance = 0.0;
  double BoundsSize = 0.0;
};

// How far the point is from the actor's world bounding box: 0 when the box contains it, so a big slab
// under the point is at distance 0. The box is GetActorBounds' without debug-draw components: the gameplay
// debugger's renderer claims a million-unit box round the origin, which put its replicator at distance 0
// from every point in PIE. An actor with no bounds (a camera or target point in PIE, whose sprites are
// editor-only) is measured to its location, not to the world origin. OutBoundsSize is the box's
// half-diagonal, which orders equally near actors: of those containing the point, the smallest is the
// thing at that spot, and the foliage actor, whose box covers the level, comes last.
double McpDistanceToActorBounds(const AActor *Actor, const FVector &Point, double &OutBoundsSize) {
  FBox Box(ForceInit);
  Actor->ForEachComponent<UPrimitiveComponent>(false, [&Box](const UPrimitiveComponent *Primitive) {
    if (Primitive->IsRegistered() && !Primitive->IsA<UDebugDrawComponent>())
      Box += Primitive->Bounds.GetBox();
  });
  if (!Box.IsValid)
    Box = FBox(Actor->GetActorLocation(), Actor->GetActorLocation());
  FVector Origin, Extent;
  Box.GetCenterAndExtents(Origin, Extent);
  OutBoundsSize = Extent.Size();
  const FVector Outside = (Point - Origin).GetAbs() - Extent;
  return FVector(FMath::Max(Outside.X, 0.0), FMath::Max(Outside.Y, 0.0), FMath::Max(Outside.Z, 0.0)).Size();
}
} // namespace

bool UMcpAutomationBridgeSubsystem::HandleControlActorList(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  FString Filter, TagFilter, ClassFilter, FolderFilter;
  Payload->TryGetStringField(TEXT("filter"), Filter);
  Payload->TryGetStringField(TEXT("tag"), TagFilter);
  Payload->TryGetStringField(TEXT("className"), ClassFilter);
  Payload->TryGetStringField(TEXT("folder"), FolderFilter);

  double LimitValue = 0.0;
  Payload->TryGetNumberField(TEXT("limit"), LimitValue);
  // Default page size: a bare list of a big level used to return every actor (dogfood #18).
  const int32 Limit = LimitValue > 0.0
      ? FMath::Max(1, static_cast<int32>(LimitValue))
      : 100;
  // The next page: the reply said hasMore with no way to ask for the rest, so
  // the 90th stair of a staircase could not be listed at all.
  double OffsetValue = 0.0;
  Payload->TryGetNumberField(TEXT("offset"), OffsetValue);
  const int32 Offset = FMath::Max(0, static_cast<int32>(OffsetValue));
  // Variable values across many actors in one call (which ? blocks hold what took one inspect per block).
  // A name is an actor property, or "Component.Property" for one of its components (StaticMeshComponent.LDMaxDrawDistance).
  TArray<FString> PropertyNames;
  const TArray<TSharedPtr<FJsonValue>> *PropertyNamesArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("propertyNames"), PropertyNamesArray)) {
    for (const TSharedPtr<FJsonValue> &Value : *PropertyNamesArray) {
      if (Value.IsValid() && Value->Type == EJson::String)
        PropertyNames.AddUnique(Value->AsString());
    }
  }
  // near ({x,y,z} or [x,y,z]) and radius: only the actors whose bounds come within radius of the point, nearest
  // first; near alone sorts every matching actor by distance. Each row then carries its distance. It composes with
  // every filter above. An agent that sees something in a screenshot can ask what is at that spot.
  bool bNear = false;
  FVector Near = FVector::ZeroVector;
  if (Payload->HasField(TEXT("near"))) {
    static const TCHAR *const Axes[3] = {TEXT("x"), TEXT("y"), TEXT("z")};
    double Point[3] = {0.0, 0.0, 0.0};
    if (!ReadJsonTriple(Payload->TryGetField(TEXT("near")), Axes, Point)) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                                TEXT("near must be a world point as [x, y, z] or {x, y, z}"), nullptr);
      return true;
    }
    Near = FVector(Point[0], Point[1], Point[2]);
    bNear = true;
  }
  double Radius = 0.0;
  const bool bRadius = Payload->TryGetNumberField(TEXT("radius"), Radius);
  if (bRadius && (!bNear || Radius < 0.0)) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              bNear ? TEXT("radius is a distance in world units and cannot be negative")
                                    : TEXT("radius is the distance from the point near names: pass near too"),
                              nullptr);
    return true;
  }

  TArray<AActor *> AllActors;
  UWorld *SourceWorld = nullptr;
  bool bUsingPieWorld = false;

  if (GEditor->PlayWorld) {
    SourceWorld = GEditor->PlayWorld.Get();
    bUsingPieWorld = true;
    if (!SourceWorld) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("WORLD_NOT_FOUND"),
                                TEXT("PIE world unavailable"), nullptr);
      return true;
    }

    for (TActorIterator<AActor> It(SourceWorld); It; ++It) {
      AllActors.Add(*It);
    }
  } else {
    SourceWorld = GEditor->GetEditorWorldContext().World();
    UEditorActorSubsystem *ActorSS =
        GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("SUBSYSTEM_MISSING"),
                                TEXT("EditorActorSubsystem unavailable"), nullptr);
      return true;
    }

    AllActors = ActorSS->GetAllLevelActors();
  }

  // GetAllLevelActors deliberately hides templates, transient actors, the
  // builder brush and WorldSettings, so this list is always shorter than the
  // actorCount get_editor_state reports for the same world. Reporting the gap
  // turns what read as a contradiction into an explanation.
  int32 WorldActorCount = 0;
  if (SourceWorld)
  {
    for (TActorIterator<AActor> It(SourceWorld); It; ++It) { ++WorldActorCount; }
  }

  // First pass: the actors that pass every filter and the radius.
  TArray<FMcpListedActor> Listed;
  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    if (!Filter.IsEmpty() &&
        !Actor->GetActorLabel().Contains(Filter, ESearchCase::IgnoreCase) &&
        !Actor->GetName().Contains(Filter, ESearchCase::IgnoreCase))
      continue;
    if (!McpActorMatchesListFilters(Actor, TagFilter, ClassFilter, FolderFilter))
      continue;
    double BoundsSize = 0.0;
    const double Distance = bNear ? McpDistanceToActorBounds(Actor, Near, BoundsSize) : 0.0;
    if (bRadius && Distance > Radius)
      continue;
    Listed.Add({Actor, Distance, BoundsSize});
  }
  if (bNear) {
    // Nearest first; equally near, the smaller box first; then by path, so the order never depends on
    // the level's actor order.
    Listed.Sort([](const FMcpListedActor &A, const FMcpListedActor &B) {
      if (A.Distance != B.Distance) return A.Distance < B.Distance;
      if (A.BoundsSize != B.BoundsSize) return A.BoundsSize < B.BoundsSize;
      return A.Actor->GetPathName().Compare(B.Actor->GetPathName()) < 0;
    });
  }

  TArray<TSharedPtr<FJsonValue>> ActorsArray;
  int32 TotalCount = 0;
  // summary: counts by class, tag and outliner folder instead of one row per
  // actor. Learning what a 300-actor level is made of used to take a page of
  // transforms per 100 actors, or a reply too large to return at all.
  bool bSummary = false;
  Payload->TryGetBoolField(TEXT("summary"), bSummary);
  TMap<FString, int32> ByClass, ByTag, ByFolder;

  // Second pass: count, page and describe.
  for (const FMcpListedActor &Item : Listed) {
    AActor *Actor = Item.Actor;
    ++TotalCount;
    if (bSummary) {
      ++ByClass.FindOrAdd(Actor->GetClass()->GetName());
      for (const FName &Tag : Actor->Tags)
        ++ByTag.FindOrAdd(Tag.ToString());
      ++ByFolder.FindOrAdd(McpActorFolder(Actor));
      continue;
    }

    if (TotalCount <= Offset || (Limit > 0 && ActorsArray.Num() >= Limit))
      continue;

    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("label"), Actor->GetActorLabel());
    Entry->SetStringField(TEXT("name"), Actor->GetName());
    Entry->SetStringField(TEXT("path"), Actor->GetPathName());
    Entry->SetStringField(TEXT("class"), Actor->GetClass()
                                             ? Actor->GetClass()->GetPathName()
                                             : TEXT(""));
    if (bNear)
      Entry->SetNumberField(TEXT("distance"), FMath::RoundToDouble(Item.Distance * 10.0) / 10.0);
    // The layout in one call: without these, reading N placements took N get_transform calls.
    if (Actor->GetRootComponent()) {
      const FTransform Transform = Actor->GetActorTransform();
      Entry->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(Transform.GetLocation()));
      Entry->SetObjectField(TEXT("rotation"), McpHandlerUtils::RotatorToJson(Transform.Rotator()));
      Entry->SetObjectField(TEXT("scale"), McpHandlerUtils::VectorToJson(Transform.GetScale3D()));
    }
    if (PropertyNames.Num() > 0) {
      TSharedPtr<FJsonObject> Properties = McpHandlerUtils::CreateResultObject();
      // A name this actor's class lacks used to vanish from the reply, which
      // read exactly like "that property is empty". A component's property is keyed
      // as it was asked for ("Component.Property"), an actor's by its own name.
      TArray<TSharedPtr<FJsonValue>> Missing;
      for (const FString &Wanted : PropertyNames) {
        UObject *Owner = nullptr;
        if (FProperty *Property = McpResolveActorPropertyPath(Actor, Wanted, Owner)) {
          Properties->SetStringField(Wanted.Contains(TEXT(".")) ? Wanted : Property->GetName(),
                                     McpPropertyReflection::GetPropertyValueAsString(Owner, Property));
          continue;
        }
        // A struct member of the actor itself (a post process volume's Settings.BloomIntensity) or a deeper
        // path read as missing here while get_property found it: fall back to the resolver get_property uses.
        void *Container = nullptr;
        FString Resolved, Error, Value;
        if (FProperty *Nested = McpResolvePropertyPath(Actor, Wanted, Container, Resolved, Error)) {
          MCP_PROPERTY_EXPORT_TEXT(Nested, Value, Nested->ContainerPtrToValuePtr<void>(Container), nullptr, nullptr, PPF_None);
          Properties->SetStringField(Wanted, Value);
        } else {
          Missing.Add(MakeShared<FJsonValueString>(Wanted));
        }
      }
      Entry->SetObjectField(TEXT("properties"), Properties);
      if (Missing.Num() > 0)
        Entry->SetArrayField(TEXT("missingProperties"), Missing);
    }
    ActorsArray.Add(MakeShared<FJsonValueObject>(Entry));
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetArrayField(TEXT("actors"), ActorsArray);
  Data->SetNumberField(TEXT("count"), ActorsArray.Num());
  Data->SetNumberField(TEXT("totalCount"), TotalCount);
  Data->SetNumberField(TEXT("excludedCount"), FMath::Max(0, WorldActorCount - AllActors.Num()));
  Data->SetNumberField(TEXT("limit"), Limit);
  Data->SetNumberField(TEXT("offset"), Offset);
  if (bSummary) {
    // [{name, count}], never {name: count}: receipt redaction reads a JSON key
    // as a field name, so a folder, tag or class named like a credential
    // ("Level/Stage/Secrets", a "Token" pickup) lost its count to [REDACTED].
    auto CountsToJson = [](TMap<FString, int32> &Counts) {
      Counts.KeySort(TLess<FString>());
      TArray<TSharedPtr<FJsonValue>> Out;
      for (const TPair<FString, int32> &Pair : Counts) {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Pair.Key.IsEmpty() ? TEXT("(none)") : Pair.Key);
        Row->SetNumberField(TEXT("count"), Pair.Value);
        Out.Add(MakeShared<FJsonValueObject>(Row));
      }
      return Out;
    };
    Data->SetArrayField(TEXT("byClass"), CountsToJson(ByClass));
    Data->SetArrayField(TEXT("byTag"), CountsToJson(ByTag));
    Data->SetArrayField(TEXT("byFolder"), CountsToJson(ByFolder));
  }
  const bool bHasMore = !bSummary && TotalCount > Offset + ActorsArray.Num();
  Data->SetBoolField(TEXT("hasMore"), bHasMore);
  if (bHasMore)
    Data->SetNumberField(TEXT("nextOffset"), Offset + ActorsArray.Num());
  Data->SetBoolField(TEXT("isPieWorld"), bUsingPieWorld);
  if (SourceWorld)
    Data->SetStringField(TEXT("worldName"), SourceWorld->GetName());
  if (!Filter.IsEmpty())
    Data->SetStringField(TEXT("filter"), Filter);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Actors listed"), Data);
  return true;
}
