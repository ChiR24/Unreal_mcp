// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabInterchange.h"

#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

namespace McpFabInterchange
{
namespace
{
const TCHAR* const AssetsPipelinePath = TEXT("/Script/InterchangePipelines.InterchangeGenericAssetsPipeline");
const TCHAR* const MeshPipelinePath = TEXT("/Script/InterchangePipelines.InterchangeGenericMeshPipeline");
const TCHAR* const ManagerPath = TEXT("/Script/InterchangeEngine.InterchangeManager");

/** UE 5.8 names the setting CombineStaticMeshesBehavior (an enum); earlier engines have bCombineStaticMeshes. */
const FEnumProperty* FindBehaviorProperty(const UClass* MeshPipeline)
{
	return CastField<FEnumProperty>(MeshPipeline->FindPropertyByName(TEXT("CombineStaticMeshesBehavior")));
}

const FBoolProperty* FindFlagProperty(const UClass* MeshPipeline)
{
	return CastField<FBoolProperty>(MeshPipeline->FindPropertyByName(TEXT("bCombineStaticMeshes")));
}

// The Interchange manager, through its own BlueprintCallable accessor, so InterchangeEngine is never linked.
UObject* FindManager()
{
	const UClass* ManagerClass = FindObject<UClass>(nullptr, ManagerPath);
	UFunction* GetManager = ManagerClass != nullptr ? ManagerClass->FindFunctionByName(TEXT("GetInterchangeManagerScripted")) : nullptr;
	if (GetManager == nullptr)
	{
		return nullptr;
	}
	TArray<uint8> Params;
	Params.SetNumZeroed(GetManager->ParmsSize);
	ManagerClass->GetDefaultObject()->ProcessEvent(GetManager, Params.GetData());
	const FObjectProperty* Returned = CastField<FObjectProperty>(GetManager->GetReturnProperty());
	return Returned != nullptr ? Returned->GetObjectPropertyValue_InContainer(Params.GetData()) : nullptr;
}

/** Turns combining off on one mesh pipeline; false when it was already off or has no such setting. */
bool ClearCombine(UObject* MeshPipeline)
{
	const UClass* Class = MeshPipeline->GetClass();
	if (const FEnumProperty* Behavior = FindBehaviorProperty(Class))
	{
		const UEnum* Enum = Behavior->GetEnum();
		const int64 Separate = Enum != nullptr ? Enum->GetValueByNameString(TEXT("DoNotCombine")) : INDEX_NONE;
		const FNumericProperty* Underlying = Behavior->GetUnderlyingProperty();
		void* Value = Behavior->ContainerPtrToValuePtr<void>(MeshPipeline);
		if (Separate == INDEX_NONE || Underlying == nullptr || Underlying->GetSignedIntPropertyValue(Value) == Separate)
		{
			return false;
		}
		Underlying->SetIntPropertyValue(Value, Separate);
		return true;
	}
	if (const FBoolProperty* Flag = FindFlagProperty(Class))
	{
		if (!Flag->GetPropertyValue_InContainer(MeshPipeline))
		{
			return false;
		}
		Flag->SetPropertyValue_InContainer(MeshPipeline, false);
		return true;
	}
	return false;
}
} // namespace

bool CanSeparateMeshes()
{
	const UClass* AssetsPipeline = FindObject<UClass>(nullptr, AssetsPipelinePath);
	const UClass* MeshPipeline = FindObject<UClass>(nullptr, MeshPipelinePath);
	return AssetsPipeline != nullptr && MeshPipeline != nullptr &&
		AssetsPipeline->FindPropertyByName(TEXT("MeshPipeline")) != nullptr &&
		(FindBehaviorProperty(MeshPipeline) != nullptr || FindFlagProperty(MeshPipeline) != nullptr);
}

int32 SeparateMeshes()
{
	const UClass* AssetsPipeline = FindObject<UClass>(nullptr, AssetsPipelinePath);
	if (AssetsPipeline == nullptr)
	{
		return 0;
	}
	const FObjectProperty* MeshProperty = CastField<FObjectProperty>(AssetsPipeline->FindPropertyByName(TEXT("MeshPipeline")));
	if (MeshProperty == nullptr)
	{
		return 0;
	}
	TArray<UObject*> Pipelines;
	GetObjectsOfClass(AssetsPipeline, Pipelines, /*bIncludeDerivedClasses=*/true, RF_ClassDefaultObject);
	int32 Changed = 0;
	for (UObject* Pipeline : Pipelines)
	{
		// Fab roots the pipelines it generates for an import task; the project's own live in assets.
		if (Pipeline == nullptr || !Pipeline->IsRooted())
		{
			continue;
		}
		UObject* Mesh = MeshProperty->GetObjectPropertyValue_InContainer(Pipeline);
		if (Mesh != nullptr && ClearCombine(Mesh))
		{
			++Changed;
		}
	}
	return Changed;
}

bool CancelTasks()
{
	UObject* Manager = FindManager();
	UFunction* CancelAll = Manager != nullptr ? Manager->FindFunction(TEXT("CancelAllTasks")) : nullptr;
	if (CancelAll == nullptr)
	{
		return false;
	}
	Manager->ProcessEvent(CancelAll, nullptr);
	return true;
}

bool IsActive()
{
	UObject* Manager = FindManager();
	UFunction* Query = Manager != nullptr ? Manager->FindFunction(TEXT("IsInterchangeActive")) : nullptr;
	if (Query == nullptr)
	{
		return false;
	}
	TArray<uint8> Params;
	Params.SetNumZeroed(Query->ParmsSize);
	Manager->ProcessEvent(Query, Params.GetData());
	const FBoolProperty* Answer = CastField<FBoolProperty>(Query->GetReturnProperty());
	return Answer != nullptr && Answer->GetPropertyValue_InContainer(Params.GetData());
}
} // namespace McpFabInterchange
