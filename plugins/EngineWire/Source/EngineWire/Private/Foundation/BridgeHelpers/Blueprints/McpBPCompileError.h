#pragma once

#include "CoreMinimal.h"

#include "Engine/Blueprint.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"
#include "RenderingThread.h"

// Compiles the way McpSafeCompileBlueprint does (flushing rendering commands on
// both sides of the compile) and puts the compiler's first error in OutError, so
// a refusal can say why instead of guessing. True when the Blueprint compiled.
static inline bool McpCompileBlueprintWithError(UBlueprint *Blueprint, FString &OutError) {
  OutError.Reset();
  if (!Blueprint)
    return false;

  FCompilerResultsLog Results;
  FlushRenderingCommands();
  FKismetEditorUtilities::CompileBlueprint(
      Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
  FlushRenderingCommands();

  for (const TSharedRef<FTokenizedMessage> &Message : Results.Messages) {
    if (Message->GetSeverity() == EMessageSeverity::Error) {
      OutError = Message->ToText().ToString();
      break;
    }
  }

  return Blueprint->Status == EBlueprintStatus::BS_UpToDate ||
         Blueprint->Status == EBlueprintStatus::BS_UpToDateWithWarnings;
}
