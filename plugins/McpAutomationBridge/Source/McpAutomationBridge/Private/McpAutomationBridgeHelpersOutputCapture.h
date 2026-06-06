#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

struct FMcpOutputCapture : public FOutputDevice {
  TArray<FString> Lines;

  virtual void Serialize(const TCHAR *V, ELogVerbosity::Type Verbosity,
                         const FName &Category) override {
    if (!V)
      return;

    FString Line(V);
    while (Line.EndsWith(TEXT("\n")))
      Line.RemoveAt(Line.Len() - 1);
    Lines.Add(Line);
  }

  TArray<FString> Consume() {
    TArray<FString> CapturedLines = MoveTemp(Lines);
    Lines.Empty();
    return CapturedLines;
  }
};
