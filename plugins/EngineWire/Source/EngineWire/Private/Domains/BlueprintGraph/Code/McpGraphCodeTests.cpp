#include "Domains/BlueprintGraph/Code/McpGraphCode.h"
#include "Domains/BlueprintGraph/Code/McpGraphCodeParse.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace McpGraphCode
{
namespace GraphCodeTestParts
{
FPin MakePin(const TCHAR* Name, bool bExec = false, const TCHAR* Value = TEXT(""))
{
    FPin Out;
    Out.Name = Name;
    Out.bExec = bExec;
    Out.Value = Value;
    return Out;
}

FPin ExecPin(const TCHAR* Name)
{
    return MakePin(Name, true);
}

FPin TextPin(const TCHAR* Name, const TCHAR* Value = TEXT(""))
{
    FPin Out = MakePin(Name, false, Value);
    Out.bText = true;
    return Out;
}

FPin BoolPin(const TCHAR* Name, const TCHAR* Value)
{
    FPin Out = MakePin(Name, false, Value);
    Out.bBool = true;
    return Out;
}

// Fake graphs laid out as the _make_node_info pins of Epic's test_blueprint_dsl.py.
struct FFakeGraph
{
    FGraph Graph;

    int32 Add(const TCHAR* TypeId, TArray<FPin> Ins, TArray<FPin> Outs, EKind Kind = EKind::Call)
    {
        FNode& Node = Graph.Nodes.AddDefaulted_GetRef();
        Node.TypeId = TypeId;
        Node.Kind = Kind;
        Node.Id = FString::Printf(TEXT("n%d"), Graph.Nodes.Num());
        Node.Guid = FString::Printf(TEXT("%08d"), Graph.Nodes.Num());
        Node.Ins = MoveTemp(Ins);
        Node.Outs = MoveTemp(Outs);
        return Graph.Nodes.Num() - 1;
    }
    int32 Event(const TCHAR* TypeId = TEXT("AddEvent|Foo")) { return Add(TypeId, {}, {ExecPin(TEXT("then"))}); }
    int32 Print(const TCHAR* Say = TEXT(""))
    {
        return Add(TEXT("Development|PrintString"), {ExecPin(TEXT("execute")), TextPin(TEXT("InString"), Say)}, {ExecPin(TEXT("then")), MakePin(TEXT("ReturnValue"))});
    }
    int32 Branch(const TCHAR* Test = TEXT("true"))
    {
        return Add(TEXT("Utilities|FlowControl|Branch"), {ExecPin(TEXT("execute")), BoolPin(TEXT("Condition"), Test)}, {ExecPin(TEXT("then")), ExecPin(TEXT("else"))}, EKind::Branch);
    }
    void Link(int32 From, const TCHAR* Out, int32 To, const TCHAR* In = TEXT("execute"))
    {
        const int32 OutAt = Graph.Nodes[From].Outs.IndexOfByPredicate([Out](const FPin& Pin) { return Pin.Name == Out; });
        const int32 InAt = Graph.Nodes[To].Ins.IndexOfByPredicate([In](const FPin& Pin) { return Pin.Name == In; });
        if (OutAt != INDEX_NONE && InAt != INDEX_NONE)
        {
            Graph.Nodes[From].Outs[OutAt].Links.Add(FPinRef{To, InAt});
            Graph.Nodes[To].Ins[InAt].Links.Add(FPinRef{From, OutAt});
        }
    }
    FString Read() const
    {
        TSet<int32> Placed;
        TArray<FString> Warnings;
        return Decompile(Graph, Placed, Warnings);
    }
};
} // namespace GraphCodeTestParts
} // namespace McpGraphCode

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpGraphCodeDecompileTest, "EngineWire.BlueprintGraph.Code.Decompile",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers from Epic's decompiler tests, plus what this reader adds: (goto) where Epic's stops, the loop index, and
// binds by output position. Every text must parse back under Epic's grammar with its node tags outside the parens.
bool FMcpGraphCodeDecompileTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpGraphCode;
    using namespace McpGraphCode::GraphCodeTestParts;
    const auto Has = [](const FString& Text, const TCHAR* Part) { return Text.Contains(Part, ESearchCase::CaseSensitive); };
    const auto Parses = [this](const TCHAR* What, const FString& Text)
    {
        TArray<FSExpr> Forms;
        FString Error;
        const bool bParsed = ParseSExpr(Text, Forms, Error);
        TestTrue(FString::Printf(TEXT("%s parses (%s)"), What, *Error), bParsed && Forms.Num() > 0);
    };
    {
        FFakeGraph G;
        const int32 Event = G.Event();
        const int32 Getter = G.Add(TEXT("Transformation|GetActorLocation"), {ExecPin(TEXT("execute"))}, {ExecPin(TEXT("then")), MakePin(TEXT("ReturnValue"))});
        const int32 Printer = G.Print(TEXT("hello"));
        G.Link(Event, TEXT("then"), Getter);
        G.Link(Getter, TEXT("then"), Printer);
        G.Link(Getter, TEXT("ReturnValue"), Printer, TEXT("InString"));
        const FString Text = G.Read();
        TestEqual(TEXT("a read output is bound where its node runs"), Text,
                  FString(TEXT("(event Foo  ; n1\n  (bind _returnvalue (Transformation|GetActorLocation))  ; n2\n  (Development|PrintString _returnvalue))  ; n3\n")));
        TestEqual(TEXT("two reads give the same text"), G.Read(), Text);
        Parses(TEXT("bind"), Text);
    }
    {
        FFakeGraph G;
        const int32 Event = G.Event();
        const int32 Branch = G.Branch();
        const int32 Body = G.Print(TEXT("on cooldown"));
        G.Link(Event, TEXT("then"), Branch);
        G.Link(Branch, TEXT("else"), Body);
        const FString Text = G.Read();
        TestTrue(TEXT("an empty true side turns the condition"), Has(Text, TEXT("(if (not true)")) && !Has(Text, TEXT("(else")) && !Has(Text, TEXT("  _")));
        Parses(TEXT("flip"), Text);
    }
    {
        FFakeGraph G;
        const int32 Event = G.Event();
        const int32 Not = G.Add(TEXT("Math|Boolean|NOTBoolean"), {BoolPin(TEXT("Input"), TEXT("true"))}, {MakePin(TEXT("Result"))});
        const int32 Branch = G.Branch();
        G.Link(Event, TEXT("then"), Branch);
        G.Link(Not, TEXT("Result"), Branch, TEXT("Condition"));
        G.Link(Branch, TEXT("else"), G.Print(TEXT("x")));
        TestTrue(TEXT("a turned NOT loses both negations"), Has(G.Read(), TEXT("(if true")) && !Has(G.Read(), TEXT("(not")));
    }
    {
        FFakeGraph G;
        const int32 Event = G.Event();
        const int32 Outer = G.Branch();
        const int32 Inner = G.Branch(TEXT("false"));
        const int32 A = G.Print(TEXT("A"));
        const int32 B = G.Print(TEXT("B"));
        const int32 C = G.Print(TEXT("C"));
        const int32 End = G.Print(TEXT("end"));
        G.Link(Event, TEXT("then"), Outer);
        G.Link(Outer, TEXT("then"), Inner);
        G.Link(Outer, TEXT("else"), C);
        G.Link(Inner, TEXT("then"), A);
        G.Link(Inner, TEXT("else"), B);
        for (const int32 Side : {A, B, C})
        {
            G.Link(Side, TEXT("then"), End);
        }
        const FString Text = G.Read();
        TestTrue(TEXT("the node both sides reach follows the outermost if, once"),
                 Has(Text, TEXT("\n  (Development|PrintString \"end\"))  ; n7")) && Text.Find(TEXT("\"end\"")) == Text.Find(TEXT("\"end\""), ESearchCase::CaseSensitive, ESearchDir::FromEnd));
        Parses(TEXT("nested if"), Text);
    }
    {
        FFakeGraph G;
        const int32 Event = G.Event();
        const int32 Loop = G.Add(TEXT("Utilities|FlowControl|ForLoop"), {ExecPin(TEXT("execute")), MakePin(TEXT("FirstIndex"), false, TEXT("2")), MakePin(TEXT("LastIndex"), false, TEXT("9"))},
                                 {ExecPin(TEXT("LoopBody")), MakePin(TEXT("Index")), ExecPin(TEXT("Completed"))}, EKind::ForLoop);
        G.Link(Event, TEXT("then"), Loop);
        G.Link(Loop, TEXT("LoopBody"), G.Print());
        TestTrue(TEXT("an inclusive last index reads as an exclusive stop"), Has(G.Read(), TEXT("(for _ (range 2 10)")));
    }
    {
        FFakeGraph G;
        const int32 Event = G.Event();
        const int32 Loop = G.Add(TEXT("Utilities|FlowControl|ForLoopWithBreak"), {ExecPin(TEXT("execute")), ExecPin(TEXT("Break")), MakePin(TEXT("FirstIndex"), false, TEXT("0")), MakePin(TEXT("LastIndex"), false, TEXT("4"))},
                                 {ExecPin(TEXT("LoopBody")), MakePin(TEXT("Index")), ExecPin(TEXT("Completed"))}, EKind::ForLoopBreak);
        const int32 Body = G.Print();
        G.Link(Event, TEXT("then"), Loop);
        G.Link(Loop, TEXT("LoopBody"), Body);
        G.Link(Body, TEXT("then"), Loop, TEXT("Break"));
        TestTrue(TEXT("a wire into Break is (break)"), Has(G.Read(), TEXT("(for _ (range 5)")) && Has(G.Read(), TEXT("(break)")));
    }
    {
        FFakeGraph G;
        const int32 Event = G.Event();
        const int32 Loop = G.Add(TEXT("Utilities|FlowControl|ForEachLoop"), {ExecPin(TEXT("execute")), MakePin(TEXT("Array"))},
                                 {ExecPin(TEXT("LoopBody")), MakePin(TEXT("Array Element")), MakePin(TEXT("Array Index")), ExecPin(TEXT("Completed"))}, EKind::ForEach);
        const int32 Body = G.Print();
        G.Link(Event, TEXT("then"), Loop);
        G.Link(Loop, TEXT("LoopBody"), Body);
        G.Link(Loop, TEXT("Array Index"), Body, TEXT("InString"));
        const FString Text = G.Read();
        TestTrue(TEXT("a read loop index keeps the loop as a call, bound by position"),
                 Has(Text, TEXT("(bind (_ _array_index) (Utilities|FlowControl|ForEachLoop 0)")) && Has(Text, TEXT("(:LoopBody")) && Has(Text, TEXT("(Development|PrintString _array_index)")));
        Parses(TEXT("for each index"), Text);
    }
    {
        FFakeGraph G;
        const int32 Event = G.Event();
        const int32 Sequence = G.Add(TEXT("Utilities|FlowControl|Sequence"), {ExecPin(TEXT("execute"))}, {ExecPin(TEXT("then_0")), ExecPin(TEXT("then_1"))});
        const int32 A = G.Print(TEXT("a"));
        const int32 B = G.Print(TEXT("b"));
        const int32 Shared = G.Print(TEXT("shared"));
        G.Link(Event, TEXT("then"), Sequence);
        G.Link(Sequence, TEXT("then_0"), A);
        G.Link(Sequence, TEXT("then_1"), B);
        G.Link(A, TEXT("then"), Shared);
        G.Link(B, TEXT("then"), Shared);
        const FString Text = G.Read();
        TestTrue(TEXT("a sequence keeps its outputs; a second path into a printed node says where it goes"),
                 Has(Text, TEXT("(Utilities|FlowControl|Sequence  ; n2")) && Has(Text, TEXT("(:then_0")) && Has(Text, TEXT("(:then_1")) && Has(Text, TEXT("(goto n5)")));
        Parses(TEXT("goto"), Text);
    }
    {
        FFakeGraph G;
        const int32 Event = G.Event();
        const int32 Task = G.Add(TEXT("Utilities|IsValid"), {ExecPin(TEXT("execute")), MakePin(TEXT("Object"))}, {ExecPin(TEXT("Is Valid")), ExecPin(TEXT("Is Not Valid"))});
        const int32 Gate = G.Add(TEXT("Utilities|FlowControl|MultiGate"), {ExecPin(TEXT("execute"))}, {ExecPin(TEXT("Out 0")), ExecPin(TEXT("Out 1"))});
        G.Link(Event, TEXT("then"), Task);
        G.Link(Task, TEXT("Is Valid"), Gate);
        G.Link(Task, TEXT("Is Not Valid"), G.Print(TEXT("nil")));
        G.Link(Gate, TEXT("Out 1"), G.Print(TEXT("second")));
        const FString Text = G.Read();
        TestTrue(TEXT("pin names with spaces are quoted; a multigate is a call with continuations"),
                 Has(Text, TEXT("(:\"Is Valid\"")) && Has(Text, TEXT("(Utilities|FlowControl|MultiGate")) && Has(Text, TEXT("(:\"Out 1\"")));
        Parses(TEXT("continuations"), Text);
    }
    {
        FFakeGraph G;
        const int32 Br1 = G.Branch();
        const int32 Br2 = G.Branch();
        const int32 Br3 = G.Branch();
        G.Link(G.Event(), TEXT("then"), Br1);
        G.Link(Br1, TEXT("then"), G.Print(TEXT("a")));
        G.Link(Br1, TEXT("else"), Br2);
        G.Link(Br2, TEXT("then"), G.Print(TEXT("b")));
        G.Link(Br2, TEXT("else"), Br3);
        G.Link(Br3, TEXT("then"), G.Print(TEXT("c")));
        G.Link(Br3, TEXT("else"), G.Print(TEXT("d")));
        const FString Text = G.Read();
        TestTrue(TEXT("a branch on else is elif, the last else stays"), Has(Text, TEXT("(elif true  ; n2")) && Has(Text, TEXT("(elif true  ; n3")) && Has(Text, TEXT("(else")));
        Parses(TEXT("elif"), Text);
    }
    {
        FFakeGraph G;
        const int32 Entry = G.Add(TEXT("|DoubleIt"), {}, {ExecPin(TEXT("then")), MakePin(TEXT("Value"))});
        const int32 Times = G.Add(TEXT("Utilities|Operators|Multiply"), {MakePin(TEXT("A")), MakePin(TEXT("B"), false, TEXT("2"))}, {MakePin(TEXT("Result"))});
        const int32 Return = G.Add(TEXT("|ReturnNode"), {ExecPin(TEXT("execute")), MakePin(TEXT("ReturnValue"))}, {}, EKind::Return);
        G.Link(Entry, TEXT("then"), Return);
        G.Link(Entry, TEXT("Value"), Times, TEXT("A"));
        G.Link(Times, TEXT("Result"), Return, TEXT("ReturnValue"));
        TestEqual(TEXT("a function: header with parameters, operator inlined"), G.Read(), FString(TEXT("(fn DoubleIt (Value)  ; n1\n  (return (* Value 2)))  ; n3\n")));
        FFakeGraph Custom;
        Custom.Event(TEXT("AddEvent|Custom|OnDamageTaken"));
        TestEqual(TEXT("a custom event keeps its prefix"), Custom.Read(), FString(TEXT("(event Custom|OnDamageTaken)  ; n1\n")));
    }
    return true;
}
#endif
