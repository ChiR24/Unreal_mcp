#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Exporters/Exporter.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersOutputCapture.h"
#include "Misc/FileHelper.h"
#include "UObject/MetaData.h"

namespace McpEnvironmentHandlers {

namespace {
UObject *McpResolveQueryTarget(const TSharedPtr<FJsonObject> &Payload, FString &OutRequested)
{
    OutRequested = McpGetFirstStringField(Payload, {TEXT("actorName"), TEXT("objectPath"), TEXT("name"),
                                                    TEXT("assetPath"), TEXT("path")});
    return OutRequested.IsEmpty() ? nullptr : McpHandlerUtils::ResolveObjectFromPath(OutRequested);
}

bool McpComputeObjectBounds(UObject *Object, FBox &OutBox, FString &OutSource)
{
    if (AActor *Actor = Cast<AActor>(Object))
    {
        FVector Origin, Extent;
        Actor->GetActorBounds(false, Origin, Extent, true);
        OutBox = FBox::BuildAABB(Origin, Extent);
        OutSource = TEXT("actor");
        return true;
    }
    if (USceneComponent *Scene = Cast<USceneComponent>(Object))
    {
        OutBox = Scene->Bounds.GetBox();
        OutSource = TEXT("component");
        return true;
    }
    if (UStaticMesh *Mesh = Cast<UStaticMesh>(Object))
    {
        OutBox = Mesh->GetBounds().GetBox();
        OutSource = TEXT("staticMesh");
        return true;
    }
    if (USkeletalMesh *Skeletal = Cast<USkeletalMesh>(Object))
    {
        OutBox = Skeletal->GetBounds().GetBox();
        OutSource = TEXT("skeletalMesh");
        return true;
    }
    return false;
}

// Package metadata (UMetaData before 5.6, FMetaData from 5.6) plus, for assets,
// the asset-registry tags the object publishes.
void McpAppendObjectMetadata(UObject *Object, TSharedPtr<FJsonObject> Resp)
{
    TSharedPtr<FJsonObject> Metadata = McpHandlerUtils::CreateResultObject();
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
    const TMap<FName, FString> *MetadataMap = FMetaData::GetMapForObject(Object);
#else
    const TMap<FName, FString> *MetadataMap = UMetaData::GetMapForObject(Object);
#endif
    if (MetadataMap)
    {
        for (const TPair<FName, FString> &Pair : *MetadataMap)
        {
            Metadata->SetStringField(Pair.Key.ToString(), Pair.Value);
        }
    }
    Resp->SetObjectField(TEXT("metadata"), Metadata);
    Resp->SetNumberField(TEXT("metadataCount"), Metadata->Values.Num());
    TSharedPtr<FJsonObject> AssetTags = McpHandlerUtils::CreateResultObject();
    if (Object->IsAsset())
    {
        const FAssetData AssetData(Object);
        const FAssetDataTagMap Tags = AssetData.TagsAndValues.CopyMap();
        for (const TPair<FName, FString> &Pair : Tags)
        {
            AssetTags->SetStringField(Pair.Key.ToString(), Pair.Value);
        }
    }
    Resp->SetObjectField(TEXT("assetRegistryTags"), AssetTags);
}
} // namespace

// get_bounding_box / get_metadata / export for a world actor, a component
// ("Actor.Component"), or an asset path. Answered here so the inspect surface
// returns structured fields instead of the control_actor message-only payloads.
bool HandleInspectActorQueryAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &LowerSubAction, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    auto Fail = [&](const FString &Message, const TCHAR *Code) {
        Bridge.SendAutomationError(RequestingSocket, RequestId, Message, Code);
        return true;
    };
    FString Requested;
    UObject *Target = McpResolveQueryTarget(Payload, Requested);
    if (Requested.IsEmpty())
    {
        return Fail(FString::Printf(TEXT("actorName, objectPath, or name required for %s"), *LowerSubAction),
                    TEXT("INVALID_ARGUMENT"));
    }
    if (!Target)
    {
        return Fail(FString::Printf(TEXT("Object not found: %s"), *Requested), TEXT("OBJECT_NOT_FOUND"));
    }
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("objectPath"), Target->GetPathName());
    Resp->SetStringField(TEXT("objectName"), Target->GetName());
    Resp->SetStringField(TEXT("className"), Target->GetClass()->GetName());
    AActor *Actor = Cast<AActor>(Target);
    if (Actor)
    {
        Resp->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
        Resp->SetStringField(TEXT("actorLabel"), Actor->GetActorLabel());
    }
    FString Message;
    if (LowerSubAction.Equals(TEXT("get_bounding_box")))
    {
        FBox Box(ForceInit);
        FString Source;
        if (!McpComputeObjectBounds(Target, Box, Source))
        {
            return Fail(FString::Printf(TEXT("Bounds are not available for %s (%s)"), *Requested, *Target->GetClass()->GetName()),
                        TEXT("UNSUPPORTED_OBJECT"));
        }
        Resp->SetObjectField(TEXT("bounds"), McpMakeBoundsObject(Box));
        Resp->SetObjectField(TEXT("origin"), McpMakeVectorObject(Box.GetCenter()));
        Resp->SetObjectField(TEXT("extent"), McpMakeVectorObject(Box.GetExtent()));
        Resp->SetStringField(TEXT("boundsSource"), Source);
        Message = TEXT("Bounding box retrieved");
    }
    else if (LowerSubAction.Equals(TEXT("get_metadata")))
    {
        McpAppendObjectMetadata(Target, Resp);
        if (Actor)
        {
            McpAddActorTags(Resp, Actor);
            Resp->SetStringField(TEXT("folderPath"), Actor->GetFolderPath().ToString());
            Resp->SetObjectField(TEXT("location"), McpMakeVectorObject(Actor->GetActorLocation()));
        }
        Message = TEXT("Metadata retrieved");
    }
    else
    {
        FString FileType = McpGetFirstStringField(Payload, {TEXT("format"), TEXT("fileType"), TEXT("exportFormat")});
        if (FileType.IsEmpty())
        {
            FileType = TEXT("T3D");
        }
        FMcpOutputCapture Capture;
        const bool bExported = UExporter::ExportToOutputDevice(nullptr, Target, nullptr, Capture, *FileType, 0, 0, false, nullptr);
        const FString Text = FString::Join(Capture.Consume(), TEXT("\n"));
        if (!bExported && Text.IsEmpty())
        {
            return Fail(FString::Printf(TEXT("No %s exporter produced output for %s"), *FileType, *Target->GetPathName()),
                        TEXT("EXPORT_FAILED"));
        }
        constexpr int32 MaxInlineChars = 200000;
        Resp->SetStringField(TEXT("format"), FileType);
        Resp->SetNumberField(TEXT("exportedLength"), Text.Len());
        Resp->SetBoolField(TEXT("exportedTextTruncated"), Text.Len() > MaxInlineChars);
        Resp->SetStringField(TEXT("exportedText"), Text.Len() > MaxInlineChars ? Text.Left(MaxInlineChars) : Text);
        const FString OutputPath = McpGetFirstStringField(Payload, {TEXT("outputPath"), TEXT("filePath"), TEXT("exportPath")});
        if (!OutputPath.IsEmpty())
        {
            FString AbsolutePath, SafePath, PathError;
            if (!McpResolveProjectFilePath(OutputPath, AbsolutePath, SafePath, PathError))
            {
                return Fail(PathError, TEXT("INVALID_PATH"));
            }
            if (!FFileHelper::SaveStringToFile(Text, *AbsolutePath))
            {
                return Fail(FString::Printf(TEXT("Failed to write export to %s"), *AbsolutePath), TEXT("EXPORT_WRITE_FAILED"));
            }
            Resp->SetStringField(TEXT("filePath"), AbsolutePath);
            Resp->SetStringField(TEXT("relativePath"), SafePath);
            Resp->SetNumberField(TEXT("charactersWritten"), Text.Len());
        }
        Message = TEXT("Object exported");
    }
    Resp->SetBoolField(TEXT("success"), true);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true, Message, Resp, FString());
    return true;
}

} // namespace McpEnvironmentHandlers
#endif
