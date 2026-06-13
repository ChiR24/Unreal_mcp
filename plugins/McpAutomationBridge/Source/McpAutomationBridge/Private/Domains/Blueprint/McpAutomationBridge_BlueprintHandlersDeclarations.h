#pragma once

FBlueprintActionContext BuildBlueprintActionContext(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &Action, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
FBlueprintActionContext BuildScsActionContext(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &Action, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool ActionMatchesPattern(const FBlueprintActionContext &Context,
                          const TCHAR *Pattern);
bool ActionMatchesPatternImpl(const FString &Lower,
                              const FString &AlphaNumLower,
                              const TCHAR *Pattern);
void DiagnosticPatternChecks(const FBlueprintActionContext &Context);
FString ResolveBlueprintRequestedPath(
    const TSharedPtr<FJsonObject> &LocalPayload);
UBlueprint *ResolveScsBlueprint(const TSharedPtr<FJsonObject> &Payload);
UEdGraphPin *FMcpAutomationBridge_FindExecPin(UEdGraphNode *Node,
                                              EEdGraphPinDirection Direction);
UEdGraphPin *FMcpAutomationBridge_FindPreferredEventExec(UEdGraph *Graph);
UEdGraphPin *FMcpAutomationBridge_FindDataPin(
    UEdGraphNode *Node, EEdGraphPinDirection Direction,
    const FName &PreferredName = NAME_None);
UK2Node_VariableGet *FMcpAutomationBridge_CreateVariableGetter(
    UEdGraph *Graph, const FMemberReference &VarRef, float NodePosX,
    float NodePosY);
bool FMcpAutomationBridge_AttachValuePin(UK2Node_VariableSet *VarSet,
                                         UEdGraph *Graph,
                                         const UEdGraphSchema_K2 *Schema,
                                         bool &bOutLinked);
bool FMcpAutomationBridge_EnsureExecLinked(UEdGraph *Graph);
void FMcpAutomationBridge_LogConnectionFailure(
    const TCHAR *Context, UEdGraphPin *SourcePin, UEdGraphPin *TargetPin,
    const FPinConnectionResponse &Response);
FEdGraphPinType FMcpAutomationBridge_MakePinType(const FString &InType);
void FMcpAutomationBridge_AddUserDefinedPin(UK2Node *Node,
                                            const FString &PinName,
                                            const FString &PinType,
                                            EEdGraphPinDirection Direction);
UFunction *FMcpAutomationBridge_ResolveFunction(UBlueprint *Blueprint,
                                                const FString &FunctionName);
FProperty *FMcpAutomationBridge_FindProperty(UBlueprint *Blueprint,
                                             const FString &PropertyName);
FString FMcpAutomationBridge_JsonValueToString(
    const TSharedPtr<FJsonValue> &Value);
FName FMcpAutomationBridge_ResolveMetadataKey(const FString &RawKey);
FString FMcpAutomationBridge_DescribePinType(const FEdGraphPinType &PinType);
void FMcpAutomationBridge_AppendPinsJson(
    const TArray<TSharedPtr<FUserPinInfo>> &Pins,
    TArray<TSharedPtr<FJsonValue>> &Out);
bool FMcpAutomationBridge_CollectVariableMetadata(
    const UBlueprint *Blueprint, const FBPVariableDescription &VarDesc,
    TSharedPtr<FJsonObject> &OutMetadata);
TSharedPtr<FJsonObject> FMcpAutomationBridge_BuildVariableJson(
    const UBlueprint *Blueprint, const FBPVariableDescription &VarDesc);
FString FMcpAutomationBridge_DescribePropertyType(const FProperty *Property);
void FMcpAutomationBridge_AnnotateVariableJson(
    const TSharedPtr<FJsonObject> &Obj, const UBlueprint *RequestedBlueprint,
    const UBlueprint *DeclaringBlueprint, bool bIsSCSVariable);
TArray<TSharedPtr<FJsonValue>> FMcpAutomationBridge_CollectBlueprintVariables(
    UBlueprint *Blueprint);
TSharedPtr<FJsonObject> FMcpAutomationBridge_CollectBlueprintDefaults(
    UBlueprint *Blueprint, const TArray<TSharedPtr<FJsonValue>> &Variables);
TArray<TSharedPtr<FJsonValue>> FMcpAutomationBridge_CollectBlueprintFunctions(
    UBlueprint *Blueprint);
void FMcpAutomationBridge_CollectEventPins(
    UK2Node *Node, TArray<TSharedPtr<FJsonValue>> &Out);
TArray<TSharedPtr<FJsonValue>> FMcpAutomationBridge_CollectBlueprintEvents(
    UBlueprint *Blueprint);
TSharedPtr<FJsonObject> FMcpAutomationBridge_FindNamedEntry(
    const TArray<TSharedPtr<FJsonValue>> &Array, const FString &FieldName,
    const FString &DesiredValue);
TSharedPtr<FJsonObject> FMcpAutomationBridge_EnsureBlueprintEntry(
    const FString &Key);
TSharedPtr<FJsonObject> FMcpAutomationBridge_BuildBlueprintSnapshot(
    UBlueprint *Blueprint, const FString &NormalizedPath);

struct FModifyScsState {
  FString BlueprintPath;
  TArray<FString> CandidatePaths;
  const TArray<TSharedPtr<FJsonValue>> *OperationsArray = nullptr;
  bool bCompile = false;
  bool bSave = false;
  FString NormalizedBlueprintPath;
  FString LoadError;
  TArray<FString> TriedCandidates;
  TArray<TSharedPtr<FJsonValue>> DeferredOps;
  TSharedPtr<FJsonObject> CompletionResult;
  TArray<FString> LocalWarnings;
  TArray<TSharedPtr<FJsonValue>> FinalSummaries;
  bool bOk = false;
  bool bSaveResult = false;
};

bool PrepareModifyScsPayload(const FBlueprintActionContext &Context,
                             FModifyScsState &State);
bool ResolveModifyScsTarget(const FBlueprintActionContext &Context,
                            FModifyScsState &State);
bool AcquireModifyScsBusy(const FBlueprintActionContext &Context,
                          FModifyScsState &State);
bool ValidateModifyScsOperations(const FBlueprintActionContext &Context,
                                 FModifyScsState &State);
bool LoadModifyScsBlueprint(const FBlueprintActionContext &Context,
                            FModifyScsState &State, UBlueprint *&LocalBP,
                            USimpleConstructionScript *&LocalSCS);
void ApplyModifyScsOperation(UBlueprint *LocalBP,
                             USimpleConstructionScript *LocalSCS,
                             const FString &NormalizedType,
                             const TSharedPtr<FJsonObject> &Op,
                             TSharedPtr<FJsonObject> OpSummary);
void FinalizeModifyScsResponse(const FBlueprintActionContext &Context,
                               FModifyScsState &State,
                               UBlueprint *LocalBP);
UEdGraph *FindOrCreateBlueprintNodeGraph(UBlueprint *BP,
                                         const FString &GraphName);
UEdGraphNode *CreateBlueprintGraphNode(UEdGraph *TargetGraph,
                                       UBlueprint *BP,
                                       const FString &NodeType,
                                       const FString &FunctionName,
                                       const FString &VariableName,
                                       const FString &NodeName,
                                       FString &OutErrorMessage,
                                       FString &OutErrorCode,
                                       TSharedPtr<FJsonObject> &OutErrorResult);
void LinkBlueprintGraphNodePins(UEdGraph *TargetGraph, UEdGraphNode *NewNode,
                                bool &bExecLinked, bool &bValueLinked);
void SendBlueprintAddNodeResult(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    const FString &RegistryKey, UEdGraph *TargetGraph, UEdGraphNode *NewNode,
    float PosX, float PosY, bool bSaved, bool bExecLinked, bool bValueLinked,
    const FString &NodeName, const FString &FunctionName,
    const FString &VariableName);
void SendBlueprintAddEventResult(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UBlueprint *BP,
    const FString &RegistryKey, const FName &EventName,
    const FString &FinalType, const TArray<TSharedPtr<FJsonValue>> &Params,
    bool bSaved);
void SendBlueprintAddFunctionResult(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UBlueprint *Blueprint,
    const FString &RegistryKey, const FString &FuncName, bool bIsPublic,
    const TArray<TSharedPtr<FJsonValue>> &Inputs,
    const TArray<TSharedPtr<FJsonValue>> &Outputs, bool bSaved);

bool HandleBlueprintModifyScs(const FBlueprintActionContext &Context);
bool HandleBlueprintScsWrappers(const FBlueprintActionContext &Context);
bool HandleBlueprintSetVariableMetadata(const FBlueprintActionContext &Context);
bool HandleBlueprintAddConstructionScript(const FBlueprintActionContext &Context);
bool HandleBlueprintAddVariable(const FBlueprintActionContext &Context);
bool HandleBlueprintSetDefaultLiteral(const FBlueprintActionContext &Context);
bool HandleBlueprintRemoveRenameVariable(const FBlueprintActionContext &Context);
bool HandleBlueprintAddEvent(const FBlueprintActionContext &Context);
bool HandleBlueprintRemoveEvent(const FBlueprintActionContext &Context);
bool HandleBlueprintAddFunction(const FBlueprintActionContext &Context);
bool HandleBlueprintRemoveFunction(const FBlueprintActionContext &Context);
bool HandleBlueprintSetDefaultObject(const FBlueprintActionContext &Context);
bool HandleBlueprintCompile(const FBlueprintActionContext &Context);
bool HandleBlueprintProbeCreateExists(const FBlueprintActionContext &Context);
bool HandleBlueprintGet(const FBlueprintActionContext &Context);
bool HandleBlueprintAddNode(const FBlueprintActionContext &Context);
bool HandleBlueprintConnectPins(const FBlueprintActionContext &Context);
bool HandleBlueprintEnsureProbe(const FBlueprintActionContext &Context);
bool HandleBlueprintSetMetadata(const FBlueprintActionContext &Context);
bool HandleScsAddComponent(const FBlueprintActionContext &Context);
bool HandleScsSetTransform(const FBlueprintActionContext &Context);
bool HandleScsRemoveComponent(const FBlueprintActionContext &Context);
bool HandleScsGet(const FBlueprintActionContext &Context);
bool HandleScsReparentComponent(const FBlueprintActionContext &Context);
bool HandleScsSetProperty(const FBlueprintActionContext &Context);
