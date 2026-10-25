#include "Domains/Landscape/McpLandscapeLookup.h"

#include "EngineUtils.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "Misc/App.h"
#include "EngineWireSubsystem.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
#include "LandscapeEditLayer.h"
#endif

namespace McpLandscapeHandlers {
// The landscape's first edit layer: FLandscapeLayer carried its guid up to 5.5, the edit layer object does from 5.6.
static FGuid FirstEditLayerGuid(const ALandscape *Landscape) {
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
  const ULandscapeEditLayerBase *Layer = Landscape->GetEditLayerConst(0);
  return Layer ? Layer->GetGuid() : FGuid();
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 5
  const FLandscapeLayer *Layer = Landscape->GetLayerConst(0);
  return Layer ? Layer->Guid : FGuid();
#else
  const FLandscapeLayer *Layer = Landscape->GetLayer(0);
  return Layer ? Layer->Guid : FGuid();
#endif
}

FMcpLandscapeEditScope::FMcpLandscapeEditScope(ALandscape *InLandscape, const TCHAR *Title, bool bInDeferMerge)
    : Transaction(MakeUnique<FMcpScopedEditorTransaction>(FText::FromString(Title), EMcpMutationDurability::EditorStateOnly,
                                                          TArray<UObject *>{InLandscape})),
      Landscape(InLandscape), bDeferMerge(bInDeferMerge) {
  Landscape->Modify();
  bLayered = Landscape->HasLayersContent();
  if (bLayered) {
    PreviousLayer = Landscape->GetEditingLayer();
    Landscape->SetEditingLayer(PreviousLayer.IsValid() ? PreviousLayer : FirstEditLayerGuid(Landscape));
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

static ALandscape *FindLandscapeForEdit(const FString &LandscapePath,
                                        const FString &LandscapeName,
                                        const FString &ActorPath) {
  if (UWorld *World = McpHandlerUtils::GetEditorWorld()) {
    for (TActorIterator<ALandscape> It(World); It; ++It) {
      if ((!LandscapeName.IsEmpty() &&
           It->GetActorLabel().Equals(LandscapeName, ESearchCase::IgnoreCase)) ||
          (!LandscapePath.IsEmpty() &&
           It->GetPackage()->GetPathName().Equals(LandscapePath,
                                                  ESearchCase::IgnoreCase)) ||
          (!ActorPath.IsEmpty() &&
           It->GetPathName().Equals(ActorPath, ESearchCase::IgnoreCase))) {
        return *It;
      }
    }
  }
  return LandscapePath.IsEmpty()
             ? nullptr
             : Cast<ALandscape>(StaticLoadObject(ALandscape::StaticClass(),
                                                 nullptr, *LandscapePath));
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
  ALandscape *Landscape = FindLandscapeForEdit(LandscapePath, LandscapeName, ActorPath);
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
}
