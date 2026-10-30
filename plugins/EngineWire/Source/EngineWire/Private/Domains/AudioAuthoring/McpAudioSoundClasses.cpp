#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAudioAuthoringPrivate.h"
#include "Foundation/McpScopedEditorTransaction.h"

namespace McpAudioAuthoring
{
// Loads the parentClass (parentPath is the older spelling) into OutParent; nullptr with no error when neither is sent.
static TSharedPtr<FJsonObject> ResolveParentSoundClass(const TSharedPtr<FJsonObject>& Params, USoundClass*& OutParent)
{
	OutParent = nullptr;
	const FString ParentPath = McpGetFirstStringField(Params, {TEXT("parentClass"), TEXT("parentPath")});
	if (!ParentPath.IsEmpty())
	{
		OutParent = LoadSoundClassFromPath(ParentPath);
	}
	if (!ParentPath.IsEmpty() && !OutParent)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("PARENT_NOT_FOUND"), FString::Printf(TEXT("Could not load parent SoundClass: %s"), *ParentPath));
	}
	return nullptr;
}

// Moves SoundClass under NewParent, keeping both ChildClasses lists in step (a mix modifier's applyToChildren walks
// them). The caller records the classes for undo.
static void LinkSoundClass(USoundClass* SoundClass, USoundClass* NewParent)
{
	if (USoundClass* OldParent = SoundClass->ParentClass)
	{
		OldParent->ChildClasses.Remove(SoundClass);
	}
	SoundClass->ParentClass = NewParent;
	if (NewParent)
	{
		NewParent->ChildClasses.AddUnique(SoundClass);
	}
}

// An error response when NewParent is SoundClass itself or one of its children; nullptr otherwise.
static TSharedPtr<FJsonObject> RefuseCyclicParent(USoundClass* SoundClass, USoundClass* NewParent)
{
	for (USoundClass* Ancestor = NewParent; Ancestor; Ancestor = Ancestor->ParentClass)
	{
		if (Ancestor == SoundClass)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CYCLIC_PARENT"), FString::Printf(TEXT("%s is %s itself or one of its children"), *NewParent->GetPathName(), *SoundClass->GetName()));
		}
	}
	return nullptr;
}

