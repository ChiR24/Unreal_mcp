#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "RenderingThread.h"

// Flushing around compilation avoids automation-triggered Slate/render-thread
// races observed on UE 5.7 D3D12.
static inline bool McpSafeCompileBlueprint(UBlueprint *Blueprint) {
  if (!Blueprint)
    return false;

  FlushRenderingCommands();
  FKismetEditorUtilities::CompileBlueprint(
      Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
  FlushRenderingCommands();

  return Blueprint->Status == EBlueprintStatus::BS_UpToDate ||
         Blueprint->Status == EBlueprintStatus::BS_UpToDateWithWarnings;
}
#endif
