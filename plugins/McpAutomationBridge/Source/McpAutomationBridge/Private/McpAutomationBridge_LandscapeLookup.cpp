#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LandscapeLookup.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Landscape.h"

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

namespace McpLandscapeHandlers {
static bool LandscapePathMatches(const ALandscape &Landscape,
                                 const FString &LandscapePath) {
  FString ActorPath = Landscape.GetPackage()->GetPathName();
  FString NormalizedRequest = LandscapePath;
  NormalizedRequest.ReplaceInline(TEXT("\\"), TEXT("/"));
  ActorPath.ReplaceInline(TEXT("\\"), TEXT("/"));
  if (ActorPath.EndsWith(TEXT(".uasset"))) {
    ActorPath = ActorPath.LeftChop(7);
  }
  return ActorPath.Equals(NormalizedRequest, ESearchCase::IgnoreCase);
}

ALandscape *FindLandscapeForEdit(const FString &LandscapePath,
                                 const FString &LandscapeName) {
  ALandscape *Landscape = nullptr;
  if (GEditor) {
    if (UEditorActorSubsystem *ActorSS =
            GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
      TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
      for (AActor *Actor : AllActors) {
        ALandscape *Candidate = Cast<ALandscape>(Actor);
        if (!Candidate) {
          continue;
        }
        if (!LandscapeName.IsEmpty() &&
            Candidate->GetActorLabel().Equals(LandscapeName,
                                              ESearchCase::IgnoreCase)) {
          Landscape = Candidate;
          break;
        }
        if (!LandscapePath.IsEmpty() &&
            LandscapePathMatches(*Candidate, LandscapePath)) {
          Landscape = Candidate;
          break;
        }
      }
    }
  }

  if (!Landscape && !LandscapePath.IsEmpty()) {
    Landscape = Cast<ALandscape>(
        StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
  }
  return Landscape;
}

FString MakeLandscapeNotFoundMessage(const FString &LandscapePath,
                                     const FString &LandscapeName) {
  return LandscapeName.IsEmpty()
             ? FString::Printf(TEXT("Landscape not found at path: %s"),
                               *LandscapePath)
             : FString::Printf(TEXT("Landscape '%s' not found (path: %s)"),
                               *LandscapeName, *LandscapePath);
}
}
#endif
