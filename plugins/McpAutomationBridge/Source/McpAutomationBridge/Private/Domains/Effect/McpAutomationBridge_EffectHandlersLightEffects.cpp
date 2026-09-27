#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Effect/McpAutomationBridge_EffectHandlersPrivate.h"

#include "Editor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/RectLight.h"
#include "Engine/SpotLight.h"
#include "Subsystems/EditorActorSubsystem.h"

namespace McpEffectHandlers
{
bool HandleCreateDynamicLight(const FEffectActionContext& Context)
{
    if (!Context.Payload->HasField(TEXT("location")))
    {
        TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("location parameter is required for create_dynamic_light"));
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("Missing required parameter: location"), Response, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString LightName;
    Context.Payload->TryGetStringField(TEXT("lightName"), LightName);
    if (LightName.IsEmpty())
    {
        // The published contract names the actor `name` (dogfood #104).
        Context.Payload->TryGetStringField(TEXT("name"), LightName);
    }
    FString LightType;
    Context.Payload->TryGetStringField(TEXT("lightType"), LightType);
    if (LightType.IsEmpty())
    {
        LightType = TEXT("Point");
    }
    double Intensity = 0.0;
    Context.Payload->TryGetNumberField(TEXT("intensity"), Intensity);
    const bool bHasColor = Context.Payload->HasField(TEXT("color"));
    const FLinearColor LightColor = ExtractLinearColorField(Context.Payload, TEXT("color"), FLinearColor::White);
    bool bPulseEnabled = false;
    double PulseFrequency = 1.0;
    const TSharedPtr<FJsonObject>* PulseObject = nullptr;
    if (Context.Payload->TryGetObjectField(TEXT("pulse"), PulseObject) && PulseObject && (*PulseObject).IsValid())
    {
        (*PulseObject)->TryGetBoolField(TEXT("enabled"), bPulseEnabled);
        (*PulseObject)->TryGetNumberField(TEXT("frequency"), PulseFrequency);
    }

    if (!GEditor)
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }
    if (!GetEditorActorSubsystem())
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("EditorActorSubsystem not available"), nullptr,
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    UClass* ChosenClass = APointLight::StaticClass();
    UClass* ComponentClass = UPointLightComponent::StaticClass();
    const FString LowerLightType = LightType.ToLower();
    if (LowerLightType == TEXT("spot") || LowerLightType == TEXT("spotlight"))
    {
        ChosenClass = ASpotLight::StaticClass();
        ComponentClass = USpotLightComponent::StaticClass();
    }
    else if (LowerLightType == TEXT("directional") || LowerLightType == TEXT("directionallight"))
    {
        ChosenClass = ADirectionalLight::StaticClass();
        ComponentClass = UDirectionalLightComponent::StaticClass();
    }
    else if (LowerLightType == TEXT("rect") || LowerLightType == TEXT("rectlight"))
    {
        ChosenClass = ARectLight::StaticClass();
        ComponentClass = URectLightComponent::StaticClass();
    }

    AActor* Spawned = SpawnActorInActiveWorld<AActor>(
        ChosenClass, ExtractVectorField(Context.Payload, TEXT("location"), FVector::ZeroVector), FRotator::ZeroRotator);
    if (!Spawned)
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("Failed to spawn light actor"), nullptr,
            TEXT("CREATE_DYNAMIC_LIGHT_FAILED"));
        return true;
    }

    if (UActorComponent* Component = Spawned->GetComponentByClass(ComponentClass))
    {
        if (ULightComponent* LightComponent = Cast<ULightComponent>(Component))
        {
            LightComponent->SetIntensity(static_cast<float>(Intensity));
            if (bHasColor)
            {
                LightComponent->SetLightColor(LightColor);
            }
        }
    }
    if (!LightName.IsEmpty())
    {
        Spawned->SetActorLabel(LightName);
    }
    if (bPulseEnabled)
    {
        Spawned->Tags.Add(FName(*FString::Printf(TEXT("MCP_PULSE:%g"), PulseFrequency)));
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetStringField(TEXT("actorName"), McpActorRef(Spawned));
    Response->SetStringField(TEXT("actorPath"), Spawned->GetPathName());
    McpHandlerUtils::AddVerification(Response, Spawned);
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, true,
        TEXT("Dynamic light created"), Response);
    return true;
}
}
