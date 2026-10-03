#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Audio/McpAutomationBridge_AudioHandlersPrivate.h"
#include "Containers/Ticker.h"
#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorScreenshotSupport.h"

// play_sound playback measure: plays a sound and reads the level the mixer's own per-sound meter (its envelope
// follower) reports while it plays, so a caller that cannot hear tells a working sound from a silent one.
// au.DumpActiveSounds only proves a sound started (a 0.2 s click is over before it lists anything), and a
// MetaSound whose audio output is never fed starts, lists and finishes like any other.
namespace
{
constexpr double McpSoundSilentDb = -60.0;
constexpr int32 McpSoundMeasureSteps = 100;

struct FMcpSoundMeasure
{
  TWeakObjectPtr<UAudioComponent> Component;
  double StartReal = 0.0, MaxSeconds = 5.0, Step = 0.05, Peak = 0.0, Sum = 0.0;
  int32 Updates = 0;
  TArray<double> Timeline; // the loudest reading in each step, -1 for a step no reading landed in
  bool bFinished = false;
  FMcpEditorRunHold Hold;
};

double McpSoundLevelDb(double Linear)
{
  return Linear > 1e-6 ? FMath::RoundToDouble(200.0 * FMath::LogX(10.0, Linear)) / 10.0 : -120.0;
}

void McpSoundMeasureReading(FMcpSoundMeasure &Run, float Value)
{
  const double Elapsed = FPlatformTime::Seconds() - Run.StartReal;
  const int32 Index = FMath::Clamp(static_cast<int32>(Elapsed / Run.Step), 0, McpSoundMeasureSteps - 1);
  while (Run.Timeline.Num() <= Index)
  {
    Run.Timeline.Add(-1.0);
  }
  Run.Timeline[Index] = FMath::Max(Run.Timeline[Index], static_cast<double>(Value));
  Run.Peak = FMath::Max(Run.Peak, static_cast<double>(Value));
  Run.Sum += Value;
  ++Run.Updates;
}

TSharedPtr<FJsonObject> McpSoundMeasureResult(const FMcpSoundMeasure &Run, const FString &Ended)
{
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetNumberField(TEXT("peakDb"), McpSoundLevelDb(Run.Peak));
  Data->SetNumberField(TEXT("averageDb"), McpSoundLevelDb(Run.Updates > 0 ? Run.Sum / Run.Updates : 0.0));
  Data->SetBoolField(TEXT("silent"), McpSoundLevelDb(Run.Peak) < McpSoundSilentDb);
  Data->SetNumberField(TEXT("envelopeUpdates"), Run.Updates);
  Data->SetStringField(TEXT("endedBecause"), Ended);
  Data->SetNumberField(TEXT("timelineStepSeconds"), Run.Step);
  TArray<TSharedPtr<FJsonValue>> Timeline;
  for (const double Reading : Run.Timeline)
  {
    if (Reading < 0.0)
    {
      Timeline.Add(MakeShared<FJsonValueNull>());
    }
    else
    {
      Timeline.Add(MakeShared<FJsonValueNumber>(McpSoundLevelDb(Reading)));
    }
  }
  Data->SetArrayField(TEXT("timeline"), Timeline);
  McpHandlerUtils::MarkNoAssetsChanged(Data); // the sound asset is played, not changed
  return Data;
}

FString McpSilentSoundWarning(const FMcpSoundMeasure &Run, bool bDeviceMuted)
{
  if (bDeviceMuted)
  {
    return TEXT("The world's audio device is muted (another world's device is the active one), so every sound reads "
                "silent here.");
  }
  return Run.Updates == 0
             ? TEXT("The engine never rendered it: it has nothing to play (a Sound Cue with no wave reaching its "
                    "output), or it failed to start or was culled (a concurrency limit, no free voice).")
             : TEXT("It played but stayed below -60 dB: nothing feeds its output (an empty Sound Cue, a MetaSound "
                    "whose audio output is unconnected) or a gain or volume of 0 silences it.");
}
} // namespace

