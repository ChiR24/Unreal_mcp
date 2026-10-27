// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Solvers/McpCassowary.h"

namespace McpCassowary
{
namespace
{
constexpr int32 CassowaryMaxPivots = 100000; // Bland's order cannot cycle; this only bounds a broken input
} // namespace

int32 FSolver::NewSymbol(ESymbol Type)
{
    return Symbols.Add(Type);
}

int32 FSolver::AddVariable()
{
    return VariableSymbols.Add(NewSymbol(ESymbol::External));
}

// The constraint as a row over the symbols that are not basic, with its slack (an inequality), its error symbols (a
// preferred constraint, weighed in the objective by its strength) or a dummy (a required equality); constant made >= 0.
FSolver::FRow FSolver::CreateRow(const FConstraint& C, int32& OutMarker, int32& OutOther)
{
    FRow Row;
    Row.Constant = C.Constant;
    for (const TPair<int32, double>& Term : C.Terms)
    {
        if (CassowaryNearZero(Term.Value) || !VariableSymbols.IsValidIndex(Term.Key))
        {
            continue;
        }
        const int32 Symbol = VariableSymbols[Term.Key];
        if (const FRow* Basic = Rows.Find(Symbol))
        {
            Row.Insert(*Basic, Term.Value);
        }
        else
        {
            Row.Insert(Symbol, Term.Value);
        }
    }
    OutMarker = INDEX_NONE;
    OutOther = INDEX_NONE;
    const double Strength = FMath::Min(C.Strength, Required);
    if (C.Relation != ERelation::Equal)
    {
        const double Sign = C.Relation == ERelation::LessOrEqual ? 1.0 : -1.0;
        OutMarker = NewSymbol(ESymbol::Slack);
        Row.Insert(OutMarker, Sign);
        if (Strength < Required)
        {
            OutOther = NewSymbol(ESymbol::Error);
            Row.Insert(OutOther, -Sign);
            Objective.Insert(OutOther, Strength);
        }
    }
    else if (Strength < Required)
    {
        OutMarker = NewSymbol(ESymbol::Error);
        OutOther = NewSymbol(ESymbol::Error);
        Row.Insert(OutMarker, -1.0);
        Row.Insert(OutOther, 1.0);
        Objective.Insert(OutMarker, Strength);
        Objective.Insert(OutOther, Strength);
    }
    else
    {
        OutMarker = NewSymbol(ESymbol::Dummy);
        Row.Insert(OutMarker, 1.0);
    }
    if (Row.Constant < 0.0)
    {
        Row.Constant = -Row.Constant;
        for (TPair<int32, double>& Cell : Row.Cells)
        {
            Cell.Value = -Cell.Value;
        }
    }
    return Row;
}

// An unrestricted (external) symbol when the row has one; else its own slack or error symbol with a negative
// coefficient, which can enter without leaving the solution infeasible.
int32 FSolver::ChooseSubject(const FRow& Row, int32 Marker, int32 Other) const
{
    for (const TPair<int32, double>& Cell : Row.Cells)
    {
        if (Symbols[Cell.Key] == ESymbol::External)
        {
            return Cell.Key;
        }
    }
    for (const int32 Symbol : {Marker, Other})
    {
        const double* Coefficient = Symbol == INDEX_NONE ? nullptr : Row.Cells.Find(Symbol);
        if (Coefficient && *Coefficient < 0.0 && (Symbols[Symbol] == ESymbol::Slack || Symbols[Symbol] == ESymbol::Error))
        {
            return Symbol;
        }
    }
    return INDEX_NONE;
}

bool FSolver::AddConstraint(const FConstraint& C)
{
    int32 Marker = INDEX_NONE;
    int32 Other = INDEX_NONE;
    FRow Row = CreateRow(C, Marker, Other);
    int32 Subject = ChooseSubject(Row, Marker, Other);
    bool bAllDummies = true;
    for (const TPair<int32, double>& Cell : Row.Cells)
    {
        bAllDummies &= Symbols[Cell.Key] == ESymbol::Dummy;
    }
    if (Subject == INDEX_NONE && bAllDummies)
    {
        if (!CassowaryNearZero(Row.Constant))
        {
            return false; // required equalities that cannot all hold
        }
        Subject = Marker; // the same equality again
    }
    if (Subject == INDEX_NONE)
    {
        if (!AddWithArtificial(Row))
        {
            return false;
        }
    }
    else
    {
        Row.SolveFor(Subject);
        Substitute(Subject, Row);
        Rows.Add(Subject, MoveTemp(Row));
    }
    return Optimize(Objective);
}

// The row enters through an artificial variable driven to zero; it cannot be when the row contradicts the required
// constraints already in.
bool FSolver::AddWithArtificial(const FRow& Row)
{
    const int32 Art = NewSymbol(ESymbol::Slack);
    Rows.Add(Art, Row);
    Artificial = Row;
    const bool bBounded = Optimize(Artificial.GetValue());
    const bool bSuccess = bBounded && CassowaryNearZero(Artificial->Constant);
    Artificial.Reset();
    if (FRow* Basic = Rows.Find(Art))
    {
        FRow Leaving = MoveTemp(*Basic);
        Rows.Remove(Art);
        if (Leaving.Cells.Num() == 0)
        {
            return bSuccess;
        }
        int32 Entering = INDEX_NONE;
        for (const TPair<int32, double>& Cell : Leaving.Cells)
        {
            if (Symbols[Cell.Key] == ESymbol::Slack || Symbols[Cell.Key] == ESymbol::Error)
            {
                Entering = Cell.Key;
                break;
            }
        }
        if (Entering == INDEX_NONE)
        {
            return false;
        }
        Leaving.SolveFor(Art, Entering);
        Substitute(Entering, Leaving);
        Rows.Add(Entering, MoveTemp(Leaving));
    }
    for (TPair<int32, FRow>& Pair : Rows)
    {
        Pair.Value.Cells.Remove(Art);
    }
    Objective.Cells.Remove(Art);
    return bSuccess;
}

void FSolver::Substitute(int32 Symbol, const FRow& Row)
{
    for (TPair<int32, FRow>& Pair : Rows)
    {
        Pair.Value.Substitute(Symbol, Row);
    }
    Objective.Substitute(Symbol, Row);
    if (Artificial.IsSet())
    {
        Artificial->Substitute(Symbol, Row);
    }
}

// Primal simplex on Target: the first symbol (lowest number) that would lower it enters, the row that bounds it first
// leaves. False only for an unbounded target.
bool FSolver::Optimize(FRow& Target)
{
    for (int32 Pivot = 0; Pivot < CassowaryMaxPivots; ++Pivot)
    {
        int32 Entering = INDEX_NONE;
        for (const TPair<int32, double>& Cell : Target.Cells)
        {
            if (Symbols[Cell.Key] != ESymbol::Dummy && Cell.Value < 0.0)
            {
                Entering = Cell.Key;
                break;
            }
        }
        if (Entering == INDEX_NONE)
        {
            return true;
        }
        int32 Leaving = INDEX_NONE;
        double Ratio = TNumericLimits<double>::Max();
        for (const TPair<int32, FRow>& Pair : Rows)
        {
            const double* Coefficient = Symbols[Pair.Key] == ESymbol::External ? nullptr : Pair.Value.Cells.Find(Entering);
            if (Coefficient && *Coefficient < 0.0 && -Pair.Value.Constant / *Coefficient < Ratio)
            {
                Ratio = -Pair.Value.Constant / *Coefficient;
                Leaving = Pair.Key;
            }
        }
        if (Leaving == INDEX_NONE)
        {
            return false;
        }
        FRow Row = MoveTemp(Rows.FindChecked(Leaving));
        Rows.Remove(Leaving);
        Row.SolveFor(Leaving, Entering);
        Substitute(Entering, Row);
        Rows.Add(Entering, MoveTemp(Row));
    }
    return false;
}

double FSolver::Value(int32 Variable) const
{
    const FRow* Row = VariableSymbols.IsValidIndex(Variable) ? Rows.Find(VariableSymbols[Variable]) : nullptr;
    return Row ? Row->Constant : 0.0;
}
} // namespace McpCassowary
