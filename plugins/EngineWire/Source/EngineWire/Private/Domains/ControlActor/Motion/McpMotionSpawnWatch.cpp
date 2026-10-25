#include "Domains/ControlActor/Motion/McpMotionSpawnWatch.h"
#include "Containers/Ticker.h"
#include "Domains/ControlActor/McpActorSupport.h"
#include "Domains/ControlEditor/McpEditorScreenshotSupport.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"

// A thrown block, a projectile or an enemy of a wave lives a second or two: by the time a call could name it, it was
// gone or not born yet. The wait listens to the world's spawn event, so the first actor of the class the game spawns
// after the call is the one watched, from its first frame.
namespace {
struct FMcpSpawnWatch {
  TWeakObjectPtr<UWorld> World;
  TWeakObjectPtr<UClass> Class;
  TWeakObjectPtr<AActor> Spawned;
  FDelegateHandle Handle;
  double StartGame = 0.0, WaitFor = 10.0, StartReal = 0.0, MaxReal = 25.0;
  // A minimized editor steps PIE at about 3 fps, so the game the wait counts on runs on screen, as the run does.
  FMcpEditorRunHold Hold;
};

// Nothing spawned: answered as a startWhen that never fired is, a run with no samples that says why it ended.
void McpAnswerNoSpawn(UEngineWireSubsystem *Self, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &RequestId,
                      const FString &ClassName, double Waited, const FString &Ended) {
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), FString());
  Data->SetArrayField(TEXT("samples"), TArray<TSharedPtr<FJsonValue>>());
  Data->SetNumberField(TEXT("sampleCount"), 0);
  Data->SetNumberField(TEXT("gameSeconds"), 0);
  Data->SetNumberField(TEXT("waitedSeconds"), FMath::RoundToDouble(Waited * 1000.0) / 1000.0);
  Data->SetStringField(TEXT("endedBecause"), Ended);
  const FString Warning = FString::Printf(
      TEXT("No %s spawned while the call waited: check that what spawns it runs (its timer, or the event that "
           "triggers it), or raise spawnWaitSeconds."), *ClassName);
  SendStandardSuccessResponse(Self, Socket, RequestId,
                              FString::Printf(TEXT("No %s spawned in %.2f game seconds (%s). WARNING: %s"), *ClassName,
                                              Waited, *Ended, *Warning),
                              Data, {Warning});
}
} // namespace

void McpSampleMotionOfNextSpawn(UEngineWireSubsystem *Subsystem, const FString &RequestId,
                                const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                TFunction<void(const TSharedPtr<FJsonObject> &)> Run) {
  FString ClassName;
  Payload->TryGetStringField(TEXT("spawnOf"), ClassName);
  UClass *Class = ResolveClassByName(ClassName);
  if (!Class || !Class->IsChildOf(AActor::StaticClass())) {
    const FString SameNamed = McpSameNamedAssetPaths(ClassName);
    SendStandardErrorResponse(
        Subsystem, Socket, RequestId, TEXT("CLASS_NOT_FOUND"),
        FString::Printf(TEXT("spawnOf '%s' is not an actor class. A Blueprint takes its asset name (BP_Thing) or "
                             "generated-class path (/Game/Path/BP_Thing.BP_Thing_C), a native class its short name "
                             "(Character).%s"),
                        *ClassName,
                        SameNamed.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" Assets with that name: %s."), *SameNamed)));
    return;
  }
  UWorld *World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
  if (!World) {
    SendStandardErrorResponse(Subsystem, Socket, RequestId, TEXT("NOT_SIMULATING"),
                              TEXT("spawnOf waits for an actor the running game spawns: start PIE "
                                   "(control_editor.play) first."));
    return;
  }
  TSharedRef<FMcpSpawnWatch> Watch = MakeShared<FMcpSpawnWatch>();
  Payload->TryGetNumberField(TEXT("spawnWaitSeconds"), Watch->WaitFor);
  Payload->TryGetNumberField(TEXT("maxRealSeconds"), Watch->MaxReal);
  Watch->World = World;
  Watch->Class = Class;
  Watch->WaitFor = FMath::Clamp(Watch->WaitFor, 0.1, 30.0);
  Watch->MaxReal = FMath::Clamp(Watch->MaxReal, 1.0, 50.0);
  Watch->StartGame = World->GetTimeSeconds();
  Watch->StartReal = FPlatformTime::Seconds();
  Watch->Hold = BeginEditorRunForMcp();
  const TWeakPtr<FMcpSpawnWatch> WeakWatch = Watch;
  Watch->Handle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([WeakWatch](AActor *Actor) {
    const TSharedPtr<FMcpSpawnWatch> Pinned = WeakWatch.Pin();
    if (Pinned && !Pinned->Spawned.IsValid() && Actor && Pinned->Class.IsValid() && Actor->IsA(Pinned->Class.Get())) {
      Pinned->Spawned = Actor;
    }
  }));
  const TWeakObjectPtr<UEngineWireSubsystem> WeakSubsystem(Subsystem);
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([Watch, WeakSubsystem, RequestId, Payload, Socket, ClassName, Run](float) -> bool {
        UWorld *Now = Watch->World.Get();
        AActor *Actor = Watch->Spawned.Get();
        // Born and gone between two frames: wait for the next one.
        const bool bSpawned = IsValid(Actor) && !Actor->IsActorBeingDestroyed();
        const double Waited = Now ? Now->GetTimeSeconds() - Watch->StartGame : 0.0;
        const double RealLeft = Watch->MaxReal - (FPlatformTime::Seconds() - Watch->StartReal);
        if (!bSpawned && Now && Waited < Watch->WaitFor && RealLeft > 0.0) {
          return true;
        }
        if (Now) {
          Now->RemoveOnActorSpawnedHandler(Watch->Handle);
        }
        // Given back before the run starts: the run holds the window itself, and a hold it found taken it would
        // never give back (a window left on screen, the background throttle left off).
        EndEditorRunForMcp(Watch->Hold);
        UEngineWireSubsystem *Self = WeakSubsystem.Get();
        if (!Self) {
          return false;
        }
        if (!bSpawned) {
          const TCHAR *Ended = Now ? (RealLeft <= 0.0 ? TEXT("realTimeCap") : TEXT("spawnTimeout"))
                                   : (GEditor && GEditor->PlayWorld ? TEXT("levelChanged") : TEXT("worldEnded"));
          McpAnswerNoSpawn(Self, Socket, RequestId, ClassName, Waited, Ended);
          return false;
        }
        // The run gets what is left of the wall-clock budget, so the wait and the run still answer inside it.
        TSharedPtr<FJsonObject> Named = MakeShared<FJsonObject>();
        Named->Values = Payload->Values;
        Named->RemoveField(TEXT("spawnOf"));
        Named->SetStringField(TEXT("actorName"), McpActorRef(Actor));
        Named->SetNumberField(TEXT("maxRealSeconds"), FMath::Max(1.0, RealLeft));
        Run(Named);
        return false;
      }),
      0.0f);
}
