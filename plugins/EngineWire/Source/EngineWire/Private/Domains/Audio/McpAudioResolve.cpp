#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/McpCapabilityPrincipal.h"
#include "Domains/Audio/McpAudioPrivate.h"

namespace McpAudioHandlers
{
namespace
{
// An audio asset by path or by name. A path (anything with a '/') loads directly, under whichever content mount it
// names; a bare name is the first asset of one of Classes under /Game with that name, case ignored, inside a
// path-restricted caller's prefixes. Only an asset that IsA Wanted counts.
UObject* ResolveAudioAsset(const FString& Path, std::initializer_list<UClass*> Classes, UClass* Wanted)
{
	if (Path.IsEmpty())
		return nullptr;
	// Only /Game and /Engine paths were loaded, so a sound under a plugin's or a game feature's mount read as missing.
	if (Path.Contains(TEXT("/")))
	{
		UObject* Direct = McpLoadAsset(Path);
		return Direct && Direct->IsA(Wanted) ? Direct : nullptr;
	}

	const FString AssetName = FPaths::GetBaseFilename(Path);
	FARFilter Filter;
	for (UClass* Class : Classes)
	{
		MCP_AR_FILTER_ADD_CLASS(Filter, Class);
	}
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(TEXT("/Game"));
	TArray<FAssetData> AssetData;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssets(Filter, AssetData);
	for (const FAssetData& Data : AssetData)
	{
		if (Data.AssetName.ToString().Equals(AssetName, ESearchCase::IgnoreCase) &&
			McpCapabilityPrincipal::ActiveRequestPermitsPath(Data.PackageName.ToString()))
		{
			UObject* Found = Data.GetAsset();
			if (Found && Found->IsA(Wanted))
				return Found;
		}
	}
	return nullptr;
}
} // namespace

USoundBase* ResolveSoundAsset(const FString& SoundPath)
{
	USoundBase* Sound = Cast<USoundBase>(
		ResolveAudioAsset(SoundPath, {USoundWave::StaticClass(), USoundCue::StaticClass()}, USoundBase::StaticClass()));
	// A bare name went through the registry search: say what it resolved to, or that nothing matched.
	if (!SoundPath.IsEmpty() && !SoundPath.Contains(TEXT("/")))
	{
		if (Sound)
		{
			UE_LOG(LogMcpAudioHandlers, Log, TEXT("Resolved sound '%s' to '%s'"), *SoundPath, *Sound->GetPathName());
		}
		else
		{
			UE_LOG(LogMcpAudioHandlers, Warning, TEXT("Sound asset '%s' not found."), *SoundPath);
		}
	}
	return Sound;
}

USoundMix* ResolveSoundMix(const FString& MixPath)
{
	return Cast<USoundMix>(ResolveAudioAsset(MixPath, {USoundMix::StaticClass()}, USoundMix::StaticClass()));
}

USoundClass* ResolveSoundClass(const FString& ClassPath)
{
	return Cast<USoundClass>(ResolveAudioAsset(ClassPath, {USoundClass::StaticClass()}, USoundClass::StaticClass()));
}

USoundBase* LoadSoundOrReply(UEngineWireSubsystem* Self, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
	const FString& RequestId, const TSharedPtr<FJsonObject>& Payload)
{
	FString SoundPath;
	if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) || SoundPath.IsEmpty())
	{
		Self->SendAutomationError(RequestingSocket, RequestId, TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
		return nullptr;
	}
	USoundBase* Sound = ResolveSoundAsset(SoundPath);
	if (!Sound)
	{
		Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Sound asset not found"), TEXT("ASSET_NOT_FOUND"));
	}
	return Sound;
}

UWorld* McpAudioEditorWorldOrReply(UEngineWireSubsystem* Self, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
	const FString& RequestId)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Self->SendAutomationError(RequestingSocket, RequestId, TEXT("No World Context"), TEXT("NO_WORLD"));
	}
	return World;
}
}
