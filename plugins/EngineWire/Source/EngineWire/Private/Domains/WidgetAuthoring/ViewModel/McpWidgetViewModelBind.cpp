// bind_widget_viewmodel (see McpWidgetViewModel.h): both paths are resolved on scratch values first, so a name that is
// not there changes nothing; then the editor's AddBinding makes the binding (found again by its id, as the UFUNCTION
// hands back a copy), its paths and mode are written through the same pre- and post-edit chain the View Bindings panel
// uses, and the Widget Blueprint is compiled once that undo step is closed. A binding the MVVM compiler rejects is taken
// out again, leaving no undo step.

#include "Domains/WidgetAuthoring/ViewModel/McpWidgetViewModel.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetBPLoading.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileError.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/StructOnScope.h"
#include "WidgetBlueprint.h"

namespace McpWidgetViewModel
{
namespace
{
// One field of the binding at Index written as the View Bindings panel writes it: PreEditChange, the write,
// PostEditChangeChainProperty, which tells the open widget editor its bindings changed.
void EditBinding(UObject* ViewObject, const FViewArray& Bindings, const TCHAR* Field, TFunctionRef<void()> Write)
{
	FProperty* Changed = FieldOf(Bindings.Struct, Field);
	FEditPropertyChain Chain;
	Chain.AddTail(Bindings.Property);
	Chain.AddTail(Changed);
	Chain.SetActivePropertyNode(Changed);
	ViewObject->PreEditChange(Chain);
	Write();
	FPropertyChangedEvent Event(Changed, EPropertyChangeType::ValueSet);
	FPropertyChangedChainEvent ChainEvent(Chain, Event);
	ViewObject->PostEditChangeChainProperty(ChainEvent);
}
int32 IndexOf(const FViewArray& Bindings, const FGuid& Id)
{
	for (int32 At = 0; Id.IsValid() && At < Bindings.Num(); ++At)
	{
		if (ReadGuid(Bindings.Struct, Bindings.At(At), TEXT("BindingId")) == Id)
		{
			return At;
		}
	}
	return INDEX_NONE;
}

// A binding the compiler rejects, taken out again: its undo step undone with no redo left when it is still the editor's
// last one (a view made for it goes too), else the binding removed from the view's array.
void TakeBack(UWidgetBlueprint* Widget, UObject* ViewObject, const FGuid& Id, bool bRecorded)
{
	UTransactor* Trans = nullptr;
	if (GEditor)
	{
		Trans = GEditor->Trans;
	}
	const int32 Last = Trans ? Trans->GetQueueLength() - 1 : INDEX_NONE;
	const FTransaction* Top = Last >= 0 ? Trans->GetTransaction(Last) : nullptr;
	if (bRecorded && Top && Trans->GetUndoCount() == 0 && Top->GetTitle().ToString() == TEXT("Bind Viewmodel"))
	{
		GEditor->UndoTransaction(false);
		return;
	}
	const FViewArray Bindings = ViewArray(ViewObject, TEXT("Bindings"));
	const int32 Index = IndexOf(Bindings, Id);
	if (Index != INDEX_NONE)
	{
		FScriptArrayHelper(Bindings.Property, Bindings.Data).RemoveValues(Index, 1);
		FPropertyChangedEvent Removed(Bindings.Property, EPropertyChangeType::ArrayRemove);
		ViewObject->PostEditChangeProperty(Removed);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Widget);
	}
}
} // namespace

