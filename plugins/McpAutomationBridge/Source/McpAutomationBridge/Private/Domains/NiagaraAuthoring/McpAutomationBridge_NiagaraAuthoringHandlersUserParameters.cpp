#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

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
