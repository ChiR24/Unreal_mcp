// list_viewmodels and read_widget_viewmodels (see McpWidgetViewModel.h): the project's viewmodel classes with the
// fields they notify through, and a Widget Blueprint's viewmodels and bindings.

#include "Domains/WidgetAuthoring/ViewModel/McpWidgetViewModel.h"

#include "Algo/AnyOf.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Dom/JsonObject.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetBPLoading.h"
#include "Engine/Blueprint.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetResolve.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectIterator.h"
#include "WidgetBlueprint.h"
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 3
#include "INotifyFieldValueChanged.h" // FieldNotification, a public dependency of UMG from 5.3
#endif

namespace McpWidgetViewModel
{
namespace
{
constexpr int32 MaxViewModels = 200;

// The fields a binding can read the class's changes from, as the class's own field notification descriptor lists them:
// native ones carry no metadata (UHT marks them in generated code), and a Blueprint's FieldNotify variables join the
// list through its generated class. The metadata a Blueprint variable carries counts too, for a class not compiled yet.
TArray<TSharedPtr<FJsonValue>> NotifyFields(const UClass* Class)
{
	TArray<FName> Names;
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 3
	// The interface found by path and reached through its address: UNotifyFieldValueChanged::StaticClass() would link
	// the FieldNotification module, which EngineWire only sees through UMG's headers.
	UClass* Interface = FindObject<UClass>(nullptr, TEXT("/Script/FieldNotification.NotifyFieldValueChanged"));
	UObject* Default = Class->GetDefaultObject(false);
	if (const INotifyFieldValueChanged* Notifier = static_cast<const INotifyFieldValueChanged*>(Default && Interface ? Default->GetInterfaceAddress(Interface) : nullptr))
	{
		Notifier->GetFieldNotificationDescriptor().ForEachField(Class, [&Names](UE::FieldNotification::FFieldId Id)
		{
			Names.AddUnique(Id.GetName());
			return true;
		});
	}
#endif
	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		if (It->HasMetaData(TEXT("FieldNotify")))
		{
			Names.AddUnique(It->GetFName());
		}
	}
	TArray<TSharedPtr<FJsonValue>> Fields;
	for (const FName Name : Names)
	{
		const FProperty* Property = FindFProperty<FProperty>(Class, Name);
		const UFunction* Function = Property ? nullptr : Class->FindFunctionByName(Name);
		const FProperty* Value = Property ? Property : Function ? Function->GetReturnProperty() : nullptr;
		TSharedPtr<FJsonObject> Field = MakeShared<FJsonObject>();
		Field->SetStringField(TEXT("name"), Name.ToString());
		FString Inner; // a container's element types: TArray then <USoundWave*>
		const FString Type = Value ? Value->GetCPPType(&Inner, 0) : FString();
		Field->SetStringField(TEXT("type"), Type + Inner);
		if (Function)
		{
			Field->SetBoolField(TEXT("function"), true);
		}
		Fields.Add(MakeShared<FJsonValueObject>(Field));
	}
	return Fields;
}

TSharedPtr<FJsonValue> ClassRow(const UClass* Class, const FString& AssetPath)
{
	TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
	Row->SetStringField(TEXT("class"), Class->GetPathName());
	if (!AssetPath.IsEmpty())
	{
		Row->SetStringField(TEXT("assetPath"), AssetPath);
	}
	Row->SetArrayField(TEXT("fields"), NotifyFields(Class));
	return MakeShared<FJsonValueObject>(Row);
}

bool IsStaleClass(const UClass* Class)
{
	static const TCHAR* const Prefixes[] = {TEXT("SKEL_"), TEXT("REINST_"), TEXT("TRASH_"), TEXT("HOTRELOADED_")};
	const FString Name = Class->GetName();
	return Class->HasAnyClassFlags(CLASS_NewerVersionExists | CLASS_Deprecated) ||
		Algo::AnyOf(Prefixes, [&Name](const TCHAR* Prefix) { return Name.StartsWith(Prefix); });
}
} // namespace

