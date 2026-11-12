#pragma once

#include "McpConnectionManager.h"
#include "Foundation/McpSecureTokenCompare.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "EngineWireSettings.h"
#include "EngineWireSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Core/Subsystem/EngineWireSubsystemResponseSanitization.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// The editor's mounted content roots, no trailing slash, sorted
// (McpConnectionManagerContentRoots.cpp). Sent in bridge_ack and content_roots_changed.
TArray<TSharedPtr<FJsonValue>> McpBuildContentRootValues();

// {type: automation_event, event, payload}, serialized once and sent to each target.
inline void McpSendAutomationEvent(const TCHAR* EventName, const TSharedRef<FJsonObject>& Payload,
                                   const TArray<TSharedPtr<FMcpBridgeWebSocket>>& Targets) {
  const TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
  Event->SetStringField(TEXT("type"), TEXT("automation_event"));
  Event->SetStringField(TEXT("event"), EventName);
  Event->SetObjectField(TEXT("payload"), Payload);
  FString Serialized;
  const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
  FJsonSerializer::Serialize(Event, Writer);
  for (const TSharedPtr<FMcpBridgeWebSocket>& Sock : Targets) {
    Sock->Send(Serialized);
  }
}

// A requestId or action longer than this is refused.
inline constexpr int32 McpBridgeMaxIdChars = 128;

// Unreal logs a console command itself, so the request and response log lines leave it out.
inline bool McpIsEngineLoggedAction(const FString& Action) {
  return Action.Equals(TEXT("console_command"), ESearchCase::IgnoreCase);
}

// "key=value ..." log preview of Obj: strings quoted and cut to MaxString, arrays as [n], objects as {...},
// image payload fields omitted. bRequest also drops type/requestId and redacts code.
inline FString PreviewJsonFields(const TSharedPtr<FJsonObject>& Obj, int32 MaxString, bool bRequest) {
  TArray<FString> Parts;
  if (!Obj.IsValid()) {
    return FString();
  }
  for (const auto& Pair : Obj->Values) {
    const FString Key(Pair.Key.Len(), *Pair.Key);
    if (bRequest && (Key == TEXT("type") || Key == TEXT("requestId"))) {
      continue;
    }
    const TSharedPtr<FJsonValue>& Value = Pair.Value;
    FString Val = TEXT("?");
    if (bRequest && Key == TEXT("code")) {
      Val = TEXT("<redacted>");
    } else if (Key.Equals(TEXT("imageBase64"), ESearchCase::IgnoreCase) ||
               Key.Equals(TEXT("imageData"), ESearchCase::IgnoreCase) ||
               Key.Equals(TEXT("data"), ESearchCase::IgnoreCase)) {
      Val = TEXT("\"<omitted; see image content>\"");
    } else if (Value->Type == EJson::String) {
      Val = FString::Printf(TEXT("\"%s\""), *Value->AsString().Left(MaxString));
    } else if (Value->Type == EJson::Boolean) {
      Val = Value->AsBool() ? TEXT("true") : TEXT("false");
    } else if (Value->Type == EJson::Number) {
      Val = FString::Printf(TEXT("%g"), Value->AsNumber());
    } else if (Value->Type == EJson::Array) {
      Val = FString::Printf(TEXT("[%d]"), Value->AsArray().Num());
    } else if (Value->Type == EJson::Object) {
      Val = TEXT("{...}");
    }
    Parts.Add(Key + TEXT("=") + Val);
  }
  return FString::Join(Parts, TEXT(" "));
}
