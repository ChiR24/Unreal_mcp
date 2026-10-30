// The editor's own Slate UI, read: control_editor read_editor_ui (its windows and widgets as an outline, each listed
// widget with a ref that from takes to read one branch). It only reads; the capabilities do the editor's work.

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
