// Viewmodels on Widget Blueprints (manage_blueprint list_viewmodels, read_widget_viewmodels, add_widget_viewmodel,
// bind_widget_viewmodel, remove_widget_viewmodel, set_field_notify), done by reflection: ModelViewViewModel is a Beta
// plugin in every 5.x, so nothing here links it (a module linking it would also leave the Fab build, which drops Beta
// and Experimental plugins). The editor's own UMVVMEditorSubsystem UFUNCTIONs make the view and add viewmodels and
// bindings; a binding's paths are written field by field as FMVVMBlueprintFieldPath's constructor writes them. The
// names read here are the same from 5.4 (the records' minEngineVersion) to 5.8.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FArrayProperty;
class FJsonObject;
class FJsonValue;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UWidgetBlueprint;

namespace McpWidgetViewModel
{
using FSocket = TSharedPtr<FMcpBridgeWebSocket>;

// One of the six actions; false for any other.
bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& Action,
            const TSharedPtr<FJsonObject>& Payload, FSocket Socket);
bool HandleList(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FSocket Socket);
bool HandleRead(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FSocket Socket);
bool HandleAdd(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FSocket Socket);
bool HandleBind(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FSocket Socket);
bool HandleRemove(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FSocket Socket);
bool HandleFieldNotify(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FSocket Socket);

// The MVVM editor subsystem; null where the plugin is off.
UObject* Subsystem();
// A subsystem UFUNCTION called with these object and name arguments in order (the rest keep their defaults); Read
// gets its return value. False when it is missing or takes one of them as another kind.
struct FArg
{
	UObject* Object = nullptr;
	FName Name;
};
bool Call(const TCHAR* Function, TArrayView<const FArg> Args, TFunctionRef<void(const FProperty&, const void*)> Read);
// The Widget Blueprint's UMVVMBlueprintView: RequestView makes it when missing, GetView does not.
UObject* View(UWidgetBlueprint* Widget, bool bCreate);

// One of the view's arrays (AvailableViewModels, Bindings) with the struct of its elements.
struct FViewArray
{
	FArrayProperty* Property = nullptr;
	UScriptStruct* Struct = nullptr;
	void* Data = nullptr;
	int32 Num() const;
	uint8* At(int32 Index) const;
};
FViewArray ViewArray(UObject* View, const TCHAR* Name);

// Reflected fields of a struct value; the zero value (and false from a write) when this engine has no such field.
FProperty* FieldOf(const UStruct* Struct, const TCHAR* Name);
FName ReadName(const UStruct* Struct, const void* Value, const TCHAR* Name);
FGuid ReadGuid(const UStruct* Struct, const void* Value, const TCHAR* Name);
bool ReadBool(const UStruct* Struct, const void* Value, const TCHAR* Name);
UObject* ReadObject(const UStruct* Struct, const void* Value, const TCHAR* Name);
FString ReadEnum(const UStruct* Struct, const void* Value, const TCHAR* Name);
bool WriteEnum(const UStruct* Struct, void* Value, const TCHAR* Name, const TCHAR* Entry);
void* ValuePtr(const UStruct* Struct, void* Value, const TCHAR* Name);

// The view's viewmodel named Name (case-insensitive): its index in AvailableViewModels, or INDEX_NONE.
int32 FindViewModel(UObject* View, const FString& Name);
// A viewmodel row (name, class, id, creation) and a binding row (id, source, destination, mode, enabled, compile,
// conversion), the paths written as "Head.Field.Field".
TSharedPtr<FJsonObject> ViewModelRow(const FViewArray& Models, int32 Index);
TSharedPtr<FJsonObject> BindingRow(UObject* View, const FViewArray& Bindings, int32 Index);
FString PathText(UObject* View, const UScriptStruct* PathStruct, const void* Path);
// Resolves "Head.Field..." (a viewmodel of the view, a widget of the tree, or Self) and writes it into Path, an
// FMVVMBlueprintPropertyPath value; OutError says what is missing, with the names that are there.
bool WritePath(UWidgetBlueprint* Widget, UObject* View, const FString& Text, const UScriptStruct* PathStruct, void* Path,
               FString& OutError);

// Whether Class sends field notifications (implements NotifyFieldValueChanged), as a viewmodel and a widget do.
bool Notifies(const UClass* Class);
// The binding modes by the names the records take (one_way, two_way, one_time, to_source) and back.
const TCHAR* ModeEntry(const FString& Mode);
FString ModeName(const FString& Entry);
}
