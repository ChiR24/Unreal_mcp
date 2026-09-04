#include "Domains/Animation/McpAutomationBridge_AnimationHandlersActionContext.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Safety/McpSafeOperations.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequenceBase.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#if __has_include("AnimGraphNode_BlendListByInt.h") && __has_include("AnimGraphNode_SequencePlayer.h") && __has_include("AnimGraphNode_Root.h")
#include "AnimGraphNode_BlendListByInt.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#define MCP_HAS_BLEND_LIST_GRAPH 1
#else
#define MCP_HAS_BLEND_LIST_GRAPH 0
#endif

// create_blend_tree (dogfood #87): UE 5 has no "blend tree asset"; a blend tree is a graph of
// blend nodes inside an AnimBlueprint. This authors one: a Blend List (by int) node fed by one
// Sequence Player per animation, optionally wired into the AnimGraph output pose.
namespace McpAnimationHandlers {
#if WITH_EDITOR
namespace {
// FAnimNode_BlendListBase::BlendPose is protected; the pose count is the number of BlendPose_N input pins.
int32 CountBlendPosePins(const UEdGraphNode *Node) {
  int32 Count = 0;
  for (const UEdGraphPin *Pin : Node->Pins) {
    if (Pin && Pin->Direction == EGPD_Input && Pin->PinName.ToString().StartsWith(TEXT("BlendPose_"))) {
      ++Count;
    }
  }
  return Count;
}

UEdGraphPin *FindPinByName(UEdGraphNode *Node, const FString &Name, EEdGraphPinDirection Direction) {
  for (UEdGraphPin *Pin : Node->Pins) {
    if (Pin && Pin->Direction == Direction && Pin->PinName.ToString().Equals(Name, ESearchCase::IgnoreCase)) {
      return Pin;
    }
  }
  return nullptr;
}
} // namespace

bool HandleAnimationCreateBlendTreeAction(FActionContext &Context,
               const TSharedPtr<FJsonObject> &Payload) {
  TSharedPtr<FJsonObject> &Resp = Context.Resp;
  bool &bSuccess = Context.bSuccess;
  FString &Message = Context.Message;
  FString &ErrorCode = Context.ErrorCode;
  FString BlueprintPath;
  Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
  if (BlueprintPath.IsEmpty()) {
    Message = TEXT("blueprintPath is required for create_blend_tree (the AnimBlueprint that owns the graph)");
    ErrorCode = TEXT("INVALID_ARGUMENT");
    Resp->SetStringField(TEXT("error"), Message);
    return false; // the animation dispatcher sends Context.Resp when a handler returns false
  }
  FString TreeName;
  Payload->TryGetStringField(TEXT("treeName"), TreeName);
  if (TreeName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("name"), TreeName);
  }
  if (TreeName.IsEmpty()) {
    TreeName = TEXT("BlendTree");
  }
  bool bConnectToOutput = true;
  Payload->TryGetBoolField(TEXT("connectToOutput"), bConnectToOutput);
  TArray<FString> AnimationPaths;
  const TArray<TSharedPtr<FJsonValue>> *AnimationValues = nullptr;
  if (Payload->TryGetArrayField(TEXT("animations"), AnimationValues) && AnimationValues) {
    for (const TSharedPtr<FJsonValue> &Value : *AnimationValues) {
      if (Value.IsValid() && Value->Type == EJson::String) {
        AnimationPaths.Add(Value->AsString());
      }
    }
  }
  UAnimBlueprint *AnimBP = LoadObject<UAnimBlueprint>(nullptr, *BlueprintPath);
  if (!AnimBP) {
    Message = FString::Printf(TEXT("AnimBlueprint not found: %s"), *BlueprintPath);
    ErrorCode = TEXT("ASSET_NOT_FOUND");
    Resp->SetStringField(TEXT("error"), Message);
    return false; // the animation dispatcher sends Context.Resp when a handler returns false
  }
#if MCP_HAS_BLEND_LIST_GRAPH
  UEdGraph *AnimGraph = nullptr;
  for (UEdGraph *Graph : AnimBP->FunctionGraphs) {
    if (Graph && Graph->GetName() == TEXT("AnimGraph")) {
      AnimGraph = Graph;
      break;
    }
  }
  if (!AnimGraph) {
    Message = TEXT("Could not find AnimGraph in blueprint");
    ErrorCode = TEXT("GRAPH_NOT_FOUND");
    Resp->SetStringField(TEXT("error"), Message);
    return false; // the animation dispatcher sends Context.Resp when a handler returns false
  }
  const int32 PoseCount = FMath::Max(2, AnimationPaths.Num());
  FGraphNodeCreator<UAnimGraphNode_BlendListByInt> BlendCreator(*AnimGraph);
  UAnimGraphNode_BlendListByInt *BlendNode = BlendCreator.CreateNode();
  BlendNode->NodePosX = -300;
  BlendNode->NodePosY = 0;
  BlendNode->NodeComment = TreeName;
  BlendNode->bCommentBubbleVisible = true;
  BlendCreator.Finalize();
  int32 SafetyCounter = 0;
  while (CountBlendPosePins(BlendNode) < PoseCount && SafetyCounter++ < 64) {
    BlendNode->AddPinToBlendList();
  }
  const UEdGraphSchema *Schema = AnimGraph->GetSchema();
  TArray<TSharedPtr<FJsonValue>> Players;
  TArray<FString> Warnings;
  for (int32 Index = 0; Index < AnimationPaths.Num(); ++Index) {
    UAnimSequenceBase *Sequence = LoadObject<UAnimSequenceBase>(nullptr, *AnimationPaths[Index]);
    if (!Sequence) {
      Warnings.Add(FString::Printf(TEXT("animation not found: %s"), *AnimationPaths[Index]));
      continue;
    }
    FGraphNodeCreator<UAnimGraphNode_SequencePlayer> PlayerCreator(*AnimGraph);
    UAnimGraphNode_SequencePlayer *Player = PlayerCreator.CreateNode();
    Player->NodePosX = -700;
    Player->NodePosY = Index * 160;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
    Player->Node.Sequence = Sequence;
#else
    Player->Node.SetSequence(Sequence);
#endif
    PlayerCreator.Finalize();
    UEdGraphPin *Out = FindPinByName(Player, TEXT("Pose"), EGPD_Output);
    UEdGraphPin *In = FindPinByName(BlendNode, FString::Printf(TEXT("BlendPose_%d"), Index), EGPD_Input);
    const bool bLinked = Out && In && Schema && Schema->TryCreateConnection(Out, In);
    if (!bLinked) {
      Warnings.Add(FString::Printf(TEXT("could not connect %s to BlendPose_%d"), *Sequence->GetName(), Index));
    }
    TSharedPtr<FJsonObject> PlayerJson = MakeShared<FJsonObject>();
    PlayerJson->SetStringField(TEXT("nodeId"), Player->NodeGuid.ToString());
    PlayerJson->SetStringField(TEXT("animationPath"), Sequence->GetPathName());
    PlayerJson->SetNumberField(TEXT("poseIndex"), Index);
    PlayerJson->SetBoolField(TEXT("connected"), bLinked);
    Players.Add(MakeShared<FJsonValueObject>(PlayerJson));
  }
  bool bConnectedToOutput = false;
  if (bConnectToOutput) {
    for (UEdGraphNode *GraphNode : AnimGraph->Nodes) {
      if (UAnimGraphNode_Root *Root = Cast<UAnimGraphNode_Root>(GraphNode)) {
        UEdGraphPin *BlendOut = FindPinByName(BlendNode, TEXT("Pose"), EGPD_Output);
        UEdGraphPin *ResultIn = FindPinByName(Root, TEXT("Result"), EGPD_Input);
        if (BlendOut && ResultIn && Schema) {
          ResultIn->BreakAllPinLinks();
          bConnectedToOutput = Schema->TryCreateConnection(BlendOut, ResultIn);
        }
        break;
      }
    }
  }
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
  const bool bSaved = McpSafeOperations::McpSafeAssetSave(AnimBP);
  Resp->SetStringField(TEXT("blueprintPath"), AnimBP->GetPathName());
  Resp->SetStringField(TEXT("treeName"), TreeName);
  Resp->SetStringField(TEXT("blendNodeId"), BlendNode->NodeGuid.ToString());
  Resp->SetStringField(TEXT("blendNodeType"), TEXT("BlendListByInt"));
  Resp->SetNumberField(TEXT("poseCount"), CountBlendPosePins(BlendNode));
  Resp->SetArrayField(TEXT("players"), Players);
  Resp->SetBoolField(TEXT("connectedToOutput"), bConnectedToOutput);
  Resp->SetBoolField(TEXT("saved"), bSaved);
  if (Warnings.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> WarningValues;
    for (const FString &Warning : Warnings) {
      WarningValues.Add(MakeShared<FJsonValueString>(Warning));
    }
    Resp->SetArrayField(TEXT("warnings"), WarningValues);
  }
  bSuccess = true;
  Message = FString::Printf(TEXT("Blend tree '%s' authored: BlendListByInt with %d poses and %d sequence players%s"),
                            *TreeName, CountBlendPosePins(BlendNode), Players.Num(),
                            bConnectedToOutput ? TEXT(", wired to the AnimGraph output") : TEXT(""));
  return false; // the animation dispatcher sends Context.Resp when a handler returns false
#else
  Message = TEXT("Blend tree authoring needs the AnimGraph editor module (BlendListByInt / SequencePlayer nodes), which is unavailable in this build");
  ErrorCode = TEXT("NOT_SUPPORTED");
  Resp->SetStringField(TEXT("error"), Message);
  return false; // the animation dispatcher sends Context.Resp when a handler returns false
#endif
}
#endif
} // namespace McpAnimationHandlers
