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

/** UE 5.8 names the setting CombineStaticMeshesBehavior (an enum); earlier engines have bCombineStaticMeshes. */
const FEnumProperty* FindBehaviorProperty(const UClass* MeshPipeline)
{
	return CastField<FEnumProperty>(MeshPipeline->FindPropertyByName(TEXT("CombineStaticMeshesBehavior")));
}

const FBoolProperty* FindFlagProperty(const UClass* MeshPipeline)
{
	return CastField<FBoolProperty>(MeshPipeline->FindPropertyByName(TEXT("bCombineStaticMeshes")));
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
} // namespace McpFabInterchange
