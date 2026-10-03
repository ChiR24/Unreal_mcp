#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"
#include "Editor.h"

// A MetaSound plays the graph the frontend registered at its first play, and
// later plays reuse it, so an edit was saved while the editor kept playing the
// old graph until a restart (nine gain changes measured exactly as before).
// Re-register after every graph edit, as the MetaSound editor does after its own.
static void ReregisterEditedMetaSound(const TSharedPtr<FJsonObject>& Params)
{
#if MCP_HAS_METASOUND && MCP_HAS_METASOUND_FRONTEND && __has_include("MetasoundEditorSubsystem.h")
	const FString AssetPath = McpAudioAuthoring::NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
	UMetaSoundSource* MetaSound = AssetPath.IsEmpty() ? nullptr : FindObject<UMetaSoundSource>(nullptr, *AssetPath);
	UMetaSoundEditorSubsystem* MetaSoundEditor = GEditor ? GEditor->GetEditorSubsystem<UMetaSoundEditorSubsystem>() : nullptr;
	if (MetaSound && MetaSoundEditor) { MetaSoundEditor->RegisterGraphWithFrontend(*MetaSound); }
#endif
}

static TSharedPtr<FJsonObject> HandleAudioAuthoringRequest(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
	FString SubAction = GetJsonStringField(Params, TEXT("subAction"), TEXT(""));

	using namespace McpAudioAuthoring;
	if (TSharedPtr<FJsonObject> Result = HandleSoundCueAssetActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleSoundCueNodeActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleSoundCueDopplerAction(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleMetaSoundAssetActions(SubAction, Params, Response)) { return Result; }
	// Graph edits: a batch, nodes and links, inputs, outputs and literals, removals.
	TSharedPtr<FJsonObject> Edited = HandleMetaSoundBatchAction(SubAction, Params, Response);
	if (!Edited) { Edited = HandleMetaSoundNodeActions(SubAction, Params, Response); }
	if (!Edited) { Edited = HandleMetaSoundInterfaceActions(SubAction, Params, Response); }
	if (!Edited) { Edited = HandleMetaSoundGraphEditActions(SubAction, Params, Response); }
	if (Edited)
	{
		ReregisterEditedMetaSound(Params);
		return Edited;
	}
	if (TSharedPtr<FJsonObject> Result = HandleMetaSoundGraphReadAction(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleSoundClassActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleSoundMixActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleSoundMixEqActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleAttenuationActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleDialogueActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleEffectActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleAudioInfoActions(SubAction, Params, Response)) { return Result; }

	return McpHandlerUtils::BuildErrorResponse(TEXT("UNKNOWN_ACTION"), FString::Printf(TEXT("Unknown audio authoring action: %s"), *SubAction));
}

bool UMcpAutomationBridgeSubsystem::HandleManageAudioAuthoringAction(
	const FString& RequestId,
	const FString& Action,
	const TSharedPtr<FJsonObject>& Payload,
	TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
	FString LowerAction = Action.ToLower();
	if (!LowerAction.StartsWith(TEXT("manage_audio_authoring")))
	{
		return false;
	}

	if (!Payload.IsValid())
	{
		SendAutomationError(RequestingSocket, RequestId, TEXT("Audio authoring payload missing"), TEXT("INVALID_PAYLOAD"));
		return true;
	}

	TSharedPtr<FJsonObject> Response = HandleAudioAuthoringRequest(Payload);

	if (Response.IsValid())
	{
		bool bSuccess = Response->HasField(TEXT("success")) && GetJsonBoolField(Response, TEXT("success"));
		FString Message = GetJsonStringField(Response, TEXT("message"), TEXT("Operation complete"));
		FString ErrorCode = GetJsonStringField(Response, TEXT("code"), TEXT(""));

		if (bSuccess)
		{
			SendAutomationResponse(RequestingSocket, RequestId, true, Message, Response);
		}
		else
		{
			FString ErrorMsg = Response->HasField(TEXT("error")) ? GetJsonStringField(Response, TEXT("error")) : Message.Len() > 0 ? Message : TEXT("Unknown error");
			// Preserve diagnostic payload fields handlers attach to failures
			// (e.g. availableNodes/availableInputs); SendAutomationError would
			// discard the whole response object. Envelope fields keep the same
			// shape: message = human text, error = code.
			TSharedPtr<FJsonObject> Details = MakeShared<FJsonObject>();
			for (const auto& Pair : Response->Values)
			{
				const FString Key(Pair.Key.Len(), *Pair.Key);
				if (Key == TEXT("success") || Key == TEXT("error") ||
					Key == TEXT("code") || Key == TEXT("message") || Key == TEXT("type") ||
					Key == TEXT("requestId") || Key == TEXT("data") || Key == TEXT("result"))
				{
					continue;
				}
				Details->SetField(Key, Pair.Value);
			}
			if (Details->Values.Num() > 0)
			{
				SendAutomationResponse(RequestingSocket, RequestId, false, ErrorMsg, Details,
					ErrorCode.IsEmpty() ? TEXT("AUTOMATION_ERROR") : ErrorCode);
			}
			else
			{
				SendAutomationError(RequestingSocket, RequestId, ErrorMsg, ErrorCode);
			}
		}
	}
	else
	{
		SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to process audio authoring request"), TEXT("PROCESS_FAILED"));
	}

	return true;
}
