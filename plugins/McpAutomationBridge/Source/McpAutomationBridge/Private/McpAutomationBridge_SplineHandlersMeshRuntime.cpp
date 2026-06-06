#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SplineHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Components/SplineMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

static USplineMeshComponent* FindRuntimeSplineMeshComponent(AActor* Actor, const FString& ComponentName)
{
    TArray<USplineMeshComponent*> MeshComponents;
    Actor->GetComponents<USplineMeshComponent>(MeshComponents);

    if (!ComponentName.IsEmpty())
    {
        for (USplineMeshComponent* Comp : MeshComponents)
        {
            if (Comp && Comp->GetName() == ComponentName)
            {
                return Comp;
            }
        }
    }
    else if (MeshComponents.Num() > 0)
    {
        return MeshComponents[0];
    }

    return nullptr;
}

bool HandleSetSplineMeshMaterial(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    FString ComponentName = GetJsonStringFieldSpline(Payload, TEXT("componentName"));
    FString MaterialPath = GetJsonStringFieldSpline(Payload, TEXT("materialPath"));
    int32 MaterialIndex = GetJsonIntFieldSpline(Payload, TEXT("materialIndex"), 0);

    if (ActorName.IsEmpty() || MaterialPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName and materialPath are required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    FString SafeMaterialPath = SanitizeProjectRelativePath(MaterialPath);
    if (SafeMaterialPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid or unsafe materialPath: %s. Path must be relative to project (e.g., /Game/...)"), *MaterialPath),
            nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    AActor* Actor = FindActorByName(World, ActorName);
    if (!Actor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    USplineMeshComponent* TargetComp = FindRuntimeSplineMeshComponent(Actor, ComponentName);
    if (!TargetComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No SplineMeshComponent found on actor"), nullptr, TEXT("NO_COMPONENT"));
        return true;
    }

    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *SafeMaterialPath);
    if (!Material)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Material not found: %s"), *SafeMaterialPath), nullptr, TEXT("MATERIAL_NOT_FOUND"));
        return true;
    }

    TargetComp->SetMaterial(MaterialIndex, Material);
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("materialPath"), SafeMaterialPath);
    Result->SetNumberField(TEXT("materialIndex"), MaterialIndex);
    McpHandlerUtils::AddVerification(Result, Actor);
    AddComponentVerification(Result, TargetComp);

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Spline mesh material set"), Result);
    return true;
}

bool HandleCreateSplineMeshActor(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"), TEXT("SplineMeshActor"));
    FString ComponentName = GetJsonStringFieldSpline(Payload, TEXT("componentName"), TEXT("SplineMesh"));
    FString MeshPath = GetJsonStringFieldSpline(Payload, TEXT("meshPath"));
    FString ForwardAxis = GetJsonStringFieldSpline(Payload, TEXT("forwardAxis"), TEXT("X"));
    FVector Location = GetJsonVectorFieldSpline(Payload, TEXT("location"));
    FRotator Rotation = GetJsonRotatorFieldSpline(Payload, TEXT("rotation"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    FString SafeMeshPath;
    if (!MeshPath.IsEmpty())
    {
        SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
        if (SafeMeshPath.IsEmpty())
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Invalid or unsafe meshPath: %s. Path must be relative to project (e.g., /Game/...)"), *MeshPath),
                nullptr, TEXT("SECURITY_VIOLATION"));
            return true;
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* NewActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
    if (!NewActor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn spline mesh actor"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(*ActorName);

    USplineMeshComponent* SplineMeshComp = NewObject<USplineMeshComponent>(NewActor, *ComponentName);
    if (!SplineMeshComp)
    {
        NewActor->Destroy();
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create SplineMeshComponent"), nullptr, TEXT("COMPONENT_FAILED"));
        return true;
    }

    SplineMeshComp->RegisterComponent();
    NewActor->AddInstanceComponent(SplineMeshComp);
    NewActor->SetRootComponent(SplineMeshComp);

    if (!SafeMeshPath.IsEmpty())
    {
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *SafeMeshPath);
        if (!Mesh)
        {
            NewActor->Destroy();
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Mesh not found: %s"), *SafeMeshPath), nullptr, TEXT("MESH_NOT_FOUND"));
            return true;
        }
        SplineMeshComp->SetStaticMesh(Mesh);
    }

    if (SplineMeshComp->GetMaterial(0) == nullptr)
    {
        UMaterialInterface* FallbackMaterial = McpLoadMaterialWithFallback(TEXT(""), true);
        if (FallbackMaterial)
        {
            SplineMeshComp->SetMaterial(0, FallbackMaterial);
        }
    }

    ESplineMeshAxis::Type Axis = ESplineMeshAxis::X;
    if (ForwardAxis == TEXT("Y")) Axis = ESplineMeshAxis::Y;
    else if (ForwardAxis == TEXT("Z")) Axis = ESplineMeshAxis::Z;
    SplineMeshComp->SetForwardAxis(Axis);
    SplineMeshComp->SetStartAndEnd(FVector::ZeroVector, FVector(100, 0, 0),
                                    FVector(500, 0, 0), FVector(-100, 0, 0));

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("actorPath"), NewActor->GetPathName());
    Result->SetStringField(TEXT("componentName"), ComponentName);
    McpHandlerUtils::AddVerification(Result, NewActor);
    AddComponentVerification(Result, SplineMeshComp);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("SplineMeshActor '%s' created with component '%s'"), *ActorName, *ComponentName), Result);
    return true;
}
#endif
