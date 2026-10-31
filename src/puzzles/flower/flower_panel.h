#pragma once

#include "puzzles/flower/solver.h"

#include <mutex>
#include <optional>
#include <thread>

#include <wx/panel.h>
#include <wx/timer.h>

class FlowerGrid;
class wxButton;
class wxChoice;
class wxSpinCtrl;
class wxStaticText;
class wxTextCtrl;
class wxThreadEvent;

class FlowerPanel final : public wxPanel
{
private:
    FlowerBoard m_board;
    std::jthread m_worker;
    std::mutex m_previewMutex;
    std::optional<FlowerSearchPreview> m_pendingPreview;
    wxTimer m_previewTimer;
    bool m_solving = false;

    FlowerGrid* m_grid    = nullptr;
    wxSpinCtrl* m_rows    = nullptr;
    wxSpinCtrl* m_columns = nullptr;
    wxSpinCtrl* m_seconds = nullptr;
    wxChoice* m_color     = nullptr;

    wxButton* m_resize = nullptr;
    wxButton* m_clear  = nullptr;
    wxButton* m_solve  = nullptr;
    wxButton* m_cancel = nullptr;
    wxButton* m_copy   = nullptr;

    wxStaticText* m_status     = nullptr;
    wxTextCtrl* m_instructions = nullptr;

public:
    explicit FlowerPanel(wxWindow* parent);
    ~FlowerPanel() override;

private:
    void createControls();
    void changeCell(FlowerCell cell, bool erase);
    void refreshBoard();
    void resizeBoard();
    void startSolve();
    void finishSolve(wxThreadEvent& event);
    void showPreview();
    void setSolving(bool solving);
    void copySolution();
};
