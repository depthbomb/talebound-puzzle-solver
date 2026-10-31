#include "puzzles/mushroom/mushroom_panel.h"

#include "puzzles/mushroom/icons.h"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/tglbtn.h>

namespace
{
    constexpr int CellWidth  = 44;
    constexpr int CellHeight = 34;

    wxStaticText* addHeading(wxWindow* parent, wxSizer* grid, const wxString& label)
    {
        auto* heading = new wxStaticText(parent, wxID_ANY, label);
        heading->SetFont(heading->GetFont().Bold());
        grid->Add(heading, 0, wxALIGN_CENTER);
        return heading;
    }

    wxString formatCoordinates(const MushroomBoard& board, const MushroomSolution& solution)
    {
        if (solution.empty())
        {
            return "No clicks needed. All cells are off.";
        }

        wxString text;
        for (int row = 0; row < board.rows(); ++row)
        {
            wxString columns;
            for (const auto cell : solution)
            {
                if (cell.row == row)
                {
                    if (!columns.empty())
                    {
                        columns += ", ";
                    }

                    columns += wxString::Format("%d", cell.column + 1);
                }
            }

            if (!columns.empty())
            {
                text += wxString::Format("Row %d: columns %s\r\n", row + 1, columns);
            }
        }

        return text;
    }
}

MushroomPanel::MushroomPanel(wxWindow* parent) : wxPanel(parent)
{
    SetName("mushroom-panel");
    createControls();
    updateSolution();
}

