#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundAttenuation.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EditorAssetLibrary.h"
#include "Factories/SoundCueFactoryNew.h"
#include "Factories/SoundClassFactory.h"
#include "Factories/SoundMixFactory.h"
#include "Factories/SoundAttenuationFactory.h"
#include "AssetToolsModule.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundNodeAttenuation.h"
#include "Sound/SoundNodeLooping.h"
#include "Sound/SoundNodeModulator.h"
#include "AudioDevice.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleAudioAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.StartsWith(TEXT("create_sound_")) && 
        !Lower.StartsWith(TEXT("play_sound_")) && 
        !Lower.StartsWith(TEXT("set_sound_")) &&
        !Lower.StartsWith(TEXT("push_sound_")) &&
        !Lower.StartsWith(TEXT("pop_sound_")) &&
        !Lower.StartsWith(TEXT("create_audio_")) &&
        !Lower.StartsWith(TEXT("create_ambient_")) &&
        !Lower.StartsWith(TEXT("create_reverb_")) &&
        !Lower.StartsWith(TEXT("enable_audio_")) &&
        !Lower.StartsWith(TEXT("fade_sound")) &&
        !Lower.StartsWith(TEXT("set_doppler_")) &&
        !Lower.StartsWith(TEXT("set_audio_")))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid()) 
    { 
        SendAutomationError(RequestingSocket, RequestId, TEXT("Audio payload missing"), TEXT("INVALID_PAYLOAD")); 
        return true; 
    }

    if (Lower == TEXT("create_sound_cue"))
    {
        FString Name;
        if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("name required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString PackagePath;
        Payload->TryGetStringField(TEXT("packagePath"), PackagePath);
        if (PackagePath.IsEmpty()) PackagePath = TEXT("/Game/Audio/Cues");

        FString WavePath;
        Payload->TryGetStringField(TEXT("wavePath"), WavePath);

        USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, PackagePath, USoundCue::StaticClass(), Factory);
        USoundCue* SoundCue = Cast<USoundCue>(NewAsset);

        if (!SoundCue)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create SoundCue"), TEXT("ASSET_CREATION_FAILED"));
            return true;
        }

        // Basic graph setup if wave provided
        if (!WavePath.IsEmpty())
        {
            USoundWave* Wave = LoadObject<USoundWave>(nullptr, *WavePath);
            if (Wave)
            {
                USoundNodeWavePlayer* PlayerNode = SoundCue->ConstructSoundNode<USoundNodeWavePlayer>();
                PlayerNode->SetSoundWave(Wave);
                
                USoundNode* LastNode = PlayerNode;

                // Optional looping
                bool bLooping = false;
                if (Payload->TryGetBoolField(TEXT("looping"), bLooping) && bLooping)
                {
                    USoundNodeLooping* LoopNode = SoundCue->ConstructSoundNode<USoundNodeLooping>();
                    LoopNode->ChildNodes.Add(LastNode);
                    LastNode = LoopNode;
                }

                // Optional modulation (volume/pitch)
                double Volume = 1.0;
                double Pitch = 1.0;
                bool bHasVolume = Payload->TryGetNumberField(TEXT("volume"), Volume);
                bool bHasPitch = Payload->TryGetNumberField(TEXT("pitch"), Pitch);

                if (bHasVolume || bHasPitch)
                {
                    USoundNodeModulator* ModNode = SoundCue->ConstructSoundNode<USoundNodeModulator>();
                    ModNode->PitchMin = ModNode->PitchMax = (float)Pitch;
                    ModNode->VolumeMin = ModNode->VolumeMax = (float)Volume;
                    ModNode->ChildNodes.Add(LastNode);
                    LastNode = ModNode;
                }

                // Optional attenuation
                FString AttenuationPath;
                if (Payload->TryGetStringField(TEXT("attenuationPath"), AttenuationPath) && !AttenuationPath.IsEmpty())
                {
                    USoundAttenuation* Attenuation = LoadObject<USoundAttenuation>(nullptr, *AttenuationPath);
                    if (Attenuation)
                    {
                        USoundNodeAttenuation* AttenNode = SoundCue->ConstructSoundNode<USoundNodeAttenuation>();
                        AttenNode->AttenuationSettings = Attenuation;
                        AttenNode->ChildNodes.Add(LastNode);
                        LastNode = AttenNode;
                    }
                }

                SoundCue->FirstNode = LastNode;
                SoundCue->LinkGraphNodesFromSoundNodes();
            }
        }

        UEditorAssetLibrary::SaveAsset(SoundCue->GetPathName());

        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        Resp->SetBoolField(TEXT("success"), true);
        Resp->SetStringField(TEXT("path"), SoundCue->GetPathName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SoundCue created"), Resp);
        return true;
    }
    else if (Lower == TEXT("play_sound_at_location"))
    {
        FString SoundPath;
        if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) || SoundPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        USoundBase* Sound = LoadObject<USoundBase>(nullptr, *SoundPath);
        if (!Sound)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Sound asset not found"), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FVector Location = FVector::ZeroVector;
        const TArray<TSharedPtr<FJsonValue>>* LocArr;
        if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr && LocArr->Num() >= 3)
        {
            Location = FVector((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(), (*LocArr)[2]->AsNumber());
        }

        double Volume = 1.0; Payload->TryGetNumberField(TEXT("volume"), Volume);
        double Pitch = 1.0; Payload->TryGetNumberField(TEXT("pitch"), Pitch);
        double StartTime = 0.0; Payload->TryGetNumberField(TEXT("startTime"), StartTime);

        UGameplayStatics::PlaySoundAtLocation(GEditor->GetEditorWorldContext().World(), Sound, Location, (float)Volume, (float)Pitch, (float)StartTime);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sound played at location"), nullptr);
        return true;
    }
    else if (Lower == TEXT("play_sound_2d"))
    {
        FString SoundPath;
        if (!Payload->TryGetStringField(TEXT("soundPath"), SoundPath) || SoundPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("soundPath required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        USoundBase* Sound = LoadObject<USoundBase>(nullptr, *SoundPath);
        if (!Sound)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Sound asset not found"), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        double Volume = 1.0; Payload->TryGetNumberField(TEXT("volume"), Volume);
        double Pitch = 1.0; Payload->TryGetNumberField(TEXT("pitch"), Pitch);
        double StartTime = 0.0; Payload->TryGetNumberField(TEXT("startTime"), StartTime);

        UGameplayStatics::PlaySound2D(GEditor->GetEditorWorldContext().World(), Sound, (float)Volume, (float)Pitch, (float)StartTime);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sound played 2D"), nullptr);
        return true;
    }
    else if (Lower == TEXT("create_sound_class"))
    {
        FString Name;
        if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("name required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString PackagePath = TEXT("/Game/Audio/Classes");
        
        USoundClassFactory* Factory = NewObject<USoundClassFactory>();
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, PackagePath, USoundClass::StaticClass(), Factory);
        USoundClass* SoundClass = Cast<USoundClass>(NewAsset);

        if (SoundClass)
        {
            const TSharedPtr<FJsonObject>* Props;
            if (Payload->TryGetObjectField(TEXT("properties"), Props))
            {
                double Vol = 1.0;
                if ((*Props)->TryGetNumberField(TEXT("volume"), Vol))
                {
                    SoundClass->Properties.Volume = (float)Vol;
                }
                double Pitch = 1.0;
                if ((*Props)->TryGetNumberField(TEXT("pitch"), Pitch))
                {
                    SoundClass->Properties.Pitch = (float)Pitch;
                }
            }

            FString ParentClassPath;
            if (Payload->TryGetStringField(TEXT("parentClass"), ParentClassPath) && !ParentClassPath.IsEmpty())
            {
                USoundClass* Parent = LoadObject<USoundClass>(nullptr, *ParentClassPath);
                if (Parent)
                {
                    SoundClass->ParentClass = Parent;
                }
            }

            UEditorAssetLibrary::SaveAsset(SoundClass->GetPathName());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SoundClass created"), nullptr);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create SoundClass"), TEXT("ASSET_CREATION_FAILED"));
        }
        return true;
    }
    else if (Lower == TEXT("create_sound_mix"))
    {
        FString Name;
        if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("name required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString PackagePath = TEXT("/Game/Audio/Mixes");
        
        USoundMixFactory* Factory = NewObject<USoundMixFactory>();
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, PackagePath, USoundMix::StaticClass(), Factory);
        USoundMix* SoundMix = Cast<USoundMix>(NewAsset);

        if (SoundMix)
        {
            const TArray<TSharedPtr<FJsonValue>>* Adjusters;
            if (Payload->TryGetArrayField(TEXT("classAdjusters"), Adjusters))
            {
                for (const auto& Val : *Adjusters)
                {
                    const TSharedPtr<FJsonObject> AdjObj = Val->AsObject();
                    FString ClassPath;
                    if (AdjObj->TryGetStringField(TEXT("soundClass"), ClassPath))
                    {
                        USoundClass* SC = LoadObject<USoundClass>(nullptr, *ClassPath);
                        if (SC)
                        {
                            FSoundClassAdjuster Adjuster;
                            Adjuster.SoundClassObject = SC;
                            double Vol = 1.0; AdjObj->TryGetNumberField(TEXT("volumeAdjuster"), Vol);
                            Adjuster.VolumeAdjuster = (float)Vol;
                            double Pitch = 1.0; AdjObj->TryGetNumberField(TEXT("pitchAdjuster"), Pitch);
                            Adjuster.PitchAdjuster = (float)Pitch;
                            SoundMix->SoundClassEffects.Add(Adjuster);
                        }
                    }
                }
            }

            UEditorAssetLibrary::SaveAsset(SoundMix->GetPathName());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SoundMix created"), nullptr);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create SoundMix"), TEXT("ASSET_CREATION_FAILED"));
        }
        return true;
    }
    else if (Lower == TEXT("push_sound_mix"))
    {
        FString MixName;
        if (!Payload->TryGetStringField(TEXT("mixName"), MixName) || MixName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("mixName required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        USoundMix* Mix = LoadObject<USoundMix>(nullptr, *MixName);
        if (Mix)
        {
            UGameplayStatics::PushSoundMixModifier(GEditor->GetEditorWorldContext().World(), Mix);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SoundMix pushed"), nullptr);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("SoundMix not found"), TEXT("ASSET_NOT_FOUND"));
        }
        return true;
    }
    else if (Lower == TEXT("pop_sound_mix"))
    {
        FString MixName;
        if (!Payload->TryGetStringField(TEXT("mixName"), MixName) || MixName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("mixName required"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        USoundMix* Mix = LoadObject<USoundMix>(nullptr, *MixName);
        if (Mix)
        {
            UGameplayStatics::PopSoundMixModifier(GEditor->GetEditorWorldContext().World(), Mix);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("SoundMix popped"), nullptr);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("SoundMix not found"), TEXT("ASSET_NOT_FOUND"));
        }
        return true;
    }

    // Fallback for other audio actions not fully implemented yet
    SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Audio action '%s' not fully implemented"), *Action), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Audio actions require editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
