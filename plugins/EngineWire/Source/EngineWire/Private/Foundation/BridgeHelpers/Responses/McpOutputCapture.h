#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

struct FMcpOutputCapture : public FOutputDevice {
  TArray<FString> Lines;

  virtual void Serialize(const TCHAR *V, ELogVerbosity::Type Verbosity,
                         const FName &Category) override {
    if (!V)
      return;

    // An error or warning keeps its severity, as the engine's own log writes it, so a reply built from these lines
    // can tell them apart (console_command keeps them however long the rest runs).
    const ELogVerbosity::Type Level =
        static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
    FString Line = Level == ELogVerbosity::Error     ? TEXT("Error: ")
                   : Level == ELogVerbosity::Warning ? TEXT("Warning: ")
                                                     : TEXT("");
    Line += V;
    while (Line.EndsWith(TEXT("\n")))
      Line.RemoveAt(Line.Len() - 1);
    Lines.Add(Line);
  }

  TArray<FString> Consume() { return MoveTemp(Lines); }
};
