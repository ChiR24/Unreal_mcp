#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LightingHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Dom/JsonObject.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/Light.h"

#if WITH_EDITOR
namespace McpLightingHandlers
{

void ApplyLightProperties(AActor& NewLight, const TSharedPtr<FJsonObject>& PropertiesPayload)
{
    ULightComponent* LightComp = NewLight.FindComponentByClass<ULightComponent>();
    if (!LightComp)
    {
        return;
    }

    double Intensity;
    if (PropertiesPayload->TryGetNumberField(TEXT("intensity"), Intensity))
    {
        if (!FMath::IsFinite(Intensity))
        {
            UE_LOG(
                LogMcpAutomationBridgeSubsystem,
                Warning,
                TEXT("spawn_light: Invalid intensity (not finite), using 0"));
            Intensity = 0.0;
        }
        else if (Intensity < 0.0)
        {
            UE_LOG(
                LogMcpAutomationBridgeSubsystem,
                Warning,
                TEXT("spawn_light: Negative intensity %.2f clamped to 0"),
                Intensity);
            Intensity = 0.0;
        }
        LightComp->SetIntensity(static_cast<float>(Intensity));
    }

    const TSharedPtr<FJsonObject>* ColorObj;
    if (PropertiesPayload->TryGetObjectField(TEXT("color"), ColorObj))
    {
        FLinearColor Color;
        Color.R = GetJsonNumberField((*ColorObj), TEXT("r"));
        Color.G = GetJsonNumberField((*ColorObj), TEXT("g"));
        Color.B = GetJsonNumberField((*ColorObj), TEXT("b"));
        Color.A = (*ColorObj)->HasField(TEXT("a")) ? GetJsonNumberField((*ColorObj), TEXT("a")) : 1.0f;

        if (!FMath::IsFinite(Color.R) || !FMath::IsFinite(Color.G) ||
            !FMath::IsFinite(Color.B) || !FMath::IsFinite(Color.A))
        {
            UE_LOG(
                LogMcpAutomationBridgeSubsystem,
                Warning,
                TEXT("spawn_light: Invalid color components, using white"));
            Color = FLinearColor::White;
        }
        LightComp->SetLightColor(Color);
    }

    bool bCastShadows;
    if (PropertiesPayload->TryGetBoolField(TEXT("castShadows"), bCastShadows))
    {
        LightComp->SetCastShadows(bCastShadows);
    }

    if (UDirectionalLightComponent* DirComp = Cast<UDirectionalLightComponent>(LightComp))
    {
        bool bUseSun = true;
        if (PropertiesPayload->TryGetBoolField(TEXT("useAsAtmosphereSunLight"), bUseSun))
        {
            DirComp->SetAtmosphereSunLight(bUseSun);
        }
        else
        {
            DirComp->SetAtmosphereSunLight(true);
        }
    }

    if (UPointLightComponent* PointComp = Cast<UPointLightComponent>(LightComp))
    {
        double Radius;
        if (PropertiesPayload->TryGetNumberField(TEXT("attenuationRadius"), Radius))
        {
            if (!FMath::IsFinite(Radius) || Radius <= 0.0)
            {
                UE_LOG(
                    LogMcpAutomationBridgeSubsystem,
                    Warning,
                    TEXT("spawn_light: Invalid attenuationRadius %.2f, using 1000"),
                    Radius);
                Radius = 1000.0;
            }
            PointComp->SetAttenuationRadius(static_cast<float>(Radius));
        }
    }

    if (USpotLightComponent* SpotComp = Cast<USpotLightComponent>(LightComp))
    {
        double InnerCone;
        if (PropertiesPayload->TryGetNumberField(TEXT("innerConeAngle"), InnerCone))
        {
            if (!FMath::IsFinite(InnerCone) || InnerCone < 0.0 || InnerCone > 180.0)
            {
                UE_LOG(
                    LogMcpAutomationBridgeSubsystem,
                    Warning,
                    TEXT("spawn_light: Invalid innerConeAngle %.2f, clamping to 0-180"),
                    InnerCone);
                InnerCone = FMath::Clamp(InnerCone, 0.0, 180.0);
            }
            SpotComp->SetInnerConeAngle(static_cast<float>(InnerCone));
        }

        double OuterCone;
        if (PropertiesPayload->TryGetNumberField(TEXT("outerConeAngle"), OuterCone))
        {
            if (!FMath::IsFinite(OuterCone) || OuterCone < 0.0 || OuterCone > 180.0)
            {
                UE_LOG(
                    LogMcpAutomationBridgeSubsystem,
                    Warning,
                    TEXT("spawn_light: Invalid outerConeAngle %.2f, clamping to 0-180"),
                    OuterCone);
                OuterCone = FMath::Clamp(OuterCone, 0.0, 180.0);
            }
            SpotComp->SetOuterConeAngle(static_cast<float>(OuterCone));
        }
    }

    if (URectLightComponent* RectComp = Cast<URectLightComponent>(LightComp))
    {
        double Width;
        if (PropertiesPayload->TryGetNumberField(TEXT("sourceWidth"), Width))
        {
            if (!FMath::IsFinite(Width) || Width <= 0.0)
            {
                UE_LOG(
                    LogMcpAutomationBridgeSubsystem,
                    Warning,
                    TEXT("spawn_light: Invalid sourceWidth %.2f, using 100"),
                    Width);
                Width = 100.0;
            }
            RectComp->SetSourceWidth(static_cast<float>(Width));
        }

        double Height;
        if (PropertiesPayload->TryGetNumberField(TEXT("sourceHeight"), Height))
        {
            if (!FMath::IsFinite(Height) || Height <= 0.0)
            {
                UE_LOG(
                    LogMcpAutomationBridgeSubsystem,
                    Warning,
                    TEXT("spawn_light: Invalid sourceHeight %.2f, using 100"),
                    Height);
                Height = 100.0;
            }
            RectComp->SetSourceHeight(static_cast<float>(Height));
        }
    }
}

}
#endif
