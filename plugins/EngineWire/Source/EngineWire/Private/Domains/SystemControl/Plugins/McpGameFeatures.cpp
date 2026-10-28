// list_game_features and set_game_feature_active: game feature plugins through IMcpGameFeaturesProvider, which the
// module EngineWireGameFeatures registers where the project enables the GameFeatures plugin. Activation finishes
// later (the subsystem loads the plugin's content first), so its reply is sent when the subsystem says it is done, or
// after 20 seconds with the state reached by then.

#include "Domains/SystemControl/McpSystemPrivate.h"

#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "EngineWireSubsystem.h"
#include "Features/IModularFeatures.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpGameFeaturesProvider.h"
#include "Modules/ModuleManager.h"
#include "Runtime/Launch/Resources/Version.h"

namespace
{
constexpr float GameFeatureReplyAfterSeconds = 20.0f;

TSharedPtr<FJsonObject> GameFeatureRowJson(const FMcpGameFeatureRow& Row)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("pluginName"), Row.PluginName);
	Json->SetStringField(TEXT("state"), Row.State);
	if (!Row.InitialState.IsEmpty())
	{
		Json->SetStringField(TEXT("initialState"), Row.InitialState);
	}
	return Json;
}
} // namespace

namespace McpSystemControlHandlers
{
IMcpGameFeaturesProvider* GameFeaturesProvider()
{
	IModularFeatures& Features = IModularFeatures::Get();
	return Features.IsModularFeatureAvailable(IMcpGameFeaturesProvider::FeatureName())
		? &Features.GetModularFeature<IMcpGameFeaturesProvider>(IMcpGameFeaturesProvider::FeatureName()) : nullptr;
}

void SendGameFeaturesMissing(UEngineWireSubsystem* Self, const FString& RequestId, FSystemControlSocket RequestingSocket,
                             const TCHAR* Tail)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 1
	Self->SendAutomationError(RequestingSocket, RequestId, FString(TEXT("Game feature plugins are reached from UE 5.1 on: "
		"this engine keeps their state private.")) + Tail, TEXT("ENGINE_VERSION_UNSUPPORTED"));
#else
	if (!FModuleManager::Get().ModuleExists(TEXT("EngineWireGameFeatures")))
	{
		Self->SendAutomationError(RequestingSocket, RequestId, FString(TEXT("This build of EngineWire leaves game feature "
			"plugins out: its EngineWireGameFeatures module is not installed.")) + Tail, TEXT("GAME_FEATURES_UNAVAILABLE"));
		return;
	}
	Self->SendAutomationError(RequestingSocket, RequestId, FString(TEXT("Game feature plugins need the GameFeatures plugin, "
		"which this project leaves off: enable_plugin GameFeatures, then restart the editor.")) + Tail, TEXT("GAME_FEATURES_DISABLED"));
#endif
}

bool HandleGameFeatures(UEngineWireSubsystem* Self, const FString& RequestId, const FString& SubAction,
                        const TSharedPtr<FJsonObject>& Payload, FSystemControlSocket RequestingSocket)
{
	IMcpGameFeaturesProvider* Provider = GameFeaturesProvider();
	if (!Provider)
	{
		SendGameFeaturesMissing(Self, RequestId, RequestingSocket, TEXT(""));
		return true;
	}
	if (SubAction == TEXT("list_game_features"))
	{
		TArray<TSharedPtr<FJsonValue>> Rows;
		for (const FMcpGameFeatureRow& Row : Provider->List())
		{
			Rows.Add(MakeShared<FJsonValueObject>(GameFeatureRowJson(Row)));
		}
		TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
		Result->SetArrayField(TEXT("gameFeatures"), Rows);
		Self->SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("%d game feature plugin(s)."), Rows.Num()), Result);
		return true;
	}
	FString Name;
	bool bActive = true;
	Payload->TryGetStringField(TEXT("pluginName"), Name);
	Payload->TryGetBoolField(TEXT("active"), bActive);
	FMcpGameFeatureRow Before;
	if (!Provider->Find(Name, Before))
	{
		Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s is no game feature plugin this "
			"editor knows (list_game_features lists them)."), *Name), TEXT("GAME_FEATURE_NOT_FOUND"));
		return true;
	}
	if ((Before.State == TEXT("active")) == bActive)
	{
		TSharedPtr<FJsonObject> Result = GameFeatureRowJson(Before);
		Result->SetBoolField(TEXT("changed"), false);
		Self->SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("%s is %s already."), *Name,
			*Before.State), Result);
		return true;
	}
	// One reply, from whichever comes first: the subsystem's answer or the deadline. Both may arrive after the editor
	// began closing, so the subsystem and the provider are looked up again then.
	const TSharedRef<bool> bReplied = MakeShared<bool>(false);
	const TWeakObjectPtr<UEngineWireSubsystem> WeakSelf(Self);
	const auto Reply = [WeakSelf, RequestId, RequestingSocket, Name, bReplied](const FString& Error, bool bTimedOut)
	{
		UEngineWireSubsystem* Bridge = WeakSelf.Get();
		IMcpGameFeaturesProvider* Live = GameFeaturesProvider();
		if (*bReplied || !Bridge || !Live)
		{
			return;
		}
		*bReplied = true;
		FMcpGameFeatureRow After;
		Live->Find(Name, After);
		Bridge->RunLatentReply(RequestId, TEXT("system_control"), [&]()
		{
			TSharedPtr<FJsonObject> Result = GameFeatureRowJson(After);
			Result->SetBoolField(TEXT("changed"), true);
			if (!Error.IsEmpty() || bTimedOut)
			{
				Bridge->SendAutomationResponse(RequestingSocket, RequestId, false, bTimedOut
					? FString::Printf(TEXT("%s is still on its way after %.0f s (now %s); list_game_features shows when it "
						"gets there."), *Name, GameFeatureReplyAfterSeconds, *After.State)
					: FString::Printf(TEXT("%s did not get there: %s (now %s)."), *Name, *Error, *After.State), Result,
					bTimedOut ? TEXT("GAME_FEATURE_PENDING") : TEXT("GAME_FEATURE_FAILED"));
				return;
			}
			Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("%s is %s."), *Name, *After.State), Result);
		});
	};
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Reply](float) { Reply(FString(), true); return false; }),
		GameFeatureReplyAfterSeconds);
	Provider->SetActive(Name, bActive, [Reply](const FString& Error) { Reply(Error, false); });
	return true;
}
} // namespace McpSystemControlHandlers
