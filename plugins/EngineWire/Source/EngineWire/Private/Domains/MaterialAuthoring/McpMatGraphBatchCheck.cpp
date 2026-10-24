#include "Domains/MaterialAuthoring/McpMatPrivate.h"

#include "Domains/MaterialAuthoring/McpMatBatchSteps.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

// What build_material_graph checks before a step runs (split from McpMatGraphBatch.cpp): each step's edit is one the
// batch runs and each "$id" names an earlier step that makes a node, resolved as the run resolves it. Node names are
// not looked up: a step may name a parameter an earlier step of the batch adds. A problem refuses the batch with
// nothing applied, and dryRun answers with every one and runs nothing.
namespace McpMaterialAuthoringHandlers
{
namespace
{
// The node adders the dispatcher runs (each handler's add_* names and McpMatAddClassNode's table): an "add_" prefix let
// a misspelt adder pass the check and stop the run once the steps before it were applied.
const TSet<FString>& MaterialNodeAdders()
{
  static const TSet<FString> Adders = {
      TEXT("add_material_node"), TEXT("add_texture_sample"), TEXT("add_texture_coordinate"), TEXT("add_scalar_parameter"),
      TEXT("add_vector_parameter"), TEXT("add_static_switch_parameter"), TEXT("add_math_node"),
      TEXT("add_custom_expression"), TEXT("add_landscape_layer"), TEXT("add_function_input"), TEXT("add_function_output"),
      TEXT("add_world_position"), TEXT("add_vertex_normal"), TEXT("add_pixel_depth"), TEXT("add_fresnel"),
      TEXT("add_reflection_vector"), TEXT("add_panner"), TEXT("add_rotator"), TEXT("add_noise"), TEXT("add_voronoi"),
      TEXT("add_if"), TEXT("add_switch")};
  return Adders;
}
} // namespace

// Additive edits only: deleting and disconnecting stay single calls behind
// their own consent, and creating an asset is not a graph edit.
bool IsBatchableMaterialEdit(const FString& Edit)
{
  static const TSet<FString> Others = {
      TEXT("connect_nodes"), TEXT("connect_material_pins"), TEXT("use_material_function"),
      TEXT("set_node_position"), TEXT("update_custom_expression"), TEXT("set_blend_mode"),
      TEXT("set_shading_model"), TEXT("set_material_domain"), TEXT("set_two_sided")};
  return MaterialNodeAdders().Contains(Edit) || Others.Contains(Edit);
}

bool CreatesNode(const FString& Edit)
{
  return MaterialNodeAdders().Contains(Edit) || Edit == TEXT("use_material_function");
}

bool HasPosition(const TSharedPtr<FJsonObject>& Step)
{
  return Step->HasField(TEXT("x")) || Step->HasField(TEXT("posX"));
}

// "from": "$uv.G" / "to": "Main.EmissiveColor" -> connect_nodes' node and pin fields.
void ExpandEndpoint(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Key, const TCHAR* NodeField, const TCHAR* PinField)
{
  FString Endpoint, Node, Pin;
  if (!Payload->TryGetStringField(Key, Endpoint)) {
    return;
  }
  if (!Endpoint.Split(TEXT("."), &Node, &Pin)) {
    Node = Endpoint;
  }
  Payload->SetStringField(NodeField, Node);
  if (!Pin.IsEmpty()) {
    Payload->SetStringField(PinField, Pin);
  }
  Payload->RemoveField(Key);
}

// The step's own fields over the batch's shared ones (assetPath).
TSharedPtr<FJsonObject> BuildStepPayload(const TSharedPtr<FJsonObject>& Batch, const TSharedPtr<FJsonObject>& Step, const FString& Edit)
{
  TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
  for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Batch->Values) {
    if (Pair.Key != TEXT("operations")) {
      Out->SetField(Pair.Key, Pair.Value);
    }
  }
  for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Step->Values) {
    // One asset per batch: the final compile and save are the batch's, so a step
    // naming another material would edit it and never save it.
    if (Pair.Key != TEXT("assetPath") && Pair.Key != TEXT("materialPath")) {
      Out->SetField(Pair.Key, Pair.Value);
    }
  }
  Out->RemoveField(TEXT("edit"));
  Out->RemoveField(TEXT("id"));
  Out->RemoveField(TEXT("nodeKind"));
  Out->SetStringField(TEXT("subAction"), Edit);
  ExpandEndpoint(Out, TEXT("from"), TEXT("sourceNodeId"), TEXT("sourcePin"));
  ExpandEndpoint(Out, TEXT("to"), TEXT("targetNodeId"), TEXT("inputName"));
  // connect_nodes reads the target pin as inputName, and add_material_node reads
  // only x/y; the published spellings are targetPin and posX/posY.
  FString Pin;
  if (!Out->HasField(TEXT("inputName")) && Out->TryGetStringField(TEXT("targetPin"), Pin)) {
    Out->SetStringField(TEXT("inputName"), Pin);
  }
  double Coord = 0.0;
  if (!Out->HasField(TEXT("x")) && Out->TryGetNumberField(TEXT("posX"), Coord)) {
    Out->SetNumberField(TEXT("x"), Coord);
  }
  if (!Out->HasField(TEXT("y")) && Out->TryGetNumberField(TEXT("posY"), Coord)) {
    Out->SetNumberField(TEXT("y"), Coord);
  }
  return Out;
}

