#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "RenderingThread.h"

// Flushing around compilation avoids automation-triggered Slate/render-thread
// races observed on UE 5.7 D3D12.
static inline bool McpSafeCompileBlueprint(
    UBlueprint *Blueprint,
    FCompilerResultsLog* Results = nullptr) {
  if (!Blueprint)
    return false;

  FlushRenderingCommands();
  FKismetEditorUtilities::CompileBlueprint(
      Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, Results);
  FlushRenderingCommands();

  return Blueprint->Status == EBlueprintStatus::BS_UpToDate ||
         Blueprint->Status == EBlueprintStatus::BS_UpToDateWithWarnings;
}
#endif
