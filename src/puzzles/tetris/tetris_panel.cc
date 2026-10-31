#include "puzzles/tetris/tetris_panel.h"

#include "puzzles/tetris/tetris_grid.h"

#include <algorithm>
#include <exception>
#include <numeric>

#include <wx/button.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/thread.h>

namespace
{
    class PiecePreview final : public wxPanel
    {
    private:
        const TetrisPiece m_piece;

    public:
        PiecePreview(wxWindow* parent, const TetrisPiece piece) : wxPanel(parent), m_piece(piece)
        {
            int rows = 0;
            for (const auto cell : tetrisBaseShape(piece))
            {
                rows = std::max(rows, cell.row + 1);
            }

            SetName(wxString::Format("tetris-preview-%d", static_cast<int>(piece)));
            SetMinSize(FromDIP(wxSize(62, 8 + rows * 16)));
            SetBackgroundStyle(wxBG_STYLE_PAINT);
            Bind(wxEVT_PAINT,
                [this](wxPaintEvent&)
                {
                    wxAutoBufferedPaintDC dc(this);
                    dc.SetBackground(wxBrush(GetBackgroundColour()));
                    dc.Clear();
                    dc.SetPen(wxPen(wxColour(65, 80, 100)));
                    dc.SetBrush(wxBrush(wxColour(165, 201, 235)));
                    for (const auto cell : tetrisBaseShape(m_piece))
                    {
                        dc.DrawRectangle(
                            FromDIP(5 + cell.column * 16), FromDIP(4 + cell.row * 16), FromDIP(16), FromDIP(16));
                    }
                });
        }
    };

    wxString formatPlacements(const std::vector<TetrisPlacement>& placements)
    {
        wxString text = "Coordinates: (row, column)";
        for (std::size_t index = 0; index < placements.size(); ++index)
        {
            const auto& placement = placements[index];
            text += wxString::Format(
                "\n%c - %s: ", 'A' + static_cast<int>(index), wxString::FromUTF8(tetrisPieceName(placement.piece)));
            for (std::size_t cell = 0; cell < placement.cells.size(); ++cell)
            {
                if (cell != 0)
                {
                    text += ", ";
                }

                text += wxString::Format("(%d, %d)", placement.cells[cell].row + 1, placement.cells[cell].column + 1);
            }
        }

        return text;
    }
}

TetrisPanel::TetrisPanel(wxWindow* parent) : wxPanel(parent)
{
    SetName("tetris-panel");
    createControls();
    Bind(wxEVT_THREAD, &TetrisPanel::finishSolve, this);
    inventoryChanged();
}

TetrisPanel::~TetrisPanel()
{
    // Finish the worker while its event target and child controls are still alive.
    m_worker.request_stop();
    if (m_worker.joinable())
    {
        m_worker.join();
    }
}

void TetrisPanel::createControls()
{
    const int gap = FromDIP(12);
    auto* layout  = new wxBoxSizer(wxVERTICAL);
    auto* content = new wxBoxSizer(wxHORIZONTAL);
    {
        auto* inventoryBox = new wxStaticBoxSizer(wxVERTICAL, this, "Pieces");
        auto* box          = inventoryBox->GetStaticBox();
        auto* counts       = new wxFlexGridSizer(3, gap / 2, gap / 2);
        for (std::size_t index = 0; index < m_counts.size(); ++index)
        {
            const auto piece = static_cast<TetrisPiece>(index);
            counts->Add(new PiecePreview(box, piece), 0, wxALIGN_CENTER_VERTICAL);
            counts->Add(new wxStaticText(box, wxID_ANY, wxString::FromUTF8(tetrisPieceName(piece))), 0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
            auto* count = new wxSpinCtrl(box, wxID_ANY, "0", wxDefaultPosition, FromDIP(wxSize(60, -1)),
                wxSP_ARROW_KEYS, 0, TetrisRequiredPieces, 0);
            count->SetName(wxString::Format("tetris-count-%zu", index));
            count->SetToolTip(wxString::Format("Number of %s pieces", wxString::FromUTF8(tetrisPieceName(piece))));
            m_counts[index] = count;
            counts->Add(count, 0, wxALIGN_CENTER_VERTICAL);
        }

        inventoryBox->Add(counts, 0, wxALL, gap);
        m_total = new wxStaticText(box, wxID_ANY, "");
        m_total->SetName("tetris-total");
        inventoryBox->Add(m_total, 0, wxLEFT | wxRIGHT | wxBOTTOM, gap);
        inventoryBox->AddStretchSpacer();
        m_clear = new wxButton(box, wxID_ANY, "C&lear");
        m_clear->SetName("tetris-clear");
        inventoryBox->Add(m_clear, 0, wxLEFT | wxRIGHT | wxBOTTOM, gap);
        content->Add(inventoryBox, 0, wxEXPAND | wxRIGHT, gap);
    }

    {
        auto* solution    = new wxStaticBoxSizer(wxVERTICAL, this, "Solution");
        auto* solutionBox = solution->GetStaticBox();
        m_grid            = new TetrisGrid(solutionBox);
        solution->Add(m_grid, 1, wxEXPAND | wxALL, gap);
        m_status = new wxStaticText(
            solutionBox, wxID_ANY, "", wxDefaultPosition, FromDIP(wxSize(430, 40)), wxST_NO_AUTORESIZE);
        m_status->SetName("tetris-status");
        solution->Add(m_status, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, gap);
        m_instructions = new wxTextCtrl(
            solutionBox, wxID_ANY, "", wxDefaultPosition, FromDIP(wxSize(430, 150)), wxTE_MULTILINE | wxTE_READONLY);
        m_instructions->SetName("tetris-instructions");
        solution->Add(m_instructions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, gap);

        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        m_solve       = new wxButton(solutionBox, wxID_ANY, "&Solve");
        m_cancel      = new wxButton(solutionBox, wxID_ANY, "Cancel");
        m_copy        = new wxButton(solutionBox, wxID_ANY, "&Copy solution");
        m_solve->SetName("tetris-solve");
        m_cancel->SetName("tetris-cancel");
        m_copy->SetName("tetris-copy");
        actions->Add(m_solve, 0, wxRIGHT, gap / 2);
        actions->Add(m_cancel, 0, wxRIGHT, gap / 2);
        actions->AddStretchSpacer();
        actions->Add(m_copy);
        solution->Add(actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, gap);
        content->Add(solution, 1, wxEXPAND);
    }

    layout->Add(content, 1, wxEXPAND | wxALL, gap);
    SetSizer(layout);

    for (auto* count : m_counts)
    {
        count->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent&)
        {
            inventoryChanged();
        });
        count->Bind(wxEVT_TEXT, [this](wxCommandEvent&)
        {
            inventoryChanged();
        });
    }

    m_clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        for (auto* count : m_counts)
        {
            count->SetValue(0);
        }

        inventoryChanged();
    });
    m_solve->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        startSolve();
    });
    m_cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        m_worker.request_stop();
        m_cancel->Disable();
        m_status->SetLabel("Cancelling...");
    });
    m_copy->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        copySolution();
    });
}

