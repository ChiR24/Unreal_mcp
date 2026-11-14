#include "Domains/NiagaraAuthoring/McpNiagaraContext.h"

namespace McpNiagaraAuthoringHandlers
{
// One data interface as a user parameter of the system (User.<parameterName>). Every DI but the static-mesh one used
// to reply success with dataInterfaceAdded false; a second one under a taken name was built by NewObject over the
// first (same outer, same name), replacing an object the system still used. No emitter is read: one used to be
// resolved, and a system with several refused a call that sent no emitterName.
static bool AddDataInterface(FActionContext& Context, const TCHAR* Label, UClass* DataInterfaceClass)
{
    UNiagaraSystem* System = LoadSystemOrError(Context);
    if (!System)
    {
        return true;
    }
    const FString ParamName = GetJsonStringField(Context.Payload, TEXT("parameterName"), FString::Printf(TEXT("MCP_%sDataInterface"), Label));
    if (!UserParameterNameFree(Context, System, ParamName))
    {
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
    // Each data interface by its class in the Niagara module: their headers moved between versions (the static mesh
    // one twice), and on 5.1 the collision query one cannot be included from another module.
    struct FDataInterfaceKind
    {
        const TCHAR* Action;
        const TCHAR* Label;
        const TCHAR* ClassPath;
    };
    static const FDataInterfaceKind Kinds[] = {
        {TEXT("add_skeletal_mesh_data_interface"), TEXT("SkeletalMesh"), TEXT("/Script/Niagara.NiagaraDataInterfaceSkeletalMesh")},
        {TEXT("add_static_mesh_data_interface"), TEXT("StaticMesh"), TEXT("/Script/Niagara.NiagaraDataInterfaceStaticMesh")},
        {TEXT("add_spline_data_interface"), TEXT("Spline"), TEXT("/Script/Niagara.NiagaraDataInterfaceSpline")},
        {TEXT("add_audio_spectrum_data_interface"), TEXT("AudioSpectrum"), TEXT("/Script/Niagara.NiagaraDataInterfaceAudioSpectrum")},
        {TEXT("add_collision_query_data_interface"), TEXT("CollisionQuery"), TEXT("/Script/Niagara.NiagaraDataInterfaceCollisionQuery")},
    };
    for (const FDataInterfaceKind& Kind : Kinds)
    {
        if (SubAction == Kind.Action)
        {
            return AddDataInterface(Context, Kind.Label, FindObject<UClass>(nullptr, Kind.ClassPath));
        }
    }
    return false;
}
}