void MushroomPanel::createControls()
{
    const int gap  = FromDIP(12);
    m_thornsIcon   = wxBitmapBundle::FromSVG(MushroomThornsSvg, wxSize(24, 24));
    m_mushroomIcon = wxBitmapBundle::FromSVG(MushroomSvg, wxSize(24, 24));

    auto* layout       = new wxBoxSizer(wxVERTICAL);
    auto* variantSizer = new wxBoxSizer(wxHORIZONTAL);
    variantSizer->Add(new wxStaticText(this, wxID_ANY, "&Variant:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
    m_variantChoice = new wxChoice(this, wxID_ANY);
    m_variantChoice->SetName("puzzle-variant");
    for (std::size_t index = 0; index < m_savedBoards.size(); ++index)
    {
        m_variantChoice->Append(wxString::Format("%zu", index + 1));
    }

    m_variantChoice->SetSelection(0);
    variantSizer->Add(m_variantChoice);
    layout->Add(variantSizer, 0, wxALL, gap);

    auto* columns = new wxBoxSizer(wxHORIZONTAL);
    {
        auto* boardSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Starting grid");
        m_boardBox       = boardSizer->GetStaticBox();
        m_boardHint      = new wxStaticText(m_boardBox, wxID_ANY, "Click cells to set the puzzle. Solves as you edit.");
        boardSizer->Add(m_boardHint, 0, wxALL, gap);

        m_boardGrid = new wxFlexGridSizer(0, MushroomBoard::Columns + 1, FromDIP(4), FromDIP(4));
        boardSizer->Add(m_boardGrid, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxBOTTOM, gap);

        auto* editingButtons = new wxBoxSizer(wxHORIZONTAL);
        m_clearButton        = new wxButton(m_boardBox, wxID_CLEAR, "C&lear");
        m_allOnButton        = new wxButton(m_boardBox, wxID_ANY, "&All on");
        m_exampleButton      = new wxButton(m_boardBox, wxID_ANY, "&Example");
        m_clearButton->SetName("clear-board");
        m_allOnButton->SetName("all-on");
        m_exampleButton->SetName("load-example");
        m_exampleButton->SetToolTip("Load the example for variant 1.");
        editingButtons->Add(m_clearButton, 0, wxRIGHT, gap / 2);
        editingButtons->Add(m_allOnButton, 0, wxRIGHT, gap / 2);
        editingButtons->Add(m_exampleButton);
        boardSizer->Add(editingButtons, 0, wxLEFT | wxRIGHT | wxBOTTOM, gap);

        m_boardStatus =
            new wxStaticText(m_boardBox, wxID_ANY, "", wxDefaultPosition, FromDIP(wxSize(340, 40)), wxST_NO_AUTORESIZE);
        m_boardStatus->SetName("board-status");
        boardSizer->Add(m_boardStatus, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, gap);
        boardSizer->AddStretchSpacer();

        auto* previewButtons = new wxBoxSizer(wxHORIZONTAL);
        m_previewButton      = new wxButton(m_boardBox, wxID_ANY, "&Preview");
        m_nextButton         = new wxButton(m_boardBox, wxID_ANY, "&Next move");
        m_editButton         = new wxButton(m_boardBox, wxID_ANY, "&Edit");
        m_previewButton->SetName("preview-solution");
        m_nextButton->SetName("next-move");
        m_editButton->SetName("back-to-editing");
        m_previewButton->SetToolTip("Step through the solution without changing your starting grid.");
        m_editButton->SetToolTip("Return to your starting grid.");
        previewButtons->Add(m_previewButton, 0, wxRIGHT, gap / 2);
        previewButtons->Add(m_nextButton, 0, wxRIGHT, gap / 2);
        previewButtons->Add(m_editButton);
        boardSizer->Add(previewButtons, 0, wxLEFT | wxRIGHT | wxBOTTOM, gap);
        columns->Add(boardSizer, 1, wxEXPAND | wxRIGHT, gap);
    }

    {
        auto* solutionSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Solution");
        m_solutionBox       = solutionSizer->GetStaticBox();
        auto* hint          = new wxStaticText(m_solutionBox, wxID_ANY, "Click each numbered cell once, in any order.");
        solutionSizer->Add(hint, 0, wxALL, gap);

        m_resultGrid = new wxFlexGridSizer(0, MushroomBoard::Columns + 1, FromDIP(4), FromDIP(4));
        solutionSizer->Add(m_resultGrid, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxBOTTOM, gap);

        m_solutionStatus = new wxStaticText(
            m_solutionBox, wxID_ANY, "", wxDefaultPosition, FromDIP(wxSize(340, 22)), wxST_NO_AUTORESIZE);
        m_solutionStatus->SetName("solution-status");
        solutionSizer->Add(m_solutionStatus, 0, wxEXPAND | wxLEFT | wxRIGHT, gap);

        m_coordinates = new wxTextCtrl(
            m_solutionBox, wxID_ANY, "", wxDefaultPosition, FromDIP(wxSize(340, 106)), wxTE_MULTILINE | wxTE_READONLY);
        m_coordinates->SetName("solution-coordinates");
        solutionSizer->Add(m_coordinates, 1, wxEXPAND | wxALL, gap);

        m_copyButton = new wxButton(m_solutionBox, wxID_COPY, "&Copy solution");
        m_copyButton->SetName("copy-solution");
        solutionSizer->Add(m_copyButton, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, gap);
        columns->Add(solutionSizer, 1, wxEXPAND);
    }

    layout->Add(columns, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, gap);
    SetSizer(layout);
    rebuildGrids();

    m_variantChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&)
    {
        selectVariant();
    });
    m_clearButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        setBoard(MushroomBoard(m_board.variant()));
    });
    m_allOnButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        MushroomBoard board(m_board.variant());
        for (const auto cell : board.cells())
        {
            board.setCell(cell, true);
        }

        setBoard(board);
    });
    m_exampleButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        setBoard(MushroomBoard::example());
    });
    m_previewButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        startPreview();
    });
    m_nextButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        advancePreview();
    });
    m_editButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        stopPreview();
    });
    m_copyButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        copySolution();
    });
}