// "$name" -> the node id an earlier step created under `id: "name"`.
bool ResolveAliases(const TMap<FString, FString>& Aliases, const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
  for (const TCHAR* Field : {TEXT("sourceNodeId"), TEXT("targetNodeId"), TEXT("nodeId")}) {
    FString Ref;
    if (!Payload->TryGetStringField(Field, Ref) || !Ref.StartsWith(TEXT("$"))) {
      continue;
    }
    const FString* NodeId = Aliases.Find(Ref.RightChop(1));
    if (!NodeId) {
      TArray<FString> Known;
      Aliases.GenerateKeyArray(Known);
      OutError = FString::Printf(TEXT("%s '%s' names no earlier step. Ids defined so far: %s."), Field, *Ref,
                                 Known.Num() > 0 ? *FString::Join(Known, TEXT(", ")) : TEXT("<none>"));
      return false;
    }
    Payload->SetStringField(Field, *NodeId);
  }
  return true;
}

FString DescribeUnbatchableMaterialEdit(const FString& Edit)
{
  static const TSet<FString> Unbatched = {TEXT("delete_node"), TEXT("remove_material_node"), TEXT("disconnect_nodes"),
                                          TEXT("break_material_connections")};
  const TCHAR* Why = Edit.IsEmpty() ? TEXT("each step needs `edit`")
                     : Unbatched.Contains(Edit)
                         ? TEXT("that edit is not batched (deleting and disconnecting keep their own consent, so call them on "
                                "their own); a step's edit is")
                         : TEXT("no material edit has that name; a step's edit is");
  return FString::Printf(TEXT("%s: a node adder (add_material_node, add_scalar_parameter, add_math_node, "
                              "add_custom_expression, ...), use_material_function, connect_nodes, set_node_position, "
                              "update_custom_expression, set_blend_mode, set_shading_model, set_material_domain or set_two_sided"),
                         Why);
}

TArray<TSharedPtr<FJsonValue>> CheckMaterialBatch(const TSharedPtr<FJsonObject>& Payload, const TArray<TSharedPtr<FJsonValue>>& Steps)
{
  TArray<TSharedPtr<FJsonValue>> Problems;
  TMap<FString, FString> Ids; // the id of an earlier node-making step -> a stand-in for the node id the run would get
  for (int32 Index = 0; Index < Steps.Num(); ++Index) {
    const TSharedPtr<FJsonObject>* StepPtr = nullptr;
    FString Edit, Error;
    const bool bStep = Steps[Index].IsValid() && Steps[Index]->TryGetObject(StepPtr) && StepPtr &&
                       (*StepPtr)->TryGetStringField(TEXT("edit"), Edit);
    Edit = bStep ? McpMaterialBatchStepEdit(Edit, *StepPtr) : Edit;
    if (!bStep || !IsBatchableMaterialEdit(Edit)) {
      Error = DescribeUnbatchableMaterialEdit(Edit);
    } else {
      ResolveAliases(Ids, BuildStepPayload(Payload, *StepPtr, Edit), Error);
    }
    if (!Error.IsEmpty()) {
      TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
      Row->SetNumberField(TEXT("index"), Index);
      Row->SetStringField(TEXT("edit"), Edit);
      Row->SetStringField(TEXT("errorCode"), TEXT("INVALID_OPERATION"));
      Row->SetStringField(TEXT("error"), Error);
      Problems.Add(MakeShared<FJsonValueObject>(Row));
    }
    FString Id;
    if (bStep && CreatesNode(Edit) && (*StepPtr)->TryGetStringField(TEXT("id"), Id) && !Id.IsEmpty()) {
      Ids.Add(Id, FString::Printf(TEXT("operations[%d]"), Index));
    }
  }
  return Problems;
}

bool AnswerMaterialBatchCheck(UEngineWireSubsystem* Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                              const TArray<TSharedPtr<FJsonValue>>& Steps, const TArray<TSharedPtr<FJsonValue>>& Problems,
                              const bool bDryRun)
{
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetNumberField(TEXT("checked"), Steps.Num());
  if (bDryRun) {
    Data->SetBoolField(TEXT("dryRun"), true);
  }
  if (Problems.Num() == 0) {
    int32 WouldCreate = 0;
    for (const TSharedPtr<FJsonValue>& Value : Steps) {
      const TSharedPtr<FJsonObject>* Step = nullptr;
      FString Edit;
      WouldCreate += Value.IsValid() && Value->TryGetObject(Step) && Step && (*Step)->TryGetStringField(TEXT("edit"), Edit) &&
                     CreatesNode(McpMaterialBatchStepEdit(Edit, *Step)) ? 1 : 0;
    }
    Data->SetNumberField(TEXT("wouldCreate"), WouldCreate);
    Bridge->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("build_material_graph dryRun: all %d steps "
        "pass the checks a run makes before it starts (edits and $id references); node names, pins and values are checked "
        "only when it runs. Nothing ran."), Steps.Num()), Data);
    return true;
  }
  const TSharedPtr<FJsonObject> First = Problems[0]->AsObject();
  const int32 FirstIndex = static_cast<int32>(First->GetNumberField(TEXT("index")));
  Data->SetNumberField(TEXT("succeeded"), 0);
  Data->SetNumberField(TEXT("failedIndex"), FirstIndex);
  Data->SetArrayField(TEXT("problems"), Problems);
  Bridge->SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("build_material_graph%s checked every step "
      "before running any: %d would fail, first operations[%d]: %s Nothing was applied."), bDryRun ? TEXT(" dryRun") : TEXT(""),
      Problems.Num(), FirstIndex, *First->GetStringField(TEXT("error"))), Data, First->GetStringField(TEXT("errorCode")));
  return true;
}
} // namespace McpMaterialAuthoringHandlers
