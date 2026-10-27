// Copyright (c) 2024 EngineWire Contributors
//
// One assertion judged against the editor. not, and, or combine verdicts three-valued (an unknown part leaves the
// whole unknown unless the rest decides it). Relations go to McpRelations::JudgePair, comparisons read the actor's
// transform or a reflected property (the resolver get_property uses), and visible runs explain_visible for its cause.

#include "Domains/Debug/Contracts/McpContracts.h"

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Domains/ControlActor/Relations/McpRelations.h"
#include "Domains/Debug/Explain/McpExplain.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"
#include "Foundation/BridgeHelpers/Properties/McpNestedPropPath.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Solvers/McpPratt.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"

namespace McpContracts
{
namespace
{
using McpPratt::FNode;

FVerdict Decide(bool bOk, const FString& Actual, const TSharedPtr<FJsonObject>& NextCall = nullptr)
{
    FVerdict Out;
    Out.Verdict = bOk ? TEXT("ok") : TEXT("failed");
    Out.Actual = Actual;
    Out.NextCall = bOk ? nullptr : NextCall;
    return Out;
}

FVerdict Unknown(const FString& Why)
{
    FVerdict Out;
    Out.Verdict = TEXT("unknown");
    Out.Actual = Why;
    return Out;
}

FString Text(const FNode& Leaf)
{
    return Leaf.bNumber ? FString::Printf(TEXT("%g"), Leaf.Number) : FString::Join(Leaf.Path, TEXT("."));
}

// The actor the first segment of a leaf names; OutMissing says so when there is none.
AActor* NamedActor(const FNode& Leaf, FVerdict& OutMissing)
{
    AActor* Actor = Leaf.Path.Num() > 0 ? McpHandlerUtils::FindActorByName(Leaf.Path[0]) : nullptr;
    if (!Actor)
    {
        OutMissing = Decide(false, FString::Printf(TEXT("no actor is named '%s'"), Leaf.Path.Num() > 0 ? *Leaf.Path[0] : TEXT("")));
    }
    return Actor;
}

// A.location.z, A.rotation.yaw, A.scale.x: the actor's transform; anything else a reflected property under the actor
// (A.LightComponent.Intensity). OutText is what was read, OutNumber too when it is a number.
bool ReadValue(AActor* Actor, const TArray<FString>& Path, FString& OutText, double& OutNumber, bool& bOutNumber, FString& OutError)
{
    const FString Head = Path.Num() > 1 ? Path[1].ToLower() : FString();
    if ((Head == TEXT("location") || Head == TEXT("rotation") || Head == TEXT("scale")) && Path.Num() == 3)
    {
        const FRotator Rotation = Actor->GetActorRotation();
        const FVector Vector = Head == TEXT("location") ? Actor->GetActorLocation()
            : Head == TEXT("scale") ? Actor->GetActorScale3D() : FVector(Rotation.Roll, Rotation.Pitch, Rotation.Yaw);
        static const TCHAR* const Axes[][3] = {{TEXT("x"), TEXT("y"), TEXT("z")}, {TEXT("roll"), TEXT("pitch"), TEXT("yaw")}};
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            if (Path[2].Equals(Axes[Head == TEXT("rotation") ? 1 : 0][Axis], ESearchCase::IgnoreCase))
            {
                OutNumber = Vector[Axis];
                bOutNumber = true;
                OutText = FString::Printf(TEXT("%g"), OutNumber);
                return true;
            }
        }
        OutError = Head == TEXT("rotation") ? TEXT("rotation takes pitch, yaw or roll") : TEXT("name an axis: x, y or z");
        return false;
    }
    TArray<FString> Rest(Path.GetData() + 1, Path.Num() - 1);
    void* Container = nullptr;
    FString Resolved;
    FProperty* Property = Rest.Num() > 0 ? McpResolvePropertyPath(Actor, FString::Join(Rest, TEXT(".")), Container, Resolved, OutError) : nullptr;
    if (!Property || !Container)
    {
        OutError = Rest.Num() > 0 ? OutError : TEXT("name what to read: location.z, rotation.yaw, scale.x or a property path");
        return false;
    }
    const void* Value = Property->ContainerPtrToValuePtr<void>(Container);
    const FNumericProperty* Numeric = CastField<FNumericProperty>(Property);
    if (Numeric && !Numeric->IsEnum())
    {
        OutNumber = Numeric->IsFloatingPoint() ? Numeric->GetFloatingPointPropertyValue(Value) : double(Numeric->GetSignedIntPropertyValue(Value));
        bOutNumber = true;
    }
    else if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property))
    {
        OutNumber = Bool->GetPropertyValue(Value) ? 1.0 : 0.0;
        bOutNumber = true;
    }
    MCP_PROPERTY_EXPORT_TEXT(Property, OutText, Value, nullptr, nullptr, PPF_None);
    return true;
}