void MushroomPanel::rebuildGrids()
{
    m_boardGrid->Clear(true);
    m_resultGrid->Clear(true);
    m_cells.fill(nullptr);
    m_steps.fill(nullptr);
    m_boardGrid->SetCols(m_board.columns() + 1);
    m_resultGrid->SetCols(m_board.columns() + 1);

    addHeading(m_boardBox, m_boardGrid, "");
    addHeading(m_solutionBox, m_resultGrid, "");
    for (int column = 0; column < m_board.columns(); ++column)
    {
        const auto label = wxString::Format("%d", column + 1);
        addHeading(m_boardBox, m_boardGrid, label);
        addHeading(m_solutionBox, m_resultGrid, label);
    }

    for (int row = 0; row < m_board.rows(); ++row)
    {
        const auto label = wxString::Format("%d", row + 1);
        addHeading(m_boardBox, m_boardGrid, label);
        addHeading(m_solutionBox, m_resultGrid, label);
        for (int column = 0; column < m_board.columns(); ++column)
        {
            const MushroomCell cell = {row, column};
            if (!m_board.hasCell(cell))
            {
                // Leave matching gaps in both grids so their row and column labels still line up.
                const auto size = FromDIP(wxSize(CellWidth, CellHeight));
                m_boardGrid->Add(size.x, size.y);
                m_resultGrid->Add(size.x, size.y);
                continue;
            }

            const int index = row * MushroomBoard::Columns + column;
            auto* button    = new wxToggleButton(m_boardBox, wxID_ANY, "Mushroom (off)", wxDefaultPosition,
                   FromDIP(wxSize(CellWidth, CellHeight)), wxBU_NOTEXT);
            button->SetName(wxString::Format("cell-%d-%d", row + 1, column + 1));
            button->Bind(wxEVT_TOGGLEBUTTON, [this, cell, button](wxCommandEvent&)
            {
                if (!m_previewing)
                {
                    // Editing sets the starting state. Only a preview move toggles the neighbors.
                    m_board.setCell(cell, button->GetValue());
                    updateSolution();
                }
            });
            m_cells[index] = button;
            m_boardGrid->Add(button, 0, wxEXPAND);

            auto* tile = new wxPanel(
                m_solutionBox, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(CellWidth, CellHeight)), wxBORDER_SIMPLE);
            auto* number = new wxStaticText(
                tile, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER | wxST_NO_AUTORESIZE);
            number->SetMinSize(FromDIP(wxSize(38, 20)));
            number->SetFont(number->GetFont().Bold());
            number->SetName(wxString::Format("solution-%d-%d", row + 1, column + 1));
            auto* tileSizer = new wxBoxSizer(wxVERTICAL);
            tileSizer->AddStretchSpacer();
            tileSizer->Add(number, 0, wxALIGN_CENTER);
            tileSizer->AddStretchSpacer();
            tile->SetSizer(tileSizer);
            m_steps[index] = number;
            m_resultGrid->Add(tile, 0, wxEXPAND);
        }
    }

    Layout();
}

void MushroomPanel::selectVariant()
{
    // Switching layouts should keep the work already entered in each one.
    m_savedBoards[static_cast<std::size_t>(m_board.variant())] = m_board;
    setBoard(m_savedBoards[static_cast<std::size_t>(m_variantChoice->GetSelection())]);
}

void MushroomPanel::setBoard(const MushroomBoard& board)
{
    const bool shapeChanged = board.variant() != m_board.variant();

    m_board       = board;
    m_previewing  = false;
    m_previewStep = 0;
    if (shapeChanged)
    {
        rebuildGrids();
    }

    updateSolution();
}

void MushroomPanel::updateSolution()
{
    m_solution = solveMushroom(m_board);
    for (auto* step : m_steps)
    {
        if (step)
        {
            step->SetLabel("");
            step->SetToolTip("Do not click this cell.");
            step->GetParent()->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
            step->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
            step->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
            step->GetParent()->Refresh();
        }
    }

    if (m_solution)
    {
        for (std::size_t index = 0; index < m_solution->size(); ++index)
        {
            const auto cell = (*m_solution)[index];
            auto* step      = m_steps[cell.row * MushroomBoard::Columns + cell.column];
            step->SetLabel(wxString::Format("%zu", index + 1));
            step->GetParent()->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT));
            step->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT));
            step->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
            step->GetParent()->Refresh();
            step->SetToolTip(wxString::Format("Move %zu: row %d, column %d", index + 1, cell.row + 1, cell.column + 1));
        }

        m_solutionStatus->SetLabel(
            wxString::Format("%zu %s", m_solution->size(), m_solution->size() == 1 ? "click" : "clicks"));
        m_coordinates->ChangeValue(formatCoordinates(m_board, *m_solution));
    }
    else
    {
        m_solutionStatus->SetLabel("No solution for this starting grid.");
        m_coordinates->Clear();
    }

    m_copyButton->Enable(m_solution.has_value());
    refreshBoard();
}

