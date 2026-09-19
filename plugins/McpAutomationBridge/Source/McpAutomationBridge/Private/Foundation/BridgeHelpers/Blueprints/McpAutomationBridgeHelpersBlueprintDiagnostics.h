#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "UObject/UObjectIterator.h"
#include "Logging/TokenizedMessage.h"
#include "RenderingThread.h"

// A compile that fails is the one result a caller most needs the detail of, and
// "see the editor's Compiler Results" is not reachable from an automation
// client. The compiler already hands the reason back through
// FCompilerResultsLog; this captures it so the refusal names the broken node.
// Graph mutations use the same capture to answer "does this blueprint still
// compile?" at the moment of the edit, instead of letting a broken graph stay
// silent until someone presses Play.

inline const TCHAR *McpBlueprintStatusName(EBlueprintStatus Status) {
  switch (Status) {
  case BS_UpToDate: return TEXT("UpToDate");
  case BS_UpToDateWithWarnings: return TEXT("UpToDateWithWarnings");
  case BS_Error: return TEXT("Error");
  case BS_Dirty: return TEXT("Dirty");
  case BS_BeingCreated: return TEXT("BeingCreated");
  default: return TEXT("Unknown");
  }
}

// Compiles and fills Out with compilerStatus/compiled/errorCount/warningCount
// and a `diagnostics` array of { severity, message }. OutFirstError receives the
// first error text so a caller can put it straight into a refusal message.
inline bool McpCompileBlueprintWithDiagnostics(
    UBlueprint *Blueprint, const TSharedPtr<FJsonObject> &Out,
    FString &OutFirstError, int32 MaxDiagnostics = 12) {
  if (!Blueprint) {
    return false;
  }
  FCompilerResultsLog Results;
  Results.bSilentMode = true;
  Results.bLogInfoOnly = false;
  FlushRenderingCommands();
  FKismetEditorUtilities::CompileBlueprint(
      Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
  FlushRenderingCommands();

  TArray<TSharedPtr<FJsonValue>> Diagnostics;
  for (const TSharedRef<FTokenizedMessage> &Message : Results.Messages) {
    const EMessageSeverity::Type Severity = Message->GetSeverity();
    const bool bIsError = Severity == EMessageSeverity::Error;
    const bool bIsWarning = Severity == EMessageSeverity::Warning;
    if (!bIsError && !bIsWarning) {
      continue;
    }
    const FString Text = Message->ToText().ToString();
    if (bIsError && OutFirstError.IsEmpty()) {
      OutFirstError = Text;
    }
    if (Diagnostics.Num() >= MaxDiagnostics) {
      continue;
    }
    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("severity"),
                          bIsError ? TEXT("error") : TEXT("warning"));
    Entry->SetStringField(TEXT("message"), Text);
    Diagnostics.Add(MakeShared<FJsonValueObject>(Entry));
  }

  const bool bCompiled = Blueprint->Status == BS_UpToDate ||
                         Blueprint->Status == BS_UpToDateWithWarnings;

  // CompileBlueprint regenerates the class without dirtying the package, so a
  // caller who edits a graph and then runs control_editor.save_all is told
  // "0 dirty" while the asset on disk still holds the PREVIOUS bytecode -- the
  // edit is lost when the editor closes. Every graph edit funnels through this
  // helper, so marking it here covers all of them, not just the compile action.
  if (bCompiled) {
    if (UPackage *Package = Blueprint->GetOutermost()) {
      Package->SetDirtyFlag(true);
    }
  }

  // Compiling reinstances every live instance, and the stale ones are garbage
  // the moment the new class exists. If the undo buffer still references this
  // blueprint it pins those stale instances -- and anything that outers them (a
  // designer preview world, a PIE world) can no longer be collected. The next
  // full GC, which the game itself triggers on the first OpenLevel, then hits
  // the engine's "Fatal World Leaks" check and takes the whole editor down,
  // many calls after the compile that caused it. This is the engine's own
  // remedy for the same situation (Kismet2.cpp, unloading a blueprint): clear
  // the buffer, but only when the blueprint is actually pinned by it. Losing
  // undo history beats losing the editor, and the receipt says which happened.
  //
  // Two different things can be pinned, and checking only the first is not
  // enough: IsReferencedByUndoBuffer() asks about the UBlueprint, while the
  // crash that prompted this pinned an INSTANCE -- a widget of a REINST_ class
  // sitting in a transient world. So also treat "this class had live instances
  // when we compiled" as pinned: those instances are exactly the ones the
  // compile just turned into garbage, and any of them still in the buffer keeps
  // its world alive.
  bool bUndoBufferReset = false;
  bool bHadLiveInstances = false;
  if (GEditor && GEditor->Trans && Blueprint->GeneratedClass) {
    for (TObjectIterator<UObject> ObjIt(
             /*AdditionalExclusionFlags=*/RF_ClassDefaultObject);
         ObjIt; ++ObjIt) {
      if (ObjIt->IsA(Blueprint->GeneratedClass)) {
        bHadLiveInstances = true;
        break;
      }
    }
  }
  if (GEditor && GEditor->Trans &&
      (bHadLiveInstances ||
       FKismetEditorUtilities::IsReferencedByUndoBuffer(Blueprint))) {
    GEditor->Trans->Reset(NSLOCTEXT("McpAutomationBridge", "McpCompiledBlueprint",
                                    "Compiled Blueprint (automation)"));
    bUndoBufferReset = true;
  }

  if (Out.IsValid()) {
    if (bUndoBufferReset) {
      Out->SetBoolField(TEXT("undoBufferReset"), true);
      Out->SetStringField(
          TEXT("undoBufferResetReason"),
          bHadLiveInstances
              ? TEXT("This blueprint had live instances, so compiling it "
                     "reinstanced them and left the originals as garbage that "
                     "the undo buffer can pin along with the world that owns "
                     "them; editor undo history was cleared to keep the next "
                     "garbage collection from killing the editor.")
              : TEXT("The undo buffer referenced this blueprint, so compiling "
                     "it would have pinned the reinstanced (now garbage) "
                     "instances and the worlds that own them; editor undo "
                     "history was cleared to keep the next garbage collection "
                     "from killing the editor."));
    }
    Out->SetBoolField(TEXT("compiled"), bCompiled);
    Out->SetStringField(TEXT("compilerStatus"),
                        McpBlueprintStatusName(Blueprint->Status));
    Out->SetNumberField(TEXT("errorCount"), Results.NumErrors);
    Out->SetNumberField(TEXT("warningCount"), Results.NumWarnings);
    if (Diagnostics.Num() > 0) {
      Out->SetArrayField(TEXT("diagnostics"), Diagnostics);
    }
  }
  return bCompiled;
}

#endif
