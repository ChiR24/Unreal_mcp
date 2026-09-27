#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Lighting/McpAutomationBridge_LightingHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Components/LightComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Light.h"
#include "Engine/PointLight.h"
#include "Engine/RectLight.h"
#include "Engine/SkyLight.h"
#include "Engine/SpotLight.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RenderingThread.h"

namespace McpLightingHandlers
{

// The engine light for a short type or its class name ("point"/"PointLight", ...); null for anything else.
static UClass* FindKnownLightClass(const FString& Name)
{
    static const TMap<FString, UClass*> Known = {
        {TEXT("point"), APointLight::StaticClass()},             {TEXT("pointlight"), APointLight::StaticClass()},
        {TEXT("directional"), ADirectionalLight::StaticClass()}, {TEXT("directionallight"), ADirectionalLight::StaticClass()},
        {TEXT("spot"), ASpotLight::StaticClass()},               {TEXT("spotlight"), ASpotLight::StaticClass()},
        {TEXT("rect"), ARectLight::StaticClass()},               {TEXT("rectlight"), ARectLight::StaticClass()},
        {TEXT("sky"), ASkyLight::StaticClass()},                 {TEXT("skylight"), ASkyLight::StaticClass()}};
    UClass* const* Found = Known.Find(Name);
    return Found ? *Found : nullptr;
}

bool HandleSpawnLight(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // lightClass may name any light class; lightType/type must be one of the engine lights.
    FString LightClassStr = GetJsonStringField(Payload, TEXT("lightClass"));
    if (LightClassStr.IsEmpty())
    {
        LightClassStr = McpGetFirstStringField(Payload, {TEXT("lightType"), TEXT("type")});
        if (LightClassStr.IsEmpty())
        {
            Subsystem.SendAutomationError(
                RequestingSocket, RequestId, TEXT("lightClass or lightType required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        if (!FindKnownLightClass(LightClassStr))
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid lightType: %s. Must be one of: point, directional, spot, rect, sky"), *LightClassStr),
                TEXT("INVALID_LIGHT_TYPE"));
            return true;
        }
    }
    // UClass names carry no "A" prefix, so ResolveUClass takes the name as given.
    UClass* LightClass = FindKnownLightClass(LightClassStr);
    if (!LightClass)
    {
        LightClass = ResolveUClass(LightClassStr);
    }
    // ASkyLight derives from AInfo, NOT from ALight (which covers the local light
    // actors PointLight/SpotLight/RectLight/DirectionalLight). Validating only
    // against ALight made SkyLight permanently unreachable even though the
    // lightType resolver above accepts "sky". Accept either hierarchy so every
    // documented light type actually spawns.
    const bool bIsLightActor =
        LightClass &&
        (LightClass->IsChildOf(ALight::StaticClass()) ||
         LightClass->IsChildOf(ASkyLight::StaticClass()));
    if (!bIsLightActor)
    {
        Subsystem.SendAutomationError(
            RequestingSocket,
            RequestId,
            FString::Printf(TEXT("Invalid light class: %s"), *LightClassStr),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UE_LOG(
        LogMcpAutomationBridgeSubsystem,
        Log,
        TEXT("spawn_light: Resolved lightClass '%s' to %s (path: %s)"),
        *LightClassStr,
        *LightClass->GetName(),
        *LightClass->GetPathName());

    const FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector(0.0, 0.0, 300.0));
    const FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || !World->IsValidLowLevel())
    {
        Subsystem.SendAutomationError(
            RequestingSocket, RequestId, TEXT("No valid world available for spawning light"), TEXT("NO_WORLD"));
        return true;
    }

    FlushRenderingCommands();
    FTransform SpawnTransform(Rotation, Location);
    AActor* NewLight = World->SpawnActorDeferred<AActor>(
        LightClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!NewLight)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn light actor"), TEXT("SPAWN_FAILED"));
        return true;
    }
    UGameplayStatics::FinishSpawningActor(NewLight, SpawnTransform);
    NewLight->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    const FString Name = GetJsonStringField(Payload, TEXT("name"));
    NewLight->SetActorLabel(Name.IsEmpty() ? LightClass->GetName() : Name);

    if (ULightComponent* BaseLightComp = NewLight->FindComponentByClass<ULightComponent>())
    {
        BaseLightComp->SetMobility(EComponentMobility::Movable);

        // ApplyLightProperties below only reads the `properties`
        // sub-object, so a documented top-level intensity would otherwise be
        // silently dropped and the component keeps the engine default. Mirror
        // spawn_sky_light / the Effect create_dynamic_light path.
        double TopLevelIntensity = 0.0;
        if (Payload->TryGetNumberField(TEXT("intensity"), TopLevelIntensity))
        {
            BaseLightComp->SetIntensity(static_cast<float>(TopLevelIntensity));
        }
        // A properties.color below still wins.
        if (Payload->HasField(TEXT("color")))
        {
            BaseLightComp->SetLightColor(ExtractLinearColorField(Payload, TEXT("color"), FLinearColor(0.f, 0.f, 0.f, 1.f)));
        }
    }

    const TSharedPtr<FJsonObject>* Props;
    if (Payload->TryGetObjectField(TEXT("properties"), Props))
    {
        ApplyLightProperties(*NewLight, *Props);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    // Report BOTH identities. The label is the editor-facing name set above; the
    // object name is what FindObject / set_transform / get_transform resolve
    // against. Previously only the label was returned under "actorName", so
    // callers that fed it back into name-based APIs could miss the actor.
    Resp->SetStringField(TEXT("actorName"), McpActorRef(NewLight));
    Resp->SetStringField(TEXT("actorLabel"), NewLight->GetActorLabel());
    Resp->SetStringField(TEXT("objectName"), NewLight->GetName());
    McpHandlerUtils::AddVerification(Resp, NewLight);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Light spawned"), Resp);
    return true;
}

}