void MushroomPanel::refreshBoard()
{
    const auto& displayed = m_previewing ? m_previewBoard : m_board;
    for (const auto cell : displayed.cells())
    {
        auto* button  = m_cells[cell.row * MushroomBoard::Columns + cell.column];
        const bool on = displayed.isOn(cell);
        button->SetValue(on);
        const auto& icon = on ? m_thornsIcon : m_mushroomIcon;
        button->SetBitmap(icon);
        button->SetBitmapPressed(icon);
        // Preview cells are disabled, but their state should stay just as easy to read.
        button->SetBitmapDisabled(icon);
        button->SetLabel(on ? "Thorns (on)" : "Mushroom (off)");
        button->SetToolTip(
            wxString::Format("Row %d, column %d: %s", cell.row + 1, cell.column + 1, button->GetLabel()));
        button->Enable(!m_previewing);
    }

    m_boardBox->SetLabel(m_previewing ? "Preview grid" : "Starting grid");
    m_boardHint->SetLabel(m_previewing ? "Preview only. Edit restores your starting grid."
                                       : "Click cells to set the puzzle. Solves as you edit.");
    m_clearButton->Enable(!m_previewing);
    m_allOnButton->Enable(!m_previewing);
    m_exampleButton->Enable(!m_previewing && m_board.variant() == MushroomVariant::Standard);
    m_variantChoice->Enable(!m_previewing);
    m_previewButton->Enable(!m_previewing && m_solution && !m_solution->empty());
    m_nextButton->Enable(m_previewing && m_solution && m_previewStep < m_solution->size());
    m_editButton->Enable(m_previewing);
    m_previewButton->Show(!m_previewing);
    m_nextButton->Show(m_previewing);
    m_editButton->Show(m_previewing);
    Layout();

    if (!m_previewing)
    {
        m_boardStatus->SetLabel(wxString::Format("%zu on", displayed.countOn()));
    }
    else if (m_solution && m_previewStep < m_solution->size())
    {
        const auto next = (*m_solution)[m_previewStep];
        m_boardStatus->SetLabel(wxString::Format("Move %zu of %zu   |   %zu on\n"
                                                 "Next: row %d, column %d",
            m_previewStep, m_solution->size(), displayed.countOn(), next.row + 1, next.column + 1));
    }
    else
    {
        m_boardStatus->SetLabel(
            displayed.isClear() ? "Preview complete. All cells are off." : "Preview did not clear the grid.");
    }
}

void MushroomPanel::startPreview()
{
    if (!m_solution || m_solution->empty() || m_previewing)
    {
        return;
    }

    // Play through a copy so Edit can always return to the untouched starting grid.
    m_previewBoard = m_board;
    m_previewStep  = 0;
    m_previewing   = true;
    refreshBoard();
    m_nextButton->SetFocus();
}

void MushroomPanel::advancePreview()
{
    if (!m_previewing || !m_solution || m_previewStep >= m_solution->size())
    {
        return;
    }

    m_previewBoard.applyClick((*m_solution)[m_previewStep]);
    ++m_previewStep;
    refreshBoard();

    if (m_previewStep == m_solution->size())
    {
        m_editButton->SetFocus();
    }
}

void MushroomPanel::stopPreview()
{
    m_previewing  = false;
    m_previewStep = 0;
    refreshBoard();

    const auto first = m_board.cells().front();
    m_cells[first.row * MushroomBoard::Columns + first.column]->SetFocus();
}

void MushroomPanel::copySolution()
{
    if (!m_solution)
    {
        return;
    }

    if (!wxTheClipboard->Open())
    {
        m_solutionStatus->SetLabel("Clipboard busy. Try again.");
        return;
    }

    const auto variant  = static_cast<std::size_t>(m_board.variant()) + 1;
    const wxString text = wxString::Format("Mushroom (variant %zu)\r\n"
                                           "Click each listed cell once, in any order.\r\n\r\n",
                              variant) +
                          formatCoordinates(m_board, *m_solution);
    const bool copied = wxTheClipboard->SetData(new wxTextDataObject(text));
    if (copied)
    {
        wxTheClipboard->Flush();
    }

    wxTheClipboard->Close();
    m_solutionStatus->SetLabel(copied ? "Solution copied." : "Could not copy. Try again.");
}