bool HandleBind(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FSocket Socket)
{
	const auto Refuse = [&](const FString& Message, const TCHAR* Code)
	{
		Bridge.SendAutomationError(Socket, RequestId, Message + TEXT(" Nothing was changed."), Code);
		return true;
	};
	const FString Path = GetJsonStringField(Payload, TEXT("widgetPath"));
	const FString Source = GetJsonStringField(Payload, TEXT("source"));
	const FString Destination = GetJsonStringField(Payload, TEXT("destination"));
	const TCHAR* Mode = ModeEntry(GetJsonStringField(Payload, TEXT("mode"), TEXT("one_way")));
	UWidgetBlueprint* Widget = WidgetAuthoringHelpers::LoadWidgetBlueprint(Path);
	const UScriptStruct* PathStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/ModelViewViewModelBlueprint.MVVMBlueprintPropertyPath"));
	if (!Widget)
	{
		return Refuse(FString::Printf(TEXT("No Widget Blueprint is at %s."), *Path), TEXT("NOT_FOUND"));
	}
	if (!Mode || !PathStruct)
	{
		return Refuse(Mode ? FString(TEXT("This engine's MVVM has no property path struct.")) : FString(TEXT("mode is one_way, two_way, one_time or to_source.")),
			TEXT("INVALID_ARGUMENT"));
	}
	if (Widget->Status == BS_Error)
	{
		return Refuse(FString::Printf(TEXT("%s does not compile now, so a binding's own errors could not be told apart: compile it "
			"(manage_blueprint compile) and fix what it reports first."), *Widget->GetName()), TEXT("WIDGET_DOES_NOT_COMPILE"));
	}
	UObject* ViewObject = View(Widget, false);
	FString Error;
	for (const FString& Text : {Source, Destination})
	{
		FStructOnScope Scratch(PathStruct);
		if (!WritePath(Widget, ViewObject, Text, PathStruct, Scratch.GetStructMemory(), Error))
		{
			return Refuse(Error, TEXT("PATH_NOT_FOUND"));
		}
	}
	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	FGuid Id;
	bool bAdded = false;
	bool bRecorded = false;
	{
		FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Bind Viewmodel")), EMcpMutationDurability::EditorStateOnly, {Widget});
		ViewObject = View(Widget, true);
		const FArg Args[] = {{Widget}};
		Call(TEXT("AddBinding"), Args, [&Id](const FProperty& Return, const void* Value)
		{
			const FStructProperty* Binding = CastField<FStructProperty>(&Return);
			Id = Binding ? ReadGuid(Binding->Struct, Value, TEXT("BindingId")) : FGuid();
		});
		const FViewArray Bindings = ViewArray(ViewObject, TEXT("Bindings"));
		const int32 Index = IndexOf(Bindings, Id);
		if (Index != INDEX_NONE)
		{
			uint8* Binding = Bindings.At(Index);
			for (const TPair<const TCHAR*, FString>& Side : {TPair<const TCHAR*, FString>(TEXT("SourcePath"), Source), TPair<const TCHAR*, FString>(TEXT("DestinationPath"), Destination)})
			{
				EditBinding(ViewObject, Bindings, Side.Key, [&] { WritePath(Widget, ViewObject, Side.Value, PathStruct, ValuePtr(Bindings.Struct, Binding, Side.Key), Error); });
			}
			EditBinding(ViewObject, Bindings, TEXT("BindingType"), [&] { WriteEnum(Bindings.Struct, Binding, TEXT("BindingType"), Mode); });
			FBlueprintEditorUtils::MarkBlueprintAsModified(Widget);
			bAdded = true;
		}
		bRecorded = Transaction.IsRecordingUndo();
		Transaction.DescribeInto(Result);
	}
	// Compiled after the undo step closes, so the compiler's own objects stay out of it.
	FString CompileError;
	if (bAdded && McpCompileBlueprintWithError(Widget, CompileError))
	{
		const FViewArray Bindings = ViewArray(ViewObject, TEXT("Bindings"));
		Result->SetObjectField(TEXT("binding"), BindingRow(ViewObject, Bindings, IndexOf(Bindings, Id)));
	}
	else if (bAdded)
	{
		TakeBack(Widget, ViewObject, Id, bRecorded);
		FString Ignored;
		McpCompileBlueprintWithError(Widget, Ignored);
	}
	if (!Result->HasField(TEXT("binding")))
	{
		Bridge.SendAutomationError(Socket, RequestId, CompileError.IsEmpty() ? FString(TEXT("The editor added no binding to the widget's view."))
			: FString::Printf(TEXT("The MVVM compiler rejects %s -> %s, so the binding was taken out again: %s"), *Source, *Destination, *CompileError),
			CompileError.IsEmpty() ? TEXT("BINDING_NOT_ADDED") : TEXT("BINDING_INVALID"));
		return true;
	}
	Result->SetStringField(TEXT("widgetPath"), WidgetAuthoringHelpers::WidgetBlueprintPackagePath(Widget));
	Result->SetBoolField(TEXT("compiled"), true);
	Result->SetBoolField(TEXT("saved"), WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(Widget));
	Bridge.SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Bound %s -> %s (%s) in %s."), *Source, *Destination,
		*ModeName(Mode), *Widget->GetName()), Result);
	return true;
}
}
