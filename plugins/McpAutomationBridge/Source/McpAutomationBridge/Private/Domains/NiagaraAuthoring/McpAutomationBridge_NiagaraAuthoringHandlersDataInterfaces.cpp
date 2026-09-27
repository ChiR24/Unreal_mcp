#include "Domains/NiagaraAuthoring/McpAutomationBridge_NiagaraAuthoringHandlersContext.h"

namespace McpNiagaraAuthoringHandlers
{
static bool FinishDataInterfaceAction(
    FActionContext& Context,
    UNiagaraSystem* System,
    const FString& DataInterfaceName,
    const FString& ParamName,
    bool bDataInterfaceAdded,
    const FString& MessageText,
    const FString& ResponseText)
{
    // Every DI but the static-mesh one used to reply success with dataInterfaceAdded:false.
    if (!bDataInterfaceAdded)
    {
        Context.SendError(FString::Printf(TEXT("Failed to create the %s data interface parameter '%s'."), *DataInterfaceName, *ParamName), TEXT("NIAGARA_DI_CREATE_FAILED"));
        return true;
    }
    MarkDirtyAndVerify(Context, System);
    Context.Result->SetStringField(TEXT("dataInterface"), DataInterfaceName);
    Context.Result->SetStringField(TEXT("parameterName"), ParamName);
    Context.Result->SetBoolField(TEXT("dataInterfaceAdded"), bDataInterfaceAdded);
    Context.Result->SetStringField(TEXT("message"), MessageText);
    Context.SendSuccess(true, ResponseText);
    return true;
}

static bool AddSkeletalMeshDI(FActionContext& Context)
{
    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
    const FString ParamName = GetJsonStringField(Context.Payload, TEXT("parameterName"), TEXT("MCP_SkeletalMeshDataInterface"));
    const bool bDataInterfaceAdded = AddDataInterfaceUserParameter(System, ParamName, UNiagaraDataInterfaceSkeletalMesh::StaticClass());
    return FinishDataInterfaceAction(Context, System, TEXT("SkeletalMesh"), ParamName, bDataInterfaceAdded, TEXT("Added Skeletal Mesh data interface."), TEXT("Skeletal Mesh DI added."));
}

static bool AddStaticMeshDI(FActionContext& Context)
{
    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
    const FString ParamName = GetJsonStringField(Context.Payload, TEXT("parameterName"), TEXT("MCP_StaticMeshDataInterface"));
    bool bDataInterfaceAdded = false;
#if MCP_HAS_NIAGARA_STATIC_MESH_DI
    bDataInterfaceAdded = AddDataInterfaceUserParameter(System, ParamName, UNiagaraDataInterfaceStaticMesh::StaticClass());
#else
    UClass* StaticMeshDataInterfaceClass = StaticLoadClass(UNiagaraDataInterface::StaticClass(), nullptr, TEXT("/Script/Niagara.NiagaraDataInterfaceStaticMesh"));
    bDataInterfaceAdded = AddDataInterfaceUserParameter(System, ParamName, StaticMeshDataInterfaceClass);
#endif
    return FinishDataInterfaceAction(Context, System, TEXT("StaticMesh"), ParamName, bDataInterfaceAdded, TEXT("Added Static Mesh data interface."), TEXT("Static Mesh DI added."));
}

static bool AddSplineDI(FActionContext& Context)
{
    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
    const FString ParamName = GetJsonStringField(Context.Payload, TEXT("parameterName"), TEXT("MCP_SplineDataInterface"));
    return FinishDataInterfaceAction(Context, System, TEXT("Spline"), ParamName, AddDataInterfaceUserParameter(System, ParamName, UNiagaraDataInterfaceSpline::StaticClass()), TEXT("Added Spline data interface."), TEXT("Spline DI added."));
}

static bool AddAudioSpectrumDI(FActionContext& Context)
{
    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
    const FString ParamName = GetJsonStringField(Context.Payload, TEXT("parameterName"), TEXT("MCP_AudioSpectrumDataInterface"));
    return FinishDataInterfaceAction(Context, System, TEXT("AudioSpectrum"), ParamName, AddDataInterfaceUserParameter(System, ParamName, UNiagaraDataInterfaceAudioSpectrum::StaticClass()), TEXT("Added Audio Spectrum data interface."), TEXT("Audio Spectrum DI added."));
}

static bool AddCollisionQueryDI(FActionContext& Context)
{
    UNiagaraSystem* System = nullptr;
    FNiagaraEmitterHandle* Handle = nullptr;
    if (!LoadSystemAndEmitter(Context, System, Handle))
    {
        return true;
    }
    const FString ParamName = GetJsonStringField(Context.Payload, TEXT("parameterName"), TEXT("MCP_CollisionQueryDataInterface"));
    return FinishDataInterfaceAction(Context, System, TEXT("CollisionQuery"), ParamName, AddDataInterfaceUserParameter(System, ParamName, UNiagaraDataInterfaceCollisionQuery::StaticClass()), TEXT("Added Collision Query data interface."), TEXT("Collision Query DI added."));
}

bool HandleDataInterfaceAction(FActionContext& Context, const FString& SubAction)
{
    if (SubAction == TEXT("add_skeletal_mesh_data_interface")) return AddSkeletalMeshDI(Context);
    if (SubAction == TEXT("add_static_mesh_data_interface")) return AddStaticMeshDI(Context);
    if (SubAction == TEXT("add_spline_data_interface")) return AddSplineDI(Context);
    if (SubAction == TEXT("add_audio_spectrum_data_interface")) return AddAudioSpectrumDI(Context);
    if (SubAction == TEXT("add_collision_query_data_interface")) return AddCollisionQueryDI(Context);
    return false;
}
}
