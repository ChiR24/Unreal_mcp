#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Effect/McpAutomationBridge_EffectHandlersPrivate.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#endif

namespace McpEffectHandlers
{
FVector ReadVectorField(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    const FVector& DefaultValue)
{
    const TSharedPtr<FJsonValue> Value = Payload->TryGetField(FieldName);
    if (!Value.IsValid())
    {
        return DefaultValue;
    }
    if (Value->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& Values = Value->AsArray();
        if (Values.Num() >= 3)
        {
            return FVector(
                static_cast<float>(Values[0]->AsNumber()),
                static_cast<float>(Values[1]->AsNumber()),
                static_cast<float>(Values[2]->AsNumber()));
        }
    }
    if (Value->Type == EJson::Object)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        if (Object.IsValid())
        {
            return FVector(
                static_cast<float>(Object->HasField(TEXT("x")) ? GetJsonNumberField(Object, TEXT("x")) : 0.0),
                static_cast<float>(Object->HasField(TEXT("y")) ? GetJsonNumberField(Object, TEXT("y")) : 0.0),
                static_cast<float>(Object->HasField(TEXT("z")) ? GetJsonNumberField(Object, TEXT("z")) : 0.0));
        }
    }
    return DefaultValue;
}

FRotator ReadRotatorField(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    const FRotator& DefaultValue)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (Payload->TryGetArrayField(FieldName, Values) && Values && Values->Num() >= 3)
    {
        return FRotator(
            static_cast<float>((*Values)[0]->AsNumber()),
            static_cast<float>((*Values)[1]->AsNumber()),
            static_cast<float>((*Values)[2]->AsNumber()));
    }
    return DefaultValue;
}

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

#if WITH_EDITOR
UWorld* GetEditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

UEditorActorSubsystem* GetEditorActorSubsystem()
{
    return GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
}

AActor* FindActorByLabel(UEditorActorSubsystem& ActorSubsystem, const FString& ActorName)
{
    for (AActor* Actor : ActorSubsystem.GetAllLevelActors())
    {
        if (Actor && Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase))
        {
            return Actor;
        }
    }
    return nullptr;
}

#endif
}
