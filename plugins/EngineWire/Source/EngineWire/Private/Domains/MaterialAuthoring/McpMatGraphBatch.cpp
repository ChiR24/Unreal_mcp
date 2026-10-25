#include "Domains/MaterialAuthoring/McpMatPrivate.h"

#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Domains/MaterialAuthoring/McpMatBatchSteps.h"
#include "Misc/PackageName.h"

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

bool HandleBuildMaterialGraph(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
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
  // Every edit and "$id" is checked before a step runs: a misspelled id at step 10 left steps 0-9 applied.
  const TArray<TSharedPtr<FJsonValue>> Problems = CheckMaterialBatch(Payload, *Steps);
  if (Problems.Num() > 0 || GetJsonBoolField(Payload, TEXT("dryRun"), false)) {
    return AnswerMaterialBatchCheck(Bridge, RequestId, Socket, *Steps, Problems, GetJsonBoolField(Payload, TEXT("dryRun"), false));
  }

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
        !(*StepPtr)->TryGetStringField(TEXT("edit"), Edit) ||
        !IsBatchableMaterialEdit(Edit = McpMaterialBatchStepEdit(Edit, *StepPtr))) {
      Error = DescribeUnbatchableMaterialEdit(Edit);
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
      // The steps before it were applied without a recompile of their own (the batch compiles once, at its end): compile what is there.
      if (Index > 0) { McpFinishMaterialEdit(RequestId, HostOuter); }
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

  // A step's own recompile logged the half-built graph ("missing input 11 (OB)" before the connect
  // feeding it) and the receipt showed that as current. Steps now leave the recompile to the compile
  // below (McpFinishMaterialEdit); this forgets what any other compile of the material logged meanwhile.
  // The compile below is the verdict, and it logs again if the material still fails. The engine names the asset in that line by the file it is
  // saved in ("[AssetLog] ...\Content\Dir\M_X.uasset: Failed to compile Material ...") and by its
  // object path only while no file exists, so both are matched: the object path alone left every
  // line of a saved material on the receipt, in warnings and in engineWarnings.
  const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
  const FString FileName = FPackageName::GetShortName(*PackageName) + FPackageName::GetAssetPackageExtension();
  Bridge->ForgetCapturedMessages([&PackageName, &FileName](const FString& Line) {
    return Line.StartsWith(TEXT("[LogMaterial]")) && (Line.Contains(PackageName)
        || Line.Contains(TEXT("\\") + FileName) || Line.Contains(TEXT("/") + FileName));
  });

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
  // Named so the receipt carries the material as its handle and change; it listed neither.
  Result->SetStringField(TEXT("assetPath"), AssetPath);
  McpSetBatchSteps(Result, Results, NodeIds);
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
