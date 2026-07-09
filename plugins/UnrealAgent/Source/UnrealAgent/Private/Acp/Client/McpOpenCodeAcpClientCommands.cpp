#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "Acp/Client/McpOpenCodeAcpClientPrivate.h"

#include "Acp/Context/UnrealAgentEditorContext.h"
#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Acp/Validation/UnrealAgentValidationRunner.h"

#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

using namespace UnrealAgent::OpenCodeAcp;

namespace
{
    FString MakeFileUri(const FString& AbsolutePath)
    {
        FString Normalized = FPaths::ConvertRelativePathToFull(AbsolutePath);
        FPaths::NormalizeFilename(Normalized);
        // RFC 8089 requires forward slashes in `file://` URIs. On Windows,
        // NormalizeFilename emits backslashes, which the receiving end will
        // misinterpret. Convert unconditionally so the wire format is portable.
        Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
        // Percent-encode the characters that are reserved in a `file://` URI component
        // (`#`, `?`, `%`, space) so paths containing them round-trip cleanly to the
        // ACP `resource_link` block. `%` must be replaced first.
        Normalized.ReplaceInline(TEXT("%"), TEXT("%25"));
        Normalized.ReplaceInline(TEXT("#"), TEXT("%23"));
        Normalized.ReplaceInline(TEXT("?"), TEXT("%3F"));
        Normalized.ReplaceInline(TEXT(" "), TEXT("%20"));
        // On Windows, an absolute path is `C:/Users/...`; RFC 8089 requires a
        // leading extra slash so the URI is `file:///C:/Users/...`.
        if (Normalized.Len() >= 2 && FChar::IsAlpha(Normalized[0]) && Normalized[1] == TEXT(':'))
        {
            return FString::Printf(TEXT("file:///%s"), *Normalized);
        }
        return FString::Printf(TEXT("file://%s"), *Normalized);
    }

    TSharedPtr<FJsonObject> MakePromptTextBlock(const FString& Text)
    {
        auto TextBlock = MakeObject();
        TextBlock->SetStringField(TEXT("type"), TEXT("text"));
        TextBlock->SetStringField(TEXT("text"), Text);
        return TextBlock;
    }

    TSharedPtr<FJsonObject> MakePromptResourceLinkBlock(const FString& AttachmentPath)
    {
        const FString AbsolutePath = FPaths::ConvertRelativePathToFull(AttachmentPath);
        auto ResourceBlock = MakeObject();
        ResourceBlock->SetStringField(TEXT("type"), TEXT("resource_link"));
        ResourceBlock->SetStringField(TEXT("uri"), MakeFileUri(AbsolutePath));
        ResourceBlock->SetStringField(TEXT("name"), FPaths::GetCleanFilename(AbsolutePath));
        ResourceBlock->SetStringField(TEXT("mimeType"), TEXT("text/plain"));
        return ResourceBlock;
    }

    // Defined below; forward-declared so it can be used by the prompt/transcript
    // builders above its definition.
    FString ResolveActorLabelFromPath(const FString& ActorPath);

    TSharedPtr<FJsonObject> MakePromptActorReferenceBlock(const FString& ActorPath)
    {
        const FString ActorLabel = ResolveActorLabelFromPath(ActorPath);
        auto TextBlock = MakeObject();
        TextBlock->SetStringField(TEXT("type"), TEXT("text"));
        TextBlock->SetStringField(TEXT("text"),
            FString::Printf(TEXT("Selected actor reference: %s"), *ActorLabel));
        return TextBlock;
    }

    TSharedPtr<FJsonObject> MakePromptAssetReferenceBlock(const FString& PackageName)
    {
        auto TextBlock = MakeObject();
        TextBlock->SetStringField(TEXT("type"), TEXT("text"));
        TextBlock->SetStringField(TEXT("text"),
            FString::Printf(TEXT("Selected asset reference: %s"), *PackageName));
        return TextBlock;
    }

