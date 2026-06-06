#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_EffectHandlersPrivate.h"

#if WITH_EDITOR
#include "NiagaraComponent.h"
#include "Subsystems/EditorActorSubsystem.h"
#endif

namespace McpEffectHandlers
{
bool HandleSetNiagaraParameter(const FEffectActionContext& Context)
{
    FString SystemName;
    Context.Payload->TryGetStringField(TEXT("systemName"), SystemName);
    FString ParameterName;
    Context.Payload->TryGetStringField(TEXT("parameterName"), ParameterName);
    FString ParameterType;
    Context.Payload->TryGetStringField(TEXT("parameterType"), ParameterType);
    if (ParameterName.IsEmpty())
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("parameterName required"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (ParameterType.IsEmpty())
    {
        ParameterType = TEXT("Float");
    }

#if WITH_EDITOR
    if (!GEditor)
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }
    UEditorActorSubsystem* ActorSubsystem = GetEditorActorSubsystem();
    if (!ActorSubsystem)
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("EditorActorSubsystem not available"), nullptr,
            TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"));
        return true;
    }

    const FName ParamName(*ParameterName);
    const TSharedPtr<FJsonValue> ValueField = Context.Payload->TryGetField(TEXT("value"));
    bool bApplied = false;
    bool bActorFound = false;
    bool bComponentFound = false;

    for (AActor* Actor : ActorSubsystem->GetAllLevelActors())
    {
        if (!Actor || !Actor->GetActorLabel().Equals(SystemName, ESearchCase::IgnoreCase))
        {
            continue;
        }
        bActorFound = true;
        UNiagaraComponent* NiagaraComponent = Actor->FindComponentByClass<UNiagaraComponent>();
        if (!NiagaraComponent)
        {
            bComponentFound = false;
            break;
        }
        bComponentFound = true;

        if (ParameterType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
        {
            double NumberValue = 0.0;
            bool bHasNumber = Context.Payload->TryGetNumberField(TEXT("value"), NumberValue);
            if (!bHasNumber && ValueField.IsValid())
            {
                if (ValueField->Type == EJson::Number)
                {
                    NumberValue = ValueField->AsNumber();
                    bHasNumber = true;
                }
                else if (ValueField->Type == EJson::Object)
                {
                    const TSharedPtr<FJsonObject> Object = ValueField->AsObject();
                    if (Object.IsValid())
                    {
                        bHasNumber = Object->TryGetNumberField(TEXT("v"), NumberValue);
                    }
                }
            }
            if (bHasNumber)
            {
                NiagaraComponent->SetVariableFloat(ParamName, static_cast<float>(NumberValue));
                bApplied = true;
            }
        }
        else if (ParameterType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase))
        {
            const TArray<TSharedPtr<FJsonValue>>* ArrayValue = nullptr;
            const TSharedPtr<FJsonObject>* ObjectValue = nullptr;
            if (Context.Payload->TryGetArrayField(TEXT("value"), ArrayValue) &&
                ArrayValue && ArrayValue->Num() >= 3)
            {
                NiagaraComponent->SetVariableVec3(
                    ParamName,
                    FVector(
                        static_cast<float>((*ArrayValue)[0]->AsNumber()),
                        static_cast<float>((*ArrayValue)[1]->AsNumber()),
                        static_cast<float>((*ArrayValue)[2]->AsNumber())));
                bApplied = true;
            }
            else if (Context.Payload->TryGetObjectField(TEXT("value"), ObjectValue) &&
                     ObjectValue && (*ObjectValue).IsValid())
            {
                double X = 0.0;
                double Y = 0.0;
                double Z = 0.0;
                (*ObjectValue)->TryGetNumberField(TEXT("x"), X);
                (*ObjectValue)->TryGetNumberField(TEXT("y"), Y);
                (*ObjectValue)->TryGetNumberField(TEXT("z"), Z);
                NiagaraComponent->SetVariableVec3(ParamName, FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z)));
                bApplied = true;
            }
        }
        else if (ParameterType.Equals(TEXT("Color"), ESearchCase::IgnoreCase))
        {
            const TArray<TSharedPtr<FJsonValue>>* ArrayValue = nullptr;
            if (Context.Payload->TryGetArrayField(TEXT("value"), ArrayValue) &&
                ArrayValue && ArrayValue->Num() >= 3)
            {
                NiagaraComponent->SetVariableLinearColor(
                    ParamName,
                    FLinearColor(
                        static_cast<float>((*ArrayValue)[0]->AsNumber()),
                        static_cast<float>((*ArrayValue)[1]->AsNumber()),
                        static_cast<float>((*ArrayValue)[2]->AsNumber()),
                        ArrayValue->Num() > 3 ? static_cast<float>((*ArrayValue)[3]->AsNumber()) : 1.0f));
                bApplied = true;
            }
        }
        else if (ParameterType.Equals(TEXT("Bool"), ESearchCase::IgnoreCase))
        {
            bool bValue = false;
            if (Context.Payload->TryGetBoolField(TEXT("value"), bValue))
            {
                NiagaraComponent->SetVariableBool(ParamName, bValue);
                bApplied = true;
            }
        }
        break;
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), bApplied);
    Response->SetBoolField(TEXT("applied"), bApplied);
    Response->SetStringField(TEXT("actorName"), SystemName);
    Response->SetStringField(TEXT("parameterName"), ParameterName);
    Response->SetStringField(TEXT("parameterType"), ParameterType);

    if (bApplied)
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, true,
            TEXT("Niagara parameter set"), Response);
        return true;
    }

    FString ErrorMessage = TEXT("Niagara parameter not applied");
    FString ErrorCode = TEXT("SET_NIAGARA_PARAM_FAILED");
    if (!bActorFound)
    {
        ErrorMessage = FString::Printf(TEXT("Actor '%s' not found"), *SystemName);
        ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
    else if (!bComponentFound)
    {
        ErrorMessage = FString::Printf(TEXT("Actor '%s' has no Niagara component"), *SystemName);
        ErrorCode = TEXT("COMPONENT_NOT_FOUND");
    }
    else if (!ParameterType.Equals(TEXT("Float"), ESearchCase::IgnoreCase) &&
             !ParameterType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase) &&
             !ParameterType.Equals(TEXT("Color"), ESearchCase::IgnoreCase) &&
             !ParameterType.Equals(TEXT("Bool"), ESearchCase::IgnoreCase))
    {
        ErrorMessage = FString::Printf(TEXT("Invalid parameter type: %s"), *ParameterType);
        ErrorCode = TEXT("INVALID_ARGUMENT");
    }
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, false, ErrorMessage, Response, ErrorCode);
    return true;
#else
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, false,
        TEXT("set_niagara_parameter requires editor build."), nullptr,
        TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
}
