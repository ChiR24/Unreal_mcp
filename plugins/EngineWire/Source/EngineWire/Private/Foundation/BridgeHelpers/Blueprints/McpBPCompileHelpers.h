#pragma once

#include "CoreMinimal.h"

#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EngineUtils.h"
#include "Foundation/BridgeHelpers/Properties/McpComponentLookup.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"
#include "RenderingThread.h"

// Every Blueprint compile the plugin runs (a Control Rig and a copy made by the
// same call aside: nothing placed is an instance of either). Flushing around
// compilation avoids automation-triggered Slate/render-thread races observed on
// UE 5.7 D3D12.
// The compile re-instances the Blueprint's placed actors (its children's too),
// and their skeletal meshes came back with every bone at identity: each part of
// a character drawn at the actor's origin until the world ticked, which an
// editor kept minimized never does, so every capture of the level showed it.
// Each mesh is initialised again as an edited component is (InitAnim, the
// editor's own remedy after re-instancing), in the editor world only: a Play
// world poses its own. Nothing is dirtied and no undo step is recorded.
static inline void McpCompileBlueprint(UBlueprint *Blueprint, FCompilerResultsLog *Results = nullptr,
                                       EBlueprintCompileOptions Options = EBlueprintCompileOptions::SkipGarbageCollection) {
  FlushRenderingCommands();
  FKismetEditorUtilities::CompileBlueprint(Blueprint, Options, Results);
  FlushRenderingCommands();

  UClass *Class = Blueprint->GeneratedClass.Get();
  UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  if (!Class || !World || !Class->IsChildOf(AActor::StaticClass())) {
    return;
  }
  for (TActorIterator<AActor> It(World, Class); It; ++It) {
    TInlineComponentArray<USkeletalMeshComponent *> Meshes(*It);
    // A mesh that copies another's pose (a face following its body) copies it
    // as it is posed: with several, the second pass reads what the first posed.
    const int32 Passes = Meshes.Num() > 1 ? 2 : 1;
    for (int32 Pass = 0; Pass < Passes; ++Pass) {
      for (USkeletalMeshComponent *Mesh : Meshes) {
        McpRefreshComponentAfterEdit(Mesh);
      }
    }
  }
}

// OutError, when given, receives the compiler's first error, so a refusal can
// say why instead of guessing.
static inline bool McpSafeCompileBlueprint(UBlueprint *Blueprint, FString *OutError = nullptr) {
  if (!Blueprint)
    return false;

  FCompilerResultsLog Results;
  McpCompileBlueprint(Blueprint, OutError ? &Results : nullptr);

  for (const TSharedRef<FTokenizedMessage> &Message : Results.Messages) {
    if (OutError && Message->GetSeverity() == EMessageSeverity::Error) {
      *OutError = Message->ToText().ToString();
      break;
    }
  }

  return Blueprint->Status == EBlueprintStatus::BS_UpToDate ||
         Blueprint->Status == EBlueprintStatus::BS_UpToDateWithWarnings;
}
