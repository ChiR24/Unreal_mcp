// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

#if WITH_EDITOR
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#endif

// source_control_enable could name a provider but never create the repository it
// needs, so on a project that had never been put under revision control it just
// answered "Failed to enable source control. Please configure provider in Editor
// preferences." -- i.e. it handed the job back to the very UI the caller is
// automating away from. init/commit_all close that: a caller can take a fresh
// project all the way from no repository to a committed snapshot without ever
// leaving the tool.

namespace {
#if WITH_EDITOR

// Everything Unreal regenerates. Without this, `git add -A` walks ~10 GB of
// Intermediate/Saved/Binaries and looks like a hang.
const TCHAR *McpGitIgnoreBody =
    TEXT("Binaries/\nBuild/\nDerivedDataCache/\nIntermediate/\nSaved/\n")
    TEXT("Packaged/\nPluginBuild/\ntmp/\n")
    TEXT("*.sln\n*.suo\n*.sdf\n*.opensdf\n*.VC.db\n*.VC.opendb\n.vs/\n")
    TEXT("Plugins/**/Binaries/\nPlugins/**/Intermediate/\n");

/** Run git in ProjectDir. Returns false when the binary could not be launched. */
bool McpRunGit(const FString &Args, const FString &WorkingDir, int32 &OutCode,
               FString &OutStd, FString &OutErr) {
  OutCode = -1;
  return FPlatformProcess::ExecProcess(TEXT("git"), *Args, &OutCode, &OutStd,
                                       &OutErr, *WorkingDir);
}

struct FMcpGitStep {
  FString Args;
  int32 Code = 0;
  FString Std;
  FString Err;
};

/** Record one git invocation into the reply so a failure is diagnosable. */
void McpAppendStep(const TArray<FMcpGitStep> &Steps,
                   const TSharedPtr<FJsonObject> &Result) {
  TArray<TSharedPtr<FJsonValue>> Arr;
  for (const FMcpGitStep &Step : Steps) {
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("command"), FString::Printf(TEXT("git %s"), *Step.Args));
    Obj->SetNumberField(TEXT("exitCode"), Step.Code);
    // Trim: `git add` on a content-heavy project is chatty and the tail is what matters.
    Obj->SetStringField(TEXT("output"), Step.Std.Right(2000));
    if (!Step.Err.IsEmpty()) {
      Obj->SetStringField(TEXT("stderr"), Step.Err.Right(2000));
    }
    Arr.Add(MakeShared<FJsonValueObject>(Obj));
  }
  Result->SetArrayField(TEXT("steps"), Arr);
}
#endif
} // namespace

