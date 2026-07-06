#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderInternal.h"

#include "Dom/JsonValue.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "McpAutomationBridgeSubsystem.h"
#include "MoviePipelineDeferredPasses.h"
#if MCP_HAS_MOVIE_PIPELINE_OBJECT_ID_PASS
#include "MoviePipelineObjectIdPass.h"
#endif
#include MCP_MOVIE_PIPELINE_CONFIG_HEADER
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueSubsystem.h"
#include "UObject/SoftObjectPath.h"

namespace McpSequenceMovieRender {
namespace {
const TCHAR *NormalMaterial =
    TEXT("/MovieRenderPipeline/Materials/MovieRenderQueue_WorldNormal."
         "MovieRenderQueue_WorldNormal");

FString GetRenderPassName(const FString &Input) {
  FString Pass = Input.ToLower();
  Pass.ReplaceInline(TEXT("-"), TEXT("_"));
  return Pass;
}

UMoviePipelineDeferredPassBase *GetDeferred(MCP_MOVIE_PIPELINE_CONFIG_CLASS *Config) {
  return Config ? Cast<UMoviePipelineDeferredPassBase>(
                      Config->FindOrAddSettingByClass(
                          UMoviePipelineDeferredPassBase::StaticClass(), true))
                : nullptr;
}

bool ValidatePostProcessMaterial(const FString &MaterialPath,
                                 FString &OutMessage, FString &OutCode) {
  UMaterialInterface *MaterialInterface =
      Cast<UMaterialInterface>(FSoftObjectPath(MaterialPath).TryLoad());
  if (!MaterialInterface) {
    OutMessage =
        FString::Printf(TEXT("Render pass material not found: %s"), *MaterialPath);
    OutCode = TEXT("RENDER_PASS_UNAVAILABLE");
    return false;
  }
  const UMaterial *Material = MaterialInterface->GetMaterial();
  if (!Material ||
      Material->MaterialDomain != EMaterialDomain::MD_PostProcess) {
    OutMessage = FString::Printf(
        TEXT("Render pass material must use the Post Process domain: %s"),
        *MaterialPath);
    OutCode = TEXT("RENDER_PASS_MATERIAL_DOMAIN_INVALID");
    return false;
  }
  return true;
}

bool UpsertMaterialPass(UMoviePipelineDeferredPassBase *Deferred,
                        const FString &MaterialPath, const FString &Name,
                        bool bHighPrecision, FString &OutMessage,
                        FString &OutCode) {
  if (!Deferred) {
    OutMessage = TEXT("Deferred MRQ pass is unavailable.");
    OutCode = TEXT("RENDER_PASS_UNAVAILABLE");
    return false;
  }
  if (!ValidatePostProcessMaterial(MaterialPath, OutMessage, OutCode)) {
    return false;
  }
  for (FMoviePipelinePostProcessPass &Pass :
       Deferred->AdditionalPostProcessMaterials) {
    if (Pass.Material.ToSoftObjectPath().ToString() == MaterialPath) {
      Pass.bEnabled = true;
#if MCP_HAS_MOVIE_PIPELINE_PASS_METADATA
      Pass.Name = Name;
      Pass.bHighPrecisionOutput = bHighPrecision;
      Pass.bUseLosslessCompression = true;
#else
      (void)Name;
      (void)bHighPrecision;
#endif
      return true;
    }
  }
  FMoviePipelinePostProcessPass &Pass =
      Deferred->AdditionalPostProcessMaterials.AddDefaulted_GetRef();
  Pass.bEnabled = true;
#if MCP_HAS_MOVIE_PIPELINE_PASS_METADATA
  Pass.Name = Name;
#endif
  Pass.Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MaterialPath));
#if MCP_HAS_MOVIE_PIPELINE_PASS_METADATA
  Pass.bHighPrecisionOutput = bHighPrecision;
  Pass.bUseLosslessCompression = true;
#else
  (void)Name;
  (void)bHighPrecision;
#endif
  return true;
}