    // Resolves an actor object path back to its display label for the prompt and
    // transcript. Falls back to the path itself when the actor is gone.
    FString ResolveActorLabelFromPath(const FString& ActorPath)
    {
        if (GEditor == nullptr)
        {
            return ActorPath;
        }
        UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
        if (World == nullptr)
        {
            return ActorPath;
        }
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetPathName().Equals(ActorPath, ESearchCase::IgnoreCase))
            {
                return It->GetActorNameOrLabel();
            }
        }
        return ActorPath;
    }

    FString FormatPromptTranscriptWithAttachments(const FString& PromptText, const TArray<FString>& AttachmentPaths)
    {
        if (AttachmentPaths.Num() == 0)
        {
            return PromptText;
        }

        FString TranscriptText = PromptText.TrimStartAndEnd().IsEmpty()
            ? FString(TEXT("Attached items"))
            : PromptText;
        FString FileList;
        FString ActorList;
        for (const FString& AttachmentPath : AttachmentPaths)
        {
            if (AttachmentPath.StartsWith(TEXT("actor:")))
            {
                ActorList += FString::Printf(TEXT("\n- %s"), *ResolveActorLabelFromPath(AttachmentPath.Mid(6)));
            }
            else
            {
                FileList += FString::Printf(TEXT("\n- %s"), *FPaths::GetCleanFilename(AttachmentPath));
            }
        }
        if (!FileList.IsEmpty())
        {
            TranscriptText += TEXT("\n\nAttached files:");
            TranscriptText += FileList;
        }
        if (!ActorList.IsEmpty())
        {
            TranscriptText += TEXT("\n\nAttached actors:");
            TranscriptText += ActorList;
        }
        return TranscriptText;
    }
}

bool FOpenCodeAcpClient::SendPrompt(const FString& PromptText)
{
    static const TArray<FString> NoAttachments;
    return SendPrompt(PromptText, NoAttachments);
}

bool FOpenCodeAcpClient::SendPrompt(const FString& PromptText, const TArray<FString>& AttachmentPaths)
{
    if (!bReady || bPromptInFlight || (PromptText.TrimStartAndEnd().IsEmpty() && AttachmentPaths.Num() == 0))
    {
        return false;
    }

    TranscriptRedactionStateByRole.Reset();
    FString PromptForAcp =
        FUnrealAgentStudioKit::RedactPromptSensitiveText(PromptText);
    if (bAttachEditorContext)
    {
        const FString ContextEnvelope = RefreshEditorContext();
        if (!ContextEnvelope.TrimStartAndEnd().IsEmpty())
        {
            PromptForAcp += TEXT("\n\n");
            PromptForAcp += ContextEnvelope;
        }
    }

    TArray<TSharedPtr<FJsonValue>> Prompt;
    if (!PromptForAcp.TrimStartAndEnd().IsEmpty())
    {
        Prompt.Add(MakeShared<FJsonValueObject>(MakePromptTextBlock(PromptForAcp)));
    }
    for (const FString& AttachmentPath : AttachmentPaths)
    {
        if (AttachmentPath.TrimStartAndEnd().IsEmpty())
        {
            continue;
        }
        if (AttachmentPath.StartsWith(TEXT("actor:")))
        {
            Prompt.Add(MakeShared<FJsonValueObject>(
                MakePromptActorReferenceBlock(AttachmentPath.Mid(6))));
        }
        else if (FPackageName::IsValidLongPackageName(AttachmentPath))
        {
            // Editor-scoped asset PackageName (e.g. /Game/Characters/Hero). Emitted
            // as a reference handle, not a file:// URI — the TS server has no
            // per-asset resource endpoint, and the asset is resolved in-editor.
            // Classified by package-name validity (not a leading slash) so absolute
            // filesystem paths are not mistaken for assets and correctly become
            // file:// resource links below.
            Prompt.Add(MakeShared<FJsonValueObject>(
                MakePromptAssetReferenceBlock(AttachmentPath)));
        }
        else
        {
            Prompt.Add(MakeShared<FJsonValueObject>(MakePromptResourceLinkBlock(AttachmentPath)));
        }
    }

    auto Params = MakeObject();
    Params->SetStringField(TEXT("sessionId"), SessionId);
    Params->SetArrayField(TEXT("prompt"), Prompt);

    const int32 RequestId = SendRequest(TEXT("session/prompt"), Params);
    if (RequestId == INDEX_NONE)
    {
        StopWithError(TEXT("Failed to send OpenCode ACP prompt request."));
        return false;
    }

    AppendTranscript(TEXT("You"), FormatPromptTranscriptWithAttachments(PromptText, AttachmentPaths));
    ActivePromptRequestId = RequestId;
    bPromptInFlight = true;
    bCancelRequested = false;
    SetStatus(TEXT("OpenCode is working..."));
    return true;
}

