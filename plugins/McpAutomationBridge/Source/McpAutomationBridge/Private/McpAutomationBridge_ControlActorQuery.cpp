#include "McpAutomationBridge_ControlActorSupport.h"

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
  if (Query.Contains(TEXT("..")) || Query.Contains(TEXT("\\")) || Query.Contains(TEXT("/"))) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              FString::Printf(TEXT("Invalid name query: '%s'. Path separators and traversal characters are not allowed."), *Query), nullptr);
    return true;
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


bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByClass(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ClassName;
  Payload->TryGetStringField(TEXT("className"), ClassName);
  if (ClassName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("class"), ClassName);
  }
  if (ClassName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("classPath"), ClassName);
  }

  if (ClassName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("className, class, or classPath is required"), nullptr);
    return true;
  }

  // Security: Validate class name format - reject path traversal attempts
  // Valid formats: "/Script/Module.ClassName", "/Game/Path/ClassName.ClassName", "ClassName"
  // Invalid: Contains "..", "\" (Windows paths), or other traversal patterns
  if (ClassName.Contains(TEXT("..")) || ClassName.Contains(TEXT("\\"))) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              FString::Printf(TEXT("Invalid class name format: '%s'. Path traversal characters are not allowed."), *ClassName), nullptr);
    return true;
  }

  // Additional security: Reject absolute filesystem paths
  if (ClassName.StartsWith(TEXT("/")) && !ClassName.StartsWith(TEXT("/Script/")) &&
      !ClassName.StartsWith(TEXT("/Game/")) && !ClassName.StartsWith(TEXT("/Engine/"))) {
    // Could be a path traversal attempt disguised as a valid path
    if (ClassName.Contains(TEXT("/etc/")) || ClassName.Contains(TEXT("/usr/")) ||
        ClassName.Contains(TEXT("/var/")) || ClassName.Contains(TEXT("/home/")) ||
        ClassName.Contains(TEXT("/root/")) || ClassName.Contains(TEXT("/tmp/")) ||
        ClassName.Contains(TEXT("C:\\")) || ClassName.Contains(TEXT("D:\\"))) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              FString::Printf(TEXT("Invalid class name format: '%s'. Filesystem paths are not allowed."), *ClassName), nullptr);
      return true;
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> ActorsArray;

  if (UWorld* World = GEditor->GetEditorWorldContext().World()) {
    UClass* ClassToFind = nullptr;

    // CRITICAL FIX: Use ResolveClassByName for proper engine class resolution
    // This handles: full paths, short names like "StaticMeshActor", and loads classes if needed
    // Without this, FindObject only finds already-loaded classes, missing engine classes like
    // AStaticMeshActor, APawn, etc. that haven't been accessed yet
    ClassToFind = ResolveClassByName(ClassName);

    if (ClassToFind) {
      for (TActorIterator<AActor> It(World, ClassToFind); It; ++It) {
        if (AActor* Actor = *It) {
          TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
          ActorObj->SetStringField(TEXT("name"), Actor->GetActorLabel());
          ActorObj->SetStringField(TEXT("path"), Actor->GetPathName());
          ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
        }
      }
    } else {
      // Class not found - return empty result (this is valid for searches)
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("HandleControlActorFindByClass: Class '%s' not found"), *ClassName);
    }
  }

  Data->SetArrayField(TEXT("actors"), ActorsArray);
  Data->SetNumberField(TEXT("count"), ActorsArray.Num());
  SendStandardSuccessResponse(this, Socket, RequestId,
                              FString::Printf(TEXT("Found %d actors"), ActorsArray.Num()), Data);
  return true;
#else
  return false;
#endif
}
