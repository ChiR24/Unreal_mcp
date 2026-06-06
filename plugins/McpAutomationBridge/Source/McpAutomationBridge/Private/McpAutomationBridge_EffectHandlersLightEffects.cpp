#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_EffectHandlersPrivate.h"

#if WITH_EDITOR
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
#endif

namespace McpEffectHandlers
{
static bool ReadLightColor(const TSharedPtr<FJsonObject>& Payload, FLinearColor& OutColor)
{
    const TArray<TSharedPtr<FJsonValue>>* ColorArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("color"), ColorArray) && ColorArray && ColorArray->Num() >= 3)
    {
        OutColor = FLinearColor(
            static_cast<float>((*ColorArray)[0]->AsNumber()),
            static_cast<float>((*ColorArray)[1]->AsNumber()),
            static_cast<float>((*ColorArray)[2]->AsNumber()),
            ColorArray->Num() > 3 ? static_cast<float>((*ColorArray)[3]->AsNumber()) : 1.0f);
        return true;
    }
    const TSharedPtr<FJsonObject>* ColorObject = nullptr;
    if (Payload->TryGetObjectField(TEXT("color"), ColorObject) && ColorObject && (*ColorObject).IsValid())
    {
        OutColor = FLinearColor(
            static_cast<float>((*ColorObject)->HasField(TEXT("r")) ? GetJsonNumberField(*ColorObject, TEXT("r")) : 1.0),
            static_cast<float>((*ColorObject)->HasField(TEXT("g")) ? GetJsonNumberField(*ColorObject, TEXT("g")) : 1.0),
            static_cast<float>((*ColorObject)->HasField(TEXT("b")) ? GetJsonNumberField(*ColorObject, TEXT("b")) : 1.0),
            static_cast<float>((*ColorObject)->HasField(TEXT("a")) ? GetJsonNumberField(*ColorObject, TEXT("a")) : 1.0));
        return true;
    }
    return false;
}

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
    FString LightType;
    Context.Payload->TryGetStringField(TEXT("lightType"), LightType);
    if (LightType.IsEmpty())
    {
        LightType = TEXT("Point");
    }
    double Intensity = 0.0;
    Context.Payload->TryGetNumberField(TEXT("intensity"), Intensity);
    FLinearColor LightColor;
    const bool bHasColor = ReadLightColor(Context.Payload, LightColor);
    bool bPulseEnabled = false;
    double PulseFrequency = 1.0;
    const TSharedPtr<FJsonObject>* PulseObject = nullptr;
    if (Context.Payload->TryGetObjectField(TEXT("pulse"), PulseObject) && PulseObject && (*PulseObject).IsValid())
    {
        (*PulseObject)->TryGetBoolField(TEXT("enabled"), bPulseEnabled);
        (*PulseObject)->TryGetNumberField(TEXT("frequency"), PulseFrequency);
    }

#if WITH_EDITOR
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
        ChosenClass, ReadVectorField(Context.Payload, TEXT("location")), FRotator::ZeroRotator);
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
    McpHandlerUtils::AddVerification(Response, Spawned);
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, true,
        TEXT("Dynamic light created"), Response);
    return true;
#else
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, false,
        TEXT("create_dynamic_light requires editor build."), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
}
