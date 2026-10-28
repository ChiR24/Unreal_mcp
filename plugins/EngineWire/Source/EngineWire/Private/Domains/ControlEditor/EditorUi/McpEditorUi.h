// The editor's own Slate UI, read and used: control_editor read_editor_ui (its windows and widgets as an outline, each
// listed widget with a ref) and use_editor_ui (a click, a key or typed text delivered to one of those widgets). The last
// resort behind real capabilities, for features that have no API. Input goes through a Slate virtual user down the
// widget's own path, so the system cursor never moves, no window is activated and nothing is hit-tested on screen (the
// editor may be minimized); a request runs unattended, so a modal dialog a click opens is cancelled, never waited on.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class SWidget;
class UEngineWireSubsystem;

namespace McpEditorUi
{
using FUiSocket = TSharedPtr<FMcpBridgeWebSocket>;

bool HandleRead(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FUiSocket Socket);
bool HandleUse(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FUiSocket Socket);

// The ref of a widget ("e12"), the same for as long as the widget lives this session.
FString RefOf(const TSharedRef<SWidget>& Widget);
// The widget a ref names; null with bOutKnown when the ref was handed out but its widget is gone.
TSharedPtr<SWidget> Resolve(const FString& Ref, bool& bOutKnown);
// What a widget says: its own text (text blocks, editable text), else its accessible text; empty when none.
FString TextOf(const TSharedRef<SWidget>& Widget);
// Whether a read lists the widget: it has text, takes keyboard focus or is a control a person uses.
bool IsListed(const TSharedRef<SWidget>& Widget, const FString& Text);
// The widget's state flags as the outline shows them: " [disabled] [focused] [checked]".
FString FlagsOf(const TSharedRef<SWidget>& Widget);
}
