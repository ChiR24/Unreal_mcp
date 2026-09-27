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
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (Payload->TryGetArrayField(FieldName, Values) && Values && Values->Num() >= 3)
    {
        const double Alpha = Values->Num() >= 4 ? (*Values)[3]->AsNumber() : DefaultValue.A;
        return FColor(
            static_cast<uint8>((*Values)[0]->AsNumber()),
            static_cast<uint8>((*Values)[1]->AsNumber()),
            static_cast<uint8>((*Values)[2]->AsNumber()),
            static_cast<uint8>(Alpha));
    }
    return DefaultValue;
}

FVector ReadScaleField(const TSharedPtr<FJsonObject>& Payload)
{
    FVector Scale(1.0f, 1.0f, 1.0f);
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    double UniformScale = 1.0;
    if (Payload->TryGetArrayField(TEXT("scale"), Values) && Values && Values->Num() >= 3)
    {
        Scale = FVector(
            static_cast<float>((*Values)[0]->AsNumber()),
            static_cast<float>((*Values)[1]->AsNumber()),
            static_cast<float>((*Values)[2]->AsNumber()));
    }
    else if (Payload->TryGetNumberField(TEXT("scale"), UniformScale))
    {
        Scale = FVector(static_cast<float>(UniformScale));
    }
    return Scale;
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
