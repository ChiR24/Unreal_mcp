#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Lighting/McpLightingPrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Components/SkyLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "Subsystems/EditorActorSubsystem.h"

namespace McpLightingHandlers
{

bool HandleSpawnSkyLight(
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Either vector spelling: a location sent as an array was dropped for the default.
    const FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector(0.0, 0.0, 500.0));
    const FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

    // cubemapPath alone selects the specified-cubemap source: it used to apply only under an
    // undeclared sourceType, so a declared cubemap was never set. A bad path fails before spawning.
    const FString CubemapPath = GetJsonStringField(Payload, TEXT("cubemapPath"));
    UTextureCube* Cubemap = nullptr;
    if (!CubemapPath.IsEmpty())
    {
        const FString SanitizedCubemapPath = SanitizeProjectRelativePath(CubemapPath);
        Cubemap = SanitizedCubemapPath.IsEmpty() ? nullptr : Cast<UTextureCube>(
            StaticLoadObject(UTextureCube::StaticClass(), nullptr, *SanitizedCubemapPath));
        if (!Cubemap)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("No cube texture at cubemapPath: %s"), *CubemapPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }
    }

    // One undo step for the spawn and its settings. During Play the sky light goes into the running game, whose
    // objects never reach the undo buffer, so no step is opened there.
    TOptional<FMcpScopedEditorTransaction> Transaction;
    if (!GEditor->PlayWorld)
    {
        Transaction.Emplace(FText::FromString(TEXT("Spawn Sky Light")), EMcpMutationDurability::EditorStateOnly, TArray<UObject*>());
    }
    AActor* SkyLight = SpawnActorInActiveWorld<ASkyLight>(ASkyLight::StaticClass(), Location, Rotation);
    if (!SkyLight)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn SkyLight"), TEXT("SPAWN_FAILED"));
        return true;
    }

    FString Name;
    if (Payload->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty())
    {
        SkyLight->SetActorLabel(Name);
    }

    USkyLightComponent* SkyComp = SkyLight->FindComponentByClass<USkyLightComponent>();
    if (SkyComp)
    {
        if (Cubemap)
        {
            SkyComp->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
            SkyComp->SetCubemap(Cubemap);
        }

        double Intensity;
        if (Payload->TryGetNumberField(TEXT("intensity"), Intensity))
        {
            SkyComp->SetIntensity(static_cast<float>(Intensity));
        }

        bool bRecapture;
        if (Payload->TryGetBoolField(TEXT("recapture"), bRecapture) && bRecapture)
        {
            SkyComp->RecaptureSky();
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("actorName"), McpActorRef(SkyLight));
    McpHandlerUtils::AddVerification(Resp, SkyLight);
    if (Transaction.IsSet())
    {
        Transaction->DescribeInto(Resp);
    }
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SkyLight spawned"), Resp);
    return true;
}

bool HandleEnsureSingleSkyLight(
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // The level's actors are not listed during Play, so a second sky light was spawned into the running game.
    if (GEditor->PlayWorld)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            TEXT("ensure_single_sky_light works on the edited level; stop Play first"), TEXT("PIE_ACTIVE"));
        return true;
    }

    FString TargetName;
    Payload->TryGetStringField(TEXT("name"), TargetName);
    if (TargetName.IsEmpty())
    {
        TargetName = TEXT("SkyLight");
    }

    TArray<AActor*> SkyLights;
    UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    for (AActor* Actor : ActorSS->GetAllLevelActors())
    {
        if (Actor && Actor->IsA<ASkyLight>())
        {
            SkyLights.Add(Actor);
        }
    }

    // Removing the extra sky lights deletes actors, which is the destructive delete's work, with its consent: this
    // write destroyed every other sky light without either. It names the one to keep and the ones to delete.
    if (SkyLights.Num() > 1)
    {
        AActor* Keep = SkyLights[0];
        for (AActor* SkyLight : SkyLights)
        {
            if (SkyLight->GetActorLabel() == TargetName)
            {
                Keep = SkyLight;
                break;
            }
        }
        TArray<FString> Others;
        for (AActor* SkyLight : SkyLights)
        {
            if (SkyLight != Keep)
            {
                Others.Add(McpActorRef(SkyLight));
            }
        }
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        Details->SetStringField(TEXT("keep"), McpActorRef(Keep));
        Details->SetArrayField(TEXT("remove"), McpHandlerUtils::ToJsonStringArray(Others));
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
            FString::Printf(TEXT("The level has %d sky lights, and their light adds up. Delete all but %s with control_actor delete (actorNames: %s), then call this again."),
                SkyLights.Num(), *McpActorRef(Keep), *FString::Join(Others, TEXT(", "))),
            Details, TEXT("MULTIPLE_SKY_LIGHTS"));
        return true;
    }

    AActor* Kept = SkyLights.Num() == 1 ? SkyLights[0] : nullptr;
    const bool bSpawned = Kept == nullptr;
    const bool bRelabelled = Kept && Kept->GetActorLabel() != TargetName;
    // Every sky light counts above, whoever may touch it, since their light adds up; changing one is for a caller whose
    // allowed paths hold its level.
    bool bRecaptureAsked = false;
    Payload->TryGetBoolField(TEXT("recapture"), bRecaptureAsked);
    if (Kept && (bRelabelled || bRecaptureAsked) && !McpCapabilityPrincipal::ActiveRequestPermitsObject(Kept))
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("The level's sky light %s is in a level outside the caller's allowed paths; nothing was changed."), *McpActorRef(Kept)),
            TEXT("PATH_NOT_PERMITTED"));
        return true;
    }
    // Only a spawn or a relabel opens an undo step: capturing the sky light when nothing changes dirtied the level.
    TOptional<FMcpScopedEditorTransaction> Transaction;
    if (bSpawned || bRelabelled)
    {
        Transaction.Emplace(FText::FromString(TEXT("Ensure Single Sky Light")), EMcpMutationDurability::EditorStateOnly,
                            Kept ? TArray<UObject*>{Kept} : TArray<UObject*>());
    }
    if (bSpawned)
    {
        Kept = SpawnActorInActiveWorld<ASkyLight>(ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, TargetName);
        if (!Kept)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn SkyLight"), TEXT("SPAWN_FAILED"));
            return true;
        }
    }
    else if (bRelabelled)
    {
        Kept->SetActorLabel(TargetName);
    }

    bool bRecapture;
    if (Payload->TryGetBoolField(TEXT("recapture"), bRecapture) && bRecapture)
    {
        if (USkyLightComponent* Comp = Kept->FindComponentByClass<USkyLightComponent>())
        {
            Comp->RecaptureSky();
        }
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("actorName"), McpActorRef(Kept));
    Resp->SetBoolField(TEXT("spawned"), bSpawned);
    Resp->SetBoolField(TEXT("relabelled"), bRelabelled);
    McpHandlerUtils::AddVerification(Resp, Kept);
    if (Transaction.IsSet())
    {
        Transaction->DescribeInto(Resp);
    }
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
        bSpawned ? TEXT("The level had no sky light; one was spawned") : TEXT("The level has one sky light"), Resp);
    return true;
}

}