TetrisInventory TetrisPanel::inventory() const
{
    TetrisInventory result = {};
    for (std::size_t index = 0; index < m_counts.size(); ++index)
    {
        result[index] = m_counts[index]->GetValue();
    }

    return result;
}

void TetrisPanel::inventoryChanged()
{
    if (m_solving)
    {
        return;
    }

    const auto pieces = inventory();
    const int total   = std::accumulate(pieces.begin(), pieces.end(), 0);
    m_total->SetLabel(wxString::Format("%d / 8 pieces", total));
    m_grid->setPlacements({});
    m_instructions->Clear();
    m_copy->Disable();
    const int difference = TetrisRequiredPieces - total;
    if (difference == 0)
    {
        m_status->SetLabel("Ready.");
    }
    else
    {
        const int count = difference > 0 ? difference : -difference;
        m_status->SetLabel(
            wxString::Format("%s %d %s.", difference > 0 ? "Add" : "Remove", count, count == 1 ? "piece" : "pieces"));
    }
    setSolving(false);
}

void TetrisPanel::setSolving(const bool solving)
{
    m_solving = solving;
    for (auto* count : m_counts)
    {
        count->Enable(!solving);
    }

    m_clear->Enable(!solving);
    m_solve->Enable(!solving && validateTetrisInventory(inventory()).empty());
    m_cancel->Enable(solving);
    m_cancel->Show(solving);
    Layout();
}

void TetrisPanel::startSolve()
{
    if (m_solving)
    {
        return;
    }

    const auto pieces = inventory();
    if (const auto error = validateTetrisInventory(pieces); !error.empty())
    {
        m_status->SetLabel(wxString::FromUTF8(error));
        return;
    }

    m_grid->setPlacements({});
    m_instructions->Clear();
    m_copy->Disable();
    m_status->SetLabel("Searching...");
    setSolving(true);

    // Search the captured inventory in the background, then hand the result to the UI thread.
    m_worker = std::jthread(
        [this, pieces](const std::stop_token stop)
        {
            TetrisSolveResult result;
            try
            {
                result = solveTetris(pieces, {}, stop);
            }
            catch (const std::exception&)
            {
                result.status = TetrisSolveStatus::Error;
            }

            auto* event = new wxThreadEvent(wxEVT_THREAD);
            event->SetPayload(std::move(result));
            wxQueueEvent(this, event);
        });
}

void TetrisPanel::finishSolve(wxThreadEvent& event)
{
    if (m_worker.joinable())
    {
        m_worker.join();
    }

    const auto result = event.GetPayload<TetrisSolveResult>();
    setSolving(false);
    switch (result.status)
    {
    case TetrisSolveStatus::Solved:
        m_grid->setPlacements(result.placements);
        m_instructions->ChangeValue(formatPlacements(result.placements));
        m_copy->Enable();
        m_status->SetLabel("Solved. Each letter is one piece.");
        break;
    case TetrisSolveStatus::NoSolution:
        m_status->SetLabel("No solution. These pieces cannot fill the grid.");
        break;
    case TetrisSolveStatus::Invalid:
        m_status->SetLabel(wxString::FromUTF8(result.message));
        break;
    case TetrisSolveStatus::Cancelled:
        m_status->SetLabel("Search cancelled.");
        break;
    case TetrisSolveStatus::LimitReached:
        m_status->SetLabel("Search limit reached. A solution may still exist.");
        break;
    case TetrisSolveStatus::Error:
        m_status->SetLabel("Search failed. Try again.");
        break;
    }
}

void TetrisPanel::copySolution()
{
    if (!wxTheClipboard->Open())
    {
        m_status->SetLabel("Clipboard busy. Try again.");
        return;
    }

    const bool copied = wxTheClipboard->SetData(new wxTextDataObject("Tetris (4 x 8)\n" + m_instructions->GetValue()));
    if (copied)
    {
        wxTheClipboard->Flush();
    }

    wxTheClipboard->Close();
    m_status->SetLabel(copied ? "Solution copied." : "Could not copy. Try again.");
}
