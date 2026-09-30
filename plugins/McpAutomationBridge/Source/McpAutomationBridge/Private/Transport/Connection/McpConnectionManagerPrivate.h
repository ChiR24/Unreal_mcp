#pragma once

#include "McpConnectionManager.h"
#include "Foundation/McpSecureTokenCompare.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "McpAutomationBridgeSettings.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Core/Subsystem/McpAutomationBridgeSubsystemResponseSanitization.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// The editor's mounted content roots, no trailing slash, sorted
// (McpConnectionManagerContentRoots.cpp). Sent in bridge_ack and content_roots_changed.
TArray<TSharedPtr<FJsonValue>> McpBuildContentRootValues();

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
