// explain_lit: why a light does not light something. The light itself (visible, affecting the world, with intensity),
// unbuilt lighting for a static light, then against targetActor when given: attenuation radius, the spot cone and the
// lighting channels both share.
#include "Domains/Debug/Explain/McpExplain.h"

#include "Components/LightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SpotLightComponent.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpExplain
{
namespace
{
TSharedPtr<FJsonObject> LightPropertyFix(const ULightComponent* Light, const TCHAR* Property, const double Value)
{
    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("objectPath"), Light->GetPathName());
    Params->SetStringField(TEXT("propertyName"), Property);
    Params->SetNumberField(TEXT("value"), Value);
    return MakeNextCall(TEXT("inspect"), TEXT("set_property"), Params);
}

bool SharesChannel(const FLightingChannels& Light, const FLightingChannels& Primitive)
{
    return (Light.bChannel0 && Primitive.bChannel0) || (Light.bChannel1 && Primitive.bChannel1) || (Light.bChannel2 && Primitive.bChannel2);
}

void CheckTarget(FTree& Tree, const ULightComponent* Light, AActor* Target)
{
    const FBox Bounds = Target->GetComponentsBoundingBox(true);
    const double Distance = FMath::Sqrt(Bounds.ComputeSquaredDistanceToPoint(Light->GetComponentLocation()));
    if (const ULocalLightComponent* Local = Cast<ULocalLightComponent>(Light))
    {
        if (Distance > Local->AttenuationRadius)
        {
            Tree.Fail(TEXT("range"), TEXT("OUT_OF_RANGE"),
                      FString::Printf(TEXT("'%s' is %.0f units away, past the light's attenuation radius of %.0f."), *McpActorRef(Target), Distance, Local->AttenuationRadius),
                      LightPropertyFix(Light, TEXT("AttenuationRadius"), FMath::CeilToDouble(Distance * 1.25)));
        }
        else
        {
            Tree.Pass(TEXT("range"), FString::Printf(TEXT("'%s' is %.0f units away, inside the radius of %.0f."), *McpActorRef(Target), Distance, Local->AttenuationRadius));
        }
    }
    if (const USpotLightComponent* Spot = Cast<USpotLightComponent>(Light))
    {
        const FVector ToTarget = (Bounds.GetCenter() - Spot->GetComponentLocation()).GetSafeNormal();
        const double Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Spot->GetForwardVector(), ToTarget)));
        if (Angle > Spot->OuterConeAngle)
        {
            Tree.Fail(TEXT("cone"), TEXT("OUTSIDE_CONE"),
                      FString::Printf(TEXT("'%s' is %.0f degrees off the spot's axis, outside its %.0f degree outer cone."), *McpActorRef(Target), Angle, Spot->OuterConeAngle));
        }
    }
    bool bAnyShares = false;
    bool bAnyPrimitive = false;
    Target->ForEachComponent<UPrimitiveComponent>(false, [&](const UPrimitiveComponent* Primitive) {
        bAnyPrimitive = true;
        bAnyShares |= SharesChannel(Light->LightingChannels, Primitive->LightingChannels);
    });
    if (bAnyPrimitive && !bAnyShares)
    {
        Tree.Fail(TEXT("channels"), TEXT("CHANNEL_MISMATCH"), TEXT("The light and the target share no lighting channel, so this light skips it."));
    }
}
} // namespace

bool ExplainLit(UEngineWireSubsystem& Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString Name = GetJsonStringField(Payload, TEXT("actorName"));
    const FString TargetName = GetJsonStringField(Payload, TEXT("targetActor"));
    AActor* Actor = Name.IsEmpty() ? nullptr : McpHandlerUtils::FindActorByName(Name);
    const ULightComponent* Light = Actor ? Actor->FindComponentByClass<ULightComponent>() : nullptr;
    AActor* Target = TargetName.IsEmpty() ? nullptr : McpHandlerUtils::FindActorByName(TargetName);
    if (!Light || (!TargetName.IsEmpty() && !Target))
    {
        Self.SendAutomationError(Socket, RequestId,
            !Light ? FString::Printf(TEXT("'%s' is not a light in the level: actorName names the light."), *Name)
                   : FString::Printf(TEXT("No actor named '%s' in the level."), *TargetName),
            !Light ? TEXT("LIGHT_NOT_FOUND") : TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    FTree Tree;
    if (!Light->IsVisible() || !Light->bAffectsWorld)
    {
        Tree.Fail(TEXT("on"), TEXT("LIGHT_OFF"), TEXT("The light is hidden or does not affect the world."));
    }
    else
    {
        Tree.Pass(TEXT("on"), TEXT("The light is on."));
    }
    if (Light->Intensity <= 0.f)
    {
        Tree.Fail(TEXT("intensity"), TEXT("NO_INTENSITY"), TEXT("Its intensity is 0."), LightPropertyFix(Light, TEXT("Intensity"), 5000));
    }
    else
    {
        Tree.Pass(TEXT("intensity"), FString::Printf(TEXT("Its intensity is %.1f."), Light->Intensity));
    }
    if (Light->Mobility == EComponentMobility::Static && Actor->GetWorld() && Actor->GetWorld()->NumLightingUnbuiltObjects > 0)
    {
        Tree.Fail(TEXT("built"), TEXT("UNBUILT_LIGHTING"),
                  FString::Printf(TEXT("It is a Static light and %d object(s) have unbuilt lighting: a static light lights nothing until lighting is built."), Actor->GetWorld()->NumLightingUnbuiltObjects));
    }
    if (Target)
    {
        CheckTarget(Tree, Light, Target);
    }
    else
    {
        Tree.Unknown(TEXT("target"), TEXT("targetActor names the actor that should be lit, to check range, cone and lighting channels."));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Tree.WriteTo(Result);
    Self.SendAutomationResponse(Socket, RequestId, true,
        Tree.Summary(Target ? FString::Printf(TEXT("Why '%s' does not light '%s'"), *McpActorRef(Actor), *McpActorRef(Target))
                            : FString::Printf(TEXT("Why '%s' lights nothing"), *McpActorRef(Actor))), Result);
    return true;
}
} // namespace McpExplain
