#include "puzzles/flower/solver.h"

#include "puzzles/flower/constraint_solver.h"

#include <algorithm>
#include <bit>
#include <utility>

namespace
{
    struct FlowerEdge
    {
    public:
        int first;
        int second;
        int variable;
    };

    class FlowerRouting
    {
    private:
        const FlowerBoard& m_board;
        FlowerConstraints m_constraints;
        FlowerPaths m_flowers;
        std::vector<FlowerEdge> m_edges;
        // Store edge indices per cell so routing and previews can visit just its neighbors.
        std::vector<std::vector<int>> m_incident;
        std::vector<std::array<int, FlowerBoard::ColorCount>> m_colors;

    public:
        explicit FlowerRouting(const FlowerBoard& board) : m_board(board), m_flowers(board.flowers()),
            m_incident(static_cast<std::size_t>(board.rows() * board.columns())), m_colors(m_incident.size())
        {
            for (int row = 0; row < board.rows(); ++row)
            {
                for (int column = 0; column < board.columns(); ++column)
                {
                    const int cell = row * board.columns() + column;
                    if (column + 1 < board.columns())
                    {
                        addEdge(cell, cell + 1);
                    }

                    if (row + 1 < board.rows())
                    {
                        addEdge(cell, cell + board.columns());
                    }
                }
            }

            for (std::size_t cell = 0; cell < m_colors.size(); ++cell)
            {
                for (std::size_t color = 0; color < m_flowers.size(); ++color)
                {
                    if (!m_flowers[color].empty())
                    {
                        m_colors[cell][color] = m_constraints.addVariable();
                    }
                }

                constrainCell(static_cast<int>(cell));
            }

            // A chosen edge joins cells of the same color. Its clauses do nothing when it isn't chosen.
            for (const auto edge : m_edges)
            {
                for (std::size_t color = 0; color < m_flowers.size(); ++color)
                {
                    if (!m_flowers[color].empty())
                    {
                        const int first  = m_colors[edge.first][color];
                        const int second = m_colors[edge.second][color];
                        m_constraints.addClause({-edge.variable, -first, second});
                        m_constraints.addClause({-edge.variable, first, -second});
                    }
                }
            }
        }

        FlowerSolveResult run(const FlowerSearchOptions& options, const std::stop_token stop)
        {
            const auto report = [&]()
            {
                options.progress(preview());
            };
            FlowerSolveResult result;
            result.status = m_constraints.solve(options, stop, options.progress ? report : std::function<void()>{});
            result.visitedNodes = m_constraints.nodes();
            if (result.status == FlowerSolveStatus::Solved)
            {
                // Follow from each flower to get the useful paths. Any separate loops can be ignored.
                result.paths = preview().paths;
                shortenPaths(result.paths);
                std::vector<bool> used(m_colors.size());
                for (std::size_t color = 0; color < m_flowers.size(); ++color)
                {
                    if (m_flowers[color].empty())
                    {
                        continue;
                    }

                    const auto& path = result.paths[color];
                    if (path.size() < 2 || path.front() != m_flowers[color][0] || path.back() != m_flowers[color][1])
                    {
                        result.status = FlowerSolveStatus::Error;
                        result.paths  = {};
                        return result;
                    }

                    for (const auto cell : path)
                    {
                        const int index = cell.row * m_board.columns() + cell.column;
                        if (used[index])
                        {
                            result.status = FlowerSolveStatus::Error;
                            result.paths  = {};
                            return result;
                        }

                        used[index] = true;
                    }
                }
            }

            return result;
        }

    private:
        void shortenPaths(FlowerPaths& paths) const
        {
            // Shorten one route at a time, treating the other colors' routes as walls.
            std::vector<int> owners(m_colors.size());
            for (std::size_t color = 0; color < paths.size(); ++color)
            {
                for (const auto cell : paths[color])
                {
                    owners[cell.row * m_board.columns() + cell.column] = static_cast<int>(color) + 1;
                }
            }

            // A shorter route frees space that another color may be able to use on the next pass.
            bool changed = true;
            while (changed)
            {
                changed = false;
                for (std::size_t color = 0; color < paths.size(); ++color)
                {
                    auto& path = paths[color];
                    if (path.empty())
                    {
                        continue;
                    }

                    const int start = path.front().row * m_board.columns() + path.front().column;
                    const int goal  = path.back().row * m_board.columns() + path.back().column;
                    std::vector<int> previous(m_colors.size(), -1);
                    std::vector<int> queue = {start};
                    previous[start]        = start;
                    for (std::size_t index = 0; index < queue.size() && previous[goal] < 0; ++index)
                    {
                        const int cell = queue[index];
                        for (const int edgeIndex : m_incident[cell])
                        {
                            const auto edge = m_edges[edgeIndex];
                            const int next  = edge.first == cell ? edge.second : edge.first;
                            if (previous[next] < 0 &&
                                (owners[next] == 0 || owners[next] == static_cast<int>(color) + 1))
                            {
                                previous[next] = cell;
                                queue.push_back(next);
                            }
                        }
                    }

                    if (previous[goal] < 0)
                    {
                        continue;
                    }

                    std::vector<FlowerCell> shorter;
                    for (int cell = goal; cell != start; cell = previous[cell])
                    {
                        shorter.push_back({cell / m_board.columns(), cell % m_board.columns()});
                    }

                    shorter.push_back(path.front());
                    if (shorter.size() < path.size())
                    {
                        for (const auto cell : path)
                        {
                            owners[cell.row * m_board.columns() + cell.column] = 0;
                        }

                        std::reverse(shorter.begin(), shorter.end());
                        path = std::move(shorter);
                        for (const auto cell : path)
                        {
                            owners[cell.row * m_board.columns() + cell.column] = static_cast<int>(color) + 1;
                        }

                        changed = true;
                    }
                }
            }
        }