FString FOpenCodeAcpClient::RefreshEditorContext()
{
    const FString ContextProjectDirectory = WorkingDirectory.IsEmpty() ? FPaths::ProjectDir() : WorkingDirectory;
    FUnrealAgentEditorContextOptions ContextOptions;
    ContextOptions.bUnrealMcpConfiguredForSession = bUnrealMcpConfiguredForSession;
    const FUnrealAgentEditorContextSnapshot Snapshot = FUnrealAgentEditorContext::Capture(ContextProjectDirectory, ContextOptions);
    LastEditorContextSummary = Snapshot.Summary;
    LastEditorContextEnvelope = Snapshot.Envelope;
    return LastEditorContextEnvelope;
}

bool FOpenCodeAcpClient::RunProjectValidation()
{
    const FString ValidationProjectDirectory = WorkingDirectory.IsEmpty() ? FPaths::ProjectDir() : WorkingDirectory;
    const FUnrealAgentValidationResult Result = FUnrealAgentValidationRunner::RunFastValidation(ValidationProjectDirectory);
    LastValidationSummary = FUnrealAgentValidationRunner::FormatForTranscript(Result);
    AppendTranscript(Result.bPassed ? TEXT("Tool") : TEXT("Error"), LastValidationSummary);
    return Result.bPassed;
}

void FOpenCodeAcpClient::CancelPrompt()
{
    if (!bRunning || !bPromptInFlight || bCancelRequested || SessionId.IsEmpty())
    {
        return;
    }

    if (!SendCancelledPermissionResponse())
    {
        StopWithError(TEXT("Failed to cancel pending OpenCode ACP permission request."));
        return;
    }

    auto Params = MakeObject();
    Params->SetStringField(TEXT("sessionId"), SessionId);
    if (!SendNotification(TEXT("session/cancel"), Params))
    {
        StopWithError(TEXT("Failed to send OpenCode ACP cancellation request."));
        return;
    }

    bCancelRequested = true;
    SetStatus(TEXT("Cancelling OpenCode turn..."));
    AppendTranscript(TEXT("System"), TEXT("Cancel requested. Waiting for OpenCode ACP to finish the turn."));
}

void FOpenCodeAcpClient::SetModel(const FString& ModelId)
{
    if (!CanSelectModel() || ModelId.IsEmpty() || ModelId == CurrentModel)
    {
        return;
    }

    const bool bKnownModel = ModelOptions.ContainsByPredicate([&ModelId](const FOpenCodeAcpModelOption& Option)
    {
        return Option.Id == ModelId;
    });

    if (!bKnownModel)
    {
        return;
    }

    auto Params = MakeObject();
    Params->SetStringField(TEXT("sessionId"), SessionId);
    Params->SetStringField(TEXT("configId"), ModelConfigId);
    Params->SetStringField(TEXT("value"), ModelId);

    const int32 RequestId = SendRequest(TEXT("session/set_config_option"), Params);
    if (RequestId == INDEX_NONE)
    {
        StopWithError(TEXT("Failed to send OpenCode ACP model switch request."));
        return;
    }

    PendingModel = ModelId;
    SetModelRequestId = RequestId;
    SetModelRequestStartedAt = FPlatformTime::Seconds();
    SetStatus(FString::Printf(TEXT("Switching model to %s..."), *GetModelDisplayName(ModelId)));
}