bool ApplySinglePass(MCP_MOVIE_PIPELINE_CONFIG_CLASS *Config,
                     const TSharedPtr<FJsonObject> &Payload,
                     const FString &PassName, FString &OutMessage,
                     FString &OutCode) {
  const FString Pass = GetRenderPassName(PassName);
  UMoviePipelineDeferredPassBase *Deferred = GetDeferred(Config);
  if (Pass == TEXT("beauty") || Pass == TEXT("final") ||
      Pass == TEXT("final_image") || Pass == TEXT("lit")) {
    if (!Deferred) {
      OutMessage = TEXT("Deferred beauty pass is unavailable.");
      OutCode = TEXT("RENDER_PASS_UNAVAILABLE");
      return false;
    }
    Deferred->bRenderMainPass = true;
    return true;
  }
  if (Pass == TEXT("depth") || Pass == TEXT("world_depth"))
    return UpsertMaterialPass(Deferred,
                              UMoviePipelineDeferredPassBase::DefaultDepthAsset,
                              TEXT("depth"), true, OutMessage, OutCode);
  if (Pass == TEXT("motion_vector") || Pass == TEXT("motion_vectors"))
    return UpsertMaterialPass(
        Deferred, UMoviePipelineDeferredPassBase::DefaultMotionVectorsAsset,
        TEXT("motion_vector"), true, OutMessage, OutCode);
  if (Pass == TEXT("normal") || Pass == TEXT("world_normal"))
    return UpsertMaterialPass(Deferred, NormalMaterial, TEXT("normal"), true,
                              OutMessage, OutCode);
  if (Pass == TEXT("object_id") || Pass == TEXT("object_ids")) {
#if MCP_HAS_MOVIE_PIPELINE_OBJECT_ID_PASS
    if (!LoadRequiredModule(TEXT("MoviePipelineMaskRenderPass"), OutMessage,
                            OutCode)) {
      OutCode = TEXT("RENDER_PASS_UNAVAILABLE");
      return false;
    }
    UMoviePipelineObjectIdRenderPass *ObjectPass =
        Cast<UMoviePipelineObjectIdRenderPass>(Config->FindOrAddSettingByClass(
            UMoviePipelineObjectIdRenderPass::StaticClass(), true));
    if (!ObjectPass) {
      OutMessage = TEXT("Object ID render pass could not be added.");
      OutCode = TEXT("RENDER_PASS_UNAVAILABLE");
      return false;
    }
    ObjectPass->bIncludeTranslucentObjects =
        McpHandlerUtils::GetOptionalBool(Payload, TEXT("includeTranslucentObjects"),
                                         false);
    return true;
#else
    OutMessage =
        TEXT("Object ID render passes require MoviePipelineMaskRenderPass.");
    OutCode = TEXT("RENDER_PASS_UNAVAILABLE");
    return false;
#endif
  }
  if (Pass == TEXT("custom_stencil")) {
    FString MaterialPath;
    Payload->TryGetStringField(TEXT("materialPath"), MaterialPath);
    if (MaterialPath.IsEmpty()) {
      OutMessage = TEXT("custom_stencil requires materialPath for classic MRQ.");
      OutCode = TEXT("RENDER_PASS_UNSUPPORTED");
      return false;
    }
    return UpsertMaterialPass(Deferred, MaterialPath, TEXT("custom_stencil"),
                              true, OutMessage, OutCode);
  }
  OutMessage = FString::Printf(TEXT("Unsupported MRQ render pass: %s"), *PassName);
  OutCode = TEXT("RENDER_PASS_UNSUPPORTED");
  return false;
}

