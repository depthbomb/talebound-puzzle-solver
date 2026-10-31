#pragma once

#include "puzzles/tetris/solver.h"

#include <thread>

#include <wx/panel.h>

class TetrisGrid;
class wxButton;
class wxSpinCtrl;
class wxStaticText;
class wxTextCtrl;
class wxThreadEvent;

class TetrisPanel final : public wxPanel
{
private:
    std::jthread m_worker;
    bool m_solving = false;

    std::array<wxSpinCtrl*, static_cast<std::size_t>(TetrisPiece::Count)> m_counts = {};

    TetrisGrid* m_grid         = nullptr;
    wxStaticText* m_total      = nullptr;
    wxStaticText* m_status     = nullptr;
    wxTextCtrl* m_instructions = nullptr;

    wxButton* m_solve  = nullptr;
    wxButton* m_cancel = nullptr;
    wxButton* m_clear  = nullptr;
    wxButton* m_copy   = nullptr;

public:
    explicit TetrisPanel(wxWindow* parent);
    ~TetrisPanel() override;

private:
    void createControls();
    TetrisInventory inventory() const;
    void inventoryChanged();
    void setSolving(bool solving);
    void startSolve();
    void finishSolve(wxThreadEvent& event);
    void copySolution();
};
