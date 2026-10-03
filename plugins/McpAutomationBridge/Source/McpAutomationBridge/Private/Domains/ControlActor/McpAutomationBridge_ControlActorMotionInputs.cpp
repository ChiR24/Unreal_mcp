#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"
#include "Engine/World.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"

// sample_motion's inputs and startWhen (FMcpMotionInput / FMcpMotionTrigger in
// ControlActorSupport.h). The run's ticker calls these on game time, so a jump
// pressed 0.6 s after a platform appears lands 0.6 s after it appears, however
// long the caller took to send the call.
namespace {
constexpr int32 McpMaxMotionInputs = 32;

// The same path simulate_input takes, so a key that moves the pawn there
// moves it here.
void McpSendMotionKey(const FString &Key, bool bDown) {
  bool bOk = false, bRouted = false, bPIE = false, bSlate = false;
  FString Message;
  SimulateEditorInputForMcp(bDown ? TEXT("key_down") : TEXT("key_up"), Key,
                            MakeShared<FJsonObject>(), bOk, bRouted, bPIE, bSlate, Message);
}

// "True" and "true" are one bool, and 3 and "3.000000" one number.
bool McpMotionValueMatches(const FString &Value, const FString &Wanted) {
  if (Value.Equals(Wanted, ESearchCase::IgnoreCase)) {
    return true;
  }
  return Value.IsNumeric() && Wanted.IsNumeric() &&
         FMath::IsNearlyEqual(FCString::Atod(*Value), FCString::Atod(*Wanted), 1e-4);
}

double McpRoundMs(double Seconds) { return FMath::RoundToDouble(Seconds * 1000.0) / 1000.0; }
} // namespace

bool McpParseMotionInputs(const TSharedPtr<FJsonObject> &Payload,
                          TArray<FMcpMotionInput> &Out, FString &Error) {
  const TArray<TSharedPtr<FJsonValue>> *Items = nullptr;
  if (!Payload->TryGetArrayField(TEXT("inputs"), Items) || !Items) {
    return true;
  }
  if (Items->Num() > McpMaxMotionInputs) {
    Error = FString::Printf(TEXT("inputs takes at most %d key presses; got %d."), McpMaxMotionInputs,
                            Items->Num());
    return false;
  }
  for (int32 Index = 0; Index < Items->Num(); ++Index) {
    const TSharedPtr<FJsonObject> *Item = nullptr;
    FMcpMotionInput Input;
    if (!(*Items)[Index].IsValid() || !(*Items)[Index]->TryGetObject(Item) || !Item ||
        !(*Item)->TryGetStringField(TEXT("key"), Input.Key) || Input.Key.IsEmpty()) {
      Error = FString::Printf(TEXT("inputs[%d] needs a key, e.g. {\"key\":\"SpaceBar\",\"atSeconds\":0.3,"
                                   "\"holdSeconds\":0.2}."), Index);
      return false;
    }
    if (!FKey(*Input.Key).IsValid()) {
      Error = FString::Printf(TEXT("inputs[%d].key '%s' is not a key name (SpaceBar, D, A, Left, Enter...)."),
                              Index, *Input.Key);
      return false;
    }
    (*Item)->TryGetNumberField(TEXT("atSeconds"), Input.At);
    (*Item)->TryGetNumberField(TEXT("holdSeconds"), Input.Hold);
    Input.At = FMath::Max(0.0, Input.At);
    Input.Hold = FMath::Clamp(Input.Hold, 0.0, 30.0);
    Out.Add(Input);
  }
  return true;
}

