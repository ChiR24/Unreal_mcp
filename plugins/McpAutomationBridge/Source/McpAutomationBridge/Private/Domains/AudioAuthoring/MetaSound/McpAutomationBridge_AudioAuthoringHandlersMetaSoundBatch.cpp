// build_metasound: many MetaSound graph edits in one call. Each step is an
// ordinary add_node / connect / disconnect / remove_node / set_default /
// add_input / add_output run by the same single-call handler, so a synth voice
// no longer costs a round trip per node, link and literal. Steps name what they
// create with `id`; later steps refer to it as "$id" (nodeId / nodeIds /
// sourceNodeId / targetNodeId, or "$id.Pin" in from/to).
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

namespace McpAudioAuthoring
{
namespace
{
constexpr int32 MaxMetaSoundBatchSteps = 200;

FString MetaSoundStepSubAction(const FString& Edit)
{
	static const TMap<FString, FString> Short = {
		{TEXT("add_node"), TEXT("add_metasound_node")}, {TEXT("connect"), TEXT("connect_metasound_nodes")},
		{TEXT("set_default"), TEXT("set_metasound_default")}, {TEXT("add_input"), TEXT("add_metasound_input")},
		{TEXT("add_output"), TEXT("add_metasound_output")}, {TEXT("remove_node"), TEXT("remove_metasound_node")},
		{TEXT("disconnect"), TEXT("disconnect_metasound_nodes")}};
	if (const FString* Mapped = Short.Find(Edit)) { return *Mapped; }
	for (const TPair<FString, FString>& Pair : Short)
	{
		if (Pair.Value == Edit) { return Edit; }
	}
	return FString();
}

// "$osc.Audio" is node "$osc" + pin "Audio". Only alias endpoints split: a real
// node or pin name may itself contain dots, so those need the explicit fields.
void ExpandMetaSoundEndpoint(const TSharedPtr<FJsonObject>& Step, const TCHAR* Key, const TCHAR* NodeField, const TCHAR* PinField)
{
	FString Endpoint;
	FString Node;
	FString Pin;
	if (Step->TryGetStringField(Key, Endpoint) && Endpoint.StartsWith(TEXT("$")) && Endpoint.Split(TEXT("."), &Node, &Pin))
	{
		Step->SetStringField(NodeField, Node);
		Step->SetStringField(PinField, Pin);
	}
}

// A pin a step names on a node this batch adds ("$id"), checked against that node class's pins before anything runs:
// empty when the pin is there, or when the node or its pins are unknown here (the step itself then decides).
FString MissingMetaSoundPin(const TMap<FString, FMcpMetaSoundNodeClassRequest>& Added, const TSharedPtr<FJsonObject>& Step,
	const TCHAR* NodeField, const TCHAR* PinField, bool bOutput, TArray<FString>& OutPins)
{
	FString Node;
	FString Pin;
	const FMcpMetaSoundNodeClassRequest* Request = Step->TryGetStringField(NodeField, Node) && Node.StartsWith(TEXT("$")) &&
		Step->TryGetStringField(PinField, Pin) ? Added.Find(Node.RightChop(1)) : nullptr;
	const TArray<FString>* Pins = Request ? (bOutput ? &Request->Outputs : &Request->Inputs) : nullptr;
	if (!Pins || Pins->Num() == 0 ||
		Pins->ContainsByPredicate([&Pin](const FString& Known) { return Known.StartsWith(Pin + TEXT(" ("), ESearchCase::IgnoreCase); }))
	{
		return FString();
	}
	OutPins = *Pins;
	return FString::Printf(TEXT("%s (%s) has no %s '%s'"), *Node, *Request->Requested, bOutput ? TEXT("output") : TEXT("input"), *Pin);
}

bool ResolveMetaSoundAliases(const TMap<FString, FString>& Aliases, const TSharedPtr<FJsonObject>& Step, FString& OutError)
{
	auto Resolve = [&Aliases, &OutError](const TCHAR* Field, FString& Ref)
	{
		if (!Ref.StartsWith(TEXT("$"))) { return true; }
		const FString* NodeId = Aliases.Find(Ref.RightChop(1));
		if (!NodeId)
		{
			TArray<FString> Known;
			Aliases.GenerateKeyArray(Known);
			OutError = FString::Printf(TEXT("%s '%s' names no earlier step; ids so far: %s"), Field, *Ref,
				Known.Num() > 0 ? *FString::Join(Known, TEXT(", ")) : TEXT("<none>"));
			return false;
		}
		Ref = *NodeId;
		return true;
	};
	for (const TCHAR* Field : {TEXT("nodeId"), TEXT("sourceNodeId"), TEXT("targetNodeId")})
	{
		FString Ref;
		if (!Step->TryGetStringField(Field, Ref)) { continue; }
		if (!Resolve(Field, Ref)) { return false; }
		Step->SetStringField(Field, Ref);
	}
	// remove_node takes several nodes at once.
	const TArray<TSharedPtr<FJsonValue>>* Refs = nullptr;
	if (Step->TryGetArrayField(TEXT("nodeIds"), Refs) && Refs)
	{
		TArray<TSharedPtr<FJsonValue>> Resolved;
		for (const TSharedPtr<FJsonValue>& Value : *Refs)
		{
			FString Ref;
			if (!Value.IsValid() || !Value->TryGetString(Ref)) { Resolved.Add(Value); continue; }
			if (!Resolve(TEXT("nodeIds"), Ref)) { return false; }
			Resolved.Add(MakeShared<FJsonValueString>(Ref));
		}
		Step->SetArrayField(TEXT("nodeIds"), Resolved);
	}
	return true;
}
}

TSharedPtr<FJsonObject> HandleMetaSoundBatchAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction != TEXT("build_metasound"))
	{
		return nullptr;
	}
	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (!Params->TryGetArrayField(TEXT("operations"), Steps) || Steps->Num() == 0 || Steps->Num() > MaxMetaSoundBatchSteps)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"), FString::Printf(
			TEXT("build_metasound needs `operations`: 1-%d steps, each {edit: add_node, connect, disconnect, remove_node, set_default, add_input or add_output, ...that edit's params}"),
			MaxMetaSoundBatchSteps));
	}

	// Every add_node class is looked up before any step runs: one the registry does not hold stopped the batch after
	// the steps before it were applied and saved, a voice's old nodes removed and its replacement never added.
	TMap<FString, FMcpMetaSoundNodeClassRequest> Added;
	for (int32 Index = 0; Index < Steps->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject>* StepObj = nullptr;
		FString Edit;
		if (!(*Steps)[Index].IsValid() || !(*Steps)[Index]->TryGetObject(StepObj) || !(*StepObj)->TryGetStringField(TEXT("edit"), Edit))
		{
			continue;
		}
		// So are the pins a connect or set_default names on a node the batch adds: a wrong output name ("Band Pass
		// Filter" for "Band Pass") stopped a batch after its nodes were added.
		const FString StepSubAction = MetaSoundStepSubAction(Edit);
		if (StepSubAction == TEXT("connect_metasound_nodes") || StepSubAction == TEXT("set_metasound_default"))
		{
			TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
			Step->Values = (*StepObj)->Values;
			ExpandMetaSoundEndpoint(Step, TEXT("from"), TEXT("sourceNodeId"), TEXT("sourceOutputName"));
			ExpandMetaSoundEndpoint(Step, TEXT("to"), TEXT("targetNodeId"), TEXT("targetInputName"));
			const bool bDefault = StepSubAction == TEXT("set_metasound_default");
			TArray<FString> Pins;
			const TCHAR* PinsKey = bDefault ? TEXT("availableInputs") : TEXT("sourceOutputs");
			FString Missing = MissingMetaSoundPin(Added, Step, bDefault ? TEXT("nodeId") : TEXT("sourceNodeId"),
				bDefault ? TEXT("inputName") : TEXT("sourceOutputName"), !bDefault, Pins);
			if (Missing.IsEmpty() && !bDefault)
			{
				PinsKey = TEXT("targetInputs");
				Missing = MissingMetaSoundPin(Added, Step, TEXT("targetNodeId"), TEXT("targetInputName"), false, Pins);
			}
			if (!Missing.IsEmpty())
			{
				TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
				TArray<TSharedPtr<FJsonValue>> PinValues;
				for (const FString& Pin : Pins) { PinValues.Add(MakeShared<FJsonValueString>(Pin)); }
				Details->SetArrayField(PinsKey, PinValues);
				Details->SetNumberField(TEXT("failedIndex"), Index);
				Details->SetNumberField(TEXT("succeeded"), 0);
				return McpHandlerUtils::BuildErrorResponse(TEXT("PIN_NOT_FOUND"), FString::Printf(
					TEXT("build_metasound: operations[%d] (%s): %s; nothing was applied."), Index, *Edit, *Missing), Details);
			}
			continue;
		}
		if (StepSubAction != TEXT("add_metasound_node"))
		{
			continue;
		}
		const FMcpMetaSoundNodeClassRequest Request = ResolveMetaSoundAddNodeClass(*StepObj);
		FString AddedId;
		if ((*StepObj)->TryGetStringField(TEXT("id"), AddedId)) { Added.Add(AddedId, Request); }
		if (!Request.Name.IsEmpty() && !Request.bInRegistry)
		{
			TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
			TArray<TSharedPtr<FJsonValue>> Candidates;
			for (const FString& Candidate : Request.Candidates) { Candidates.Add(MakeShared<FJsonValueString>(Candidate)); }
			Details->SetArrayField(TEXT("candidateNodeClasses"), Candidates);
			Details->SetNumberField(TEXT("failedIndex"), Index);
			Details->SetNumberField(TEXT("succeeded"), 0);
			const FString Hint = Request.Candidates.Num() > 0 ? FString(TEXT(" (see candidateNodeClasses)")) : FString();
			return McpHandlerUtils::BuildErrorResponse(TEXT("NODE_CLASS_NOT_FOUND"), FString::Printf(
				TEXT("build_metasound: operations[%d] (add_node) names '%s', which the MetaSound node registry does not hold%s; nothing was applied."),
				Index, *Request.Requested, *Hint), Details);
		}
	}

	TMap<FString, FString> Aliases;
	TArray<TSharedPtr<FJsonValue>> Results;
	TSharedPtr<FJsonObject> NodeIds = MakeShared<FJsonObject>();
	for (int32 Index = 0; Index < Steps->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject>* StepObj = nullptr;
		FString Edit;
		FString StepId;
		FString Reason;
		FString Code = TEXT("INVALID_ARGUMENT");
		TSharedPtr<FJsonObject> Reply;
		if (!(*Steps)[Index].IsValid() || !(*Steps)[Index]->TryGetObject(StepObj) || !(*StepObj)->TryGetStringField(TEXT("edit"), Edit))
		{
			Reason = TEXT("step is not an object with an `edit`");
		}
		else if (MetaSoundStepSubAction(Edit).IsEmpty())
		{
			Reason = FString::Printf(TEXT("edit '%s' is not add_node, connect, disconnect, remove_node, set_default, add_input or add_output"), *Edit);
		}
		else
		{
			// The step's own fields over the batch's shared ones (assetPath).
			TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Params->Values)
			{
				if (Pair.Key != TEXT("operations")) { Step->SetField(Pair.Key, Pair.Value); }
			}
			// One asset per batch: a step naming another MetaSound would edit it outside the batch's save.
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*StepObj)->Values)
			{
				if (Pair.Key != TEXT("assetPath")) { Step->SetField(Pair.Key, Pair.Value); }
			}
			const FString StepSubAction = MetaSoundStepSubAction(Edit);
			Step->SetStringField(TEXT("subAction"), StepSubAction);
			Step->TryGetStringField(TEXT("id"), StepId);
			ExpandMetaSoundEndpoint(Step, TEXT("from"), TEXT("sourceNodeId"), TEXT("sourceOutputName"));
			ExpandMetaSoundEndpoint(Step, TEXT("to"), TEXT("targetNodeId"), TEXT("targetInputName"));
			if (ResolveMetaSoundAliases(Aliases, Step, Reason))
			{
				Reply = HandleMetaSoundNodeActions(StepSubAction, Step, McpHandlerUtils::CreateResultObject());
				if (!Reply.IsValid()) { Reply = HandleMetaSoundInterfaceActions(StepSubAction, Step, McpHandlerUtils::CreateResultObject()); }
				if (!Reply.IsValid()) { Reply = HandleMetaSoundGraphEditActions(StepSubAction, Step, McpHandlerUtils::CreateResultObject()); }
				if (!Reply.IsValid() || !Reply->HasField(TEXT("success")) || !Reply->GetBoolField(TEXT("success")))
				{
					Reason = Reply.IsValid() && Reply->HasField(TEXT("error")) ? Reply->GetStringField(TEXT("error")) : TEXT("step failed");
					if (Reply.IsValid() && Reply->HasField(TEXT("code"))) { Code = Reply->GetStringField(TEXT("code")); }
				}
			}
		}

		TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
		Entry->SetNumberField(TEXT("index"), Index);
		Entry->SetStringField(TEXT("edit"), Edit);
		if (!StepId.IsEmpty()) { Entry->SetStringField(TEXT("id"), StepId); }
		if (!Reason.IsEmpty())
		{
			// Stop at the first failure; earlier steps are already saved.
			TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
			for (const TCHAR* Key : {TEXT("availableNodes"), TEXT("availableInputs"), TEXT("candidateNodeClasses"), TEXT("sourceOutputs"), TEXT("targetInputs")})
			{
				if (Reply.IsValid() && Reply->HasField(Key)) { Details->SetField(Key, Reply->TryGetField(Key)); }
			}
			Details->SetArrayField(TEXT("results"), Results);
			Details->SetObjectField(TEXT("nodeIds"), NodeIds);
			Details->SetNumberField(TEXT("failedIndex"), Index);
			Details->SetNumberField(TEXT("succeeded"), Index);
			return McpHandlerUtils::BuildErrorResponse(Code, FString::Printf(
				TEXT("build_metasound stopped at operations[%d] (%s): %s. The %d step(s) before it were applied."),
				Index, *Edit, *Reason, Index), Details);
		}
		Entry->SetBoolField(TEXT("success"), true);
		FString NodeId;
		if (Reply->TryGetStringField(TEXT("nodeId"), NodeId))
		{
			Entry->SetStringField(TEXT("nodeId"), NodeId);
			if (!StepId.IsEmpty())
			{
				Aliases.Add(StepId, NodeId);
				NodeIds->SetStringField(StepId, NodeId);
			}
		}
		FString Applied;
		if (Reply->TryGetStringField(TEXT("appliedValue"), Applied)) { Entry->SetStringField(TEXT("appliedValue"), Applied); }
		Results.Add(MakeShared<FJsonValueObject>(Entry));
	}

	Response->SetBoolField(TEXT("success"), true);
	// Names the edited MetaSound, so the receipt carries its handle and changes[] (both were empty).
	Response->SetStringField(TEXT("assetPath"), NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT(""))));
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Ran %d MetaSound operations"), Steps->Num()));
	Response->SetArrayField(TEXT("results"), McpListTellingSteps(Results));
	Response->SetObjectField(TEXT("nodeIds"), NodeIds);
	Response->SetNumberField(TEXT("succeeded"), Steps->Num());
	return Response;
}
}