void FOpenCodeAcpClient::SetThinking(const FString& ThinkingId)
{
    if (!CanSelectThinking() || ThinkingId.IsEmpty() || ThinkingId == CurrentThinking)
    {
        return;
    }

    const bool bKnownThinking = ThinkingOptions.ContainsByPredicate([&ThinkingId](const FOpenCodeAcpThinkingOption& Option)
    {
        return Option.Id == ThinkingId;
    });

    if (!bKnownThinking)
    {
        return;
    }

    auto Params = MakeObject();
    Params->SetStringField(TEXT("sessionId"), SessionId);
    Params->SetStringField(TEXT("configId"), ThinkingConfigId);
    Params->SetStringField(TEXT("value"), ThinkingId);

    const int32 RequestId = SendRequest(TEXT("session/set_config_option"), Params);
    if (RequestId == INDEX_NONE)
    {
        StopWithError(TEXT("Failed to send OpenCode ACP thinking switch request."));
        return;
    }

    PendingThinking = ThinkingId;
    SetThinkingRequestId = RequestId;
    SetThinkingRequestStartedAt = FPlatformTime::Seconds();
    SetStatus(FString::Printf(TEXT("Switching thinking to %s..."), *GetThinkingDisplayName(ThinkingId)));
}

void FOpenCodeAcpClient::SetAgent(const FString& AgentId)
{
    if (!CanSelectAgent() || AgentId.IsEmpty() || AgentId == CurrentAgent)
    {
        return;
    }

    const bool bKnownAgent = AgentOptions.ContainsByPredicate([&AgentId](const FOpenCodeAcpAgentOption& Option)
    {
        return Option.Id == AgentId;
    });

    if (!bKnownAgent)
    {
        return;
    }

    auto Params = MakeObject();
    Params->SetStringField(TEXT("sessionId"), SessionId);
    Params->SetStringField(TEXT("configId"), AgentConfigId);
    Params->SetStringField(TEXT("value"), AgentId);

    const int32 RequestId = SendRequest(TEXT("session/set_config_option"), Params);
    if (RequestId == INDEX_NONE)
    {
        StopWithError(TEXT("Failed to send OpenCode ACP agent switch request."));
        return;
    }

    PendingAgent = AgentId;
    SetAgentRequestId = RequestId;
    SetAgentRequestStartedAt = FPlatformTime::Seconds();
    SetStatus(FString::Printf(TEXT("Switching agent to %s..."), *GetAgentDisplayName(AgentId)));
}

void FOpenCodeAcpClient::ApprovePermissionOnce()
{
    ResolvePendingPermission(FindPendingPermissionOption({ TEXT("once"), TEXT("allow-once"), TEXT("allow") }, { TEXT("allow_once"), TEXT("allow") }));
}

void FOpenCodeAcpClient::ApprovePermissionAlways()
{
    ResolvePendingPermission(FindPendingPermissionOption({ TEXT("always"), TEXT("allow-always"), TEXT("allow_always") }, { TEXT("allow_always") }));
}

void FOpenCodeAcpClient::RejectPermission()
{
    ResolvePendingPermission(FindPendingPermissionOption({ TEXT("reject"), TEXT("reject-once"), TEXT("reject_once"), TEXT("reject-always"), TEXT("reject_always"), TEXT("deny") }, { TEXT("reject_once"), TEXT("reject_always"), TEXT("deny") }));
}

bool FOpenCodeAcpClient::CanApprovePermissionAlways() const
{
    return !FindPendingPermissionOption({ TEXT("always"), TEXT("allow-always"), TEXT("allow_always") }, { TEXT("allow_always") }).IsEmpty();
}
