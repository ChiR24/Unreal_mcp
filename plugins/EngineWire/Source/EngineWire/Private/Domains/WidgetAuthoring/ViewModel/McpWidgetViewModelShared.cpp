// The reflection seam of the viewmodel actions (see McpWidgetViewModel.h): the MVVM editor subsystem's UFUNCTIONs, the
// view's arrays, and typed reads and writes of their fields.

#include "Domains/WidgetAuthoring/ViewModel/McpWidgetViewModel.h"

#include "Dom/JsonObject.h"
#include "Domains/WidgetAuthoring/McpWidgetActions.h"
#include "Editor.h"
#include "EditorSubsystem.h"
#include "EngineWireSubsystem.h"
#include "Foundation/Reflection/McpReflectedInvoke.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
bool HandleWidgetAuthoringViewModels(UEngineWireSubsystem& Subsystem, const FString& RequestId, const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, TSharedPtr<FJsonObject>)
{
	return McpWidgetViewModel::Handle(Subsystem, RequestId, SubAction, Payload, RequestingSocket);
}
}

namespace McpWidgetViewModel
{
bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action,
            const TSharedPtr<FJsonObject>& Payload, FSocket Socket)
{
	using FHandler = bool (*)(UEngineWireSubsystem&, const FString&, const TSharedPtr<FJsonObject>&, FSocket);
	static const TPair<const TCHAR*, FHandler> Handlers[] = {
		{TEXT("list_viewmodels"), HandleList}, {TEXT("read_widget_viewmodels"), HandleRead},
		{TEXT("add_widget_viewmodel"), HandleAdd}, {TEXT("bind_widget_viewmodel"), HandleBind},
		{TEXT("remove_widget_viewmodel"), HandleRemove}, {TEXT("set_field_notify"), HandleFieldNotify}};
	for (const TPair<const TCHAR*, FHandler>& Entry : Handlers)
	{
		if (Action.Equals(Entry.Key, ESearchCase::IgnoreCase))
		{
			if (!Subsystem())
			{
				Bridge.SendAutomationError(Socket, RequestId, TEXT("Viewmodels need the ModelViewViewModel plugin, which this project "
					"leaves off: enable_plugin ModelViewViewModel, then restart the editor. Nothing was changed."), TEXT("MVVM_DISABLED"));
				return true;
			}
			return Entry.Value(Bridge, RequestId, Payload, Socket);
		}
	}
	return false;
}

UObject* Subsystem()
{
	UClass* Class = FindObject<UClass>(nullptr, TEXT("/Script/ModelViewViewModelEditor.MVVMEditorSubsystem"));
	return Class && GEditor && Class->IsChildOf(UEditorSubsystem::StaticClass()) ? GEditor->GetEditorSubsystemBase(Class) : nullptr;
}

bool Call(const TCHAR* FunctionName, TArrayView<const FArg> Args, TFunctionRef<void(const FProperty&, const void*)> Read)
{
	UObject* Target = Subsystem();
	UFunction* Function = Target ? Target->FindFunction(FName(FunctionName)) : nullptr;
	if (!Function)
	{
		return false;
	}
	FMcpScopedParamBlock Block(Function);
	const FProperty* Return = nullptr;
	int32 Next = 0;
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			Return = *It;
			continue;
		}
		if (Next >= Args.Num())
		{
			continue;
		}
		void* Value = It->ContainerPtrToValuePtr<void>(Block.Data());
		if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(*It))
		{
			Object->SetObjectPropertyValue(Value, Args[Next].Object);
		}
		else if (const FNameProperty* Name = CastField<FNameProperty>(*It))
		{
			Name->SetPropertyValue(Value, Args[Next].Name);
		}
		else
		{
			return false;
		}
		++Next;
	}
	if (Next < Args.Num())
	{
		return false;
	}
	Target->ProcessEvent(Function, Block.Data());
	if (Return)
	{
		Read(*Return, Return->ContainerPtrToValuePtr<void>(Block.Data()));
	}
	return true;
}

UObject* View(UWidgetBlueprint* Widget, bool bCreate)
{
	UObject* Result = nullptr;
	const FArg Args[] = {{Widget}};
	Call(bCreate ? TEXT("RequestView") : TEXT("GetView"), Args, [&Result](const FProperty& Return, const void* Value)
	{
		if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(&Return))
		{
			Result = Object->GetObjectPropertyValue(Value);
		}
	});
	return Result;
}

int32 FViewArray::Num() const
{
	return Property ? FScriptArrayHelper(Property, Data).Num() : 0;
}

uint8* FViewArray::At(int32 Index) const
{
	return FScriptArrayHelper(Property, Data).GetRawPtr(Index);
}

