#include "puzzles/flower/constraint_solver.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    std::size_t watchIndex(const int literal)
    {
        return static_cast<std::size_t>(2 * std::abs(literal) + (literal < 0 ? 1 : 0));
    }
}

int FlowerConstraints::addVariable()
{
    const int variable = static_cast<int>(m_values.size());
    m_values.push_back(-1);
    m_levels.push_back(0);
    m_reasons.push_back(-1);
    m_activity.push_back(0);
    m_phase.push_back(false);
    m_watches.resize(static_cast<std::size_t>(2 * (variable + 1)));
    return variable;
}

void FlowerConstraints::addClause(std::vector<int> clause)
{
    const int index = static_cast<int>(m_clauses.size());
    for (const int literal : clause)
    {
        m_activity[std::abs(literal)] += 0.001;
    }

    m_clauses.push_back(std::move(clause));
    const auto& stored = m_clauses.back();
    if (stored.empty())
    {
        m_consistent = false;
    }
    else if (stored.size() == 1)
    {
        m_consistent = assign(stored.front(), index) && m_consistent;
    }
    else
    {
        m_watches[watchIndex(stored[0])].push_back(index);
        m_watches[watchIndex(stored[1])].push_back(index);
    }
}

int FlowerConstraints::value(const int variable) const
{
    return m_values[variable];
}

std::uint64_t FlowerConstraints::nodes() const
{
    return m_nodes;
}

int FlowerConstraints::literalValue(const int literal) const
{
    const int result = m_values[std::abs(literal)];
    return result < 0 ? -1 : (literal > 0 ? result : 1 - result);
}

bool FlowerConstraints::assign(const int literal, const int reason)
{
    if (const int existing = literalValue(literal); existing >= 0)
    {
        return existing == 1;
    }

    const int variable  = std::abs(literal);
    m_values[variable]  = literal > 0 ? 1 : 0;
    m_levels[variable]  = static_cast<int>(m_limits.size());
    m_reasons[variable] = reason;
    m_phase[variable]   = literal > 0;
    m_trail.push_back(literal);
    return true;
}

int FlowerConstraints::propagate()
{
    while (m_next < m_trail.size())
    {
        // Check often enough to cancel promptly without reading the clock for every assignment.
        if ((m_next & 255U) == 0 && interrupted())
        {
            return -1;
        }

        // Only clauses whose watched literal just became false need another look.
        const int falseLiteral = -m_trail[m_next++];
        auto& watches          = m_watches[watchIndex(falseLiteral)];
        std::size_t index      = 0;
        while (index < watches.size())
        {
            const int clauseIndex = watches[index];
            auto& clause          = m_clauses[clauseIndex];
            if (clause[0] == falseLiteral)
            {
                std::swap(clause[0], clause[1]);
            }

            if (literalValue(clause[0]) == 1)
            {
                ++index;
                continue;
            }

            std::size_t replacement = 2;
            while (replacement < clause.size() && literalValue(clause[replacement]) == 0)
            {
                ++replacement;
            }

            if (replacement < clause.size())
            {
                std::swap(clause[1], clause[replacement]);
                m_watches[watchIndex(clause[1])].push_back(clauseIndex);
                watches[index] = watches.back();
                watches.pop_back();
                // The swapped-in watch still needs checking, so don't advance index here.
                continue;
            }

            // No replacement: the other watched literal must be true, or we have a conflict.
            if (!assign(clause[0], clauseIndex))
            {
                return clauseIndex;
            }

            ++index;
        }
    }

    return -1;
}

void FlowerConstraints::backtrack(const std::size_t level)
{
    if (m_limits.size() <= level)
    {
        return;
    }

    const auto keep = m_limits[level];
    for (std::size_t index = keep; index < m_trail.size(); ++index)
    {
        const int variable  = std::abs(m_trail[index]);
        m_values[variable]  = -1;
        m_reasons[variable] = -1;
        m_levels[variable]  = 0;
    }

    m_trail.resize(keep);
    m_limits.resize(level);
    m_next = std::min(m_next, keep);
}