namespace McpAudioHandlers
{
bool HandleMeasureActions(UMcpAutomationBridgeSubsystem *Self, const FString &RequestId, const FString &Lower,
                          const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (Lower != TEXT("play_sound_measure"))
  {
    return false;
  }
  FString SoundPath;
  Payload->TryGetStringField(TEXT("soundPath"), SoundPath);
  USoundBase *Sound = SoundPath.IsEmpty() ? nullptr : ResolveSoundAsset(SoundPath);
  if (!Sound)
  {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              FString::Printf(TEXT("Sound asset not found: %s"), *SoundPath), TEXT("ASSET_NOT_FOUND"));
    return true;
  }
  // A running game's world while PIE plays: its audio device is the active one and the editor world's is muted.
  UWorld *PlayWorld = GEditor ? GEditor->PlayWorld.Get() : nullptr;
  UWorld *World = PlayWorld ? PlayWorld : GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  FAudioDeviceHandle Device = World ? World->GetAudioDevice() : FAudioDeviceHandle();
  double Volume = 1.0, MaxSeconds = 5.0;
  Payload->TryGetNumberField(TEXT("volume"), Volume);
  Payload->TryGetNumberField(TEXT("maxSeconds"), MaxSeconds);
  UAudioComponent *Comp = Device.IsValid()
                              ? UGameplayStatics::CreateSound2D(World, Sound, static_cast<float>(Volume), 1.f, 0.f,
                                                                nullptr, false, true)
                              : nullptr;
  if (!Comp)
  {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("No sound could be created: the editor has no audio device (-nosound) or the "
                                   "sound is not playable."),
                              TEXT("AUDIO_UNAVAILABLE"));
    return true;
  }

  TSharedRef<FMcpSoundMeasure> Run = MakeShared<FMcpSoundMeasure>();
  Run->Component = Comp;
  Run->MaxSeconds = FMath::Clamp(MaxSeconds, 0.2, 20.0);
  // Looping sounds and every MetaSound report about 10000 s, so only maxSeconds bounds them.
  const double Duration = Sound->GetDuration();
  const double Window = Duration < 9000.0 ? FMath::Min(Run->MaxSeconds, Duration + 0.3) : Run->MaxSeconds;
  Run->Step = FMath::Max(0.05, Window / McpSoundMeasureSteps);
  const bool bDeviceMuted = Device->IsAudioDeviceMuted();
  // Off the editor's master volume, which drops to the unfocused level (0 by default) whenever the editor is in
  // the background: the meter reads a sound after its volume, so every sound would read silent.
  Comp->bIsPreviewSound = true;
  // Bound before Play: the active sound turns its envelope follower on only when one of these is bound then.
  Comp->OnAudioSingleEnvelopeValueNative.AddLambda(
      [Run](const UAudioComponent *, const USoundWave *, const float Value) { McpSoundMeasureReading(*Run, Value); });
  Comp->OnAudioFinishedNative.AddLambda([Run](UAudioComponent *) { Run->bFinished = true; });
  // The meter is read once per editor frame, and a minimized, throttled editor draws about 3 a second: a short
  // click would fall between two readings. A screenshot's restore (no focus taken) and the throttle off, undone below.
  Run->Hold = BeginEditorRunForMcp();
  Run->StartReal = FPlatformTime::Seconds();
  Comp->Play();

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSelf(Self);
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([WeakSelf, Run, RequestId, RequestingSocket, SoundPath, bDeviceMuted](float) -> bool {
        UAudioComponent *Playing = Run->Component.Get();
        const double Elapsed = FPlatformTime::Seconds() - Run->StartReal;
        if (!Run->bFinished && Playing && Elapsed < Run->MaxSeconds)
        {
          return true;
        }
        // finished on its own, cut at maxSeconds, or gone (stopped, or its PIE world ended).
        const FString Ended = Run->bFinished ? TEXT("finished") : Playing ? TEXT("maxSeconds") : TEXT("stopped");
        if (Playing && !Run->bFinished)
        {
          Playing->Stop();
        }
        EndEditorRunForMcp(Run->Hold);
        UMcpAutomationBridgeSubsystem *Owner = WeakSelf.Get();
        if (!Owner)
        {
          return false;
        }
        const TSharedPtr<FJsonObject> Data = McpSoundMeasureResult(*Run, Ended);
        const bool bSilent = Data->GetBoolField(TEXT("silent"));
        const FString Message =
            FString::Printf(TEXT("%s%s: peak %.1f dB, average %.1f dB over %.2f s (%s)"), bSilent ? TEXT("SILENT ") : TEXT(""),
                            *SoundPath, Data->GetNumberField(TEXT("peakDb")), Data->GetNumberField(TEXT("averageDb")),
                            Elapsed, *Ended);
        TArray<FString> Warnings;
        if (bSilent)
        {
          Warnings.Add(McpSilentSoundWarning(*Run, bDeviceMuted));
        }
        SendStandardSuccessResponse(Owner, RequestingSocket, RequestId, Message, Data, Warnings);
        return false;
      }),
      0.0f);
  return true;
}
} // namespace McpAudioHandlers
