// add_widget_viewmodel and remove_widget_viewmodel (see McpWidgetViewModel.h). Each edit is one undo step; the Blueprint is compiled and saved after it. A binding is removed from the view's array itself, since the
// editor's RemoveBinding UFUNCTION finds its binding by address and a reflected call hands it a copy; one with a
// conversion function is left to the editor, which also removes the graph that function owns.

#include "Domains/WidgetAuthoring/ViewModel/McpWidgetViewModel.h"

#include "Dom/JsonObject.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetBPLoading.h"
#include "Engine/Blueprint.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Misc/PackageName.h"
#include "WidgetBlueprint.h"

namespace McpWidgetViewModel
{
namespace
{
// A class by path (/Script/Module.Class, /Game/Folder/VM_Health_C) or the class a Blueprint asset generates.
UClass* ResolveClass(const FString& Path)
{
	if (UClass* Class = Path.IsEmpty() ? nullptr : FindObject<UClass>(nullptr, *Path))
	{
		return Class;
	}
	const FString Package = FPackageName::ObjectPathToPackageName(Path);
	if (!FPackageName::IsValidLongPackageName(Package) || !FPackageName::DoesPackageExist(Package))
	{
		return nullptr;
	}
	UObject* Object = LoadObject<UObject>(nullptr, *(Path.Contains(TEXT(".")) ? Path : Package + TEXT(".") + FPackageName::GetShortName(Package)));
	const UBlueprint* Blueprint = Cast<UBlueprint>(Object);
	return Blueprint ? Blueprint->GeneratedClass.Get() : Cast<UClass>(Object);
}

void ChangedArray(UObject* ViewObject, const FViewArray& Array, EPropertyChangeType::Type Change)
{
	FPropertyChangedEvent Event(Array.Property, Change);
	ViewObject->PostEditChangeProperty(Event);
}

void Finish(UEngineWireSubsystem& Bridge, const FString& RequestId, FSocket Socket, UWidgetBlueprint* Widget,
            const TSharedPtr<FJsonObject>& Result, const FString& Message)
{
	Result->SetStringField(TEXT("widgetPath"), WidgetAuthoringHelpers::WidgetBlueprintPackagePath(Widget));
	Result->SetBoolField(TEXT("compiled"), WidgetAuthoringHelpers::RefreshWidgetBlueprintClass(Widget));
	Result->SetBoolField(TEXT("saved"), WidgetAuthoringHelpers::MarkWidgetBlueprintModifiedAndSave(Widget));
	Bridge.SendAutomationResponse(Socket, RequestId, true, Message, Result);
}
} // namespace

bool HandleAdd(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FSocket Socket)
{
	const auto Refuse = [&](const FString& Message, const TCHAR* Code)
	{
		Bridge.SendAutomationError(Socket, RequestId, Message + TEXT(" Nothing was changed."), Code);
		return true;
	};
	const FString Path = GetJsonStringField(Payload, TEXT("widgetPath"));
	UWidgetBlueprint* Widget = WidgetAuthoringHelpers::LoadWidgetBlueprint(Path);
	const FString ClassPath = GetJsonStringField(Payload, TEXT("viewModelClass"));
	UClass* Class = ResolveClass(ClassPath);
	const FString Name = GetJsonStringField(Payload, TEXT("name"));
	const FString Creation = GetJsonStringField(Payload, TEXT("creation"), TEXT("create_instance"));
	if (!Widget)
	{
		return Refuse(FString::Printf(TEXT("No Widget Blueprint is at %s."), *Path), TEXT("NOT_FOUND"));
	}
	if (!Class)
	{
		return Refuse(FString::Printf(TEXT("No class or Blueprint is at %s."), *ClassPath), TEXT("CLASS_NOT_FOUND"));
	}
	if (!Notifies(Class))
	{
		return Refuse(FString::Printf(TEXT("%s sends no field notifications: a viewmodel derives from MVVMViewModelBase (list_viewmodels lists "
			"them)."), *Class->GetName()), TEXT("NOT_A_VIEWMODEL"));
	}
	if (!Name.IsEmpty() && (FKismetNameValidator(Widget).IsValid(FName(*Name)) != EValidatorResult::Ok || FindViewModel(View(Widget, false), Name) != INDEX_NONE))
	{
		return Refuse(FString::Printf(TEXT("%s is taken in %s (a variable, widget, function or viewmodel has it)."), *Name, *Widget->GetName()), TEXT("NAME_TAKEN"));
	}
	const bool bAbstract = Class->HasAnyClassFlags(CLASS_Abstract);
	if ((Creation != TEXT("create_instance") && Creation != TEXT("manual")) || (bAbstract && Payload->HasField(TEXT("creation")) && Creation == TEXT("create_instance")))
	{
		return Refuse(bAbstract ? FString::Printf(TEXT("%s is abstract, so the widget cannot make one: creation manual (the game hands one over)."), *Class->GetName())
			: FString(TEXT("creation is create_instance (the widget makes its own) or manual (the game hands one over).")), TEXT("INVALID_ARGUMENT"));
	}
	// The editor picks the first creation type the class allows (Manual for most); the widget makes its own unless asked
	// otherwise, or unless the class is abstract.
	const TCHAR* Wanted = Creation == TEXT("manual") || bAbstract ? TEXT("Manual") : TEXT("CreateInstance");
	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	FGuid Id;
	UObject* ViewObject = nullptr;
	{
		FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Add Viewmodel")), EMcpMutationDurability::EditorStateOnly, {Widget});
		ViewObject = View(Widget, true);
		const FArg Args[] = {{Widget}, {Class}};
		Call(TEXT("AddViewModel"), Args, [&Id](const FProperty& Return, const void* Value)
		{
			const FStructProperty* Guid = CastField<FStructProperty>(&Return);
			Id = Guid && Guid->Struct == TBaseStructure<FGuid>::Get() ? *static_cast<const FGuid*>(Value) : FGuid();
		});
		FViewArray Models = ViewArray(ViewObject, TEXT("AvailableViewModels"));
		int32 Index = INDEX_NONE;
		for (int32 At = 0; Id.IsValid() && At < Models.Num(); ++At)
		{
			Index = ReadGuid(Models.Struct, Models.At(At), TEXT("ViewModelContextId")) == Id ? At : Index;
		}
		if (Index != INDEX_NONE && !Name.IsEmpty())
		{
			const FArg Rename[] = {{Widget}, {nullptr, ReadName(Models.Struct, Models.At(Index), TEXT("ViewModelName"))}, {nullptr, FName(*Name)}};
			Call(TEXT("RenameViewModel"), Rename, [](const FProperty&, const void*) {});
			Models = ViewArray(ViewObject, TEXT("AvailableViewModels"));
		}
		if (Index != INDEX_NONE && ReadEnum(Models.Struct, Models.At(Index), TEXT("CreationType")) != Wanted)
		{
			ViewObject->PreEditChange(Models.Property);
			WriteEnum(Models.Struct, Models.At(Index), TEXT("CreationType"), Wanted);
			ChangedArray(ViewObject, Models, EPropertyChangeType::ValueSet);
		}
		if (Index != INDEX_NONE)
		{
			Result->SetObjectField(TEXT("viewModel"), ViewModelRow(Models, Index));
		}
		Transaction.DescribeInto(Result);
	}
	if (!Result->HasField(TEXT("viewModel")))
	{
		Bridge.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("The editor added no viewmodel of %s to %s."), *Class->GetName(),
			*Widget->GetName()), TEXT("VIEWMODEL_NOT_ADDED"));
		return true;
	}
	Finish(Bridge, RequestId, Socket, Widget, Result, FString::Printf(TEXT("Added viewmodel %s (%s) to %s."),
		*Result->GetObjectField(TEXT("viewModel"))->GetStringField(TEXT("name")), *Class->GetName(), *Widget->GetName()));
	return true;
}

