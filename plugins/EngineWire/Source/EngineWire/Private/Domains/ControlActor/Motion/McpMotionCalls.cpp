// McpMotionCalls.cpp — see header.
#include "Domains/ControlActor/Motion/McpMotionCalls.h"

#include "Domains/ControlActor/McpActorSupport.h"
#include "Foundation/Reflection/McpReflectedInvoke.h"

namespace {
constexpr int32 McpMaxMotionCalls = 16;
}

bool McpParseMotionCalls(const TSharedPtr<FJsonObject> &Payload, TFunctionRef<UObject *(const FString &)> Find,
                         TArray<FMcpMotionCall> &Out, FString &Error) {
  const TArray<TSharedPtr<FJsonValue>> *Entries = nullptr;
  if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("calls"), Entries) || !Entries) {
    return true;
  }
  if (Entries->Num() > McpMaxMotionCalls) {
    Error = FString::Printf(TEXT("calls takes at most %d calls."), McpMaxMotionCalls);
    return false;
  }
  for (const TSharedPtr<FJsonValue> &Value : *Entries) {
    const TSharedPtr<FJsonObject> *Entry = nullptr;
    FMcpMotionCall Call;
    if (!Value.IsValid() || !Value->TryGetObject(Entry) || !Entry || !(*Entry)->TryGetStringField(TEXT("actorName"), Call.ActorName) ||
        !(*Entry)->TryGetStringField(TEXT("functionName"), Call.FunctionName)) {
      Error = TEXT("each call is {atSeconds, actorName, componentName?, functionName, arguments?}.");
      return false;
    }
    (*Entry)->TryGetNumberField(TEXT("atSeconds"), Call.AtSeconds);
    (*Entry)->TryGetStringField(TEXT("componentName"), Call.ComponentName);
    const TSharedPtr<FJsonObject> *Arguments = nullptr;
    if ((*Entry)->TryGetObjectField(TEXT("arguments"), Arguments) && Arguments) {
      Call.Arguments = *Arguments;
    }
    UObject *Target = Find(Call.ActorName);
    if (Target && !Call.ComponentName.IsEmpty()) {
      AActor *Owner = Cast<AActor>(Target);
      Target = Owner ? FindComponentByName(Owner, Call.ComponentName) : nullptr;
    }
    UFunction *Function = Target ? Target->FindFunction(*Call.FunctionName) : nullptr;
    if (!Function) {
      Error = !Target ? FString::Printf(TEXT("calls: no %s %s in the running game."), Call.ComponentName.IsEmpty() ? TEXT("actor") : TEXT("component"),
                                        *(Call.ComponentName.IsEmpty() ? Call.ActorName : Call.ActorName + TEXT(".") + Call.ComponentName))
                      : FString::Printf(TEXT("calls: %s has no function %s."), *Target->GetClass()->GetName(), *Call.FunctionName);
      return false;
    }
    Call.Target = Target;
    Call.Function = Function;
    Out.Add(MoveTemp(Call));
  }
  return true;
}

void McpApplyMotionCalls(TArray<FMcpMotionCall> &Calls, double Elapsed) {
  for (FMcpMotionCall &Call : Calls) {
    if (Call.bDone || Elapsed < Call.AtSeconds) {
      continue;
    }
    Call.bDone = true;
    Call.CalledAt = Elapsed;
    UObject *Target = Call.Target.Get();
    UFunction *Function = Call.Function.Get();
    if (!IsValid(Target) || !Function) {
      Call.Error = TEXT("its object was gone by then");
      continue;
    }
    TArray<TSharedPtr<FJsonValue>> Unset;
    McpInvokeWithArgs(Target, Function, Call.Arguments, Call.Outputs, Unset, Call.Error);
  }
}

TArray<TSharedPtr<FJsonValue>> McpMotionCallsJson(const TArray<FMcpMotionCall> &Calls) {
  TArray<TSharedPtr<FJsonValue>> Out;
  for (const FMcpMotionCall &Call : Calls) {
    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("functionName"), Call.FunctionName);
    Entry->SetNumberField(TEXT("atSeconds"), Call.AtSeconds);
    if (Call.bDone) {
      Entry->SetNumberField(TEXT("calledAt"), McpRoundTo(Call.CalledAt, 1000.0));
    } else {
      Entry->SetField(TEXT("calledAt"), MakeShared<FJsonValueNull>());
    }
    if (!Call.Error.IsEmpty()) {
      Entry->SetStringField(TEXT("error"), Call.Error);
    } else if (Call.Outputs.IsValid() && Call.Outputs->Values.Num() > 0) {
      Entry->SetObjectField(TEXT("outputs"), Call.Outputs);
    }
    Out.Add(MakeShared<FJsonValueObject>(Entry));
  }
  return Out;
}
