#pragma once

#include "puzzles/flower/solver.h"

#include <wx/scrolwin.h>

wxDECLARE_EVENT(wxEVT_FLOWER_EDIT, wxCommandEvent);

wxColour flowerColor(int color);
wxString flowerColorName(int color);

class FlowerGrid final : public wxScrolledWindow
{
private:
    FlowerBoard m_board;
    FlowerPaths m_paths;
    FlowerPaths m_previewTails;
    FlowerCell m_selected = {0, 0};
    bool m_editable       = true;

public:
    explicit FlowerGrid(wxWindow* parent);

    void setBoard(const FlowerBoard& board);
    void setPaths(const FlowerPaths& paths);
    void setPreview(const FlowerSearchPreview& preview);
    void setEditable(bool editable);
    const FlowerBoard& board() const;
    const FlowerPaths& paths() const;

private:
    void paint(wxPaintEvent& event);
    void mouseDown(wxMouseEvent& event);
    void keyDown(wxKeyEvent& event);
    void editSelected(bool erase);
    void drawPaths(wxDC& dc, const FlowerPaths& paths) const;
    wxPoint cellCenter(FlowerCell cell) const;
};
