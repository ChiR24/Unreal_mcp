// edit_timeline: a Blueprint timeline, what a Timeline node plays, is made or changed: its length, play options and
// float, vector, color and event tracks with their keys, as the editor's timeline panel sets them. A Timeline node made
// over MCP had no timeline behind it (titled "Add Timeline..."), and no call could key one.
#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/Blueprint/Timeline/McpBPTimeline.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Components/TimelineComponent.h"
#include "Engine/Blueprint.h"
#include "K2Node_Timeline.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"

namespace McpBlueprintHandlers {
namespace {
// A timeline becomes a member of its name, so a name a variable, function or component holds is refused, as the
// editor's rename box refuses it.
UTimelineTemplate *MakeTimeline(UBlueprint *Blueprint, FName Name, FString &OutError) {
  FKismetNameValidator Validator(Blueprint);
  const auto Validity = Validator.IsValid(Name.ToString());
  if (Validity != EValidatorResult::Ok) {
    OutError = FString::Printf(TEXT("%s cannot name a timeline: %s"), *Name.ToString(),
                               *INameValidatorInterface::GetErrorString(Name.ToString(), Validity));
    return nullptr;
  }
  UTimelineTemplate *Template = FBlueprintEditorUtils::AddNewTimeline(Blueprint, Name);
  if (!Template) {
    OutError = TEXT("Only an Actor Blueprint holds timelines");
  }
  return Template;
}
} // namespace

bool McpBindTimelineNode(UEdGraphNode *Node, UBlueprint *Blueprint, const FString &TimelineName, FString &OutError) {
  UK2Node_Timeline *Timeline = Cast<UK2Node_Timeline>(Node);
  if (!Timeline || !Blueprint) {
    return true;
  }
  const FString Requested = TimelineName.TrimStartAndEnd();
  const FName Name = Requested.IsEmpty() ? FBlueprintEditorUtils::FindUniqueTimelineName(Blueprint) : FName(*Requested);
  UTimelineTemplate *Template = Blueprint->FindTimelineTemplateByVariableName(Name);
  if (!Timeline->IsCompatibleWithGraph(Timeline->GetGraph())) {
    OutError = TEXT("A Timeline goes in an Actor Blueprint's event graph, not in a function, a macro or another kind of "
                    "Blueprint");
  } else if (Template && FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, Template)) {
    OutError = FString::Printf(TEXT("Timeline '%s' already has its node: one node plays a timeline"), *Name.ToString());
  } else if (Template || MakeTimeline(Blueprint, Name, OutError)) {
    Timeline->TimelineName = Name;
    Timeline->ReconstructNode();
    return true;
  }
  Timeline->GetGraph()->RemoveNode(Timeline);
  return false;
}

void McpDescribeTimelineNode(const UEdGraphNode *Node, const TSharedPtr<FJsonObject> &Result) {
  if (const UK2Node_Timeline *Timeline = Cast<UK2Node_Timeline>(Node)) {
    Result->SetStringField(TEXT("timelineName"), Timeline->TimelineName.ToString());
  }
}