bool HandleRemove(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FSocket Socket)
{
	const auto Refuse = [&](const FString& Message, const TCHAR* Code)
	{
		Bridge.SendAutomationError(Socket, RequestId, Message + TEXT(" Nothing was changed."), Code);
		return true;
	};
	const FString Path = GetJsonStringField(Payload, TEXT("widgetPath"));
	const FString BindingText = GetJsonStringField(Payload, TEXT("bindingId"));
	const FString ModelName = GetJsonStringField(Payload, TEXT("viewModel"));
	UWidgetBlueprint* Widget = WidgetAuthoringHelpers::LoadWidgetBlueprint(Path);
	if (!Widget)
	{
		return Refuse(FString::Printf(TEXT("No Widget Blueprint is at %s."), *Path), TEXT("NOT_FOUND"));
	}
	if (BindingText.IsEmpty() == ModelName.IsEmpty())
	{
		return Refuse(TEXT("Name one thing to remove: bindingId (from read_widget_viewmodels) or viewModel."), TEXT("INVALID_ARGUMENT"));
	}
	UObject* ViewObject = View(Widget, false);
	const FViewArray Bindings = ViewArray(ViewObject, TEXT("Bindings"));
	const FStructProperty* PathField = CastField<FStructProperty>(FieldOf(Bindings.Struct, TEXT("SourcePath")));
	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	if (!BindingText.IsEmpty())
	{
		FGuid Id;
		int32 Index = INDEX_NONE;
		for (int32 At = 0; FGuid::Parse(BindingText, Id) && At < Bindings.Num(); ++At)
		{
			Index = ReadGuid(Bindings.Struct, Bindings.At(At), TEXT("BindingId")) == Id ? At : Index;
		}
		if (Index == INDEX_NONE)
		{
			return Refuse(FString::Printf(TEXT("%s has no binding %s (read_widget_viewmodels lists them)."), *Widget->GetName(), *BindingText), TEXT("BINDING_NOT_FOUND"));
		}
		const TSharedPtr<FJsonObject> Row = BindingRow(ViewObject, Bindings, Index);
		if (Row->GetBoolField(TEXT("conversion")))
		{
			return Refuse(TEXT("This binding runs a conversion function, whose graph only the editor removes with it: remove it in the "
				"widget editor's View Bindings panel."), TEXT("BINDING_HAS_CONVERSION"));
		}
		FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Remove Viewmodel Binding")), EMcpMutationDurability::EditorStateOnly, {Widget, ViewObject});
		FScriptArrayHelper(Bindings.Property, Bindings.Data).RemoveValues(Index, 1);
		ChangedArray(ViewObject, Bindings, EPropertyChangeType::ArrayRemove);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Widget);
		Result->SetObjectField(TEXT("removed"), Row);
		Transaction.DescribeInto(Result);
	}
	else
	{
		const FViewArray Models = ViewArray(ViewObject, TEXT("AvailableViewModels"));
		const int32 Index = FindViewModel(ViewObject, ModelName);
		if (Index == INDEX_NONE)
		{
			return Refuse(FString::Printf(TEXT("%s has no viewmodel %s."), *Widget->GetName(), *ModelName), TEXT("VIEWMODEL_NOT_FOUND"));
		}
		const FGuid Id = ReadGuid(Models.Struct, Models.At(Index), TEXT("ViewModelContextId"));
		TArray<FString> Users;
		for (int32 At = 0; PathField && At < Bindings.Num(); ++At)
		{
			for (const TCHAR* Side : {TEXT("SourcePath"), TEXT("DestinationPath")})
			{
				if (ReadGuid(PathField->Struct, ValuePtr(Bindings.Struct, Bindings.At(At), Side), TEXT("ContextId")) == Id)
				{
					Users.AddUnique(ReadGuid(Bindings.Struct, Bindings.At(At), TEXT("BindingId")).ToString(EGuidFormats::DigitsWithHyphens));
				}
			}
		}
		if (Users.Num() > 0)
		{
			return Refuse(FString::Printf(TEXT("Bindings still use %s: remove them first (bindingId %s)."), *ModelName, *FString::Join(Users, TEXT(", "))),
				TEXT("VIEWMODEL_IN_USE"));
		}
		const FName Removed = ReadName(Models.Struct, Models.At(Index), TEXT("ViewModelName"));
		const TSharedPtr<FJsonObject> Row = ViewModelRow(Models, Index);
		FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Remove Viewmodel")), EMcpMutationDurability::EditorStateOnly, {Widget, ViewObject});
		const FArg Args[] = {{Widget}, {nullptr, Removed}};
		Call(TEXT("RemoveViewModel"), Args, [](const FProperty&, const void*) {});
		if (FindViewModel(ViewObject, Removed.ToString()) != INDEX_NONE)
		{
			return Refuse(FString::Printf(TEXT("The editor keeps %s: this viewmodel is marked as one that cannot be removed."), *ModelName), TEXT("VIEWMODEL_LOCKED"));
		}
		Result->SetObjectField(TEXT("removed"), Row);
		Transaction.DescribeInto(Result);
	}
	Finish(Bridge, RequestId, Socket, Widget, Result, FString::Printf(TEXT("Removed %s from %s."), BindingText.IsEmpty() ? *ModelName : *BindingText, *Widget->GetName()));
	return true;
}
}
