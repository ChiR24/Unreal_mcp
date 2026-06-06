#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR
static TSharedPtr<FJsonObject> HandleAudioAuthoringRequest(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
	FString SubAction = McpHandlerUtils::GetOptionalString(Params, TEXT("subAction"), TEXT(""));

	using namespace McpAudioAuthoring;
	if (TSharedPtr<FJsonObject> Result = HandleSoundCueAssetActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleSoundCueNodeActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleMetaSoundAssetActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleMetaSoundNodeActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleMetaSoundInterfaceActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleSoundClassActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleSoundMixActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleSoundMixEqActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleAttenuationActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleDialogueActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleEffectActions(SubAction, Params, Response)) { return Result; }
	if (TSharedPtr<FJsonObject> Result = HandleAudioInfoActions(SubAction, Params, Response)) { return Result; }

	return McpHandlerUtils::BuildErrorResponse(TEXT("UNKNOWN_ACTION"), FString::Printf(TEXT("Unknown audio authoring action: %s"), *SubAction));
}
#endif

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

#if WITH_EDITOR
	if (!Payload.IsValid())
	{
		SendAutomationError(RequestingSocket, RequestId, TEXT("Audio authoring payload missing"), TEXT("INVALID_PAYLOAD"));
		return true;
	}

	TSharedPtr<FJsonObject> Response = HandleAudioAuthoringRequest(Payload);

	if (Response.IsValid())
	{
		bool bSuccess = Response->HasField(TEXT("success")) && GetJsonBoolField(Response, TEXT("success"));
		FString Message = Response->HasField(TEXT("message")) ? GetJsonStringField(Response, TEXT("message")) : TEXT("Operation complete");
		FString ErrorCode = Response->HasField(TEXT("code")) ? GetJsonStringField(Response, TEXT("code")) : TEXT("");

		if (bSuccess)
		{
			SendAutomationResponse(RequestingSocket, RequestId, true, Message, Response);
		}
		else
		{
			FString ErrorMsg = Response->HasField(TEXT("error")) ? GetJsonStringField(Response, TEXT("error")) : Message.Len() > 0 ? Message : TEXT("Unknown error");
			SendAutomationError(RequestingSocket, RequestId, ErrorMsg, ErrorCode);
		}
	}
	else
	{
		SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to process audio authoring request"), TEXT("PROCESS_FAILED"));
	}

	return true;
#else
	SendAutomationError(RequestingSocket, RequestId, TEXT("Audio authoring requires editor build"), TEXT("EDITOR_REQUIRED"));
	return true;
#endif
}
