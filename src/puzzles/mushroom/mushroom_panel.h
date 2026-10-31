#pragma once

#include "puzzles/mushroom/solver.h"

#include <array>

#include <wx/bmpbndl.h>
#include <wx/panel.h>

class wxButton;
class wxChoice;
class wxFlexGridSizer;
class wxStaticBox;
class wxStaticText;
class wxTextCtrl;
class wxToggleButton;

class MushroomPanel final : public wxPanel
{
private:
    MushroomBoard m_board = MushroomBoard::example();
    std::array<MushroomBoard, 3> m_savedBoards = {
        MushroomBoard::example(),
        MushroomBoard(MushroomVariant::Small),
        MushroomBoard(MushroomVariant::MiddleGaps)
    };

    MushroomBoard m_previewBoard;
    std::optional<MushroomSolution> m_solution;
    std::size_t m_previewStep = 0;
    bool m_previewing         = false;

    wxBitmapBundle m_thornsIcon;
    wxBitmapBundle m_mushroomIcon;

    std::array<wxToggleButton*, MushroomBoard::CellCount> m_cells{};
    std::array<wxStaticText*, MushroomBoard::CellCount> m_steps{};

    wxChoice* m_variantChoice     = nullptr;
    wxFlexGridSizer* m_boardGrid  = nullptr;
    wxFlexGridSizer* m_resultGrid = nullptr;
    wxStaticBox* m_boardBox       = nullptr;
    wxStaticBox* m_solutionBox    = nullptr;

    wxStaticText* m_boardHint      = nullptr;
    wxStaticText* m_boardStatus    = nullptr;
    wxStaticText* m_solutionStatus = nullptr;
    wxTextCtrl* m_coordinates      = nullptr;

    wxButton* m_clearButton   = nullptr;
    wxButton* m_allOnButton   = nullptr;
    wxButton* m_exampleButton = nullptr;
    wxButton* m_copyButton    = nullptr;

    wxButton* m_previewButton = nullptr;
    wxButton* m_nextButton    = nullptr;
    wxButton* m_editButton    = nullptr;

public:
    explicit MushroomPanel(wxWindow* parent);

private:
    void createControls();
    void rebuildGrids();
    void selectVariant();
    void setBoard(const MushroomBoard& board);
    void updateSolution();
    void refreshBoard();
    void startPreview();
    void advancePreview();
    void stopPreview();
    void copySolution();
};