bool UMcpAutomationBridgeSubsystem::HandleSourceControlRepo(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  const bool bInit = Lower.Equals(TEXT("source_control_init"), ESearchCase::IgnoreCase);
  const bool bCommit = Lower.Equals(TEXT("source_control_commit_all"), ESearchCase::IgnoreCase);
  if (!bInit && !bCommit) {
    return false;
  }

#if WITH_EDITOR
  const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

  FString Description = bInit ? TEXT("Initial commit") : TEXT("Snapshot");
  FString UserName, UserEmail;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("description"), Description);
    Payload->TryGetStringField(TEXT("userName"), UserName);
    Payload->TryGetStringField(TEXT("userEmail"), UserEmail);
  }
  // Only write identity the caller actually named. Defaulting it here meant a
  // plain commit_all silently replaced the author that init had configured, so
  // every snapshot after the first was attributed to nobody.
  const bool bHasIdentity = !UserName.IsEmpty() || !UserEmail.IsEmpty();
  if (Description.IsEmpty()) {
    Description = bInit ? TEXT("Initial commit") : TEXT("Snapshot");
  }

  const bool bRepoExists = FPaths::DirectoryExists(FPaths::Combine(ProjectDir, TEXT(".git")));
  if (bCommit && !bRepoExists) {
    SendAutomationError(RequestingSocket, RequestId,
        TEXT("No repository at the project root; run source_control_init first."),
        TEXT("NO_REPOSITORY"));
    return true;
  }

  // git add over a full Content tree takes tens of seconds and ExecProcess
  // blocks its caller, so this cannot run on the game thread -- the editor would
  // stop pumping and the bridge socket would look dead. Do the work on a worker
  // and hop back to answer.
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  Async(EAsyncExecution::Thread,
        [WeakThis, RequestId, RequestingSocket, ProjectDir, Description, UserName,
         UserEmail, bHasIdentity, bInit, bRepoExists]() {
    TArray<FMcpGitStep> Steps;
    auto Run = [&Steps, &ProjectDir](const FString &Args) -> bool {
      FMcpGitStep Step;
      Step.Args = Args;
      const bool bLaunched =
          McpRunGit(Args, ProjectDir, Step.Code, Step.Std, Step.Err);
      Steps.Add(Step);
      return bLaunched && Step.Code == 0;
    };

    bool bOk = true;
    FString FailureCode;

    if (bInit && !bRepoExists) {
      if (!Run(TEXT("init"))) {
        bOk = false;
        FailureCode = TEXT("GIT_INIT_FAILED");
      }
    }

    if (bOk) {
      // Identity is per-repo so this never touches the machine's global config.
      if (!UserName.IsEmpty()) {
        Run(FString::Printf(TEXT("config user.name \"%s\""), *UserName));
      }
      if (!UserEmail.IsEmpty()) {
        Run(FString::Printf(TEXT("config user.email \"%s\""), *UserEmail));
      }
      // .uasset are binary; normalising line endings would corrupt them.
      Run(TEXT("config core.autocrlf false"));
    }

    const FString IgnorePath = FPaths::Combine(ProjectDir, TEXT(".gitignore"));
    if (bOk && !FPaths::FileExists(IgnorePath)) {
      FFileHelper::SaveStringToFile(FString(McpGitIgnoreBody), *IgnorePath);
    }

    if (bOk && !Run(TEXT("add -A"))) {
      bOk = false;
      FailureCode = TEXT("GIT_ADD_FAILED");
    }

    bool bNothingToCommit = false;
    if (bOk) {
      FMcpGitStep Commit;
      Commit.Args = FString::Printf(TEXT("commit -m \"%s\""), *Description);
      McpRunGit(Commit.Args, ProjectDir, Commit.Code, Commit.Std, Commit.Err);
      Steps.Add(Commit);
      // git exits 1 with "nothing to commit" when the tree is clean. That is a
      // successful no-op, not a failure -- a caller snapshotting on a timer
      // would otherwise see spurious errors.
      if (Commit.Code != 0) {
        bNothingToCommit = Commit.Std.Contains(TEXT("nothing to commit")) ||
                           Commit.Std.Contains(TEXT("working tree clean"));
        if (!bNothingToCommit) {
          bOk = false;
          FailureCode = TEXT("GIT_COMMIT_FAILED");
        }
      }
    }

    FString HeadHash;
    if (bOk) {
      FMcpGitStep Head;
      Head.Args = TEXT("rev-parse --short HEAD");
      McpRunGit(Head.Args, ProjectDir, Head.Code, Head.Std, Head.Err);
      HeadHash = Head.Std.TrimStartAndEnd();
    }

    AsyncTask(ENamedThreads::GameThread,
              [WeakThis, RequestId, RequestingSocket, Steps, bOk, FailureCode,
               HeadHash, bNothingToCommit, bHasIdentity, bInit, ProjectDir]() {
      UMcpAutomationBridgeSubsystem *Self = WeakThis.Get();
      if (!Self) {
        return;
      }
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("repositoryRoot"), ProjectDir);
      Result->SetBoolField(TEXT("committed"), bOk && !bNothingToCommit);
      Result->SetBoolField(TEXT("alreadyClean"), bNothingToCommit);
      Result->SetBoolField(TEXT("identityWritten"), bHasIdentity);
      if (!HeadHash.IsEmpty()) {
        Result->SetStringField(TEXT("commit"), HeadHash);
      }
      McpAppendStep(Steps, Result);

      if (!bOk) {
        Self->SendAutomationResponse(RequestingSocket, RequestId, false,
            TEXT("Repository operation failed; see steps[] for the failing git command."),
            Result, FailureCode);
        return;
      }

      if (bInit) {
        // Point the editor at the repository now that one exists, so the
        // caller does not have to follow up with source_control_enable.
        ISourceControlModule::Get().SetProvider(FName(TEXT("Git")));
        Result->SetStringField(TEXT("provider"),
            ISourceControlModule::Get().GetProvider().GetName().ToString());
        Result->SetBoolField(TEXT("sourceControlEnabled"),
            ISourceControlModule::Get().IsEnabled());
      }

      Self->SendAutomationResponse(RequestingSocket, RequestId, true,
          bNothingToCommit ? TEXT("Repository already up to date")
                           : TEXT("Repository snapshot committed"),
          Result, FString());
    });
  });

  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("source control repo actions require an editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