std::vector<int> FlowerConstraints::analyze(int conflict, std::size_t& level)
{
    std::vector<bool> seen(m_values.size());
    std::vector<int> learned = {0};

    int unresolved = 0;
    int pivot      = 0;
    auto cursor    = m_trail.size();

    // Walk back through the reasons until only one literal from this decision level remains.
    // That gives us a learned clause which forces a choice after backtracking (first UIP).
    do
    {
        for (const int literal : m_clauses[conflict])
        {
            const int variable = std::abs(literal);
            if (variable == std::abs(pivot) || seen[variable] || m_levels[variable] == 0)
            {
                continue;
            }

            seen[variable] = true;
            m_activity[variable] += m_increment;
            if (m_levels[variable] == static_cast<int>(m_limits.size()))
            {
                ++unresolved;
            }
            else
            {
                learned.push_back(literal);
            }
        }

        do
        {
            pivot = m_trail[--cursor];
        } while (!seen[std::abs(pivot)]);

        seen[std::abs(pivot)] = false;
        --unresolved;
        conflict = m_reasons[std::abs(pivot)];
    } while (unresolved > 0);

    learned[0] = -pivot;
    level      = 0;

    // Put the next-highest decision level in slot 1 so the learned clause is watched correctly.
    for (std::size_t index = 1; index < learned.size(); ++index)
    {
        if (static_cast<std::size_t>(m_levels[std::abs(learned[index])]) > level)
        {
            level = static_cast<std::size_t>(m_levels[std::abs(learned[index])]);
            std::swap(learned[1], learned[index]);
        }
    }

    m_increment /= 0.95;
    if (m_increment > 1e100)
    {
        // Scale every score together to avoid overflow without changing their ranking.
        for (auto& activity : m_activity)
        {
            activity *= 1e-100;
        }

        m_increment *= 1e-100;
    }

    return learned;
}

bool FlowerConstraints::interrupted()
{
    if (m_stop.stop_requested())
    {
        m_status = FlowerSolveStatus::Cancelled;
        return true;
    }

    if (std::chrono::steady_clock::now() >= m_deadline)
    {
        m_status = FlowerSolveStatus::LimitReached;
        return true;
    }

    return false;
}

FlowerSolveStatus FlowerConstraints::solve(
    const FlowerSearchOptions& options,
    const std::stop_token stop,
    const std::function<void()>& progress
)
{
    m_stop     = stop;
    m_deadline = std::chrono::steady_clock::now() + options.timeLimit;

    const auto initialClauses = m_clauses.size();
    auto nextPreview         = std::chrono::steady_clock::now();
    unsigned int conflicts   = 0;
    unsigned int restartAt   = 128;

    while (!interrupted())
    {
        // Learned clauses help later guesses, but cap their growth on stubborn boards.
        if (m_nodes >= options.maxNodes || m_clauses.size() - initialClauses >= 200000)
        {
            return FlowerSolveStatus::LimitReached;
        }

        ++m_nodes;
        if (!m_consistent)
        {
            return FlowerSolveStatus::NoSolution;
        }

        const int conflict = propagate();
        if (m_status != FlowerSolveStatus::NoSolution)
        {
            return m_status;
        }

        if (progress && std::chrono::steady_clock::now() >= nextPreview)
        {
            progress();
            // A callback can cancel or use up the remaining time, even on the final assignment.
            if (interrupted())
            {
                return m_status;
            }

            nextPreview = std::chrono::steady_clock::now() + std::chrono::milliseconds(33);
        }

        if (conflict >= 0)
        {
            if (m_limits.empty())
            {
                return FlowerSolveStatus::NoSolution;
            }

            std::size_t level = 0;
            auto learned      = analyze(conflict, level);
            backtrack(level);
            const int literal = learned.front();
            const int reason  = static_cast<int>(m_clauses.size());
            addClause(std::move(learned));
            m_consistent = assign(literal, reason) && m_consistent;
            if (++conflicts >= restartAt)
            {
                // Drop the guesses, keeping learned clauses and the last choice for each variable.
                backtrack(0);
                conflicts = 0;
                restartAt += 64;
            }

            continue;
        }

        int decision = 0;
        for (std::size_t variable = 1; variable < m_values.size(); ++variable)
        {
            if (m_values[variable] < 0 && (decision == 0 || m_activity[variable] > m_activity[decision]))
            {
                decision = static_cast<int>(variable);
            }
        }

        if (decision == 0)
        {
            return FlowerSolveStatus::Solved;
        }

        m_limits.push_back(m_trail.size());
        assign(m_phase[decision] ? decision : -decision, -1);
    }

    return m_status;
}
