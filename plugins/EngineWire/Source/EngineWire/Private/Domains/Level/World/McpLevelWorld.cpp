#include "Domains/Level/World/McpLevelWorldAccess.h"

#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "UObject/Package.h"

namespace McpLevelHandlers {
TArray<ULevel*> GetAllLevelsFromWorld(UWorld* World) {
  TArray<ULevel*> Levels;
  if (!World) {
    return Levels;
  }

  if (World->PersistentLevel) {
    Levels.Add(World->PersistentLevel);
  }

  for (const ULevelStreaming* StreamingLevel : World->GetStreamingLevels()) {
    if (StreamingLevel) {
      ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();
      if (LoadedLevel) {
        Levels.Add(LoadedLevel);
      }
    }
  }
  return Levels;
}

// A bare suffix used to match too, so "Sub01" could stream "BigSub01" when that one came first.
TArray<ULevelStreaming*> FindStreamingLevels(UWorld* World, const FString& Name) {
  TArray<ULevelStreaming*> Matches;
  for (ULevelStreaming* Streaming : World->GetStreamingLevels()) {
    const FString Package = Streaming ? UWorld::RemovePIEPrefix(Streaming->GetWorldAssetPackageName()) : FString();
    if (!Package.IsEmpty() && Package.Equals(Name, ESearchCase::IgnoreCase)) {
      return {Streaming};
    }
    if (!Package.IsEmpty() && Package.EndsWith(TEXT("/") + Name, ESearchCase::IgnoreCase)) {
      Matches.Add(Streaming);
    }
  }
  return Matches;
}

bool IsUserSublevel(const ULevelStreaming* Streaming) {
  return !Streaming->HasAnyFlags(RF_Transient) && !Streaming->GetClass()->HasAnyClassFlags(CLASS_Transient);
}

ULevelStreaming* FindOneStreamingLevel(UWorld* World, const FString& Wanted, const FString& OpenLevel,
                                       TSharedPtr<FJsonObject>& OutLevels, FString& OutRefusal, const TCHAR*& OutCode) {
  const TArray<ULevelStreaming*> Matches = FindStreamingLevels(World, Wanted);
  TArray<FString> Loaded;
  for (const ULevel* Level : GetAllLevelsFromWorld(World)) {
    Loaded.Add(Level->GetOutermost()->GetName());
  }
  OutLevels = McpHandlerUtils::CreateResultObject();
  OutLevels->SetArrayField(TEXT("levels"), McpHandlerUtils::ToJsonStringArray(Loaded));
  if (Matches.Num() == 1) {
    return Matches[0];
  }
  OutRefusal = FString::Printf(TEXT("%s is %s of the open level %s (its loaded levels: %s)."), *Wanted,
                               Matches.Num() > 1 ? TEXT("more than one sublevel") : TEXT("not a sublevel"), *OpenLevel,
                               *FString::Join(Loaded, TEXT(", ")));
  OutCode = Matches.Num() > 1 ? TEXT("AMBIGUOUS_LEVEL") : TEXT("SUBLEVEL_NOT_FOUND");
  return nullptr;
}
} // namespace McpLevelHandlers
