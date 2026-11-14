#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Lighting/McpLightingPrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Light.h"
#include "Engine/PointLight.h"
#include "Engine/RectLight.h"
#include "Engine/SkyLight.h"
#include "Engine/SpotLight.h"
#include "Engine/World.h"
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
    UEngineWireSubsystem& Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // lightClass may name any light class; lightType must be one of the engine lights.
    FString LightClassStr = GetJsonStringField(Payload, TEXT("lightClass"));
    if (LightClassStr.IsEmpty())
    {
        LightClassStr = GetJsonStringField(Payload, TEXT("lightType"));
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
        LogEngineWireSubsystem,
        Log,
        TEXT("spawn_light: Resolved lightClass '%s' to %s (path: %s)"),
        *LightClassStr,
        *LightClass->GetName(),
        *LightClass->GetPathName());

    const FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector(0.0, 0.0, 300.0));
    const FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

    UWorld* World = McpHandlerUtils::GetEditorWorld();
    if (!World)
    {
        Subsystem.SendAutomationError(
            RequestingSocket, RequestId, TEXT("No valid world available for spawning light"), TEXT("NO_WORLD"));
        return true;
    }

    FlushRenderingCommands();
    // A light spawned outside any transaction could not be undone, where control_actor spawn could.
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Spawn Light")), EMcpMutationDurability::EditorStateOnly,
                                            TArray<UObject*>());
    FTransform SpawnTransform(Rotation, Location);
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* NewLight = World->SpawnActor<AActor>(LightClass, SpawnTransform, SpawnParams);
    if (!NewLight)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn light actor"), TEXT("SPAWN_FAILED"));
        return true;
    }
    NewLight->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    const FString Name = GetJsonStringField(Payload, TEXT("name"));
    NewLight->SetActorLabel(Name.IsEmpty() ? LightClass->GetName() : Name);

    // What was written, for the reply: the settings below and in `properties` each say here that they took.
    FLightPropertyReport Report;
    // The top-level intensity and color, which ApplyLightProperties (it reads only `properties`) would drop; a
    // properties.color below still wins. A sky light's component is no ULightComponent: both kinds take them.
    const auto ApplyTopLevel = [&Payload, &Report](auto* Component)
    {
        double TopLevelIntensity = 0.0;
        if (Payload->TryGetNumberField(TEXT("intensity"), TopLevelIntensity))
        {
            Component->SetIntensity(static_cast<float>(TopLevelIntensity));
            Report.Applied.AddUnique(TEXT("intensity"));
        }
        if (Payload->HasField(TEXT("color")))
        {
            Component->SetLightColor(ExtractLinearColorField(Payload, TEXT("color"), FLinearColor(0.f, 0.f, 0.f, 1.f)));
            Report.Applied.AddUnique(TEXT("color"));
        }
    };
    if (ULightComponent* BaseLightComp = NewLight->FindComponentByClass<ULightComponent>())
    {
        BaseLightComp->SetMobility(EComponentMobility::Movable);
        ApplyTopLevel(BaseLightComp);
    }
    else if (USkyLightComponent* SkyComp = NewLight->FindComponentByClass<USkyLightComponent>())
    {
        ApplyTopLevel(SkyComp);
    }

    const TSharedPtr<FJsonObject>* Props;
    if (Payload->TryGetObjectField(TEXT("properties"), Props))
    {
        ApplyLightProperties(*NewLight, *Props, Report);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    // Both identities: actorName, the label unless another actor shares it, and the object name FindObject resolves.
    Resp->SetStringField(TEXT("actorName"), McpActorRef(NewLight));
    Resp->SetStringField(TEXT("objectName"), NewLight->GetName());
    // Which settings took, which were refused and why, which were written with another value: a light spawned with
    // a setting dropped on the floor answered "Light spawned" and nothing else.
    FString Message = TEXT("Light spawned");
    const auto Say = [&Resp, &Message](const TCHAR* Field, const TArray<FString>& Names)
    {
        if (Names.Num() > 0)
        {
            Resp->SetArrayField(Field, McpHandlerUtils::ToJsonStringArray(Names));
            if (FCString::Strcmp(Field, TEXT("applied")) != 0)
            {
                Message += FString::Printf(TEXT("; %s: %s"), Field, *FString::Join(Names, TEXT(", ")));
            }
        }
    };
    Say(TEXT("applied"), Report.Applied);
    Say(TEXT("refused"), Report.Refused);
    Say(TEXT("adjusted"), Report.Adjusted);
    McpHandlerUtils::AddVerification(Resp, NewLight);
    Transaction.DescribeInto(Resp);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, Message, Resp);
    return true;
}

}
