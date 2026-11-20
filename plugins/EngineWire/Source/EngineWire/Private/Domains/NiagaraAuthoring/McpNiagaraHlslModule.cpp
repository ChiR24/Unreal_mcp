// manage_effect create_niagara_hlsl_module (item 10, first slice): a Niagara module script whose work is one Custom HLSL
// node on the module's parameter map, so the HLSL reads and writes particle attributes as Map.Particles.Velocity. The
// module factory and the Custom HLSL node are not exported, so both are made by class path, the node's pins come from
// its Signature UPROPERTY, and its HLSL from its private CustomHlsl one; the rest is virtual calls. Add the module to an
// emitter with add_niagara_module.

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/NiagaraAuthoring/McpNiagaraContext.h"
#include "Domains/NiagaraGraph/McpNiagaraModuleOrder.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Factories/Factory.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Modules/ModuleManager.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraTypes.h"
#include "Safety/McpSafeOperations.h"
#include "UObject/UnrealType.h"

namespace McpNiagaraAuthoringHandlers
{
namespace HlslDetail
{
// The node's pins as the module needs them: one parameter map in and one out, both named Map, so the HLSL says Map.
void SetSignature(UEdGraphNode& Node)
{
    FNiagaraFunctionSignature Signature;
    Signature.Name = TEXT("CustomHlsl");
    Signature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("Map")));
    Signature.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("Map")));
    if (FStructProperty* Property = FindFProperty<FStructProperty>(Node.GetClass(), TEXT("Signature")))
    {
        Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(&Node), &Signature);
    }
}
} // namespace HlslDetail

bool CreateNiagaraHlslModule(FActionContext& Context)
{
    using namespace HlslDetail;
    const FString Hlsl = GetJsonStringField(Context.Payload, TEXT("hlsl"));
    if (Hlsl.TrimStartAndEnd().IsEmpty())
    {
        Context.SendError(TEXT("'hlsl' is required: the module's HLSL, reading and writing attributes through Map "
                               "(Map.Particles.Velocity += float3(0, 0, 100) * Map.Engine.DeltaTime;)."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"));
    UClass* FactoryClass = FindObject<UClass>(nullptr, TEXT("/Script/NiagaraEditor.NiagaraModuleScriptFactory"));
    UClass* HlslClass = FindObject<UClass>(nullptr, TEXT("/Script/NiagaraEditor.NiagaraNodeCustomHlsl"));
    if (!FactoryClass || !HlslClass)
    {
        Context.SendError(TEXT("The Niagara editor's module factory or Custom HLSL node class is not loaded."), TEXT("FEATURE_UNAVAILABLE"));
        return true;
    }
    UPackage* Package = NewNiagaraAssetPackage(Context);
    if (!Package)
    {
        return true;
    }
    UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
    UNiagaraScript* Script = Cast<UNiagaraScript>(Factory->FactoryCreateNew(UNiagaraScript::StaticClass(), Package,
        FName(*Context.Name), RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
    UNiagaraScriptSource* Source = Script ? Cast<UNiagaraScriptSource>(Script->GetLatestSource()) : nullptr;
    UNiagaraGraph* Graph = Source ? Source->NodeGraph : nullptr;
    UNiagaraNodeOutput* Output = nullptr;
    for (int32 Index = 0; Graph && !Output && Index < Graph->Nodes.Num(); ++Index)
    {
        Output = Cast<UNiagaraNodeOutput>(Graph->Nodes[Index]);
    }
    UEdGraphPin* OutputMap = Output ? McpNiagaraModuleOrder::MapInputPin(*Output) : nullptr;
    UEdGraphPin* Previous = OutputMap && OutputMap->LinkedTo.Num() > 0 ? OutputMap->LinkedTo[0] : nullptr;
    if (!Previous)
    {
        Context.SendError(TEXT("The new module script has no parameter map leading to its output."), TEXT("CREATE_FAILED"));
        return true;
    }
    UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, HlslClass, NAME_None, RF_Transactional);
    Graph->AddNode(Node, false, false);
    Node->CreateNewGuid();
    Node->PostPlacedNewNode();
    SetSignature(*Node);
    if (FStrProperty* Text = FindFProperty<FStrProperty>(HlslClass, TEXT("CustomHlsl")))
    {
        Text->SetPropertyValue_InContainer(Node, Hlsl);
    }
    if (FEnumProperty* Usage = FindFProperty<FEnumProperty>(HlslClass, TEXT("ScriptUsage")))
    {
        Usage->GetUnderlyingProperty()->SetIntPropertyValue(Usage->ContainerPtrToValuePtr<void>(Node),
                                                            static_cast<int64>(ENiagaraScriptUsage::Module));
    }
    Node->ReconstructNode();
    UEdGraphPin* MapIn = McpNiagaraModuleOrder::MapInputPin(*Node);
    UEdGraphPin* MapOut = McpNiagaraModuleOrder::MapInputPin(*Node, EGPD_Output);
    const UEdGraphSchema* Schema = Graph->GetSchema();
    Previous->BreakLinkTo(OutputMap);
    if (!MapIn || !MapOut || !Schema->TryCreateConnection(Previous, MapIn) || !Schema->TryCreateConnection(MapOut, OutputMap))
    {
        Context.SendError(TEXT("The Custom HLSL node could not be wired onto the module's parameter map."), TEXT("CREATE_FAILED"));
        return true;
    }
    Graph->NotifyGraphChanged();
    FAssetRegistryModule::AssetCreated(Script);
    if (Context.bSave && !McpSafeAssetSave(Script))
    {
        Context.SendError(TEXT("The module script was made but not saved."), TEXT("SAVE_FAILED"));
        return true;
    }
    Context.Result->SetStringField(TEXT("modulePath"), Script->GetPathName());
    Context.Result->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
    Context.SendSuccess(true, FString::Printf(TEXT("Created Niagara module %s: one Custom HLSL node on its parameter map. "
                                                   "Add it to an emitter with add_niagara_module."), *Context.Name));
    return true;
}
} // namespace McpNiagaraAuthoringHandlers
