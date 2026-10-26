// Copyright (c) 2024 EngineWire Contributors

// The rows of the Cassowary tableau (see McpCassowary.h): a constant and coefficients by symbol, kept in symbol order.
#include "Foundation/Solvers/McpCassowary.h"

namespace McpCassowary
{
void FSolver::FRow::Insert(int32 Symbol, double Coefficient)
{
    double& Cell = Cells.FindOrAdd(Symbol);
    Cell += Coefficient;
    if (CassowaryNearZero(Cell))
    {
        Cells.Remove(Symbol);
    }
}

void FSolver::FRow::Insert(const FRow& Other, double Coefficient)
{
    Constant += Other.Constant * Coefficient;
    for (const TPair<int32, double>& Cell : Other.Cells)
    {
        Insert(Cell.Key, Cell.Value * Coefficient);
    }
}

void FSolver::FRow::SolveFor(int32 Symbol)
{
    const double Coefficient = -1.0 / Cells.FindChecked(Symbol);
    Cells.Remove(Symbol);
    Constant *= Coefficient;
    for (TPair<int32, double>& Cell : Cells)
    {
        Cell.Value *= Coefficient;
    }
}

void FSolver::FRow::SolveFor(int32 Lhs, int32 Rhs)
{
    Insert(Lhs, -1.0);
    SolveFor(Rhs);
}

void FSolver::FRow::Substitute(int32 Symbol, const FRow& Row)
{
    if (const double* Coefficient = Cells.Find(Symbol))
    {
        const double Value = *Coefficient;
        Cells.Remove(Symbol);
        Insert(Row, Value);
    }
}
} // namespace McpCassowary
