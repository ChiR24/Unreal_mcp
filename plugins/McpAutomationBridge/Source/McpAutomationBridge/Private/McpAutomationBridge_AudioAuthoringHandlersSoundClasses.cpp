#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleSoundClassActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction == TEXT("create_sound_class"))
	{
		FString Name = McpHandlerUtils::GetOptionalString(Params, TEXT("name"), TEXT(""));
		FString Path = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("path"), TEXT("/Game/Audio/Classes")), false);
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);

		if (Name.IsEmpty())
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required"));
		}

		FString PackagePath;
		FString PathError;
		if (!BuildAudioCreationPath(Path, Name, PackagePath, PathError))
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_ASSET_PATH"), PathError);
		}

		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("PACKAGE_ERROR"), TEXT("Failed to create package"));
		}

		USoundClass* NewClass = NewObject<USoundClass>(Package, FName(*Name), RF_Public | RF_Standalone);
		if (!NewClass)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_FAILED"), TEXT("Failed to create SoundClass"));
		}

		NewClass->Properties.Volume = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("volume"), 1.0));
		NewClass->Properties.Pitch = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("pitch"), 1.0));
		SaveAudioAsset(NewClass, bSave);
		Response->SetStringField(TEXT("assetPath"), NewClass->GetPathName());
		McpHandlerUtils::AddVerification(Response, NewClass);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	if (SubAction == TEXT("set_class_properties"))
	{
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);
		USoundClass* SoundClass = LoadSoundClassFromPath(AssetPath);
		if (!SoundClass)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CLASS_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundClass: %s"), *AssetPath));
		}

		if (Params->HasField(TEXT("volume"))) { SoundClass->Properties.Volume = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("volume"), 1.0)); }
		if (Params->HasField(TEXT("pitch"))) { SoundClass->Properties.Pitch = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("pitch"), 1.0)); }
		if (Params->HasField(TEXT("lowPassFilterFrequency"))) { SoundClass->Properties.LowPassFilterFrequency = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("lowPassFilterFrequency"), 20000.0)); }
		if (Params->HasField(TEXT("lfeBleed"))) { SoundClass->Properties.LFEBleed = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("lfeBleed"), 0.5)); }
		if (Params->HasField(TEXT("voiceCenterChannelVolume"))) { SoundClass->Properties.VoiceCenterChannelVolume = static_cast<float>(McpHandlerUtils::GetOptionalFloat(Params, TEXT("voiceCenterChannelVolume"), 0.0)); }

		SaveAudioAsset(SoundClass, bSave);
		Response->SetNumberField(TEXT("volume"), SoundClass->Properties.Volume);
		Response->SetNumberField(TEXT("pitch"), SoundClass->Properties.Pitch);
		Response->SetNumberField(TEXT("lowPassFilterFrequency"), SoundClass->Properties.LowPassFilterFrequency);
		McpHandlerUtils::AddVerification(Response, SoundClass);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	if (SubAction == TEXT("set_class_parent"))
	{
		FString AssetPath = NormalizeAudioPath(McpHandlerUtils::GetOptionalString(Params, TEXT("assetPath"), TEXT("")));
		FString ParentPath = McpHandlerUtils::GetOptionalString(Params, TEXT("parentPath"), TEXT(""));
		bool bSave = McpHandlerUtils::GetOptionalBool(Params, TEXT("save"), true);
		USoundClass* SoundClass = LoadSoundClassFromPath(AssetPath);
		if (!SoundClass)
		{
			return McpHandlerUtils::BuildErrorResponse(TEXT("CLASS_NOT_FOUND"), FString::Printf(TEXT("Could not load SoundClass: %s"), *AssetPath));
		}

		if (!ParentPath.IsEmpty())
		{
			USoundClass* ParentClass = LoadSoundClassFromPath(ParentPath);
			if (ParentClass)
			{
				SoundClass->ParentClass = ParentClass;
			}
		}
		else
		{
			SoundClass->ParentClass = nullptr;
		}
		SaveAudioAsset(SoundClass, bSave);
		Response->SetStringField(TEXT("parentPath"), SoundClass->ParentClass ? SoundClass->ParentClass->GetPathName() : TEXT(""));
		McpHandlerUtils::AddVerification(Response, SoundClass);
		Response->SetBoolField(TEXT("success"), true);
		return Response;
	}

	return nullptr;
}
}
#endif