FViewArray ViewArray(UObject* View, const TCHAR* Name)
{
	FViewArray Out;
	FArrayProperty* Array = View ? FindFProperty<FArrayProperty>(View->GetClass(), Name) : nullptr;
	const FStructProperty* Inner = Array ? CastField<FStructProperty>(Array->Inner) : nullptr;
	if (Inner)
	{
		Out.Property = Array;
		Out.Struct = Inner->Struct;
		Out.Data = Array->ContainerPtrToValuePtr<void>(View);
	}
	return Out;
}

FProperty* FieldOf(const UStruct* Struct, const TCHAR* Name)
{
	return Struct ? Struct->FindPropertyByName(FName(Name)) : nullptr;
}

void* ValuePtr(const UStruct* Struct, void* Value, const TCHAR* Name)
{
	const FProperty* Field = FieldOf(Struct, Name);
	return Field && Value ? Field->ContainerPtrToValuePtr<void>(Value) : nullptr;
}

FName ReadName(const UStruct* Struct, const void* Value, const TCHAR* Name)
{
	const FNameProperty* Field = CastField<FNameProperty>(FieldOf(Struct, Name));
	return Field && Value ? Field->GetPropertyValue_InContainer(Value) : NAME_None;
}

FGuid ReadGuid(const UStruct* Struct, const void* Value, const TCHAR* Name)
{
	const FStructProperty* Field = CastField<FStructProperty>(FieldOf(Struct, Name));
	return Field && Value && Field->Struct == TBaseStructure<FGuid>::Get() ? *Field->ContainerPtrToValuePtr<FGuid>(Value) : FGuid();
}

bool ReadBool(const UStruct* Struct, const void* Value, const TCHAR* Name)
{
	const FBoolProperty* Field = CastField<FBoolProperty>(FieldOf(Struct, Name));
	return Field && Value && Field->GetPropertyValue_InContainer(Value);
}

UObject* ReadObject(const UStruct* Struct, const void* Value, const TCHAR* Name)
{
	const FObjectPropertyBase* Field = CastField<FObjectPropertyBase>(FieldOf(Struct, Name));
	return Field && Value ? Field->GetObjectPropertyValue_InContainer(Value) : nullptr;
}

FString ReadEnum(const UStruct* Struct, const void* Value, const TCHAR* Name)
{
	const FEnumProperty* Field = CastField<FEnumProperty>(FieldOf(Struct, Name));
	if (!Field || !Value)
	{
		return FString();
	}
	const int64 Number = Field->GetUnderlyingProperty()->GetSignedIntPropertyValue(Field->ContainerPtrToValuePtr<void>(Value));
	return Field->GetEnum()->GetNameStringByValue(Number);
}

bool WriteEnum(const UStruct* Struct, void* Value, const TCHAR* Name, const TCHAR* Entry)
{
	const FEnumProperty* Field = CastField<FEnumProperty>(FieldOf(Struct, Name));
	const UEnum* Enum = Field ? Field->GetEnum() : nullptr;
	for (int32 Index = 0; Enum && Value && Index < Enum->NumEnums(); ++Index)
	{
		if (Enum->GetNameStringByIndex(Index) == Entry)
		{
			Field->GetUnderlyingProperty()->SetIntPropertyValue(Field->ContainerPtrToValuePtr<void>(Value), Enum->GetValueByIndex(Index));
			return true;
		}
	}
	return false;
}

int32 FindViewModel(UObject* View, const FString& Name)
{
	const FViewArray Models = ViewArray(View, TEXT("AvailableViewModels"));
	for (int32 Index = 0; Index < Models.Num(); ++Index)
	{
		if (ReadName(Models.Struct, Models.At(Index), TEXT("ViewModelName")).ToString().Equals(Name, ESearchCase::IgnoreCase))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool Notifies(const UClass* Class)
{
	const UClass* Interface = FindObject<UClass>(nullptr, TEXT("/Script/FieldNotification.NotifyFieldValueChanged"));
	return Class && Interface && Class->ImplementsInterface(Interface);
}

namespace
{
const TCHAR* const Modes[][2] = {{TEXT("one_way"), TEXT("OneWayToDestination")}, {TEXT("two_way"), TEXT("TwoWay")},
	{TEXT("one_time"), TEXT("OneTimeToDestination")}, {TEXT("to_source"), TEXT("OneWayToSource")},
	{TEXT("one_time_to_source"), TEXT("OneTimeToSource")}};
}

const TCHAR* ModeEntry(const FString& Mode)
{
	for (const auto& Pair : Modes)
	{
		if (Mode.Equals(Pair[0], ESearchCase::IgnoreCase))
		{
			return Pair[1];
		}
	}
	return nullptr;
}

FString ModeName(const FString& Entry)
{
	for (const auto& Pair : Modes)
	{
		if (Entry == Pair[1])
		{
			return Pair[0];
		}
	}
	return Entry;
}
}