bool McpInitMotionTrigger(AActor *Gate, const TSharedPtr<FJsonObject> &When, UWorld *World,
                          FMcpMotionTrigger &Out, FString &Error) {
  FString GateName, PropertyName;
  When->TryGetStringField(TEXT("actorName"), GateName);
  When->TryGetStringField(TEXT("propertyName"), PropertyName);
  if (!Gate || Gate->GetWorld() != World) {
    Error = FString::Printf(TEXT("startWhen.actorName '%s' is not in the world being sampled."), *GateName);
    return false;
  }
  Out.Property = PropertyName;
  UObject *Owner = nullptr;
  if (!McpResolveActorPropertyPath(Gate, PropertyName, Owner)) {
    Error = FString::Printf(TEXT("startWhen.propertyName '%s' is not a property of %s or, as Component.Property, "
                                 "of one of its components."), *PropertyName, *Gate->GetClass()->GetName());
    return false;
  }
  // Read by the value's own JSON type: TryGetBoolField coerces a number (non-zero)
  // and any string (FString::ToBool, so "2" is true) to a bool, and a startWhen on
  // Phase == 2 waited for "True" and never fired.
  const TSharedPtr<FJsonValue> EqualsValue = When->TryGetField(TEXT("equals"));
  const EJson EqualsType = EqualsValue.IsValid() ? EqualsValue->Type : EJson::None;
  if (EqualsType == EJson::Boolean) {
    Out.Equals = EqualsValue->AsBool() ? TEXT("True") : TEXT("False");
  } else if (EqualsType == EJson::Number) {
    Out.Equals = FString::SanitizeFloat(EqualsValue->AsNumber());
  } else if (EqualsType == EJson::String) {
    Out.Equals = EqualsValue->AsString();
  } else {
    Error = TEXT("startWhen.equals is required: the value to wait for, as samples show it (\"True\", 3, \"Walking\").");
    return false;
  }
  When->TryGetBoolField(TEXT("waitForChange"), Out.bWaitForChange);
  double MaxWait = 10.0;
  When->TryGetNumberField(TEXT("maxWaitSeconds"), MaxWait);
  Out.Actor = Gate;
  Out.WaitStart = World->GetTimeSeconds();
  Out.Deadline = Out.WaitStart + FMath::Clamp(MaxWait, 0.0, 30.0);
  return true;
}

// True on the tick the start condition is met. With waitForChange the value has
// to BECOME equals: a platform already solid when the call arrives is waited out,
// so the run starts as it next appears instead of partway through.
bool McpMotionTriggerFired(FMcpMotionTrigger &Trigger) {
  AActor *Gate = Trigger.Actor.Get();
  UObject *Owner = nullptr;
  FProperty *Property = IsValid(Gate) ? McpResolveActorPropertyPath(Gate, Trigger.Property, Owner) : nullptr;
  if (!Property) {
    return false;
  }
  const FString Value = McpPropertyReflection::GetPropertyValueAsString(Owner, Property);
  const bool bMatch = McpMotionValueMatches(Value, Trigger.Equals);
  const bool bFired = bMatch && (Trigger.bSeen ? !McpMotionValueMatches(Trigger.Last, Trigger.Equals)
                                               : !Trigger.bWaitForChange);
  Trigger.Last = Value;
  Trigger.bSeen = true;
  return bFired;
}

// Why a startWhen never fired. The usual case is a value that already held when
// the call arrived: by default the run waits for it to CHANGE into equals, and a
// bare "startWhenTimeout" left the caller to guess that.
FString McpStartWhenTimeoutWarning(const FMcpMotionTrigger &Trigger) {
  const FString Name = Trigger.Property.IsEmpty() ? FString(TEXT("the property")) : Trigger.Property;
  if (Trigger.bWaitForChange && Trigger.bSeen && McpMotionValueMatches(Trigger.Last, Trigger.Equals)) {
    return FString::Printf(TEXT("startWhen: %s already read %s and never changed; by default the run starts ")
                           TEXT("only when the value CHANGES into equals. Pass waitForChange: false to start ")
                           TEXT("while it already holds."), *Name, *Trigger.Last);
  }
  return FString::Printf(TEXT("startWhen: %s never read %s within maxWaitSeconds (last read %s)."), *Name,
                         *Trigger.Equals, Trigger.bSeen ? *Trigger.Last : TEXT("nothing"));
}

