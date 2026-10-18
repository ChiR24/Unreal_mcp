#include "Domains/NiagaraAuthoring/McpNiagaraContext.h"

namespace McpNiagaraAuthoringHandlers
{
bool AddDataInterfaceUserParameter(UNiagaraSystem* System, const FString& ParamName, UClass* DataInterfaceClass)
{
    if (!System || ParamName.IsEmpty() || !DataInterfaceClass || !DataInterfaceClass->IsChildOf(UNiagaraDataInterface::StaticClass()) || DataInterfaceClass->HasAnyClassFlags(CLASS_Abstract))
    {
        return false;
    }
    FNiagaraUserRedirectionParameterStore& UserStore = System->GetExposedParameters();
    FNiagaraVariable Param{FNiagaraTypeDefinition(DataInterfaceClass), FName(*ParamName)};
    UserStore.AddParameter(Param, true);
    if (!UserStore.FindParameterVariable(Param))
    {
        return false;
    }
    UNiagaraDataInterface* DataInterface = NewObject<UNiagaraDataInterface>(System, DataInterfaceClass, FName(*ParamName), RF_Transactional);
    if (!DataInterface)
    {
        return false;
    }
    UserStore.SetDataInterface(DataInterface, Param);
    return UserStore.GetDataInterface(Param) == DataInterface;
}

// A placed effect's own colour or rate read back only as the raw bytes of its component's override store. Each user
// parameter of the system is read from the component that runs it (componentName picks one of several), and the
// ones whose value differs from the system's default are listed as overridden.
bool AddPlacedUserParameters(FActionContext& Context, UNiagaraSystem* System)
{
    const FString ActorName = GetJsonStringField(Context.Payload, TEXT("actorName"));
    if (ActorName.IsEmpty()) return true;
    const FString ComponentName = GetJsonStringField(Context.Payload, TEXT("componentName"));
    AActor* Actor = McpHandlerUtils::FindActorByName(ActorName);
    UNiagaraComponent* Component = nullptr;
    TArray<FString> Names;
    for (UNiagaraComponent* Each : TInlineComponentArray<UNiagaraComponent*>(Actor))
    {
        Names.Add(Each->GetName());
        const bool bNamed = ComponentName.IsEmpty() ? Each->GetAsset() == System : Each->GetName().Equals(ComponentName, ESearchCase::IgnoreCase);
        Component = !Component && bNamed ? Each : Component;
    }
    if (!Component)
    {
        Context.SendError(!Actor ? FString::Printf(TEXT("Actor '%s' not found"), *ActorName)
                                 : FString::Printf(TEXT("Actor '%s' has no Niagara component %s; it has: %s"), *ActorName,
                                                   ComponentName.IsEmpty() ? *(TEXT("running ") + System->GetName()) : *ComponentName,
                                                   Names.IsEmpty() ? TEXT("none") : *FString::Join(Names, TEXT(", "))),
                          Actor ? TEXT("COMPONENT_NOT_FOUND") : TEXT("ACTOR_NOT_FOUND"));
        return false;
    }
    TSharedPtr<FJsonObject> Values = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Overridden;
    TArray<FNiagaraVariable> Params;
    System->GetExposedParameters().GetParameters(Params);
    for (const FNiagaraVariable& Param : Params)
    {
        const TSharedPtr<FJsonValue> Value = ReadNiagaraValue(Component->GetOverrideParameters(), Param);
        const TSharedPtr<FJsonValue> Default = ReadNiagaraValue(System->GetExposedParameters(), Param);
        if (!Value.IsValid()) continue;
        Values->SetField(Param.GetName().ToString(), Value);
        if (!Default.IsValid() || McpHandlerUtils::JsonValueToString(Value) != McpHandlerUtils::JsonValueToString(Default))
        {
            Overridden.Add(MakeShared<FJsonValueString>(Param.GetName().ToString()));
        }
    }
    TSharedPtr<FJsonObject> Placed = McpHandlerUtils::CreateResultObject();
    Placed->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
    Placed->SetStringField(TEXT("componentName"), Component->GetName());
    Placed->SetObjectField(TEXT("values"), Values);
    Placed->SetArrayField(TEXT("overridden"), Overridden);
    Context.Result->SetObjectField(TEXT("placed"), Placed);
    return true;
}

FNiagaraTypeDefinition ResolveNiagaraTypeByName(const FString& ParamType)
{
    if (ParamType.Equals(TEXT("Int"), ESearchCase::IgnoreCase) || ParamType.Equals(TEXT("Integer"), ESearchCase::IgnoreCase))
    {
        return FNiagaraTypeDefinition::GetIntDef();
    }
    if (ParamType.Equals(TEXT("Bool"), ESearchCase::IgnoreCase) || ParamType.Equals(TEXT("Boolean"), ESearchCase::IgnoreCase))
    {
        return FNiagaraTypeDefinition::GetBoolDef();
    }
    if (ParamType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase) || ParamType.Equals(TEXT("Vec3"), ESearchCase::IgnoreCase))
    {
        return FNiagaraTypeDefinition::GetVec3Def();
    }
    if (ParamType.Equals(TEXT("LinearColor"), ESearchCase::IgnoreCase) || ParamType.Equals(TEXT("Color"), ESearchCase::IgnoreCase))
    {
        return FNiagaraTypeDefinition::GetColorDef();
    }
    return FNiagaraTypeDefinition::GetFloatDef();
}
}
