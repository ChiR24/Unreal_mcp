#include "Domains/NiagaraAuthoring/McpNiagaraContext.h"

namespace McpNiagaraAuthoringHandlers
{
// One data interface as a user parameter of the system (User.<parameterName>). Every DI but the static-mesh one used
// to reply success with dataInterfaceAdded false; a second one under a taken name was built by NewObject over the
// first (same outer, same name), replacing an object the system still used.
static bool AddDataInterface(FActionContext& Context, const TCHAR* Label, UClass* DataInterfaceClass)
{
    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
    const FString ParamName = GetJsonStringField(Context.Payload, TEXT("parameterName"), FString::Printf(TEXT("MCP_%sDataInterface"), Label));
    if (HasUserParameterNamed(System, ParamName))
    {
        Context.SendError(FString::Printf(TEXT("%s already has a user parameter '%s'; pick another parameterName. Nothing was changed."),
            *System->GetName(), *ParamName), TEXT("NAME_TAKEN"));
        return true;
    }
    if (!AddDataInterfaceUserParameter(System, ParamName, DataInterfaceClass))
    {
        Context.SendError(FString::Printf(TEXT("Failed to create the %s data interface parameter '%s'."), Label, *ParamName),
            TEXT("NIAGARA_DI_CREATE_FAILED"));
        return true;
    }
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("dataInterface"), Label);
    Context.Result->SetStringField(TEXT("parameterName"), ParamName);
    Context.Result->SetBoolField(TEXT("dataInterfaceAdded"), true);
    Context.SendSuccess(true, FString::Printf(TEXT("%s data interface added as User.%s."), Label, *ParamName));
    return true;
}

bool HandleDataInterfaceAction(FActionContext& Context, const FString& SubAction)
{
    if (SubAction == TEXT("add_skeletal_mesh_data_interface"))
    {
        return AddDataInterface(Context, TEXT("SkeletalMesh"), UNiagaraDataInterfaceSkeletalMesh::StaticClass());
    }
    if (SubAction == TEXT("add_static_mesh_data_interface"))
    {
#if MCP_HAS_NIAGARA_STATIC_MESH_DI
        return AddDataInterface(Context, TEXT("StaticMesh"), UNiagaraDataInterfaceStaticMesh::StaticClass());
#else
        return AddDataInterface(Context, TEXT("StaticMesh"),
            StaticLoadClass(UNiagaraDataInterface::StaticClass(), nullptr, TEXT("/Script/Niagara.NiagaraDataInterfaceStaticMesh")));
#endif
    }
    if (SubAction == TEXT("add_spline_data_interface"))
    {
        return AddDataInterface(Context, TEXT("Spline"), UNiagaraDataInterfaceSpline::StaticClass());
    }
    if (SubAction == TEXT("add_audio_spectrum_data_interface"))
    {
        return AddDataInterface(Context, TEXT("AudioSpectrum"), UNiagaraDataInterfaceAudioSpectrum::StaticClass());
    }
#if MCP_HAS_NIAGARA_COLLISION_QUERY_DI
    if (SubAction == TEXT("add_collision_query_data_interface"))
    {
        return AddDataInterface(Context, TEXT("CollisionQuery"), UNiagaraDataInterfaceCollisionQuery::StaticClass());
    }
#endif
    return false;
}
}