bool ValidateSinglePass(const TSharedPtr<FJsonObject> &Payload,
                        const FString &PassName, FString &OutMessage,
                        FString &OutCode) {
  const FString Pass = GetRenderPassName(PassName);
  if (Pass == TEXT("beauty") || Pass == TEXT("final") ||
      Pass == TEXT("final_image") || Pass == TEXT("lit") ||
      Pass == TEXT("depth") || Pass == TEXT("world_depth") ||
      Pass == TEXT("motion_vector") || Pass == TEXT("motion_vectors") ||
      Pass == TEXT("normal") || Pass == TEXT("world_normal"))
    return true;
  if (Pass == TEXT("object_id") || Pass == TEXT("object_ids")) {
#if MCP_HAS_MOVIE_PIPELINE_OBJECT_ID_PASS
    if (LoadRequiredModule(TEXT("MoviePipelineMaskRenderPass"), OutMessage,
                           OutCode))
      return true;
#endif
    OutMessage =
        TEXT("Object ID render passes require MoviePipelineMaskRenderPass.");
    OutCode = TEXT("RENDER_PASS_UNAVAILABLE");
    return false;
  }
  if (Pass == TEXT("custom_stencil")) {
    FString MaterialPath;
    Payload->TryGetStringField(TEXT("materialPath"), MaterialPath);
    if (MaterialPath.IsEmpty()) {
      OutMessage =
          TEXT("custom_stencil requires a valid materialPath for classic MRQ.");
      OutCode = TEXT("RENDER_PASS_UNAVAILABLE");
      return false;
    }
    return ValidatePostProcessMaterial(MaterialPath, OutMessage, OutCode);
  }
  OutMessage = FString::Printf(TEXT("Unsupported MRQ render pass: %s"), *PassName);
  OutCode = TEXT("RENDER_PASS_UNSUPPORTED");
  return false;
}

void CollectPasses(const TSharedPtr<FJsonObject> &Payload, TArray<FString> &Out) {
  FString Pass;
  if (Payload.IsValid() && Payload->TryGetStringField(TEXT("renderPass"), Pass) &&
      !Pass.IsEmpty())
    Out.Add(Pass);
  const TArray<TSharedPtr<FJsonValue>> *Array = nullptr;
  if (Payload.IsValid() && Payload->TryGetArrayField(TEXT("renderPasses"), Array)) {
    for (const TSharedPtr<FJsonValue> &Value : *Array)
      if (Value.IsValid() && Value->Type == EJson::String)
        Out.Add(Value->AsString());
  }
}
}

bool HandleAddRenderPass(UMcpAutomationBridgeSubsystem *Subsystem,
                         const FString &RequestId,
                         const TSharedPtr<FJsonObject> &Payload,
                         TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString Message, Code;
  UMoviePipelineQueueSubsystem *QueueSubsystem =
      GetQueueSubsystem(Message, Code);
  if (!QueueSubsystem)
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  UMoviePipelineQueue *Queue = QueueSubsystem->GetQueue();
  UMoviePipelineExecutorJob *Job =
      ResolveJob(Payload, Queue, Message, Code);
  if (!Job)
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  MCP_MOVIE_PIPELINE_CONFIG_CLASS *Config = ResolveConfig(Job, Message, Code);
  if (!Config)
    return SendError(Subsystem, RequestId, Socket, Message, Code), true;

  TArray<FString> Passes;
  CollectPasses(Payload, Passes);
  if (Passes.Num() == 0)
    return SendError(Subsystem, RequestId, Socket,
                     TEXT("add_render_pass requires renderPass or renderPasses."),
                     TEXT("INVALID_RENDER_PASS")),
           true;
  for (const FString &Pass : Passes) {
    if (!ValidateSinglePass(Payload, Pass, Message, Code))
      return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  }
  for (const FString &Pass : Passes) {
    if (!ApplySinglePass(Config, Payload, Pass, Message, Code))
      return SendError(Subsystem, RequestId, Socket, Message, Code), true;
  }
  Config->Modify();
  MCP_SET_MOVIE_PIPELINE_QUEUE_DIRTY(Queue, true);
  Subsystem->SendAutomationResponse(Socket, RequestId, true,
                                    TEXT("MRQ render pass configured."),
                                    BuildJobResult(Job, Queue));
  return true;
}
}

#endif
