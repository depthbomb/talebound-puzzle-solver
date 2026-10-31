#include "puzzles/flower/flower_grid.h"

#include <algorithm>
#include <array>

#include <wx/dcbuffer.h>

wxDEFINE_EVENT(wxEVT_FLOWER_EDIT, wxCommandEvent);

namespace
{
    constexpr int CellSize  = 44;
    constexpr int LeftInset = 34;
    constexpr int TopInset  = 30;

    const std::array<wxColour, FlowerBoard::ColorCount> Colors = {wxColour(255, 255, 255), wxColour(196, 43, 52),
        wxColour(38, 106, 194), wxColour(225, 112, 28), wxColour(232, 116, 169), wxColour(145, 99, 226),
        wxColour(97, 41, 133)};
    const std::array<wxString, FlowerBoard::ColorCount> Names  = {
        "White", "Red", "Blue", "Orange", "Pink", "Violet", "Purple"};

    void centeredText(wxDC& dc, const wxString& text, const wxPoint center)
    {
        const auto size = dc.GetTextExtent(text);
        dc.DrawText(text, center.x - size.x / 2, center.y - size.y / 2);
    }
}

wxColour flowerColor(const int color)
{
    return Colors.at(static_cast<std::size_t>(color - 1));
}

wxString flowerColorName(const int color)
{
    return Names.at(static_cast<std::size_t>(color - 1));
}

FlowerGrid::FlowerGrid(wxWindow* parent) : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition,
    wxDefaultSize, wxBORDER_SUNKEN | wxHSCROLL | wxVSCROLL | wxWANTS_CHARS)
{
    SetName("flower-grid");
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(FromDIP(wxSize(400, 350)));
    SetScrollRate(FromDIP(20), FromDIP(20));
    SetToolTip(
        "Click to place the selected color. Right-click or press Delete to erase. Arrow keys move; Space places.");
    Bind(wxEVT_PAINT, &FlowerGrid::paint, this);
    Bind(wxEVT_LEFT_DOWN, &FlowerGrid::mouseDown, this);
    Bind(wxEVT_RIGHT_DOWN, &FlowerGrid::mouseDown, this);
    Bind(wxEVT_KEY_DOWN, &FlowerGrid::keyDown, this);
    Bind(wxEVT_SET_FOCUS,
        [this](wxFocusEvent& event)
        {
            Refresh();
            event.Skip();
        });
    Bind(wxEVT_KILL_FOCUS,
        [this](wxFocusEvent& event)
        {
            Refresh();
            event.Skip();
        });
    setBoard(m_board);
}

void FlowerGrid::setBoard(const FlowerBoard& board)
{
    m_board           = board;
    m_paths           = {};
    m_previewTails    = {};
    m_selected.row    = std::min(m_selected.row, board.rows() - 1);
    m_selected.column = std::min(m_selected.column, board.columns() - 1);
    SetVirtualSize(
        FromDIP(wxSize(LeftInset + board.columns() * CellSize + 12, TopInset + board.rows() * CellSize + 12)));
    Refresh();
}

void FlowerGrid::setPaths(const FlowerPaths& paths)
{
    m_paths        = paths;
    m_previewTails = {};
    Refresh();
}

void FlowerGrid::setPreview(const FlowerSearchPreview& preview)
{
    m_paths        = preview.paths;
    m_previewTails = preview.tails;
    Refresh();
}

void FlowerGrid::setEditable(const bool editable)
{
    m_editable = editable;
}

const FlowerBoard& FlowerGrid::board() const
{
    return m_board;
}

const FlowerPaths& FlowerGrid::paths() const
{
    return m_paths;
}

wxPoint FlowerGrid::cellCenter(const FlowerCell cell) const
{
    return FromDIP(
        wxPoint(LeftInset + cell.column * CellSize + CellSize / 2, TopInset + cell.row * CellSize + CellSize / 2));
}

