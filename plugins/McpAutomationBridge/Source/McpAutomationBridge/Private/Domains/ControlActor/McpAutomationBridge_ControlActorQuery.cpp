#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByName(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Query;
  Payload->TryGetStringField(TEXT("name"), Query);
  if (Query.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("name required"), nullptr);
    return true;
  }

  // Security: Validate query format - reject path traversal attempts
  if (Query.Contains(TEXT("..")) || Query.Contains(TEXT("\\"))) {
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
        FString::Printf(
            TEXT("Invalid name query: '%s'. '..' and backslashes are not accepted. Pass an actor "
                 "name (e.g. 'BP_Shelf_C_1') or a full object path ending in one "
                 "(e.g. '/Game/Maps/Lvl.Lvl:PersistentLevel.BP_Shelf_C_1')."),
            *Query),
        nullptr);
    return true;
  }

  // A caller that already holds an actor's FULL OBJECT PATH (handed out by find_by_property, inspect, and the
  // outliner) naturally passes it here. Rejecting every '/' turned that into a bare "Invalid name query" with
  // no hint about what this action actually wants, so reduce a well-formed object path to its leaf actor name
  // instead: "/Game/Maps/Lvl.Lvl:PersistentLevel.BP_Shelf_C_1" -> "BP_Shelf_C_1". Traversal and backslashes
  // are already rejected above, and the leaf can no longer contain a separator.
  if (Query.Contains(TEXT("/"))) {
    // Take the LAST separator of any kind. A short-circuiting || chain stops at the first character it
    // finds, so '/Game/My.Folder/Actors' would cut at the '.' and yield 'Folder/Actors' -- still a path,
    // contradicting the guarantee below. Compare all three and use the rightmost.
    FString Leaf = Query;
    int32 DotIndex = INDEX_NONE, ColonIndex = INDEX_NONE, SlashIndex = INDEX_NONE;
    Leaf.FindLastChar(TEXT('.'), DotIndex);
    Leaf.FindLastChar(TEXT(':'), ColonIndex);
    Leaf.FindLastChar(TEXT('/'), SlashIndex);
    const int32 SeparatorIndex = FMath::Max3(DotIndex, ColonIndex, SlashIndex);
    if (SeparatorIndex != INDEX_NONE) {
      Leaf = Leaf.RightChop(SeparatorIndex + 1);
    }
    Leaf.TrimStartAndEndInline();
    if (Leaf.IsEmpty() || Leaf.Contains(TEXT("/"))) {
      SendStandardErrorResponse(
          this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
          FString::Printf(
              TEXT("Invalid name query: '%s'. Pass an actor name (e.g. 'BP_Shelf_C_1') or a full object "
                   "path ending in one (e.g. '/Game/Maps/Lvl.Lvl:PersistentLevel.BP_Shelf_C_1')."),
              *Query),
          nullptr);
      return true;
    }
    Query = Leaf;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  const TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  TArray<TSharedPtr<FJsonValue>> Matches;
  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    const FString Label = Actor->GetActorLabel();
    const FString Name = Actor->GetName();
    const FString Path = Actor->GetPathName();
    const bool bMatches = Label.Contains(Query, ESearchCase::IgnoreCase) ||
                          Name.Contains(Query, ESearchCase::IgnoreCase) ||
                          Path.Contains(Query, ESearchCase::IgnoreCase);
    if (bMatches) {
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetStringField(TEXT("label"), Label);
      Entry->SetStringField(TEXT("name"), Name);
      Entry->SetStringField(TEXT("path"), Path);
      Entry->SetStringField(TEXT("class"),
                            Actor->GetClass() ? Actor->GetClass()->GetPathName()
                                              : TEXT(""));
      Matches.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetNumberField(TEXT("count"), Matches.Num());
  Data->SetArrayField(TEXT("actors"), Matches);
  Data->SetStringField(TEXT("query"), Query);
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Actor query executed"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetBoundingBox(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  FVector Origin, BoxExtent;
  Found->GetActorBounds(false, Origin, BoxExtent);

  auto BuildLandscapeBox = [](const FTransform& Transform, double MinX,
                              double MinY, double MaxX, double MaxY,
                              double MinZ, double MaxZ) {
    FBox LandscapeBox(EForceInit::ForceInit);
    const FVector Corners[] = {
        FVector(MinX, MinY, MinZ), FVector(MinX, MaxY, MinZ),
        FVector(MaxX, MinY, MinZ), FVector(MaxX, MaxY, MinZ),
        FVector(MinX, MinY, MaxZ), FVector(MinX, MaxY, MaxZ),
        FVector(MaxX, MinY, MaxZ), FVector(MaxX, MaxY, MaxZ),
    };
    for (const FVector& Corner : Corners) {
      LandscapeBox += Transform.TransformPosition(Corner);
    }
    return LandscapeBox;
  };

  if (ALandscape* Landscape = Cast<ALandscape>(Found);
      Landscape && BoxExtent.IsNearlyZero()) {
    ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
    int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    if (LandscapeInfo && LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY)) {
      const FBox LandscapeBox = BuildLandscapeBox(
          Landscape->GetTransform(), MinX, MinY, MaxX, MaxY, -256.0, 256.0);
      Origin = LandscapeBox.GetCenter();
      BoxExtent = LandscapeBox.GetExtent();
    }

    if (BoxExtent.IsNearlyZero()) {
      const FBox CompleteBounds = Landscape->GetCompleteBounds();
      if (CompleteBounds.IsValid) {
        Origin = CompleteBounds.GetCenter();
        BoxExtent = CompleteBounds.GetExtent();
      }
    }

    if (BoxExtent.IsNearlyZero()) {
      FMcpLandscapeMetadata Metadata;
      if (McpLandscapeMetadataTags::DecodeLandscapeMetadata(Landscape, Metadata)) {
        const double MaxXFromMetadata = Metadata.ComponentsX * Metadata.QuadsPerComponent;
        const double MaxYFromMetadata = Metadata.ComponentsY * Metadata.QuadsPerComponent;
        const FBox LandscapeBox = BuildLandscapeBox(
            Landscape->GetTransform(), 0.0, 0.0, MaxXFromMetadata,
            MaxYFromMetadata, -256.0, 256.0);
        Origin = LandscapeBox.GetCenter();
        BoxExtent = LandscapeBox.GetExtent();
      }
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();

  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };

  Data->SetArrayField(TEXT("origin"), MakeArray(Origin));
  Data->SetArrayField(TEXT("extent"), MakeArray(BoxExtent));
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Bounding box retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("name"), Found->GetName());
  Data->SetStringField(TEXT("label"), Found->GetActorLabel());
  Data->SetStringField(TEXT("path"), Found->GetPathName());
  Data->SetStringField(TEXT("class"), Found->GetClass()
                                          ? Found->GetClass()->GetPathName()
                                          : TEXT(""));

  TArray<TSharedPtr<FJsonValue>> TagsArray;
  for (const FName &Tag : Found->Tags) {
    TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
  }
  Data->SetArrayField(TEXT("tags"), TagsArray);

  const FTransform Current = Found->GetActorTransform();
  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };
  Data->SetArrayField(TEXT("location"), MakeArray(Current.GetLocation()));

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Metadata retrieved"), Data);
  return true;
#else
  return false;
#endif
}
