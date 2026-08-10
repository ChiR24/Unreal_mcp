#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

using namespace McpEnvironmentHandlers;

bool UMcpAutomationBridgeSubsystem::HandleBakeLightmap(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("bake_lightmap"), ESearchCase::IgnoreCase))
    {
        return false;
    }

#if WITH_EDITOR
    FString QualityStr = TEXT("Preview");
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("quality"), QualityStr);
    }

    // Reuse HandleExecuteEditorFunction logic
    TSharedPtr<FJsonObject> P = McpHandlerUtils::CreateResultObject();
    P->SetStringField(TEXT("functionName"), TEXT("BUILD_LIGHTING"));
    P->SetStringField(TEXT("quality"), QualityStr);

    return HandleExecuteEditorFunction(RequestId, TEXT("execute_editor_function"),
                                       P, RequestingSocket);

#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Requires editor"), nullptr,
                           TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleCreateProceduralTerrain(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("create_procedural_terrain"), ESearchCase::IgnoreCase))
    {
        return false;
    }

#if WITH_EDITOR
    if (!GEditor)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Editor not available"),
                            TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("create_procedural_terrain payload missing"),
                            TEXT("INVALID_PAYLOAD"));
        return true;
    }

    int32 SizeX = 100;
    int32 SizeY = 100;
    double Spacing = 100.0;
    double HeightScale = 500.0;
    int32 Subdivisions = 50;
    FString ActorName = TEXT("ProceduralTerrain");

    Payload->TryGetNumberField(TEXT("sizeX"), SizeX);
    Payload->TryGetNumberField(TEXT("sizeY"), SizeY);
    Payload->TryGetNumberField(TEXT("spacing"), Spacing);
    Payload->TryGetNumberField(TEXT("heightScale"), HeightScale);
    Payload->TryGetNumberField(TEXT("subdivisions"), Subdivisions);
    Payload->TryGetStringField(TEXT("actorName"), ActorName);

    if (ActorName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("actorName parameter is required for create_procedural_terrain"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (ActorName.Contains(TEXT("/")) || ActorName.Contains(TEXT("\\")) ||
        ActorName.Contains(TEXT(":")) || ActorName.Contains(TEXT("*")) ||
        ActorName.Contains(TEXT("?")) || ActorName.Contains(TEXT("\"")) ||
        ActorName.Contains(TEXT("<")) || ActorName.Contains(TEXT(">")) ||
        ActorName.Contains(TEXT("|")))
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("actorName contains invalid characters (/, \\, :, *, ?, \", <, >, |)"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (ActorName.Len() > 128)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("actorName exceeds maximum length of 128 characters"),
                            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    SizeX = FMath::Clamp(SizeX, 2, 1000);
    SizeY = FMath::Clamp(SizeY, 2, 1000);
    Subdivisions = FMath::Clamp(Subdivisions, 2, 200);
    Spacing = FMath::Max(Spacing, 1.0);
    HeightScale = FMath::Max(HeightScale, 0.0);

    UWorld *World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("World not available"),
                            TEXT("WORLD_NOT_AVAILABLE"));
        return true;
    }

    FVector Location(0, 0, 0);
    const TSharedPtr<FJsonObject> *LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj)
    {
        double X = 0, Y = 0, Z = 0;
        (*LocObj)->TryGetNumberField(TEXT("x"), X);
        (*LocObj)->TryGetNumberField(TEXT("y"), Y);
        (*LocObj)->TryGetNumberField(TEXT("z"), Z);
        Location = FVector(X, Y, Z);
    }

    FRotator Rotation(0, 0, 0);
    const TSharedPtr<FJsonObject> *RotObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj)
    {
        double Pitch = 0, Yaw = 0, Roll = 0;
        (*RotObj)->TryGetNumberField(TEXT("pitch"), Pitch);
        (*RotObj)->TryGetNumberField(TEXT("yaw"), Yaw);
        (*RotObj)->TryGetNumberField(TEXT("roll"), Roll);
        Rotation = FRotator(Pitch, Yaw, Roll);
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = FName(*ActorName);
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

    AActor *TerrainActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
    if (!TerrainActor)
    {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to spawn terrain actor"),
                            TEXT("SPAWN_FAILED"));
        return true;
    }

    TerrainActor->Modify();
    TerrainActor->SetActorLabel(*ActorName);

    UProceduralMeshComponent *ProcMesh = NewObject<UProceduralMeshComponent>(TerrainActor);
    if (!ProcMesh)
    {
        TerrainActor->Destroy();
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to create procedural mesh component"),
                            TEXT("COMPONENT_CREATION_FAILED"));
        return true;
    }

    ProcMesh->CreationMethod = EComponentCreationMethod::Instance;
    ProcMesh->SetMobility(EComponentMobility::Movable);
    ProcMesh->SetRelativeTransform(FTransform::Identity);
    TerrainActor->SetRootComponent(ProcMesh);
    TerrainActor->AddInstanceComponent(ProcMesh);
    ProcMesh->RegisterComponent();
    TerrainActor->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                             ETeleportType::TeleportPhysics);

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;

    for (int32 Y = 0; Y <= Subdivisions; ++Y)
    {
        for (int32 X = 0; X <= Subdivisions; ++X)
        {
            double NormX = static_cast<double>(X) / Subdivisions;
            double NormY = static_cast<double>(Y) / Subdivisions;

            double WorldX = (NormX - 0.5) * SizeX * Spacing;
            double WorldY = (NormY - 0.5) * SizeY * Spacing;

            double WorldZ = FMath::Sin(NormX * 4.0 * PI) * FMath::Cos(NormY * 4.0 * PI) * HeightScale * 0.3 +
                            FMath::Sin(NormX * 8.0 * PI) * FMath::Cos(NormY * 8.0 * PI) * HeightScale * 0.15 +
                            FMath::Sin(NormX * 2.0 * PI + NormY * 3.0 * PI) * HeightScale * 0.25;

            Vertices.Add(FVector(WorldX, WorldY, WorldZ));
            UVs.Add(FVector2D(NormX, NormY));
        }
    }

    for (int32 Y = 0; Y < Subdivisions; ++Y)
    {
        for (int32 X = 0; X < Subdivisions; ++X)
        {
            int32 Current = Y * (Subdivisions + 1) + X;
            int32 Next = Current + Subdivisions + 1;

            Triangles.Add(Current);
            Triangles.Add(Next);
            Triangles.Add(Current + 1);

            Triangles.Add(Current + 1);
            Triangles.Add(Next);
            Triangles.Add(Next + 1);
        }
    }

    // Validate generated geometry is non-empty before building the mesh
    FBox TerrainBounds(ForceInit);
    for (const FVector& Vertex : Vertices)
    {
        TerrainBounds += Vertex;
    }
    if (Vertices.Num() < 3 || Triangles.Num() < 3 ||
        TerrainBounds.GetExtent().IsNearlyZero())
    {
        TerrainActor->Destroy();
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Generated terrain geometry is empty"),
                            TEXT("EMPTY_GEOMETRY"));
        return true;
    }

    // Calculate normals and tangents
    UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);

    // Create the mesh section
    ProcMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), Tangents, true);

    FString MaterialPath;
    if (Payload->TryGetStringField(TEXT("material"), MaterialPath) && !MaterialPath.IsEmpty())
    {
        UMaterialInterface *Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
        if (Material)
        {
            ProcMesh->SetMaterial(0, Material);
        }
    }

    TerrainActor->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("actorName"), TerrainActor->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), TerrainActor->GetPathName());
    Resp->SetNumberField(TEXT("vertices"), Vertices.Num());
    const int32 TriangleCount = Triangles.Num() / 3;
    Resp->SetNumberField(TEXT("triangles"), TriangleCount);
    Resp->SetNumberField(TEXT("sizeX"), SizeX);
    Resp->SetNumberField(TEXT("sizeY"), SizeY);
    Resp->SetNumberField(TEXT("subdivisions"), Subdivisions);

    McpHandlerUtils::AddVerification(Resp, TerrainActor);
    Resp->SetStringField(TEXT("actorName"), TerrainActor->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), TerrainActor->GetPathName());

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Procedural terrain created successfully"), Resp, FString());
    return true;

#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("create_procedural_terrain requires editor build"), nullptr,
                           TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
