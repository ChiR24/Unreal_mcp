#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Lighting/McpAutomationBridge_LightingHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Dom/JsonObject.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/Light.h"

namespace McpLightingHandlers
{

void ApplyLightProperties(AActor& NewLight, const TSharedPtr<FJsonObject>& PropertiesPayload)
{
    ULightComponent* LightComp = NewLight.FindComponentByClass<ULightComponent>();
    if (!LightComp)
    {
        return;
    }

    // Properties[Key] when set. A non-finite value or one outside [Min, Max] becomes Fallback, or the clamped
    // value when there is no Fallback; a warning names the value that was replaced.
    constexpr double NoMax = TNumericLimits<float>::Max();
    auto ReadNumber = [&PropertiesPayload](const TCHAR* Key, double Min, double Max, TOptional<double> Fallback, double& Out)
    {
        if (!PropertiesPayload->TryGetNumberField(Key, Out))
        {
            return false;
        }
        if (!FMath::IsFinite(Out) || Out < Min || Out > Max)
        {
            const double Fixed = Fallback.IsSet() || !FMath::IsFinite(Out) ? Fallback.Get(Min) : FMath::Clamp(Out, Min, Max);
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("spawn_light: Invalid %s %.2f, using %.2f"), Key, Out, Fixed);
            Out = Fixed;
        }
        return true;
    };

    double Intensity = 0.0;
    if (ReadNumber(TEXT("intensity"), 0.0, NoMax, 0.0, Intensity))
    {
        LightComp->SetIntensity(static_cast<float>(Intensity));
    }

    if (PropertiesPayload->HasField(TEXT("color")))
    {
        // A missing channel reads 0 (alpha 1); an [r,g,b(,a)] array works too.
        FLinearColor Color = ExtractLinearColorField(PropertiesPayload, TEXT("color"), FLinearColor(0.f, 0.f, 0.f, 1.f));

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
        PropertiesPayload->TryGetBoolField(TEXT("useAsAtmosphereSunLight"), bUseSun);
        DirComp->SetAtmosphereSunLight(bUseSun);
    }

    double Value = 0.0;
    if (UPointLightComponent* PointComp = Cast<UPointLightComponent>(LightComp))
    {
        if (ReadNumber(TEXT("attenuationRadius"), SMALL_NUMBER, NoMax, 1000.0, Value))
        {
            PointComp->SetAttenuationRadius(static_cast<float>(Value));
        }
    }

    if (USpotLightComponent* SpotComp = Cast<USpotLightComponent>(LightComp))
    {
        if (ReadNumber(TEXT("innerConeAngle"), 0.0, 180.0, {}, Value))
        {
            SpotComp->SetInnerConeAngle(static_cast<float>(Value));
        }
        if (ReadNumber(TEXT("outerConeAngle"), 0.0, 180.0, {}, Value))
        {
            SpotComp->SetOuterConeAngle(static_cast<float>(Value));
        }
    }

    if (URectLightComponent* RectComp = Cast<URectLightComponent>(LightComp))
    {
        if (ReadNumber(TEXT("sourceWidth"), SMALL_NUMBER, NoMax, 100.0, Value))
        {
            RectComp->SetSourceWidth(static_cast<float>(Value));
        }
        if (ReadNumber(TEXT("sourceHeight"), SMALL_NUMBER, NoMax, 100.0, Value))
        {
            RectComp->SetSourceHeight(static_cast<float>(Value));
        }
    }
}

}
