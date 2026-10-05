#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderInternal.h"

#include "Dom/JsonValue.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#if __has_include("MaterialDomain.h")
#include "MaterialDomain.h"
#endif
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
#include "MoviePipelineWidgetRenderSetting.h"
#include "UObject/SoftObjectPath.h"

namespace McpSequenceMovieRender {
namespace {
const TCHAR *NormalMaterial =
    TEXT("/MovieRenderPipeline/Materials/MovieRenderQueue_WorldNormal."
         "MovieRenderQueue_WorldNormal");

// Lower-case, '-' as '_', and the accepted aliases folded onto one name.
FString CanonicalPass(const FString &Input) {
  static const TMap<FString, FString> Aliases = {
      {TEXT("final"), TEXT("beauty")},        {TEXT("final_image"), TEXT("beauty")},
      {TEXT("lit"), TEXT("beauty")},          {TEXT("world_depth"), TEXT("depth")},
      {TEXT("motion_vectors"), TEXT("motion_vector")}, {TEXT("world_normal"), TEXT("normal")},
      {TEXT("object_ids"), TEXT("object_id")}, {TEXT("widget"), TEXT("ui")},
      {TEXT("ui_renderer"), TEXT("ui")}};
  const FString Pass = Input.ToLower().Replace(TEXT("-"), TEXT("_"));
  const FString *Canonical = Aliases.Find(Pass);
  return Canonical ? *Canonical : Pass;
}

// The post-process material a material pass renders; empty for any other pass.
FString PassMaterial(const FString &Pass, const TSharedPtr<FJsonObject> &Payload) {
  if (Pass == TEXT("depth"))
    return UMoviePipelineDeferredPassBase::DefaultDepthAsset;
  if (Pass == TEXT("motion_vector"))
    return UMoviePipelineDeferredPassBase::DefaultMotionVectorsAsset;
  if (Pass == TEXT("normal"))
    return NormalMaterial;
  return Pass == TEXT("custom_stencil") ? GetJsonStringField(Payload, TEXT("materialPath"))
                                        : FString();
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
                        FString &OutMessage, FString &OutCode) {
  if (!Deferred) {
    OutMessage = TEXT("Deferred MRQ pass is unavailable.");
    OutCode = TEXT("RENDER_PASS_UNAVAILABLE");
    return false;
  }
  if (!ValidatePostProcessMaterial(MaterialPath, OutMessage, OutCode)) {
    return false;
  }
  FMoviePipelinePostProcessPass *Pass =
      Deferred->AdditionalPostProcessMaterials.FindByPredicate(
          [&MaterialPath](const FMoviePipelinePostProcessPass &Existing) {
            return Existing.Material.ToSoftObjectPath().ToString() == MaterialPath;
          });
  if (!Pass) {
    Pass = &Deferred->AdditionalPostProcessMaterials.AddDefaulted_GetRef();
    Pass->Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MaterialPath));
  }
  Pass->bEnabled = true;
#if MCP_HAS_MOVIE_PIPELINE_PASS_METADATA
  Pass->Name = Name;
  Pass->bHighPrecisionOutput = true;
#if MCP_HAS_MOVIE_PIPELINE_LOSSLESS
  Pass->bUseLosslessCompression = true;
#endif
#else
  (void)Name;
#endif
  return true;
}

// Runs after ValidateSinglePass accepted every requested pass.
bool ApplySinglePass(MCP_MOVIE_PIPELINE_CONFIG_CLASS *Config,
                     const TSharedPtr<FJsonObject> &Payload,
                     const FString &PassName, FString &OutMessage,
                     FString &OutCode) {
  const FString Pass = CanonicalPass(PassName);
  UMoviePipelineDeferredPassBase *Deferred = GetDeferred(Config);
  if (Pass == TEXT("beauty")) {
    if (!Deferred) {
      OutMessage = TEXT("Deferred beauty pass is unavailable.");
      OutCode = TEXT("RENDER_PASS_UNAVAILABLE");
      return false;
    }
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
    Deferred->bRenderMainPass = true;
#endif
    return true;
  }
  // The game viewport's UMG layer (HUD, menus), drawn each frame.
  if (Pass == TEXT("ui")) {
    UMoviePipelineWidgetRenderer *Ui = Cast<UMoviePipelineWidgetRenderer>(
        Config->FindOrAddSettingByClass(UMoviePipelineWidgetRenderer::StaticClass(), true));
    if (!Ui) {
      OutMessage = TEXT("UI render pass could not be added.");
      OutCode = TEXT("RENDER_PASS_UNAVAILABLE");
      return false;
    }
    Ui->bCompositeOntoFinalImage =
        GetJsonBoolField(Payload, TEXT("compositeOntoFinalImage"), true);
    return true;
  }
  if (Pass == TEXT("object_id")) {
#if MCP_HAS_MOVIE_PIPELINE_OBJECT_ID_PASS
    UMoviePipelineObjectIdRenderPass *ObjectPass =
        Cast<UMoviePipelineObjectIdRenderPass>(Config->FindOrAddSettingByClass(
            UMoviePipelineObjectIdRenderPass::StaticClass(), true));
    if (!ObjectPass) {
      OutMessage = TEXT("Object ID render pass could not be added.");
      OutCode = TEXT("RENDER_PASS_UNAVAILABLE");
      return false;
    }
    ObjectPass->bIncludeTranslucentObjects =
        GetJsonBoolField(Payload, TEXT("includeTranslucentObjects"), false);
    return true;
#endif
  }
  return UpsertMaterialPass(Deferred, PassMaterial(Pass, Payload), Pass, OutMessage, OutCode);
}

bool ValidateSinglePass(const TSharedPtr<FJsonObject> &Payload,
                        const FString &PassName, FString &OutMessage,
                        FString &OutCode) {
  const FString Pass = CanonicalPass(PassName);
  if (Pass == TEXT("object_id")) {
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
  const FString MaterialPath = PassMaterial(Pass, Payload);
  if (Pass == TEXT("custom_stencil")) {
    if (MaterialPath.IsEmpty()) {
      OutMessage =
          TEXT("custom_stencil requires a valid materialPath for classic MRQ.");
      OutCode = TEXT("RENDER_PASS_UNAVAILABLE");
      return false;
    }
    return ValidatePostProcessMaterial(MaterialPath, OutMessage, OutCode);
  }
  if (Pass == TEXT("beauty") || Pass == TEXT("ui") || !MaterialPath.IsEmpty())
    return true;
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
  UMoviePipelineQueue *Queue = nullptr;
  UMoviePipelineExecutorJob *Job =
      ResolveRequestJob(Subsystem, RequestId, Socket, Payload, Queue);
  if (!Job)
    return true;
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
