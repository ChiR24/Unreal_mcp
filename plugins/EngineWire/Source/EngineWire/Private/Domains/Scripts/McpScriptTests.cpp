#include "Domains/Scripts/McpScript.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Editor.h"
#include "Engine/World.h"
#include "EngineWireSubsystem.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpScriptTest, "EngineWire.Scripts.CheckAndRun",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers: what the check refuses before anything runs, and a script with no capability call run to its result
// (it needs no restore point, gate or handler), again in a second pass as after a wait on a step, an expectation keeping
// the verdict its first pass gave.
bool FMcpScriptTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpScript;
    using McpGraphCode::FSExpr;
    const auto CodeOf = [](const FString& Text, const TSet<FString>& Allowed)
    {
        TArray<FSExpr> Forms;
        FString Error;
        FString Code = TEXT("PARSE");
        return McpGraphCode::ParseSExpr(Text, Forms, Error) && Check(Forms, Allowed, Error, Code) ? FString(TEXT("ok")) : Code;
    };
    const TSet<FString> None;
    TestEqual(TEXT("names bound in order"), CodeOf(TEXT("(let a 1)\n(return (+ a 2))"), None), FString(TEXT("ok")));
    TestEqual(TEXT("a name used before it is bound"), CodeOf(TEXT("(return b)"), None), FString(TEXT("SCRIPT_UNBOUND_NAME")));
    TestEqual(TEXT("a for variable is not bound after the loop"),
              CodeOf(TEXT("(for i (range 2) (let x i))\n(return i)"), None), FString(TEXT("SCRIPT_UNBOUND_NAME")));
    TestEqual(TEXT("a parameter the contract does not declare"), CodeOf(TEXT("(control_actor.list :colour 1)"), None),
              FString(TEXT("UNDECLARED_PARAMETER")));
    TestEqual(TEXT("an unknown capability"), CodeOf(TEXT("(control_actor.no_such_thing)"), None), FString(TEXT("UNKNOWN_CAPABILITY")));
    TestEqual(TEXT("no script inside a script"), CodeOf(TEXT("(system_control.run_script :script \"(return 1)\")"), None),
              FString(TEXT("SCRIPT_STEP_REFUSED")));
    TestEqual(TEXT("undo, by an alias of its family"), CodeOf(TEXT("(control_editor.redo)"), None), FString(TEXT("SCRIPT_STEP_REFUSED")));
    TestEqual(TEXT("consent not listed"), CodeOf(TEXT("(control_actor.delete :actorName \"X\")"), None), FString(TEXT("CONSENT_NOT_LISTED")));
    TestEqual(TEXT("consent listed"), CodeOf(TEXT("(control_actor.delete :actorName \"X\")"), TSet<FString>{TEXT("control_actor.delete")}),
              FString(TEXT("ok")));
    TestEqual(TEXT("a folded alias pins its selector"), CodeOf(TEXT("(control_actor.find_by_name :name \"X\")"), None), FString(TEXT("ok")));
    TestEqual(TEXT("an operator takes two values"), CodeOf(TEXT("(return (+ 1))"), None), FString(TEXT("SCRIPT_INVALID")));
    TestEqual(TEXT("(if cond a b) is a value"), CodeOf(TEXT("(let a 1)\n(return (+ (if a 10 20) 1))"), None), FString(TEXT("ok")));
    TestEqual(TEXT("a value if needs its second value"), CodeOf(TEXT("(let x (if true 1))"), None), FString(TEXT("SCRIPT_INVALID")));
    TestEqual(TEXT("a value if takes two values, no more"), CodeOf(TEXT("(let x (if true 1 2 3))"), None), FString(TEXT("SCRIPT_INVALID")));
    TestEqual(TEXT("a value if takes no (else ...)"), CodeOf(TEXT("(let x (if true 1 (else 2)))"), None), FString(TEXT("SCRIPT_INVALID")));
    TestEqual(TEXT("names are checked in the value not picked too"), CodeOf(TEXT("(let x (if true 1 nobody))"), None),
              FString(TEXT("SCRIPT_UNBOUND_NAME")));
    TestEqual(TEXT("the statement if still needs no else"), CodeOf(TEXT("(if true (let x 1))"), None), FString(TEXT("ok")));
    TArray<FSExpr> Lone;
    FString Why;
    FString WhyCode;
    TestTrue(TEXT("a value if without its second value is refused at its line, saying it needs both values"),
             McpGraphCode::ParseSExpr(TEXT("(let a 1)\n(let b (if a 2))"), Lone, Why) && !Check(Lone, None, Why, WhyCode) &&
             Why.StartsWith(TEXT("Line 2:")) && Why.Contains(TEXT("both values")));

    const auto Num = [](double Value) { return MakeShared<FJsonValueNumber>(Value); };
    const auto Str = [](const TCHAR* Value) { return MakeShared<FJsonValueString>(Value); };
    TestEqual(TEXT("+ joins text, whole numbers without a point"), Apply(TEXT("+"), Str(TEXT("Crate_")), Num(3))->AsString(), FString(TEXT("Crate_3")));
    TestTrue(TEXT("== by value"), Apply(TEXT("=="), Num(1), Num(1.0))->AsBool());
    TestFalse(TEXT("text is not a number"), Apply(TEXT("=="), Str(TEXT("1")), Num(1))->AsBool());
    TestTrue(TEXT("no division by zero"), !Apply(TEXT("/"), Num(1), Num(0)).IsValid());
    TestFalse(TEXT("an empty list is false"), IsTrue(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>())));

    UEngineWireSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UEngineWireSubsystem>() : nullptr;
    TArray<FSExpr> Forms;
    FString Error;
    if (!TestNotNull(TEXT("subsystem"), Subsystem) ||
        !TestTrue(TEXT("parses"), McpGraphCode::ParseSExpr(TEXT("(let a (+ 1 2))\n(let o (obj :n a :t \"x\"))\n"
                                                                 "(if (and (> a 2) (not false)) (return (list o.n (range 1 3))) (else (return 0)))"),
                                                            Forms, Error)))
    {
        return false;
    }
    FRunLimits Limits;
    Limits.DeadlineSeconds = FPlatformTime::Seconds() + 5.0;
    FRunner Runner(*Subsystem, TEXT("script-test"), nullptr, FMcpCapabilityPrincipal(), None, Limits);
    TestTrue(TEXT("runs"), Runner.Run(Forms));
    const TArray<TSharedPtr<FJsonValue>>* Returned = nullptr;
    TestTrue(TEXT("returns a list"), Runner.Returned.IsValid() && Runner.Returned->TryGetArray(Returned) && Returned->Num() == 2);
    if (Returned && Returned->Num() == 2)
    {
        TestEqual(TEXT("o.n read through the object"), (*Returned)[0]->AsNumber(), 3.0);
        TestEqual(TEXT("(range 1 3) is 1, 2"), (*Returned)[1]->AsArray().Num(), 2);
    }
    TestEqual(TEXT("no step was a call"), Runner.Steps.Num(), 0);

    // (if cond a b) as a value evaluates only the value it picks: the (/ 1 0) beside it would stop the script (SCRIPT_TYPE).
    const auto Picks = [&](const TCHAR* Text)
    {
        TArray<FSExpr> Script;
        FRunner Picker(*Subsystem, TEXT("script-test-if"), nullptr, FMcpCapabilityPrincipal(), None, Limits);
        double Picked = -1.0;
        if (McpGraphCode::ParseSExpr(Text, Script, Error) && Picker.Run(Script) && Picker.Returned.IsValid())
        {
            Picker.Returned->TryGetNumber(Picked);
        }
        return Picked;
    };
    TestEqual(TEXT("a true cond picks a, b never evaluated"), Picks(TEXT("(return (if (> 3 2) 200 (/ 1 0)))")), 200.0);
    TestEqual(TEXT("a false cond picks b, a never evaluated; an empty list is false, as for the statement if"),
              Picks(TEXT("(return (if (list) (/ 1 0) 50))")), 50.0);
    TestEqual(TEXT("a value if inside an operator"), Picks(TEXT("(let a 1)\n(return (+ (if (== a 1) 10 20) 1))")), 11.0);

    // A pass after a wait on a step goes again from the first form: run twice, the runner rebuilds its values and
    // returns what it did the first time.
    FRunner Again(*Subsystem, TEXT("script-test-again"), nullptr, FMcpCapabilityPrincipal(), None, Limits);
    TestTrue(TEXT("a first pass runs"), Again.Run(Forms));
    TestTrue(TEXT("a second pass runs"), Again.Run(Forms));
    const TArray<TSharedPtr<FJsonValue>>* AgainReturned = nullptr;
    TestTrue(TEXT("and returns the same list"), Again.Returned.IsValid() && Again.Returned->TryGetArray(AgainReturned) && AgainReturned->Num() == 2);

    // An expectation an earlier pass judged is not judged again: a later pass takes the verdict it had, as the steps run
    // since may have changed what it read. Shown with an actor of this test's own that appears between two passes.
    UWorld* World = GEditor->GetEditorWorldContext().World();
    const FString Probe = TEXT("McpScriptMemoProbe_") + FGuid::NewGuid().ToString();
    TArray<FSExpr> Judged;
    if (TestNotNull(TEXT("editor world"), World) &&
        TestTrue(TEXT("parses an expectation"), McpGraphCode::ParseSExpr(FString::Printf(TEXT("(expect \"not %s exists\")\n(return 1)"), *Probe), Judged, Error)))
    {
        FRunner Twice(*Subsystem, TEXT("script-test-memo"), nullptr, FMcpCapabilityPrincipal(), None, Limits);
        TestTrue(TEXT("it holds while nothing has the name"), Twice.Run(Judged));
        FActorSpawnParameters Spawn;
        Spawn.Name = FName(*Probe);
        AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Spawn);
        FRunner Fresh(*Subsystem, TEXT("script-test-memo-fresh"), nullptr, FMcpCapabilityPrincipal(), None, Limits);
        TestFalse(TEXT("judged again once the actor is there, it fails"), Actor && Fresh.Run(Judged));
        TestEqual(TEXT("with EXPECTATION_FAILED"), Fresh.ErrorCode, FString(TEXT("EXPECTATION_FAILED")));
        TestTrue(TEXT("a later pass of the first run takes the verdict it had"), Twice.Run(Judged));
        TestTrue(TEXT("and runs on to its return"), Twice.Returned.IsValid() && Twice.Returned->Type == EJson::Number);
        if (Actor)
        {
            World->DestroyActor(Actor);
        }
    }

    // A deadline already past stops at the first statement, call or not.
    Limits.DeadlineSeconds = FPlatformTime::Seconds() - 1.0;
    FRunner Late(*Subsystem, TEXT("script-test-late"), nullptr, FMcpCapabilityPrincipal(), None, Limits);
    TestFalse(TEXT("a past deadline stops the script"), Late.Run(Forms));
    TestEqual(TEXT("with SCRIPT_DEADLINE"), Late.ErrorCode, FString(TEXT("SCRIPT_DEADLINE")));
    return true;
}
#endif
