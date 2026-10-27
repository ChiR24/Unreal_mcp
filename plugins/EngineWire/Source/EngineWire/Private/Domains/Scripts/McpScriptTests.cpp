#include "Domains/Scripts/McpScript.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Editor.h"
#include "EngineWireSubsystem.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpScriptTest, "EngineWire.Scripts.CheckAndRun",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers: what the check refuses before anything runs, and a script with no capability call run to its result
// (it needs no restore point, gate or handler).
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

    // A deadline already past stops at the first statement, call or not.
    Limits.DeadlineSeconds = FPlatformTime::Seconds() - 1.0;
    FRunner Late(*Subsystem, TEXT("script-test-late"), nullptr, FMcpCapabilityPrincipal(), None, Limits);
    TestFalse(TEXT("a past deadline stops the script"), Late.Run(Forms));
    TestEqual(TEXT("with SCRIPT_DEADLINE"), Late.ErrorCode, FString(TEXT("SCRIPT_DEADLINE")));
    return true;
}
#endif
