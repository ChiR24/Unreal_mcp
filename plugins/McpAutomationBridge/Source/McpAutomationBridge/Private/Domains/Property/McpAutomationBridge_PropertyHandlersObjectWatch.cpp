#include "Domains/Property/McpAutomationBridge_PropertyHandlersObjectWatch.h"

#include "Containers/Ticker.h"
#include "Dom/JsonValue.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "HAL/PlatformTime.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Safety/McpSafeReflectionTarget.h"

// set_property's watch. A UMG pop or fade set off by a write runs on Slate time, even with PIE paused, and
// ended before any later call could read it: the HUD's 0.2 s LivesPop was only provable by slowing the asset
// 50x. The write and the values of what it set off now come back in one reply.
namespace McpPropertyWatch
{
namespace
{
constexpr int32 MaxWatchSamples = 400;

struct FWatchRun
{
  FWatch Watch;
  TSharedPtr<FJsonObject> Result;
  TArray<TSharedPtr<FJsonValue>> Samples;
  FString LastText;
  double Start = 0.0;
  double NextSample = 0.0;
  int32 Frames = 0;
};

// The watched value, resolved again every sample (a path can cross object references); null once it is gone.
TSharedPtr<FJsonValue> ReadWatched(const FWatch& Watch)
{
  UObject* Object = Watch.Object.Get();
  if (!Object)
  {
    return MakeShared<FJsonValueNull>();
  }
  const McpHandlerUtils::FPropertyResolveResult Resolved = McpHandlerUtils::ResolveProperty(Object, Watch.PropertyName);
  const TSharedPtr<FJsonValue> Value =
      Resolved.IsValid() ? McpPropertyReflection::ExportPropertyToJsonValue(Resolved.Container, Resolved.Property)
                         : TSharedPtr<FJsonValue>();
  return Value.IsValid() ? Value : MakeShared<FJsonValueNull>();
}

// One tick: sample when due and keep the sample only when the value changed. True once the run is over.
bool AdvanceWatch(FWatchRun& Run)
{
  const double Now = FPlatformTime::Seconds() - Run.Start;
  ++Run.Frames;
  if (Now >= Run.NextSample)
  {
    Run.NextSample = Now + Run.Watch.Interval;
    const TSharedPtr<FJsonValue> Value = ReadWatched(Run.Watch);
    const FString Text = McpHandlerUtils::JsonValueToString(Value);
    if (Run.Samples.Num() == 0 || Text != Run.LastText)
    {
      TSharedPtr<FJsonObject> Sample = MakeShared<FJsonObject>();
      Sample->SetNumberField(TEXT("t"), FMath::RoundToDouble(Now * 1000.0) / 1000.0);
      Sample->SetField(TEXT("value"), Value);
      Run.Samples.Add(MakeShared<FJsonValueObject>(Sample));
      Run.LastText = Text;
    }
  }
  return Now >= Run.Watch.Duration || Run.Samples.Num() >= MaxWatchSamples;
}
}

bool ParseWatch(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                TSharedPtr<FMcpBridgeWebSocket> Socket, UObject* DefaultObject, FWatch& Out, bool& bOutWatch)
{
  bOutWatch = false;
  const TSharedPtr<FJsonObject>* WatchObject = nullptr;
  if (!Payload.IsValid() || !Payload->TryGetObjectField(TEXT("watch"), WatchObject) || !WatchObject ||
      !WatchObject->IsValid())
  {
    return true;
  }
  (*WatchObject)->TryGetStringField(TEXT("propertyName"), Out.PropertyName);
  Out.PropertyName.TrimStartAndEndInline();
  // Omitted, the watch samples the property the call writes: watching the value just set is the common case.
  for (const TCHAR* Field : {TEXT("propertyPath"), TEXT("propertyName")})
  {
    if (Out.PropertyName.IsEmpty())
    {
      Payload->TryGetStringField(Field, Out.PropertyName);
      Out.PropertyName.TrimStartAndEndInline();
    }
  }
  if (Out.PropertyName.IsEmpty())
  {
    Bridge.SendAutomationError(Socket, RequestId,
                               TEXT("watch.propertyName is required when the call writes several properties: the one to sample after the write."),
                               TEXT("INVALID_ARGUMENT"));
    return false;
  }
  FString ObjectPath;
  (*WatchObject)->TryGetStringField(TEXT("objectPath"), ObjectPath);
  ObjectPath.TrimStartAndEndInline();
  UObject* Object = ObjectPath.IsEmpty() ? DefaultObject : McpHandlerUtils::ResolveObjectFromPath(ObjectPath);
  if (!Object)
  {
    Bridge.SendAutomationError(Socket, RequestId, McpHandlerUtils::DescribeObjectNotFound(ObjectPath), TEXT("OBJECT_NOT_FOUND"));
    return false;
  }
  // The same boundary get_property reads through: never the plugin's own settings.
  if (!McpSafeReflectionTarget::IsAddressable(Object))
  {
    Bridge.SendAutomationError(Socket, RequestId, McpSafeReflectionTarget::DenyMessage(), McpSafeReflectionTarget::DenyCode());
    return false;
  }
  const McpHandlerUtils::FPropertyResolveResult Resolved = McpHandlerUtils::ResolveProperty(Object, Out.PropertyName);
  if (!Resolved.IsValid())
  {
    Bridge.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("watch: %s"), *Resolved.Error), TEXT("PROPERTY_NOT_FOUND"));
    return false;
  }
  double Duration = 0.5;
  double Interval = 0.0;
  (*WatchObject)->TryGetNumberField(TEXT("durationSeconds"), Duration);
  (*WatchObject)->TryGetNumberField(TEXT("intervalSeconds"), Interval);
  Out.Object = Object;
  Out.ObjectLabel = ObjectPath.IsEmpty() ? Object->GetPathName() : ObjectPath;
  Out.Duration = FMath::Clamp(Duration, 0.05, 20.0);
  Out.Interval = FMath::Clamp(Interval, 0.0, 5.0);
  bOutWatch = true;
  return true;
}

