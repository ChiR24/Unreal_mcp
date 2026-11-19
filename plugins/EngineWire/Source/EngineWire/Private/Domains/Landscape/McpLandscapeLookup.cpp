#include "Domains/Landscape/McpLandscapeLookup.h"

#include "EngineUtils.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "Misc/App.h"
#include "EngineWireSubsystem.h"
#if MCP_HAS_LANDSCAPE_EDIT_LAYER_CONST
#include "LandscapeEditLayer.h"
#endif

namespace McpLandscapeHandlers {
// The landscape's first edit layer: its edit layer object carries the guid where there is one, else FLandscapeLayer.
static FGuid FirstEditLayerGuid(const ALandscape *Landscape) {
#if MCP_HAS_LANDSCAPE_EDIT_LAYER_CONST
  const ULandscapeEditLayerBase *Layer = Landscape->GetEditLayerConst(0);
  return Layer ? Layer->GetGuid() : FGuid();
#elif MCP_HAS_LANDSCAPE_GET_LAYER_CONST
  const FLandscapeLayer *Layer = Landscape->GetLayerConst(0);
  return Layer ? Layer->Guid : FGuid();
#else
  const FLandscapeLayer *Layer = Landscape->GetLayer(0);
  return Layer ? Layer->Guid : FGuid();
#endif
}

FGuid LandscapeWriteLayer(const ALandscape *Landscape) {
  const FGuid Editing = Landscape->GetEditingLayer();
  return Editing.IsValid() ? Editing : FirstEditLayerGuid(Landscape);
}

FMcpLandscapeEditScope::FMcpLandscapeEditScope(ALandscape *InLandscape, const TCHAR *Title, bool bInDeferMerge)
    : Transaction(MakeUnique<FMcpScopedEditorTransaction>(FText::FromString(Title), EMcpMutationDurability::EditorStateOnly,
                                                          TArray<UObject *>{InLandscape})),
      Landscape(InLandscape), bDeferMerge(bInDeferMerge) {
  Landscape->Modify();
  bLayered = Landscape->HasLayersContent();
  if (bLayered) {
    PreviousLayer = Landscape->GetEditingLayer();
    Landscape->SetEditingLayer(LandscapeWriteLayer(Landscape));
  }
}

FMcpLandscapeEditScope::~FMcpLandscapeEditScope() {
  if (bLayered) {
    Landscape->SetEditingLayer(PreviousLayer);
  }
  // The undo step holds the layer textures; the merge rebuilds what renders and collides from them, outside it.
  Transaction.Reset();
  if (bLayered && !bDeferMerge && FApp::CanEverRender()) {
    Landscape->ForceUpdateLayersContent();
  }
}

void FMcpLandscapeEditScope::DescribeInto(const TSharedPtr<FJsonObject> &Result) const {
  Transaction->DescribeInto(Result);
}

// The editor world's landscape labelled Name, or whose package or object path is Path; with neither, the first. A
// label names a landscape in any level: a path-restricted caller's stays in a level inside its prefixes, whatever
// path it also sent. A level that is not open is never loaded to look.
static ALandscape *FindLandscapeBy(const FString &Name, const FString &Path) {
  UWorld *World = McpHandlerUtils::GetEditorWorld();
  if (!World) {
    return nullptr;
  }
  for (TActorIterator<ALandscape> It(World); It; ++It) {
    ALandscape *Landscape = *It;
    if (!Landscape || !McpCapabilityPrincipal::ActiveRequestPermitsObject(Landscape)) {
      continue;
    }
    if ((Name.IsEmpty() && Path.IsEmpty()) ||
        (!Name.IsEmpty() && Landscape->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase)) ||
        (!Path.IsEmpty() && ((Landscape->GetPackage() && Landscape->GetPackage()->GetPathName().Equals(Path, ESearchCase::IgnoreCase)) ||
                             Landscape->GetPathName().Equals(Path, ESearchCase::IgnoreCase)))) {
      return Landscape;
    }
  }
  return nullptr;
}

ALandscape *ResolveLandscapeOrReply(UEngineWireSubsystem &Bridge,
                                    const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
                                    ULandscapeInfo **OutInfo) {
  FString LandscapePath = GetJsonStringField(Payload, TEXT("landscapePath"));
  const FString LandscapeName = GetJsonStringField(Payload, TEXT("landscapeName"));
  // The actor's object path (what create_landscape and the spline actions return); declared
  // by the landscape edit actions but never read, so a call naming only it found nothing.
  const FString ActorPath = GetJsonStringField(Payload, TEXT("landscapeActorPath"));
  if (!LandscapePath.IsEmpty()) {
    const FString SafePath = SanitizeProjectRelativePath(LandscapePath);
    if (SafePath.IsEmpty()) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          McpPathRefusalMessage(TEXT("landscape path"), LandscapePath),
          TEXT("SECURITY_VIOLATION"));
      return nullptr;
    }
    LandscapePath = SafePath;
  }
  ALandscape *Landscape = FindLandscapeBy(LandscapeName, LandscapePath.IsEmpty() ? ActorPath : LandscapePath);
  if (!Landscape) {
    Bridge.SendAutomationError(
        RequestingSocket, RequestId,
        LandscapeName.IsEmpty()
            ? FString::Printf(TEXT("Landscape not found at path: %s"), *(LandscapePath.IsEmpty() ? ActorPath : LandscapePath))
            : FString::Printf(TEXT("Landscape '%s' not found (path: %s)"), *LandscapeName, *LandscapePath),
        TEXT("LANDSCAPE_NOT_FOUND"));
    return nullptr;
  }
  if (OutInfo) {
    *OutInfo = Landscape->GetLandscapeInfo();
  }
  if (OutInfo && !*OutInfo) {
    Bridge.SendAutomationError(RequestingSocket, RequestId,
                               TEXT("Landscape has no info"),
                               TEXT("INVALID_LANDSCAPE"));
    return nullptr;
  }
  return Landscape;
}

ALandscape *McpFindLandscape(const TSharedPtr<FJsonObject> &Payload) {
  const FString LandscapeName = GetJsonStringField(Payload, TEXT("landscapeName"));
  FString LandscapePath = McpGetFirstStringField(Payload, {TEXT("landscapePath"), TEXT("landscapeActorPath")});
  LandscapePath.TrimStartAndEndInline();
  if (!LandscapePath.IsEmpty() && !LandscapePath.StartsWith(TEXT("/"))) {
    LandscapePath = TEXT("/") + LandscapePath;
  }
  return FindLandscapeBy(LandscapeName, LandscapePath);
}
}
