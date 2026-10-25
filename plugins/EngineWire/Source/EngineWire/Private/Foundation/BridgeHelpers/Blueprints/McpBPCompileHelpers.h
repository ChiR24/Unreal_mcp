#pragma once

#include "CoreMinimal.h"

#include "Engine/Blueprint.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"
#include "RenderingThread.h"

// Flushing around compilation avoids automation-triggered Slate/render-thread
// races observed on UE 5.7 D3D12. OutError, when given, receives the compiler's
// first error, so a refusal can say why instead of guessing.
static inline bool McpSafeCompileBlueprint(UBlueprint *Blueprint, FString *OutError = nullptr) {
  if (!Blueprint)
    return false;

  FCompilerResultsLog Results;
  FlushRenderingCommands();
  FKismetEditorUtilities::CompileBlueprint(
      Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, OutError ? &Results : nullptr);
  FlushRenderingCommands();

  for (const TSharedRef<FTokenizedMessage> &Message : Results.Messages) {
    if (OutError && Message->GetSeverity() == EMessageSeverity::Error) {
      *OutError = Message->ToText().ToString();
      break;
    }
  }

  return Blueprint->Status == EBlueprintStatus::BS_UpToDate ||
         Blueprint->Status == EBlueprintStatus::BS_UpToDateWithWarnings;
}
