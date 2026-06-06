#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioHandlersPrivate.h"

namespace McpAudioHandlers
{
#if WITH_EDITOR
USoundBase *ResolveSoundAsset(const FString &SoundPath) {
	if (SoundPath.IsEmpty())
		return nullptr;

	USoundBase *Sound = nullptr;
	if (SoundPath.Contains(TEXT("/Game/")) || SoundPath.Contains(TEXT("/Engine/")))
	{
		if (UEditorAssetLibrary::DoesAssetExist(SoundPath)) {
			Sound = Cast<USoundBase>(UEditorAssetLibrary::LoadAsset(SoundPath));
		}
		if (Sound)
			return Sound;
	}

	if (SoundPath.Contains(TEXT("/")))
		return nullptr;

	FString AssetName = FPaths::GetBaseFilename(SoundPath);
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FAssetData> AssetData;
  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(USoundWave::StaticClass()->GetClassPathName());
  Filter.ClassPaths.Add(USoundCue::StaticClass()->GetClassPathName());
#else
  // UE 5.0 fallback - use ClassNames instead of ClassPaths
  Filter.ClassNames.Add(USoundWave::StaticClass()->GetFName());
  Filter.ClassNames.Add(USoundCue::StaticClass()->GetFName());
#endif
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(TEXT("/Game"));
  AssetRegistryModule.Get().GetAssets(Filter, AssetData);

  for (const FAssetData &Data : AssetData) {
    if (Data.AssetName.ToString().Equals(AssetName, ESearchCase::IgnoreCase)) {
      Sound = Cast<USoundBase>(Data.GetAsset());
      if (Sound) {
        UE_LOG(LogMcpAudioHandlers, Log,
               TEXT("Resolved sound '%s' to '%s'"), *SoundPath,
               *Sound->GetPathName());
        return Sound;
      }
    }
  }

  UE_LOG(LogMcpAudioHandlers, Warning,
         TEXT("Sound asset '%s' not found."), *SoundPath);
  return nullptr;
}

/**
 * @brief Resolve a USoundMix by asset path or asset name.
 *
 * Attempts to load a USoundMix using the provided MixPath. If MixPath contains a
 * full asset path and the asset exists, that asset is returned. If MixPath does
 * not contain a path separator, the function treats it as an asset name and
 * searches the /Game packages for a matching USoundMix (case-insensitive).
 *
 * VERSION COMPATIBILITY:
 * - UE 5.0: FARFilter uses ClassNames (FName)
 * - UE 5.1+: FARFilter uses ClassPaths (FTopLevelAssetPath)
 *
 * @param MixPath Asset path or asset name to resolve.
 * @return USoundMix* Pointer to the resolved USoundMix, or nullptr if not found.
 */
USoundMix *ResolveSoundMix(const FString &MixPath) {
	if (MixPath.IsEmpty())
		return nullptr;

	USoundMix *Mix = nullptr;
	// Only call DoesAssetExist for paths that look like full UE asset paths (contain /Game/ or /Engine/)
	// Bare names like "TestSoundMix" would cause DoesAssetExist errors and need asset registry search
	if (MixPath.Contains(TEXT("/Game/")) || MixPath.Contains(TEXT("/Engine/")))
	{
		if (UEditorAssetLibrary::DoesAssetExist(MixPath)) {
			Mix = Cast<USoundMix>(UEditorAssetLibrary::LoadAsset(MixPath));
		}
		if (Mix)
			return Mix;
	}

	// For paths without a root (e.g. "TestSoundMix"), skip DoesAssetExist to avoid UE log errors
	// and go straight to asset registry search below
	if (MixPath.Contains(TEXT("/")))
	{
		return nullptr;
	}

  FString AssetName = FPaths::GetBaseFilename(MixPath);
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FAssetData> AssetData;
  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(USoundMix::StaticClass()->GetClassPathName());
#else
  Filter.ClassNames.Add(USoundMix::StaticClass()->GetFName());
#endif
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(TEXT("/Game"));
  AssetRegistryModule.Get().GetAssets(Filter, AssetData);

  for (const FAssetData &Data : AssetData) {
    if (Data.AssetName.ToString().Equals(AssetName, ESearchCase::IgnoreCase)) {
      Mix = Cast<USoundMix>(Data.GetAsset());
      if (Mix)
        return Mix;
    }
  }
  return nullptr;
}

/**
 * @brief Locates and returns a USoundClass by asset path or by asset name.
 *
 * Attempts to load the sound class directly if ClassPath refers to an existing asset; otherwise,
 * if ClassPath does not contain a '/' it searches the project's /Game assets for a sound class
 * with a matching name (case-insensitive).
 *
 * VERSION COMPATIBILITY:
 * - UE 5.0: FARFilter uses ClassNames (FName)
 * - UE 5.1+: FARFilter uses ClassPaths (FTopLevelAssetPath)
 *
 * @param ClassPath Asset path (e.g. "/Game/Audio/MyClass") or asset name ("MyClass").
 * @return USoundClass* Pointer to the resolved sound class, or nullptr if not found or ClassPath is empty.
 */
USoundClass *ResolveSoundClass(const FString &ClassPath) {
	if (ClassPath.IsEmpty())
		return nullptr;

	USoundClass *Class = nullptr;
	if (ClassPath.Contains(TEXT("/Game/")) || ClassPath.Contains(TEXT("/Engine/")))
	{
		if (UEditorAssetLibrary::DoesAssetExist(ClassPath)) {
			Class = Cast<USoundClass>(UEditorAssetLibrary::LoadAsset(ClassPath));
		}
		if (Class)
			return Class;
	}

	if (ClassPath.Contains(TEXT("/")))
		return nullptr;

	FString AssetName = FPaths::GetBaseFilename(ClassPath);
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FAssetData> AssetData;
  FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  Filter.ClassPaths.Add(USoundClass::StaticClass()->GetClassPathName());
#else
  Filter.ClassNames.Add(USoundClass::StaticClass()->GetFName());
#endif
  Filter.bRecursivePaths = true;
  Filter.PackagePaths.Add(TEXT("/Game"));
  AssetRegistryModule.Get().GetAssets(Filter, AssetData);

  for (const FAssetData &Data : AssetData) {
    if (Data.AssetName.ToString().Equals(AssetName, ESearchCase::IgnoreCase)) {
      Class = Cast<USoundClass>(Data.GetAsset());
      if (Class)
        return Class;
    }
  }
  return nullptr;
}
#endif
}