FVerdict JudgeComparison(const FNode& Node)
{
    FVerdict Missing;
    const FNode& Subject = *Node.Args[0];
    AActor* Actor = NamedActor(Subject, Missing);
    if (!Actor)
    {
        return Missing;
    }
    FString Read, Error;
    double Number = 0.0;
    bool bNumber = false;
    if (!ReadValue(Actor, Subject.Path, Read, Number, bNumber, Error))
    {
        return Unknown(FString::Printf(TEXT("'%s' cannot be read: %s"), *Text(Subject), *Error));
    }
    const FString Actual = FString::Printf(TEXT("%s is %s"), *Text(Subject), *Read);
    if (Node.Op == TEXT("in"))
    {
        return bNumber ? Decide(Number >= Node.Args[1]->Number && Number <= Node.Args[2]->Number, Actual)
                       : Unknown(Actual + TEXT(", not a number"));
    }
    const FNode& Want = *Node.Args[1];
    // A bool reads True or False; true, false, 1 and 0 all compare with it.
    const bool bWantBool = !Want.bNumber && (Text(Want).Equals(TEXT("true"), ESearchCase::IgnoreCase) || Text(Want).Equals(TEXT("false"), ESearchCase::IgnoreCase));
    const double Target = Want.bNumber ? Want.Number : bWantBool ? (Text(Want).Equals(TEXT("true"), ESearchCase::IgnoreCase) ? 1.0 : 0.0) : 0.0;
    if (bNumber && (Want.bNumber || bWantBool))
    {
        const bool bOk = Node.Op == TEXT("==") ? FMath::IsNearlyEqual(Number, Target, 1e-4) : Node.Op == TEXT("!=") ? !FMath::IsNearlyEqual(Number, Target, 1e-4)
            : Node.Op == TEXT("<") ? Number < Target : Node.Op == TEXT("<=") ? Number <= Target : Node.Op == TEXT(">") ? Number > Target : Number >= Target;
        return Decide(bOk, Actual);
    }
    if (Node.Op == TEXT("==") || Node.Op == TEXT("!="))
    {
        return Decide(Read.Equals(Text(Want), ESearchCase::IgnoreCase) == (Node.Op == TEXT("==")), Actual);
    }
    return Unknown(Actual + TEXT(": only numbers compare with < <= > >="));
}

// Whether an actor is drawn, from the explain tree inspect explain_visible runs, with its fix. Visible means drawn:
// the checks that depend on where the camera stands (view, drawDistance, nearClip, inView, sightLines, ownerSee) do
// not decide it.
FVerdict Drawn(AActor* Actor)
{
    UEngineWireSubsystem* Bridge = GEditor ? GEditor->GetEditorSubsystem<UEngineWireSubsystem>() : nullptr;
    if (!Bridge)
    {
        return Unknown(TEXT("the plugin's subsystem is not running"));
    }
    const FString Id = FGuid::NewGuid().ToString();
    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("actorName"), Actor->GetName());
    FMcpResponseCaptureRegistry::Get().Begin(Id);
    McpExplain::Handle(*Bridge, Id, TEXT("explain_visible"), Payload, nullptr);
    const FMcpCapturedResponse Reply = FMcpResponseCaptureRegistry::Get().End(Id);
    static const TSet<FString> CameraChecks = {TEXT("view"), TEXT("drawDistance"), TEXT("nearClip"), TEXT("inView"),
                                               TEXT("sightLines"), TEXT("ownerSee")};
    const TArray<TSharedPtr<FJsonValue>>* Causes = nullptr;
    if (Reply.Result.IsValid() && Reply.Result->TryGetArrayField(TEXT("causes"), Causes))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Causes)
        {
            const TSharedPtr<FJsonObject> Cause = Value->AsObject();
            FString Check, Detail;
            if (Cause->TryGetStringField(TEXT("check"), Check) && !CameraChecks.Contains(Check) && Cause->TryGetStringField(TEXT("detail"), Detail))
            {
                const TSharedPtr<FJsonObject>* Fix = nullptr;
                Cause->TryGetObjectField(TEXT("nextCall"), Fix);
                return Decide(false, Detail, Fix ? *Fix : nullptr);
            }
        }
    }
    return Reply.Result.IsValid() ? Decide(true, TEXT("it is drawn (whether the camera sees it is not judged)")) : Unknown(Reply.Message);
}

