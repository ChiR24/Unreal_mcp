#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPSoundCheck.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPGraphChecks.h"

#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Components/ActorComponent.h"
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

inline FString McpBlueprintStatusName(EBlueprintStatus Status) {
  return StaticEnum<EBlueprintStatus>()->GetNameStringByValue(Status).RightChop(3); // "BS_UpToDate" -> "UpToDate"
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
    GEditor->Trans->Reset(NSLOCTEXT("EngineWire", "McpCompiledBlueprint",
                                    "Compiled Blueprint (automation)"));
    bUndoBufferReset = true;
  }

  if (Out.IsValid()) {
    // Why is the comment above: the same 230 characters on nearly every graph edit's reply only repeated it.
    if (bUndoBufferReset) {
      Out->SetBoolField(TEXT("undoBufferReset"), true);
    }
    Out->SetBoolField(TEXT("compiled"), bCompiled);
    Out->SetStringField(TEXT("compilerStatus"),
                        McpBlueprintStatusName(Blueprint->Status));
    Out->SetNumberField(TEXT("errorCount"), Results.NumErrors);
    Out->SetNumberField(TEXT("warningCount"), Results.NumWarnings);
    const int32 FirstCheck = Diagnostics.Num();
    McpAddSoundNodeWarnings(Blueprint, Diagnostics, MaxDiagnostics);
    McpAddStaleReadWarnings(Blueprint, Diagnostics, MaxDiagnostics);
    McpAddRandomReadWarnings(Blueprint, Diagnostics, MaxDiagnostics);
    McpAddTimerNameWarnings(Blueprint, Diagnostics, MaxDiagnostics);
    McpAddLoopLatentWarnings(Blueprint, Diagnostics, MaxDiagnostics);
    McpAddTickRestartWarnings(Blueprint, Diagnostics, MaxDiagnostics);
    McpAddWaitRereadWarnings(Blueprint, Diagnostics, MaxDiagnostics);
    if (Diagnostics.Num() > 0) {
      Out->SetArrayField(TEXT("diagnostics"), Diagnostics);
    }
    // These checks find what the compiler accepts, so a reply that only says it compiled reads as all is well: the
    // receipt carries the result's warnings. The compiler's own warnings go with them: "DoesSaveGameExist was
    // pruned because its Exec pin is not connected" (a node that never runs) sat only in diagnostics.
    TArray<TSharedPtr<FJsonValue>> Warnings;
    const TArray<TSharedPtr<FJsonValue>> *Existing = nullptr;
    if (Out->TryGetArrayField(TEXT("warnings"), Existing) && Existing) {
      Warnings = *Existing;
    }
    const int32 Before = Warnings.Num();
    for (int32 Index = 0; Index < Diagnostics.Num(); ++Index) {
      const TSharedPtr<FJsonObject> Entry = Diagnostics[Index]->AsObject();
      if (Index >= FirstCheck || Entry->GetStringField(TEXT("severity")) == TEXT("warning")) {
        Warnings.Add(MakeShared<FJsonValueString>(Entry->GetStringField(TEXT("message"))));
      }
    }
    if (Warnings.Num() > Before) {
      Out->SetArrayField(TEXT("warnings"), Warnings);
    }
  }
  return bCompiled;
}

// The variables a Blueprint class declares (its own and its Blueprint parents', Blueprint-visible),
// so a refusal for a name that missed can name the ones that exist. Components are left out: they
// are reached as Component.Property, not as variables. "none" when the class declares none.
inline FString McpBlueprintVariableList(const UClass *Class) {
  TArray<FString> Names;
  if (!Class) {
    return TEXT("none");
  }
  for (TFieldIterator<FProperty> It(Class); It; ++It) {
    const FObjectProperty *Object = CastField<FObjectProperty>(*It);
    if (Cast<UBlueprintGeneratedClass>(It->GetOwnerClass()) && It->HasAnyPropertyFlags(CPF_BlueprintVisible) &&
        !(Object && Object->PropertyClass && Object->PropertyClass->IsChildOf(UActorComponent::StaticClass())))
      Names.Add(It->GetName());
  }
  return Names.Num() > 0 ? FString::Join(Names, TEXT(", ")) : FString(TEXT("none"));
}