bool HandleList(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FSocket Socket)
{
	const FString Requested = GetJsonStringField(Payload, TEXT("folder"));
	const FNormalizedAssetPath Folder = NormalizeAssetPath(Requested.IsEmpty() ? FString(TEXT("/Game")) : Requested);
	if (!Folder.bIsValid)
	{
		Bridge.SendAutomationError(Socket, RequestId, Folder.ErrorMessage, TEXT("INVALID_ARGUMENT"));
		return true;
	}
	const UClass* Base = FindObject<UClass>(nullptr, TEXT("/Script/ModelViewViewModel.MVVMViewModelBase"));
	TSet<FString> NativeBases;
	TArray<TSharedPtr<FJsonValue>> Native;
	for (TObjectIterator<UClass> It; It && Base; ++It)
	{
		if (It->IsChildOf(Base) && !UBlueprint::GetBlueprintFromClass(*It) && !IsStaleClass(*It))
		{
			NativeBases.Add(It->GetPathName());
			if (*It != Base && !It->HasAnyClassFlags(CLASS_Abstract))
			{
				Native.Add(ClassRow(*It, FString()));
			}
		}
	}
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*Folder.Path));
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;
#if MCP_HAS_ASSET_CLASS_PATHS
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
#else
	Filter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
#endif
	TArray<FAssetData> Assets;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssets(Filter, Assets);
	TArray<TSharedPtr<FJsonValue>> Blueprints;
	for (const FAssetData& Asset : Assets)
	{
		FString NativeParent;
		Asset.GetTagValue(FName(TEXT("NativeParentClass")), NativeParent);
		if (Blueprints.Num() >= MaxViewModels || !NativeBases.Contains(FPackageName::ExportTextPathToObjectPath(NativeParent)))
		{
			continue;
		}
		const UBlueprint* Blueprint = Cast<UBlueprint>(Asset.GetAsset());
		if (Blueprint && Blueprint->GeneratedClass)
		{
			Blueprints.Add(ClassRow(Blueprint->GeneratedClass, Asset.PackageName.ToString()));
		}
	}
	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	Result->SetArrayField(TEXT("blueprints"), Blueprints);
	Result->SetArrayField(TEXT("native"), Native);
	Bridge.SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%d Blueprint viewmodel(s) under %s and %d native one(s)%s."),
		Blueprints.Num(), *Folder.Path, Native.Num(), Blueprints.Num() >= MaxViewModels ? TEXT(" (the first 200: name a folder)") : TEXT("")), Result);
	return true;
}

bool HandleRead(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FSocket Socket)
{
	const FString Path = GetJsonStringField(Payload, TEXT("widgetPath"));
	UWidgetBlueprint* Widget = WidgetAuthoringHelpers::LoadWidgetBlueprint(Path);
	if (!Widget)
	{
		Bridge.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No Widget Blueprint is at %s."), *Path), TEXT("NOT_FOUND"));
		return true;
	}
	UObject* ViewObject = View(Widget, false);
	const FViewArray Models = ViewArray(ViewObject, TEXT("AvailableViewModels"));
	const FViewArray Bindings = ViewArray(ViewObject, TEXT("Bindings"));
	TArray<TSharedPtr<FJsonValue>> ModelRows, BindingRows;
	for (int32 Index = 0; Index < Models.Num(); ++Index)
	{
		ModelRows.Add(MakeShared<FJsonValueObject>(ViewModelRow(Models, Index)));
	}
	for (int32 Index = 0; Index < Bindings.Num(); ++Index)
	{
		BindingRows.Add(MakeShared<FJsonValueObject>(BindingRow(ViewObject, Bindings, Index)));
	}
	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	Result->SetStringField(TEXT("widgetPath"), WidgetAuthoringHelpers::WidgetBlueprintPackagePath(Widget));
	Result->SetArrayField(TEXT("viewModels"), ModelRows);
	Result->SetArrayField(TEXT("bindings"), BindingRows);
	Bridge.SendAutomationResponse(Socket, RequestId, true, ViewObject ? FString::Printf(TEXT("%d viewmodel(s) and %d binding(s) on %s."),
		ModelRows.Num(), BindingRows.Num(), *Widget->GetName()) : FString::Printf(TEXT("%s has no viewmodel yet (add_widget_viewmodel adds one)."),
		*Widget->GetName()), Result);
	return true;
}
}