        void addEdge(const int first, const int second)
        {
            const int index = static_cast<int>(m_edges.size());
            m_edges.push_back({first, second, m_constraints.addVariable()});
            m_incident[first].push_back(index);
            m_incident[second].push_back(index);
        }

        void constrainCell(const int cell)
        {
            const int flower     = m_board.colorAt({cell / m_board.columns(), cell % m_board.columns()});
            const auto& incident = m_incident[cell];
            std::vector<int> colors;
            for (const int variable : m_colors[cell])
            {
                if (variable != 0)
                {
                    colors.push_back(variable);
                }
            }

            for (std::size_t first = 0; first < colors.size(); ++first)
            {
                for (std::size_t second = first + 1; second < colors.size(); ++second)
                {
                    m_constraints.addClause({-colors[first], -colors[second]});
                }

                std::vector<int> used = {-colors[first]};
                for (const int edge : incident)
                {
                    used.push_back(m_edges[edge].variable);
                }

                m_constraints.addClause(std::move(used));
            }

            for (const int edge : incident)
            {
                auto colored = colors;
                colored.push_back(-m_edges[edge].variable);
                m_constraints.addClause(std::move(colored));
            }

            if (flower != 0)
            {
                m_constraints.addClause({m_colors[cell][flower - 1]});
            }

            // Flowers need one edge; other cells need zero or two. Forbid every other local pattern.
            for (unsigned int mask = 0; mask < (1U << incident.size()); ++mask)
            {
                const int degree = std::popcount(mask);
                if (flower != 0 ? degree == 1 : (degree == 0 || degree == 2))
                {
                    continue;
                }

                std::vector<int> forbidden;
                for (std::size_t index = 0; index < incident.size(); ++index)
                {
                    const int variable = m_edges[incident[index]].variable;
                    forbidden.push_back((mask & (1U << index)) != 0 ? -variable : variable);
                }

                m_constraints.addClause(std::move(forbidden));
            }
        }

        std::vector<FlowerCell> follow(const FlowerCell start, const FlowerCell target, const int color) const
        {
            std::vector<FlowerCell> path = {start};
            std::vector<bool> seen(m_colors.size());
            int current   = start.row * m_board.columns() + start.column;
            seen[current] = true;
            while (path.back() != target)
            {
                int next = -1;
                for (const int edgeIndex : m_incident[current])
                {
                    const auto edge    = m_edges[edgeIndex];
                    const int neighbor = edge.first == current ? edge.second : edge.first;
                    const int flower   = m_board.colorAt({neighbor / m_board.columns(), neighbor % m_board.columns()});
                    if (m_constraints.value(edge.variable) == 1 && !seen[neighbor] && (flower == 0 || flower == color))
                    {
                        next = neighbor;
                        break;
                    }
                }

                if (next < 0)
                {
                    break;
                }

                current       = next;
                seen[current] = true;
                path.push_back({current / m_board.columns(), current % m_board.columns()});
            }

            return path;
        }

        FlowerSearchPreview preview() const
        {
            FlowerSearchPreview result;
            result.visitedNodes = m_constraints.nodes();
            for (std::size_t color = 0; color < m_flowers.size(); ++color)
            {
                if (!m_flowers[color].empty())
                {
                    result.paths[color] = follow(m_flowers[color][0], m_flowers[color][1], static_cast<int>(color) + 1);
                    if (result.paths[color].back() != m_flowers[color][1])
                    {
                        // Show progress from both ends without drawing a made-up connection between them.
                        result.tails[color] =
                            follow(m_flowers[color][1], m_flowers[color][0], static_cast<int>(color) + 1);
                    }
                }
            }

            return result;
        }
    };
}

FlowerSolveResult solveFlower(const FlowerBoard& board, const FlowerSearchOptions& options, const std::stop_token stop)
{
    if (const auto error = board.validationError(); !error.empty())
    {
        FlowerSolveResult result;
        result.status  = FlowerSolveStatus::Invalid;
        result.message = error;
        return result;
    }

    return FlowerRouting(board).run(options, stop);
}
