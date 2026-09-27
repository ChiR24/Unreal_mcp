#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"

#include "Domains/Effect/McpAutomationBridge_EffectHandlersPrivate.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "Subsystems/EditorActorSubsystem.h"

namespace McpEffectHandlers
{
FColor ReadColorField(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    const FColor& DefaultValue)
{
    // 0-255 channels as an {r,g,b,a} object or an [r,g,b(,a)] array; the object form
    // used to be ignored, so every object colour drew white.
    const FLinearColor Channels = ExtractLinearColorField(Payload, FieldName,
        FLinearColor(DefaultValue.R, DefaultValue.G, DefaultValue.B, DefaultValue.A));
    const auto ToByte = [](float Value) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Value), 0, 255)); };
    return FColor(ToByte(Channels.R), ToByte(Channels.G), ToByte(Channels.B), ToByte(Channels.A));
}

FVector ReadScaleField(const TSharedPtr<FJsonObject>& Payload)
{
    // A uniform number, or [x,y,z] or {x,y,z}: the object spelling used to be ignored.
    double UniformScale = 1.0;
    if (Payload->TryGetNumberField(TEXT("scale"), UniformScale))
    {
        return FVector(UniformScale);
    }
    return ExtractVectorField(Payload, TEXT("scale"), FVector::OneVector);
}

FString ReadNiagaraSystemPathField(const TSharedPtr<FJsonObject>& Payload)
{
    // systemPath is the documented spelling; the others are accepted aliases so a
    // caller that names the asset any of these ways is never told it is missing.
    for (const TCHAR* Field : {TEXT("systemPath"), TEXT("system"), TEXT("niagaraSystemPath"), TEXT("assetPath")})
    {
        FString Value;
        if (Payload.IsValid() && Payload->TryGetStringField(Field, Value) && !Value.IsEmpty())
        {
            return Value;
        }
    }
    return FString();
}

UEditorActorSubsystem* GetEditorActorSubsystem()
{
    return GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
}

// UEditorAssetLibrary and UEditorActorSubsystem::GetAllLevelActors refuse every call
// while PIE runs (CheckIfInEditorAndPIE), so these lookups answered "not found" during
// play even though the effect actions spawn into, and act on, the play world.
UObject* LoadEffectAsset(const FString& AssetPath)
{
    const FString PackagePath = FPackageName::ObjectPathToPackageName(AssetPath);
    if (!FPackageName::IsValidLongPackageName(PackagePath))
    {
        return nullptr;
    }
    const FString ObjectPath = PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath);
    // In memory first, so an asset created but not saved yet still resolves.
    if (UObject* Loaded = FindObject<UObject>(nullptr, *ObjectPath))
    {
        return Loaded;
    }
    return FPackageName::DoesPackageExist(PackagePath) ? LoadObject<UObject>(nullptr, *ObjectPath) : nullptr;
}

AActor* FindActorByLabel(const FString& ActorName)
{
    UWorld* World = GEditor && GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GetEditorWorld();
    return FindActorByNameInWorldForMcp(World, ActorName, true);
}

}
