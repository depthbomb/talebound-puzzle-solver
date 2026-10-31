#include "puzzles/tetris/tetris_grid.h"

#include <algorithm>

#include <wx/dcbuffer.h>
#include <wx/settings.h>

namespace
{
    const std::array<wxColour, TetrisRequiredPieces> Colors = {wxColour(158, 203, 244), wxColour(255, 200, 143),
        wxColour(168, 220, 175), wxColour(222, 179, 232), wxColour(248, 177, 183), wxColour(246, 225, 136),
        wxColour(151, 220, 220), wxColour(198, 196, 245)};

    void centeredText(wxDC& dc, const wxString& text, const wxPoint center)
    {
        const auto size = dc.GetTextExtent(text);
        dc.DrawText(text, center.x - size.x / 2, center.y - size.y / 2);
    }
}

TetrisGrid::TetrisGrid(wxWindow* parent) : wxPanel(parent)
{
    SetName("tetris-grid");
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(FromDIP(wxSize(430, 224)));
    SetToolTip("Each letter is one piece. Match its four cells to the coordinates below.");
    Bind(wxEVT_PAINT, &TetrisGrid::paint, this);
    Bind(wxEVT_SIZE,
        [this](wxSizeEvent& event)
        {
            Refresh();
            event.Skip();
        });
}

void TetrisGrid::setPlacements(const std::vector<TetrisPlacement>& placements)
{
    m_placements = placements;
    Refresh();
}

const std::vector<TetrisPlacement>& TetrisGrid::placements() const
{
    return m_placements;
}

void TetrisGrid::paint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    dc.SetFont(GetFont().Bold());
    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    const auto size  = GetClientSize();
    const int margin = FromDIP(32);
    const int cell   = std::max(
        1, std::min((size.x - margin - FromDIP(8)) / TetrisColumns, (size.y - margin - FromDIP(8)) / TetrisRows));
    const int left = margin + (size.x - margin - cell * TetrisColumns) / 2;
    const int top  = margin + (size.y - margin - cell * TetrisRows) / 2;
    std::array<int, TetrisCellCount> owners;
    owners.fill(-1);
    for (std::size_t index = 0; index < m_placements.size(); ++index)
    {
        for (const auto position : m_placements[index].cells)
        {
            owners.at(position.row * TetrisColumns + position.column) = static_cast<int>(index);
        }
    }

    for (int column = 0; column < TetrisColumns; ++column)
    {
        centeredText(dc, wxString::Format("%d", column + 1), {left + column * cell + cell / 2, top - margin / 2});
    }

    for (int row = 0; row < TetrisRows; ++row)
    {
        centeredText(dc, wxString::Format("%d", row + 1), {left - margin / 2, top + row * cell + cell / 2});
        for (int column = 0; column < TetrisColumns; ++column)
        {
            const int owner = owners[row * TetrisColumns + column];
            const int x     = left + column * cell;
            const int y     = top + row * cell;
            dc.SetPen(wxPen(wxColour(190, 190, 190)));
            dc.SetBrush(wxBrush(owner < 0 ? *wxWHITE : Colors.at(owner)));
            dc.DrawRectangle(x, y, cell, cell);
            if (owner >= 0)
            {
                dc.SetTextForeground(*wxBLACK);
                centeredText(dc, wxString::Format("%c", 'A' + owner), {x + cell / 2, y + cell / 2});
                dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
            }
        }
    }

    // Compare piece instances, not shapes, so two touching squares still have a visible boundary.
    dc.SetPen(wxPen(wxColour(55, 55, 55), FromDIP(2)));
    for (int row = 0; row < TetrisRows; ++row)
    {
        for (int column = 0; column < TetrisColumns; ++column)
        {
            const int index = row * TetrisColumns + column;
            const int x     = left + column * cell;
            const int y     = top + row * cell;
            if (row == 0 || owners[index] != owners[index - TetrisColumns])
            {
                dc.DrawLine(x, y, x + cell, y);
            }

            if (column == 0 || owners[index] != owners[index - 1])
            {
                dc.DrawLine(x, y, x, y + cell);
            }
        }
    }

    dc.DrawLine(left, top + TetrisRows * cell, left + TetrisColumns * cell, top + TetrisRows * cell);
    dc.DrawLine(left + TetrisColumns * cell, top, left + TetrisColumns * cell, top + TetrisRows * cell);
}
