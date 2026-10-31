#pragma once

#include "puzzles/tetris/pieces.h"

#include <wx/panel.h>

class TetrisGrid final : public wxPanel
{
private:
    TetrisBoard m_board;
    std::vector<TetrisPlacement> m_placements;

public:
    explicit TetrisGrid(wxWindow* parent);

    void setVariant(TetrisVariant variant);
    const TetrisBoard& board() const;
    void setPlacements(const std::vector<TetrisPlacement>& placements);
    const std::vector<TetrisPlacement>& placements() const;

private:
    void paint(wxPaintEvent& event);
};