bool HandleBlueprintEditTimeline(const FBlueprintActionContext &Context) {
  using namespace McpTimeline;
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (!ActionMatchesPattern(TEXT("edit_timeline"))) {
    return false;
  }
  const FString Path = ResolveBlueprintRequestedPath();
  FString Name, Error;
  LocalPayload->TryGetStringField(TEXT("timelineName"), Name);
  Name.TrimStartAndEndInline();
  double Length = 0.0;
  const bool bLength = LocalPayload->TryGetNumberField(TEXT("length"), Length);
  TArray<FTrackEdit> Tracks;
  if (Path.IsEmpty() || Name.IsEmpty()) {
    Error = TEXT("blueprintPath and timelineName are required");
  } else if (bLength && Length <= KINDA_SMALL_NUMBER) {
    Error = TEXT("length is in seconds and must be above 0");
  }
  if (!Error.IsEmpty() || !ParseTracks(LocalPayload, Tracks, Error)) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Error, nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString Normalized;
  UBlueprint *Blueprint = LoadBlueprintAsset(Path, Normalized, Error);
  if (!Blueprint) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                  Error.IsEmpty() ? TEXT("Failed to load blueprint") : Error, nullptr,
                                  TEXT("BLUEPRINT_NOT_FOUND"));
    return true;
  }
  // A track named again keeps its type and its own curve; anything else is refused before the timeline changes.
  UTimelineTemplate *Template = Blueprint->FindTimelineTemplateByVariableName(FName(*Name));
  for (const FTrackEdit &Edit : Tracks) {
    FTTTrackBase::ETrackType Type = Edit.Type;
    const int32 Index = Template ? FindTrack(Template, Edit.Name, Type) : INDEX_NONE;
    if (Index != INDEX_NONE && (Type != Edit.Type || !OwnCurve(Template, Type, Index))) {
      const TCHAR *Problem = Type != Edit.Type ? TEXT("already exists as type") : TEXT("plays a curve asset of type");
      Error = FString::Printf(TEXT("Track %s %s %s; give the %s track another name"), *Edit.Name.ToString(), Problem,
                              TrackTypeNames[Type], TrackTypeNames[Edit.Type]);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Error, nullptr, TEXT("TRACK_TYPE_MISMATCH"));
      return true;
    }
  }
  const bool bCreated = !Template;
  if (bCreated && (Template = MakeTimeline(Blueprint, FName(*Name), Error)) == nullptr) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Error, nullptr, TEXT("TIMELINE_NOT_CREATED"));
    return true;
  }
  Template->Modify();
  bool bFlag = false;
  if (bLength) {
    Template->TimelineLength = static_cast<float>(Length);
  }
  if (LocalPayload->TryGetBoolField(TEXT("useLastKeyframe"), bFlag)) {
    Template->LengthMode = bFlag ? TL_LastKeyFrame : TL_TimelineLength;
  }
  if (LocalPayload->TryGetBoolField(TEXT("autoPlay"), bFlag)) {
    Template->bAutoPlay = bFlag;
  }
  if (LocalPayload->TryGetBoolField(TEXT("loop"), bFlag)) {
    Template->bLoop = bFlag;
  }
  if (LocalPayload->TryGetBoolField(TEXT("ignoreTimeDilation"), bFlag)) {
    Template->bIgnoreTimeDilation = bFlag;
  }
  for (const FTrackEdit &Edit : Tracks) {
    FTTTrackBase::ETrackType Type = Edit.Type;
    const int32 Index = FindTrack(Template, Edit.Name, Type);
    WriteKeys(Index == INDEX_NONE ? AddTrack(Blueprint, Template, Edit) : OwnCurve(Template, Type, Index), Edit);
  }
  // The node re-reads its timeline: a pin per track, and the play options it caches.
  UK2Node_Timeline *Node = FBlueprintEditorUtils::FindNodeForTimeline(Blueprint, Template);
  if (Node) {
    Node->ReconstructNode();
  }
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
  const bool bCompiled = McpSafeCompileBlueprint(Blueprint);
  const bool bSaved = McpSafeAssetSave(Blueprint);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("blueprintPath"), Normalized.IsEmpty() ? Path : Normalized);
  Resp->SetStringField(TEXT("timelineName"), Name);
  Resp->SetBoolField(TEXT("created"), bCreated);
  Resp->SetNumberField(TEXT("length"), Template->TimelineLength);
  Resp->SetBoolField(TEXT("useLastKeyframe"), Template->LengthMode == TL_LastKeyFrame);
  Resp->SetBoolField(TEXT("autoPlay"), Template->bAutoPlay != 0);
  Resp->SetBoolField(TEXT("loop"), Template->bLoop != 0);
  Resp->SetArrayField(TEXT("tracks"), DescribeTracks(Template));
  if (Node) {
    Resp->SetStringField(TEXT("nodeGuid"), Node->NodeGuid.ToString());
  }
  McpHandlerUtils::AddVerification(Resp, Blueprint);
  if (!bCompiled || !bSaved) {
    Resp->SetStringField(TEXT("warning"), !bCompiled ? TEXT("The Blueprint did not compile cleanly; compile it to read "
                                                            "the errors.")
                                                     : TEXT("The Blueprint changed but could not be saved."));
  }
  const TCHAR *Done = bCreated ? TEXT("created") : TEXT("updated");
  const FString Message = Node ? FString::Printf(TEXT("Timeline %s"), Done)
                               : FString::Printf(TEXT("Timeline %s; no node plays it yet: edit_graph create_node "
                                                      "nodeType Timeline with timelineName %s"), Done, *Name);
  Bridge.SendAutomationResponse(RequestingSocket, RequestId, true, Message, Resp);
  return true;
}
} // namespace McpBlueprintHandlers