// Game time stood still for seconds while the editor kept drawing frames: before this, the run sat out its whole
// real-time cap (25 s) for a single sample and the caller read it as a frozen actor.
FString McpGamePausedWarning(const UWorld *World) {
  return FString::Printf(TEXT("game time stopped advancing (the world %s paused): a title or pause menu holding the ")
                         TEXT("game (control_editor.simulate_input widget_list / widget_click gets past it), ")
                         TEXT("control_editor pause, or a set_game_speed step_frame session. Unpause, then run ")
                         TEXT("again."), World && World->IsPaused() ? TEXT("reads") : TEXT("does not read"));
}

// Keys pressed while the player's pawn stood perfectly still never reached it:
// a title or pause menu was up, or the game had locked its input. The samples
// alone read like a level that blocks the way, so the reply says it.
FString McpIgnoredInputsWarning(const AActor *Actor, int32 InputCount, int32 SampleCount, const FBox &Extent) {
  const APawn *Pawn = Cast<APawn>(Actor);
  if (InputCount == 0 || SampleCount < 2 || !Pawn || !Pawn->IsPlayerControlled() || Extent.GetSize().GetMax() > 1.0) {
    return FString();
  }
  return TEXT("keys were pressed but the player's pawn never moved, so the game did not act on them: a menu "
              "or pause screen on top (control_editor.simulate_input widget_list / widget_click gets past "
              "it), or input locked by the game. A screenshot shows which.");
}

// A slow editor steps PIE a third of a second at a time: every key and sample lands that late, so a 0.2 s jump
// held for 0.67 s and the samples read as the game's own behaviour. The run itself put the window up and the
// background throttle off, so a slow run is the render cost (video memory ran out once), never the throttle the
// warning used to blame.
FString McpSlowFrameWarning(double GameSeconds, int32 Frames) {
  const double PerFrame = Frames > 1 ? GameSeconds / Frames : 0.0;
  if (PerFrame < 0.1) {
    return FString();
  }
  return FString::Printf(TEXT("the game advanced %.2f s per frame (about %.0f fps), so inputs and samples landed "
                              "up to that late and holds ran long. The run had the editor window up and its "
                              "background throttle off, so the editor renders that slowly: a heavy scene, or video "
                              "memory exhausted (the viewport says so). set_game_speed fixed_delta_time makes "
                              "timing exact; the console command r.ScreenPercentage 50 lowers the render cost."),
                         PerFrame, 1.0 / PerFrame);
}

// Presses and releases what is due at Elapsed game seconds into the run; when
// the run has ended, releases every key still down so none stays stuck. A key
// is never released in the frame it went down, or the game would not see it.
void McpApplyMotionInputs(TArray<FMcpMotionInput> &Inputs, double Elapsed, bool bRunEnded) {
  for (FMcpMotionInput &Input : Inputs) {
    if (!bRunEnded && Input.DownAt < 0.0 && Elapsed >= Input.At) {
      McpSendMotionKey(Input.Key, true);
      Input.DownAt = Elapsed;
      Input.DownFrame = GFrameCounter;
    }
    const bool bDue = bRunEnded || (Elapsed >= Input.At + Input.Hold && GFrameCounter >= Input.DownFrame + 2);
    if (Input.DownAt >= 0.0 && Input.UpAt < 0.0 && bDue) {
      McpSendMotionKey(Input.Key, false);
      Input.UpAt = Elapsed;
    }
  }
}

TArray<TSharedPtr<FJsonValue>> McpMotionInputsJson(const TArray<FMcpMotionInput> &Inputs) {
  TArray<TSharedPtr<FJsonValue>> Out;
  for (const FMcpMotionInput &Input : Inputs) {
    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("key"), Input.Key);
    Entry->SetNumberField(TEXT("at"), McpRoundMs(Input.At));
    Entry->SetNumberField(TEXT("hold"), McpRoundMs(Input.Hold));
    if (Input.DownAt >= 0.0) {
      Entry->SetNumberField(TEXT("down"), McpRoundMs(Input.DownAt));
    }
    if (Input.UpAt >= 0.0) {
      Entry->SetNumberField(TEXT("up"), McpRoundMs(Input.UpAt));
    }
    Out.Add(MakeShared<FJsonValueObject>(Entry));
  }
  return Out;
}