void FlowerGrid::paint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    PrepareDC(dc);
    dc.SetBackground(wxBrush(wxColour(248, 248, 248)));
    dc.Clear();
    dc.SetFont(GetFont().Bold());
    dc.SetTextForeground(*wxBLACK);
    for (int column = 0; column < m_board.columns(); ++column)
    {
        centeredText(dc, wxString::Format("%d", column + 1), wxPoint(cellCenter({0, column}).x, FromDIP(TopInset / 2)));
    }

    for (int row = 0; row < m_board.rows(); ++row)
    {
        centeredText(dc, wxString::Format("%d", row + 1), wxPoint(FromDIP(LeftInset / 2), cellCenter({row, 0}).y));
        for (int column = 0; column < m_board.columns(); ++column)
        {
            dc.SetPen(wxPen(wxColour(185, 185, 185)));
            dc.SetBrush(*wxWHITE_BRUSH);
            dc.DrawRectangle(FromDIP(LeftInset + column * CellSize), FromDIP(TopInset + row * CellSize),
                FromDIP(CellSize), FromDIP(CellSize));
        }
    }

    dc.SetFont(GetFont());
    // Paint routes underneath the flowers so their endpoints stay visible.
    drawPaths(dc, m_paths);
    drawPaths(dc, m_previewTails);

    const auto flowers = m_board.flowers();
    for (int color = 1; color <= FlowerBoard::ColorCount; ++color)
    {
        for (const auto cell : flowers[color - 1])
        {
            const auto center = cellCenter(cell);
            dc.SetPen(wxPen(wxColour(90, 90, 90), FromDIP(1)));
            dc.SetBrush(wxBrush(flowerColor(color)));
            for (const auto offset : std::array<wxPoint, 4>{{{-7, 0}, {7, 0}, {0, -7}, {0, 7}}})
            {
                dc.DrawCircle(center + FromDIP(offset), FromDIP(9));
            }

            dc.SetBrush(*wxWHITE_BRUSH);
            dc.DrawCircle(center, FromDIP(8));
            centeredText(dc, wxString::Format("%d", color), center);
        }
    }

    if (HasFocus())
    {
        dc.SetPen(wxPen(*wxBLACK, FromDIP(1), wxPENSTYLE_DOT));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(FromDIP(LeftInset + m_selected.column * CellSize + 2),
            FromDIP(TopInset + m_selected.row * CellSize + 2), FromDIP(CellSize - 4), FromDIP(CellSize - 4));
    }
}

void FlowerGrid::drawPaths(wxDC& dc, const FlowerPaths& paths) const
{
    for (int color = 1; color <= FlowerBoard::ColorCount; ++color)
    {
        const auto& path = paths[color - 1];
        wxPen pen(flowerColor(color), FromDIP(9));
        pen.SetCap(wxCAP_ROUND);
        pen.SetJoin(wxJOIN_ROUND);
        if (color == 1)
        {
            // White needs a darker edge to stand out against the empty cells.
            wxPen outline(wxColour(90, 90, 90), FromDIP(12));
            outline.SetCap(wxCAP_ROUND);
            outline.SetJoin(wxJOIN_ROUND);
            dc.SetPen(outline);
            for (std::size_t index = 1; index < path.size(); ++index)
            {
                dc.DrawLine(cellCenter(path[index - 1]), cellCenter(path[index]));
            }
        }

        dc.SetPen(pen);
        for (std::size_t index = 1; index < path.size(); ++index)
        {
            dc.DrawLine(cellCenter(path[index - 1]), cellCenter(path[index]));
        }

        for (const auto cell : path)
        {
            if (m_board.colorAt(cell) == 0)
            {
                const auto center = cellCenter(cell);
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.SetBrush(*wxWHITE_BRUSH);
                dc.DrawCircle(center, FromDIP(8));
                centeredText(dc, wxString::Format("%d", color), center);
            }
        }
    }
}

void FlowerGrid::mouseDown(wxMouseEvent& event)
{
    int x = 0;
    int y = 0;
    // Mouse coordinates are in the visible window; cells live in the scrolled canvas.
    CalcUnscrolledPosition(event.GetX(), event.GetY(), &x, &y);
    x -= FromDIP(LeftInset);
    y -= FromDIP(TopInset);
    if (x < 0 || y < 0 || x >= FromDIP(m_board.columns() * CellSize) || y >= FromDIP(m_board.rows() * CellSize))
    {
        return;
    }

    m_selected = {y / FromDIP(CellSize), x / FromDIP(CellSize)};
    SetFocus();
    editSelected(event.RightDown());
    Refresh();
}

void FlowerGrid::keyDown(wxKeyEvent& event)
{
    switch (event.GetKeyCode())
    {
    case WXK_LEFT:
        m_selected.column = std::max(0, m_selected.column - 1);
        break;
    case WXK_RIGHT:
        m_selected.column = std::min(m_board.columns() - 1, m_selected.column + 1);
        break;
    case WXK_UP:
        m_selected.row = std::max(0, m_selected.row - 1);
        break;
    case WXK_DOWN:
        m_selected.row = std::min(m_board.rows() - 1, m_selected.row + 1);
        break;
    case WXK_SPACE:
        editSelected(false);
        break;
    case WXK_DELETE:
    case WXK_BACK:
        editSelected(true);
        break;
    default:
        event.Skip();
        return;
    }

    const auto center = cellCenter(m_selected);
    Scroll(std::max(0, center.x - GetClientSize().x / 2) / FromDIP(20),
        std::max(0, center.y - GetClientSize().y / 2) / FromDIP(20));
    Refresh();
}

void FlowerGrid::editSelected(const bool erase)
{
    if (!m_editable)
    {
        return;
    }

    wxCommandEvent event(wxEVT_FLOWER_EDIT, GetId());
    event.SetEventObject(this);
    event.SetInt(m_selected.row * m_board.columns() + m_selected.column);
    event.SetExtraLong(erase ? 1 : 0);
    ProcessWindowEvent(event);
}
