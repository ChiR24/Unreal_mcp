#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
#include "Core/Requests/McpResponseCaptureRegistry.h"

// build_material_graph: one call that adds a material's nodes, wires them and
// sets its properties. A small gradient used to cost a describe + execute pair
// (and a consent nonce) per node and per wire. The batch runs the SAME
// single-step handlers in-process with their replies captured, so every step
// keeps its own checks. It stops at the first failing step; the steps before
// it stay applied and their node ids come back so the caller can go on.
namespace McpMaterialAuthoringHandlers
{
namespace
{
constexpr int32 MaxMaterialBatchSteps = 200;
constexpr int32 AutoRows = 5;

// Additive edits only: deleting and disconnecting stay single calls behind
// their own consent, and creating an asset is not a graph edit.
bool IsBatchableMaterialEdit(const FString& Edit)
{
  static const TSet<FString> Others = {
      TEXT("connect_nodes"), TEXT("connect_material_pins"), TEXT("use_material_function"),
      TEXT("set_node_position"), TEXT("update_custom_expression"), TEXT("set_blend_mode"),
      TEXT("set_shading_model"), TEXT("set_material_domain"), TEXT("set_two_sided")};
  return Edit.StartsWith(TEXT("add_")) || Others.Contains(Edit);
}

bool CreatesNode(const FString& Edit)
{
  return Edit.StartsWith(TEXT("add_")) || Edit == TEXT("use_material_function");
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

// Auto-placed nodes fill columns left of everything the graph already holds,
// the first created furthest left: material graphs flow right, into the output.
// Within a column each node sits below the last by that node's reported height
// (a vector parameter's swatch makes it ~320 tall, so a fixed pitch stacked them).
float AutoOriginX(UMaterial* Material, UMaterialFunction* Function)
{
  float Leftmost = 0.0f;
  auto Visit = [&Leftmost](UMaterialExpression* Expr) {
    if (Expr) {
      Leftmost = FMath::Min(Leftmost, static_cast<float>(Expr->MaterialExpressionEditorX));
    }
  };
  if (Material) {
    for (UMaterialExpression* Expr : MCP_GET_MATERIAL_EXPRESSIONS(Material)) { Visit(Expr); }
  } else if (Function) {
    for (UMaterialExpression* Expr : MCP_GET_FUNCTION_EXPRESSIONS(Function)) { Visit(Expr); }
  }
  return Leftmost - 320.0f;
}
} // namespace

bool HandleBuildMaterialGraph(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                              TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<void(const FString&, const TSharedPtr<FJsonObject>&)> RunStep)
{
  const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
  if (!Payload->TryGetArrayField(TEXT("operations"), Steps) || Steps->Num() == 0 || Steps->Num() > MaxMaterialBatchSteps) {
    Bridge->SendAutomationError(Socket, RequestId, FString::Printf(
        TEXT("build_material_graph needs `operations`: 1-%d steps, each {edit, ...that edit's params}, optionally `id` "
             "to name a created node for later steps as \"$id\"."), MaxMaterialBatchSteps), TEXT("INVALID_OPERATIONS"));
    return true;
  }
  LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();
  Payload->SetStringField(TEXT("assetPath"), AssetPath);

  int32 AutoTotal = 0;
  for (const TSharedPtr<FJsonValue>& Value : *Steps) {
    const TSharedPtr<FJsonObject>* Step = nullptr;
    FString Edit;
    if (Value.IsValid() && Value->TryGetObject(Step) && (*Step)->TryGetStringField(TEXT("edit"), Edit) && CreatesNode(Edit) && !HasPosition(*Step)) {
      ++AutoTotal;
    }
  }
  const int32 AutoColumns = FMath::DivideAndRoundUp(AutoTotal, AutoRows);
  const float OriginX = AutoOriginX(Material, Function);

  FMcpResponseCaptureRegistry& Capture = FMcpResponseCaptureRegistry::Get();
  TMap<FString, FString> Aliases;
  TSharedPtr<FJsonObject> NodeIds = MakeShared<FJsonObject>();
  TArray<TSharedPtr<FJsonValue>> Results;
  int32 AutoPlaced = 0;
  double CursorY = 0.0;
  for (int32 Index = 0; Index < Steps->Num(); ++Index) {
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetNumberField(TEXT("index"), Index);
    Results.Add(MakeShared<FJsonValueObject>(Entry));
    const TSharedPtr<FJsonObject>* StepPtr = nullptr;
    FString Edit, Alias, Error, ErrorCode = TEXT("INVALID_OPERATION");
    if (!(*Steps)[Index].IsValid() || !(*Steps)[Index]->TryGetObject(StepPtr) ||
        !(*StepPtr)->TryGetStringField(TEXT("edit"), Edit) || !IsBatchableMaterialEdit(Edit)) {
      Error = FString::Printf(TEXT("%s: a node adder (add_material_node, add_scalar_parameter, add_math_node, "
                   "add_custom_expression, ...), use_material_function, connect_nodes, set_node_position, "
                   "update_custom_expression, set_blend_mode, set_shading_model, set_material_domain or set_two_sided"),
          Edit.IsEmpty() ? TEXT("each step needs `edit`")
                         : TEXT("that edit is not batched (deleting and disconnecting keep their own consent, so call "
                                "them on their own); a step's edit is"));
    } else {
      Entry->SetStringField(TEXT("edit"), Edit);
      if ((*StepPtr)->TryGetStringField(TEXT("id"), Alias)) {
        Entry->SetStringField(TEXT("id"), Alias);
      }
      const TSharedPtr<FJsonObject> StepPayload = BuildStepPayload(Payload, *StepPtr, Edit);
      const bool bAutoPlaced = CreatesNode(Edit) && !HasPosition(*StepPtr);
      if (bAutoPlaced) {
        const int32 Slot = AutoPlaced++;
        CursorY = Slot % AutoRows == 0 ? -440.0 : CursorY;
        StepPayload->SetNumberField(TEXT("x"), OriginX - (AutoColumns - 1 - Slot / AutoRows) * 400.0f);
        StepPayload->SetNumberField(TEXT("y"), CursorY);
      }
      if (ResolveAliases(Aliases, StepPayload, Error)) {
        const FString StepId = FString::Printf(TEXT("%s#step%d"), *RequestId, Index);
        Capture.Begin(StepId);
        RunStep(StepId, StepPayload);
        const FMcpCapturedResponse Reply = Capture.End(StepId);
        FString NodeId, Warning;
        if (Reply.Result.IsValid() && Reply.Result->TryGetStringField(TEXT("nodeId"), NodeId)) {
          Entry->SetStringField(TEXT("nodeId"), NodeId);
        }
        if (Reply.Result.IsValid() && Reply.Result->TryGetStringField(TEXT("placementWarning"), Warning)) {
          Entry->SetStringField(TEXT("placementWarning"), Warning);
        }
        double Height = 0.0;
        if (bAutoPlaced && Reply.Result.IsValid()) {
          Reply.Result->TryGetNumberField(TEXT("estimatedHeight"), Height);
        }
        CursorY += FMath::Max(Height, 64.0) + 48.0;
        if (!Reply.bSuccess) {
          Error = Reply.bCaptured ? Reply.Message : FString(TEXT("the step sent no reply"));
          ErrorCode = Reply.ErrorCode.IsEmpty() ? TEXT("STEP_FAILED") : Reply.ErrorCode;
        } else if (!Alias.IsEmpty() && !NodeId.IsEmpty()) {
          Aliases.Add(Alias, NodeId);
          NodeIds->SetStringField(Alias, NodeId);
        }
      }
    }
    Entry->SetBoolField(TEXT("success"), Error.IsEmpty());
    if (!Error.IsEmpty()) {
      Entry->SetStringField(TEXT("error"), Error);
      TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
      Data->SetArrayField(TEXT("results"), Results);
      Data->SetObjectField(TEXT("nodeIds"), NodeIds);
      Data->SetNumberField(TEXT("succeeded"), Index);
      Data->SetNumberField(TEXT("failedIndex"), Index);
      Error.RemoveFromEnd(TEXT("."));
      Bridge->SendAutomationResponse(Socket, RequestId, false, FString::Printf(
          TEXT("build_material_graph stopped at operations[%d] (%s): %s. %s"),
          Index, Edit.IsEmpty() ? TEXT("no edit") : *Edit, *Error,
          Index == 0 ? TEXT("Nothing was applied.")
                     : *FString::Printf(TEXT("The %d step(s) before it were applied; their node ids are in nodeIds."), Index)),
          Data, ErrorCode);
      return true;
    }
  }

  // One recompile and save for the whole graph, through compile_material itself.
  TSharedPtr<FJsonObject> Compile = MakeShared<FJsonObject>();
  Compile->SetStringField(TEXT("subAction"), TEXT("compile_material"));
  Compile->SetStringField(TEXT("assetPath"), AssetPath);
  const FString CompileId = RequestId + TEXT("#compile");
  Capture.Begin(CompileId);
  RunStep(CompileId, Compile);
  const FMcpCapturedResponse Compiled = Capture.End(CompileId);
  bool bSaved = false;
  bool bCompiles = Compiled.bSuccess;
  const TArray<TSharedPtr<FJsonValue>>* CompileErrors = nullptr;
  if (Compiled.Result.IsValid()) {
    Compiled.Result->TryGetBoolField(TEXT("saved"), bSaved);
    Compiled.Result->TryGetBoolField(TEXT("compiled"), bCompiles);
    Compiled.Result->TryGetArrayField(TEXT("compileErrors"), CompileErrors);
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetArrayField(TEXT("results"), Results);
  Result->SetObjectField(TEXT("nodeIds"), NodeIds);
  Result->SetNumberField(TEXT("succeeded"), Results.Num());
  Result->SetBoolField(TEXT("compiled"), bCompiles);
  if (CompileErrors) {
    Result->SetArrayField(TEXT("compileErrors"), *CompileErrors);
  }
  Result->SetBoolField(TEXT("saved"), bSaved);
  Bridge->SendAutomationResponse(Socket, RequestId, true, bCompiles
      ? FString::Printf(TEXT("Ran %d material graph operations; %s compiles%s."), Results.Num(), *AssetPath,
                        bSaved ? TEXT(" and was saved") : TEXT(", but it was NOT saved"))
      : FString::Printf(TEXT("Ran %d material graph operations. %s"), Results.Num(), *Compiled.Message),
      Result);
  return true;
}
}
#endif
