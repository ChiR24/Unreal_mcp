// build_metasound: many MetaSound graph edits in one call. Each step is an
// ordinary add_node / connect / set_default / add_input / add_output run by the
// same single-call handler, so a synth voice no longer costs a round trip per
// node, link and literal. Steps name what they create with `id`; later steps
// refer to it as "$id" (nodeId / sourceNodeId / targetNodeId, or "$id.Pin" in
// from/to).
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR
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
		{TEXT("add_output"), TEXT("add_metasound_output")}};
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

bool ResolveMetaSoundAliases(const TMap<FString, FString>& Aliases, const TSharedPtr<FJsonObject>& Step, FString& OutError)
{
	for (const TCHAR* Field : {TEXT("nodeId"), TEXT("sourceNodeId"), TEXT("targetNodeId")})
	{
		FString Ref;
		if (!Step->TryGetStringField(Field, Ref) || !Ref.StartsWith(TEXT("$")))
		{
			continue;
		}
		const FString* NodeId = Aliases.Find(Ref.RightChop(1));
		if (!NodeId)
		{
			TArray<FString> Known;
			Aliases.GenerateKeyArray(Known);
			OutError = FString::Printf(TEXT("%s '%s' names no earlier step; ids so far: %s"), Field, *Ref,
				Known.Num() > 0 ? *FString::Join(Known, TEXT(", ")) : TEXT("<none>"));
			return false;
		}
		Step->SetStringField(Field, *NodeId);
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
			TEXT("build_metasound needs `operations`: 1-%d steps, each {edit: add_node|connect|set_default|add_input|add_output, ...that edit's params}"),
			MaxMetaSoundBatchSteps));
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
			Reason = FString::Printf(TEXT("edit '%s' is not add_node, connect, set_default, add_input or add_output"), *Edit);
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
			for (const TCHAR* Key : {TEXT("availableNodes"), TEXT("availableInputs"), TEXT("candidateNodeClasses")})
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
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Ran %d MetaSound operations"), Steps->Num()));
	Response->SetArrayField(TEXT("results"), Results);
	Response->SetObjectField(TEXT("nodeIds"), NodeIds);
	Response->SetNumberField(TEXT("succeeded"), Steps->Num());
	return Response;
}
}
#endif