void SendAfterWatch(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                    const TSharedPtr<FJsonObject>& Result, const FWatch& Watch)
{
  TSharedRef<FWatchRun> Run = MakeShared<FWatchRun>();
  Run->Watch = Watch;
  Run->Result = Result;
  Run->Start = FPlatformTime::Seconds();
  AdvanceWatch(*Run); // the value right after the write, before any frame ran
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakBridge(&Bridge);
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([WeakBridge, Run, RequestId, Socket](float) -> bool
      {
        UMcpAutomationBridgeSubsystem* Self = WeakBridge.Get();
        if (!Self)
        {
          return false;
        }
        if (!AdvanceWatch(*Run))
        {
          return true;
        }
        TSharedPtr<FJsonObject> WatchJson = MakeShared<FJsonObject>();
        WatchJson->SetStringField(TEXT("objectPath"), Run->Watch.ObjectLabel);
        WatchJson->SetStringField(TEXT("propertyName"), Run->Watch.PropertyName);
        WatchJson->SetArrayField(TEXT("samples"), Run->Samples);
        WatchJson->SetNumberField(TEXT("sampleCount"), Run->Samples.Num());
        WatchJson->SetBoolField(TEXT("changed"), Run->Samples.Num() > 1);
        WatchJson->SetNumberField(TEXT("frames"), Run->Frames);
        Run->Result->SetObjectField(TEXT("watch"), WatchJson);
        Self->SendAutomationResponse(Socket, RequestId, true,
                                     FString::Printf(TEXT("Property value updated; the watched %s took %d value(s) in %.2f s."),
                                                     *Run->Watch.PropertyName, Run->Samples.Num(), Run->Watch.Duration),
                                     Run->Result);
        return false;
      }),
      0.0f);
}
}