TSharedPtr<FJsonObject> HandleSoundClassActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction == TEXT("create_sound_class"))
	{
		FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
		FString Path = NormalizeAudioPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Audio/Classes")), false);
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

		if (Name.IsEmpty())
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required"));
		}
		USoundClass* ParentClass = nullptr;
		if (TSharedPtr<FJsonObject> ParentError = ResolveParentSoundClass(Params, ParentClass))
		{
			return ParentError;
		}

		FString PackagePath;
		FString PathError;
		if (!BuildAudioCreationPath(Path, Name, PackagePath, PathError))
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ASSET_PATH"), PathError);
		}

		FString CreateError, CreateCode;
		UPackage* Package = McpCreatePackageForNewAsset(PackagePath, CreateError, CreateCode);
		if (!Package)
		{
			return McpHandlerUtils::BuildErrorResponse(CreateCode, CreateError);
		}

		USoundClass* NewClass = NewObject<USoundClass>(Package, FName(*Name), RF_Public | RF_Standalone);
		if (!NewClass)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_FAILED"), TEXT("Failed to create SoundClass"));
		}

		NewClass->Properties.Volume = static_cast<float>(GetJsonNumberField(Params, TEXT("volume"), 1.0));
		NewClass->Properties.Pitch = static_cast<float>(GetJsonNumberField(Params, TEXT("pitch"), 1.0));
		LinkSoundClass(NewClass, ParentClass);
		if (ParentClass)
		{
			ParentClass->MarkPackageDirty();
			SaveAudioAsset(ParentClass, bSave);
		}
		Response->SetBoolField(TEXT("saved"), SaveAudioAsset(NewClass, bSave));
		Response->SetStringField(TEXT("assetPath"), NewClass->GetPathName());
		Response->SetStringField(TEXT("parentClass"), ParentClass ? ParentClass->GetPathName() : TEXT(""));
		Response->SetStringField(TEXT("message"),
			FString::Printf(TEXT("SoundClass '%s' created"), *NewClass->GetName()));
		McpHandlerUtils::AddVerification(Response, NewClass);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	if (SubAction == TEXT("set_class_properties"))
	{
		FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
		USoundClass* SoundClass = LoadSoundClassFromPath(AssetPath);
		if (!SoundClass)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CLASS_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundClass: %s"), *AssetPath));
		}

		// Each field is written only when sent; a call sending none saved the class untouched and answered success.
		const TPair<const TCHAR*, float*> Fields[] = {
			{TEXT("volume"), &SoundClass->Properties.Volume}, {TEXT("pitch"), &SoundClass->Properties.Pitch},
			{TEXT("lowPassFilterFrequency"), &SoundClass->Properties.LowPassFilterFrequency}, {TEXT("lfeBleed"), &SoundClass->Properties.LFEBleed},
			{TEXT("voiceCenterChannelVolume"), &SoundClass->Properties.VoiceCenterChannelVolume}};
		bool bAnySent = false;
		for (const TPair<const TCHAR*, float*>& Field : Fields)
		{
			bAnySent |= Params->HasField(Field.Key);
		}
		if (!bAnySent)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"), TEXT("Nothing to set: send volume, pitch, lowPassFilterFrequency, lfeBleed or voiceCenterChannelVolume."));
		}
		{
			// Modify with no transaction open recorded nothing: Ctrl+Z undid the edit before this one.
			const FMcpAssetEdit Edit(TEXT("Set Sound Class Properties"), {SoundClass}, Response);
			for (const TPair<const TCHAR*, float*>& Field : Fields)
			{
				if (Params->HasField(Field.Key))
				{
					*Field.Value = static_cast<float>(GetJsonNumberField(Params, Field.Key));
				}
			}
		}
		for (const TPair<const TCHAR*, float*>& Field : Fields)
		{
			Response->SetNumberField(Field.Key, *Field.Value);
		}
		Response->SetBoolField(TEXT("saved"), SaveAudioAsset(SoundClass, bSave));
		Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Sound class properties set on %s"), *SoundClass->GetName()));
		McpHandlerUtils::AddVerification(Response, SoundClass);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	if (SubAction == TEXT("set_class_parent"))
	{
		FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
		bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
		USoundClass* SoundClass = LoadSoundClassFromPath(AssetPath);
		if (!SoundClass)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CLASS_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundClass: %s"), *AssetPath));
		}

		// The contract names the parent `parentClass`; only `parentPath` was read, so every
		// gateway call cleared the parent, and a wrong path was ignored with success. A call that
		// sends no parent at all still cleared it; clearing takes an explicit empty parentClass.
		if (!Params->HasField(TEXT("parentClass")) && !Params->HasField(TEXT("parentPath")))
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ARGUMENT"), TEXT("Send parentClass: the parent SoundClass, or an empty string to clear the parent."));
		}
		USoundClass* ParentClass = nullptr;
		TSharedPtr<FJsonObject> Error = ResolveParentSoundClass(Params, ParentClass);
		if (!Error.IsValid())
		{
			Error = RefuseCyclicParent(SoundClass, ParentClass);
		}
		if (Error.IsValid())
		{
			return Error;
		}
		USoundClass* OldParent = SoundClass->ParentClass;
		TArray<UObject*> Edited = {SoundClass};
		for (USoundClass* Parent : {OldParent, ParentClass})
		{
			if (Parent && !Edited.Contains(Parent))
			{
				Edited.Add(Parent);
			}
		}
		{
			// The class and both parents' child lists change together: one undo step, the parents saved after it.
			const FMcpAssetEdit Edit(TEXT("Set Sound Class Parent"), Edited, Response);
			LinkSoundClass(SoundClass, ParentClass);
		}
		for (USoundClass* Parent : {OldParent, ParentClass})
		{
			if (Parent)
			{
				SaveAudioAsset(Parent, bSave);
			}
		}
		Response->SetBoolField(TEXT("saved"), SaveAudioAsset(SoundClass, bSave));
		Response->SetStringField(TEXT("parentPath"), SoundClass->ParentClass ? SoundClass->ParentClass->GetPathName() : TEXT(""));
		Response->SetStringField(TEXT("message"), SoundClass->ParentClass
			? FString::Printf(TEXT("Parent of %s set to %s"), *SoundClass->GetName(), *SoundClass->ParentClass->GetName())
			: FString::Printf(TEXT("Parent of %s cleared"), *SoundClass->GetName()));
		McpHandlerUtils::AddVerification(Response, SoundClass);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	return nullptr;
}
}
