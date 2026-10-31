#pragma once

#include "puzzles/flower/solver.h"

#include <functional>

// Variables start at 1. A negative literal means "this variable must be false";
// each clause needs at least one of its literals to be true.
class FlowerConstraints
{
private:
    std::vector<std::vector<int>> m_clauses;

    // Only the first two literals of each non-unit clause need watching.
    std::vector<std::vector<int>> m_watches;

    // -1 means undecided, 0 means false, and 1 means true. Slot 0 is unused.
    std::vector<int> m_values      = {-1};
    std::vector<int> m_levels      = {0};
    std::vector<int> m_reasons     = {-1};
    std::vector<double> m_activity = {0};
    std::vector<bool> m_phase      = {false};

    // The trail records assignments in order; limits mark where each guess began.
    std::vector<int> m_trail;
    std::vector<std::size_t> m_limits;
    std::size_t m_next = 0;

    double m_increment = 1.0;
    bool m_consistent  = true;

    std::chrono::steady_clock::time_point m_deadline;
    std::stop_token m_stop;
    FlowerSolveStatus m_status = FlowerSolveStatus::NoSolution;
    std::uint64_t m_nodes      = 0;

public:
    int addVariable();
    void addClause(std::vector<int> clause);
    int value(int variable) const;
    std::uint64_t nodes() const;
    FlowerSolveStatus solve(
        const FlowerSearchOptions& options,
        std::stop_token stop,
        const std::function<void()>& progress = {}
    );

private:
    int literalValue(int literal) const;
    bool assign(int literal, int reason);
    int propagate();
    void backtrack(std::size_t level);
    std::vector<int> analyze(int conflict, std::size_t& level);
    bool interrupted();
};
