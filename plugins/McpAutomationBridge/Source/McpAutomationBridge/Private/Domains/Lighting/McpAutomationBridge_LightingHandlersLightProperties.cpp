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

void ApplyLightProperties(AActor& NewLight, const TSharedPtr<FJsonObject>& PropertiesPayload, FLightPropertyReport& Report)
{
    const auto Refuse = [&Report](const TCHAR* Key, const TCHAR* Why)
    {
        Report.Refused.Add(FString::Printf(TEXT("%s: %s"), Key, Why));
    };

    ULightComponent* LightComp = NewLight.FindComponentByClass<ULightComponent>();
    if (!LightComp)
    {
        // A sky light's component is no ULightComponent: only the top-level intensity and color reach it.
        for (const auto& Pair : PropertiesPayload->Values)
        {
            Refuse(*Pair.Key, TEXT("a sky light takes only the top-level intensity and color"));
        }
        return;
    }

    // The keys this reads. Any other is named in the reply instead of being dropped without a word.
    static const TCHAR* const Known[] = {TEXT("intensity"), TEXT("color"), TEXT("castShadows"), TEXT("useAsAtmosphereSunLight"),
                                         TEXT("attenuationRadius"), TEXT("innerConeAngle"), TEXT("outerConeAngle"),
                                         TEXT("sourceWidth"), TEXT("sourceHeight")};
    for (const auto& Pair : PropertiesPayload->Values)
    {
        const FString Key(*Pair.Key);
        bool bKnown = false;
        for (const TCHAR* Name : Known)
        {
            bKnown |= Key.Equals(Name, ESearchCase::CaseSensitive);
        }
        if (!bKnown)
        {
            Refuse(*Key, TEXT("not a light property this action reads"));
        }
    }

    // True when the light is the kind that has Key; a key sent to any other kind is refused by name.
    const auto Fits = [&PropertiesPayload, &Refuse](bool bFits, const TCHAR* Key, const TCHAR* Why)
    {
        if (!bFits && PropertiesPayload->HasField(Key))
        {
            Refuse(Key, Why);
        }
        return bFits;
    };

    // Properties[Key] when set. A value that is not a number is refused. A non-finite one, or one outside
    // [Min, Max], becomes Fallback (or the clamped value when there is no Fallback), is logged and reported as
    // adjusted; Range says in words what the key accepts.
    constexpr double NoMax = TNumericLimits<float>::Max();
    auto ReadNumber = [&PropertiesPayload, &Report, &Refuse](const TCHAR* Key, double Min, double Max, TOptional<double> Fallback,
                                                              const TCHAR* Range, double& Out)
    {
        if (!PropertiesPayload->HasField(Key))
        {
            return false;
        }
        if (!PropertiesPayload->TryGetNumberField(Key, Out))
        {
            Refuse(Key, TEXT("must be a number"));
            return false;
        }
        if (!FMath::IsFinite(Out) || Out < Min || Out > Max)
        {
            const double Fixed = Fallback.IsSet() || !FMath::IsFinite(Out) ? Fallback.Get(Min) : FMath::Clamp(Out, Min, Max);
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("spawn_light: Invalid %s %.2f, using %.2f"), Key, Out, Fixed);
            Report.Adjusted.Add(FString::Printf(TEXT("%s: %s must be %s; %s was used"), Key, *FString::SanitizeFloat(Out, 0), Range,
                                                *FString::SanitizeFloat(Fixed, 0)));
            Out = Fixed;
        }
        return true;
    };

    double Value = 0.0;
    if (ReadNumber(TEXT("intensity"), 0.0, NoMax, 0.0, TEXT("0 or more"), Value))
    {
        LightComp->SetIntensity(static_cast<float>(Value));
        Report.Applied.AddUnique(TEXT("intensity"));
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
            Report.Adjusted.Add(TEXT("color: a channel was not a finite number; white was used"));
            Color = FLinearColor::White;
        }
        LightComp->SetLightColor(Color);
        Report.Applied.AddUnique(TEXT("color"));
    }

    if (PropertiesPayload->HasField(TEXT("castShadows")))
    {
        bool bCastShadows = false;
        if (PropertiesPayload->TryGetBoolField(TEXT("castShadows"), bCastShadows))
        {
            LightComp->SetCastShadows(bCastShadows);
            Report.Applied.AddUnique(TEXT("castShadows"));
        }
        else
        {
            Refuse(TEXT("castShadows"), TEXT("must be true or false"));
        }
    }

    UDirectionalLightComponent* DirComp = Cast<UDirectionalLightComponent>(LightComp);
    if (Fits(DirComp != nullptr, TEXT("useAsAtmosphereSunLight"), TEXT("only a directional light can be the atmosphere sun light")))
    {
        // A directional light given properties is the atmosphere sun unless this says otherwise.
        bool bUseSun = true;
        if (PropertiesPayload->HasField(TEXT("useAsAtmosphereSunLight")))
        {
            if (PropertiesPayload->TryGetBoolField(TEXT("useAsAtmosphereSunLight"), bUseSun))
            {
                Report.Applied.AddUnique(TEXT("useAsAtmosphereSunLight"));
            }
            else
            {
                Refuse(TEXT("useAsAtmosphereSunLight"), TEXT("must be true or false"));
            }
        }
        DirComp->SetAtmosphereSunLight(bUseSun);
    }

    UPointLightComponent* PointComp = Cast<UPointLightComponent>(LightComp);
    if (Fits(PointComp != nullptr, TEXT("attenuationRadius"), TEXT("this action sets an attenuation radius only on a point or spot light")) &&
        ReadNumber(TEXT("attenuationRadius"), SMALL_NUMBER, NoMax, 1000.0, TEXT("above 0"), Value))
    {
        PointComp->SetAttenuationRadius(static_cast<float>(Value));
        Report.Applied.AddUnique(TEXT("attenuationRadius"));
    }

    USpotLightComponent* SpotComp = Cast<USpotLightComponent>(LightComp);
    if (Fits(SpotComp != nullptr, TEXT("innerConeAngle"), TEXT("only a spot light has a cone angle")) &&
        ReadNumber(TEXT("innerConeAngle"), 0.0, 180.0, {}, TEXT("0 to 180"), Value))
    {
        SpotComp->SetInnerConeAngle(static_cast<float>(Value));
        Report.Applied.AddUnique(TEXT("innerConeAngle"));
    }
    if (Fits(SpotComp != nullptr, TEXT("outerConeAngle"), TEXT("only a spot light has a cone angle")) &&
        ReadNumber(TEXT("outerConeAngle"), 0.0, 180.0, {}, TEXT("0 to 180"), Value))
    {
        SpotComp->SetOuterConeAngle(static_cast<float>(Value));
        Report.Applied.AddUnique(TEXT("outerConeAngle"));
    }

    URectLightComponent* RectComp = Cast<URectLightComponent>(LightComp);
    if (Fits(RectComp != nullptr, TEXT("sourceWidth"), TEXT("only a rect light has a source size")) &&
        ReadNumber(TEXT("sourceWidth"), SMALL_NUMBER, NoMax, 100.0, TEXT("above 0"), Value))
    {
        RectComp->SetSourceWidth(static_cast<float>(Value));
        Report.Applied.AddUnique(TEXT("sourceWidth"));
    }
    if (Fits(RectComp != nullptr, TEXT("sourceHeight"), TEXT("only a rect light has a source size")) &&
        ReadNumber(TEXT("sourceHeight"), SMALL_NUMBER, NoMax, 100.0, TEXT("above 0"), Value))
    {
        RectComp->SetSourceHeight(static_cast<float>(Value));
        Report.Applied.AddUnique(TEXT("sourceHeight"));
    }
}

}