FVerdict JudgePredicate(const FNode& Node)
{
    const FNode& Subject = *Node.Args[0];
    if (Node.Op == TEXT("compiles"))
    {
        FString Normalized, Error;
        UBlueprint* Blueprint = LoadBlueprintAsset(Text(Subject), Normalized, Error);
        if (!Blueprint)
        {
            return Decide(false, FString::Printf(TEXT("no Blueprint at '%s': %s"), *Text(Subject), *Error));
        }
        TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
        Params->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        if (Blueprint->Status == BS_Dirty || Blueprint->Status == BS_Unknown || Blueprint->bBeingCompiled)
        {
            FVerdict Out = Unknown(TEXT("it changed since its last compile: compile it, then judge again"));
            Out.NextCall = McpExplain::MakeNextCall(TEXT("manage_blueprint"), TEXT("compile"), Params);
            return Out;
        }
        return Decide(Blueprint->Status != BS_Error, Blueprint->Status == BS_Error ? TEXT("its last compile failed")
                      : Blueprint->Status == BS_UpToDateWithWarnings ? TEXT("compiled, with warnings") : TEXT("compiled"),
                      McpExplain::MakeNextCall(TEXT("manage_blueprint"), TEXT("compile"), Params));
    }
    FVerdict Missing;
    AActor* Actor = McpHandlerUtils::FindActorByName(Subject.Path.Num() > 0 ? Subject.Path[0] : FString());
    if (Node.Op == TEXT("exists"))
    {
        const FString Path = Text(Subject);
        const bool bAsset = !Actor && Path.StartsWith(TEXT("/")) && FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Path));
        return Decide(Actor || bAsset, Actor ? FString::Printf(TEXT("'%s' is an actor in the level"), *Path)
                      : bAsset ? FString::Printf(TEXT("'%s' is an asset"), *Path) : FString::Printf(TEXT("nothing is named '%s'"), *Path));
    }
    if (!NamedActor(Subject, Missing))
    {
        return Missing;
    }
    const bool bHidden = Actor->IsHidden() || Actor->IsHiddenEd();
    if (Node.Op == TEXT("hidden"))
    {
        return Decide(bHidden, bHidden ? TEXT("it is hidden") : TEXT("it is not hidden"));
    }
    return Drawn(Actor); // visible
}
} // namespace

FVerdict Judge(const TSharedPtr<McpPratt::FNode>& Node, UWorld* World, const TArray<FString>* Warnings)
{
    if (!Node.IsValid())
    {
        return Unknown(TEXT("nothing to judge"));
    }
    const FString& Op = Node->Op;
    if (Op == TEXT("not"))
    {
        FVerdict Inner = Judge(Node->Args[0], World, Warnings);
        Inner.Verdict = Inner.Verdict == TEXT("ok") ? TEXT("failed") : Inner.Verdict == TEXT("failed") ? TEXT("ok") : Inner.Verdict;
        Inner.NextCall = nullptr; // a fix for the inner assertion would undo what not asks for
        return Inner;
    }
    if (Op == TEXT("and") || Op == TEXT("or"))
    {
        // Three-valued: a side that decides it (failed for and, ok for or) wins; else an unknown side leaves it unknown.
        const FString Decisive = Op == TEXT("and") ? TEXT("failed") : TEXT("ok");
        const FVerdict Left = Judge(Node->Args[0], World, Warnings);
        if (Left.Verdict == Decisive)
        {
            return Left;
        }
        const FVerdict Right = Judge(Node->Args[1], World, Warnings);
        if (Right.Verdict == Decisive)
        {
            return Right;
        }
        return Left.Verdict == TEXT("unknown") ? Left : Right;
    }
    if (Op == TEXT("noWarnings"))
    {
        return !Warnings ? Unknown(TEXT("there is no reply to read yet"))
            : Decide(Warnings->Num() == 0, Warnings->Num() == 0 ? TEXT("the reply has no warnings") : (*Warnings)[0]);
    }
    if (Op == TEXT("on") || Op == TEXT("inside") || Op == TEXT("nextTo") || Op == TEXT("against") || Op == TEXT("facing") || Op == TEXT("near"))
    {
        FVerdict Missing;
        AActor* Subject = NamedActor(*Node->Args[0], Missing);
        AActor* Object = Subject ? NamedActor(*Node->Args[1], Missing) : nullptr;
        if (!Subject || !Object || !World)
        {
            return World ? Missing : Unknown(TEXT("no world is open"));
        }
        FString Actual;
        TSharedPtr<FJsonObject> NextCall;
        const double Within = Node->Args.Num() > 2 ? Node->Args[2]->Number : -1.0;
        const bool bOk = McpRelations::JudgePair(World, Subject, Op, Object, Within, Actual, NextCall);
        return Decide(bOk, Actual, NextCall);
    }
    if (Op == TEXT("in") || Op == TEXT("==") || Op == TEXT("!=") || Op == TEXT("<") || Op == TEXT("<=") || Op == TEXT(">") || Op == TEXT(">="))
    {
        return JudgeComparison(*Node);
    }
    return JudgePredicate(*Node);
}
} // namespace McpContracts
