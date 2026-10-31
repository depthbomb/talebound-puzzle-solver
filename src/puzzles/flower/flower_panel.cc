#include "puzzles/flower/flower_panel.h"

#include "puzzles/flower/flower_grid.h"

#include <exception>

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/thread.h>

namespace
{
    wxString formatPaths(const FlowerPaths& paths)
    {
        wxString text = "Coordinates: (row, column)\n";
        for (int color = 1; color <= FlowerBoard::ColorCount; ++color)
        {
            const auto& path = paths[color - 1];
            if (path.empty())
            {
                continue;
            }

            text += wxString::Format("\n%d - %s:\n", color, flowerColorName(color));
            for (std::size_t index = 0; index < path.size(); ++index)
            {
                if (index != 0)
                {
                    text += " -> ";
                }

                text += wxString::Format("(%d, %d)", path[index].row + 1, path[index].column + 1);
            }

            text += "\n";
        }

        return text;
    }
}

FlowerPanel::FlowerPanel(wxWindow* parent) : wxPanel(parent), m_previewTimer(this)
{
    SetName("flower-panel");
    createControls();
    Bind(wxEVT_THREAD, &FlowerPanel::finishSolve, this);
    Bind(
        wxEVT_TIMER,
        [this](wxTimerEvent&)
        {
            showPreview();
        },
        m_previewTimer.GetId());
    refreshBoard();
}

FlowerPanel::~FlowerPanel()
{
    // The worker can still post to this panel, so stop it before wx destroys the controls.
    m_previewTimer.Stop();
    m_worker.request_stop();
    if (m_worker.joinable())
    {
        m_worker.join();
    }
}

void FlowerPanel::createControls()
{
    const int gap = FromDIP(12);
    auto* layout  = new wxBoxSizer(wxVERTICAL);

    {
        auto* dimensions = new wxBoxSizer(wxHORIZONTAL);
        dimensions->Add(new wxStaticText(this, wxID_ANY, "&Rows:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap / 2);
        m_rows = new wxSpinCtrl(this, wxID_ANY, "5", wxDefaultPosition, FromDIP(wxSize(65, -1)), wxSP_ARROW_KEYS, 1,
            FlowerBoard::MaxDimension, 5);
        m_rows->SetName("flower-rows");
        dimensions->Add(m_rows, 0, wxRIGHT, gap);
        dimensions->Add(new wxStaticText(this, wxID_ANY, "C&olumns:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap / 2);
        m_columns = new wxSpinCtrl(this, wxID_ANY, "5", wxDefaultPosition, FromDIP(wxSize(65, -1)), wxSP_ARROW_KEYS, 1,
            FlowerBoard::MaxDimension, 5);
        m_columns->SetName("flower-columns");
        dimensions->Add(m_columns, 0, wxRIGHT, gap);
        m_resize = new wxButton(this, wxID_ANY, "&Resize grid");
        m_resize->SetName("flower-resize");
        m_resize->SetToolTip("Resize to 1-30 rows and columns. Flowers within the new bounds are kept.");
        dimensions->Add(m_resize, 0, wxRIGHT, gap);
        m_clear = new wxButton(this, wxID_ANY, "C&lear");
        m_clear->SetName("flower-clear");
        dimensions->Add(m_clear);
        layout->Add(dimensions, 0, wxALL, gap);
    }

    auto* content = new wxBoxSizer(wxHORIZONTAL);
    m_grid        = new FlowerGrid(this);
    content->Add(m_grid, 1, wxEXPAND | wxRIGHT, gap);

    auto* sidebar = new wxBoxSizer(wxVERTICAL);
    {
        auto* placement = new wxStaticBoxSizer(wxVERTICAL, this, "Flowers");
        auto* box       = placement->GetStaticBox();
        placement->Add(
            new wxStaticText(box, wxID_ANY, "Click two cells per color. Right-click to erase."), 0, wxALL, gap);
        m_color = new wxChoice(box, wxID_ANY);
        m_color->SetName("flower-color");
        m_color->Append("Erase");
        for (int color = 1; color <= FlowerBoard::ColorCount; ++color)
        {
            m_color->Append(wxString::Format("%d - %s (0/2)", color, flowerColorName(color)));
        }

        m_color->SetSelection(1);
        m_color->SetToolTip("Choose a color, then place its pair. Numbers identify colors in the solution.");
        placement->Add(m_color, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, gap);
        sidebar->Add(placement, 0, wxEXPAND | wxBOTTOM, gap);
    }

    auto* searchOptions = new wxBoxSizer(wxHORIZONTAL);
    searchOptions->Add(new wxStaticText(this, wxID_ANY, "Time limit (s):"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
    m_seconds =
        new wxSpinCtrl(this, wxID_ANY, "10", wxDefaultPosition, FromDIP(wxSize(65, -1)), wxSP_ARROW_KEYS, 1, 120, 10);
    m_seconds->SetName("flower-seconds");
    m_seconds->SetToolTip("Maximum search time in seconds.");
    searchOptions->Add(m_seconds);
    sidebar->Add(searchOptions, 0, wxBOTTOM, gap);

    {
        auto* solution    = new wxStaticBoxSizer(wxVERTICAL, this, "Solution");
        auto* solutionBox = solution->GetStaticBox();
        m_status = new wxStaticText(
            solutionBox, wxID_ANY, "", wxDefaultPosition, FromDIP(wxSize(310, 40)), wxST_NO_AUTORESIZE);
        m_status->SetName("flower-status");
        solution->Add(m_status, 0, wxEXPAND | wxALL, gap);

        m_instructions = new wxTextCtrl(
            solutionBox, wxID_ANY, "", wxDefaultPosition, FromDIP(wxSize(310, 160)), wxTE_MULTILINE | wxTE_READONLY);
        m_instructions->SetName("flower-instructions");
        solution->Add(m_instructions, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, gap);

        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        m_solve       = new wxButton(solutionBox, wxID_ANY, "&Solve");
        m_cancel      = new wxButton(solutionBox, wxID_ANY, "Cancel");
        m_copy        = new wxButton(solutionBox, wxID_ANY, "&Copy solution");
        m_solve->SetName("flower-solve");
        m_cancel->SetName("flower-cancel");
        m_copy->SetName("flower-copy");
        actions->Add(m_solve, 0, wxRIGHT, gap / 2);
        actions->Add(m_cancel, 0, wxRIGHT, gap / 2);
        actions->AddStretchSpacer();
        actions->Add(m_copy);
        solution->Add(actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, gap);
        sidebar->Add(solution, 1, wxEXPAND);
    }

    content->Add(sidebar, 0, wxEXPAND);
    layout->Add(content, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, gap);
    SetSizer(layout);

    m_grid->Bind(wxEVT_FLOWER_EDIT, [this](wxCommandEvent& event)
    {
        changeCell(
            {event.GetInt() / m_board.columns(), event.GetInt() % m_board.columns()}, event.GetExtraLong() != 0);
    });
    m_resize->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        resizeBoard();
    });
    m_clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        m_board = FlowerBoard(m_board.rows(), m_board.columns());
        refreshBoard();
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

void FlowerPanel::changeCell(const FlowerCell cell, const bool erase)
{
    if (m_solving)
    {
        return;
    }

    const int color = erase ? 0 : m_color->GetSelection();
    if (color == m_board.colorAt(cell))
    {
        return;
    }

    if (color > 0 && m_board.flowers()[color - 1].size() == 2)
    {
        m_status->SetLabel("This color already has two flowers.\nErase one before placing another.");
        return;
    }

    m_board.setColor(cell, color);
    refreshBoard();
}

void FlowerPanel::refreshBoard()
{
    m_grid->setBoard(m_board);
    m_instructions->Clear();
    m_copy->Disable();
    const auto pairs = m_board.flowers();
    for (int color = 1; color <= FlowerBoard::ColorCount; ++color)
    {
        m_color->SetString(
            color, wxString::Format("%d - %s (%zu/2)", color, flowerColorName(color), pairs[color - 1].size()));
    }

    const auto error = m_board.validationError();
    m_status->SetLabel(error.empty() ? wxString("Ready.") : wxString::FromUTF8(error));
    setSolving(false);
}

void FlowerPanel::resizeBoard()
{
    m_board.resize(m_rows->GetValue(), m_columns->GetValue());
    refreshBoard();
}

void FlowerPanel::setSolving(const bool solving)
{
    m_solving = solving;
    m_grid->setEditable(!solving);
    for (wxWindow* control : std::array<wxWindow*, 6>{m_rows, m_columns, m_color, m_resize, m_clear, m_seconds})
    {
        control->Enable(!solving);
    }

    m_solve->Enable(!solving && m_board.validationError().empty());
    m_cancel->Enable(solving);
    m_cancel->Show(solving);
    Layout();
}

void FlowerPanel::startSolve()
{
    if (m_solving)
    {
        return;
    }

    if (m_rows->GetValue() != m_board.rows() || m_columns->GetValue() != m_board.columns())
    {
        m_status->SetLabel("Click Resize grid to apply the new dimensions.");
        return;
    }

    if (const auto error = m_board.validationError(); !error.empty())
    {
        m_status->SetLabel(wxString::FromUTF8(error));
        return;
    }

    m_grid->setPaths({});
    m_instructions->Clear();
    m_copy->Disable();
    m_status->SetLabel("Searching... Showing tentative paths.");
    setSolving(true);
    FlowerSearchOptions options;
    options.timeLimit = std::chrono::seconds(m_seconds->GetValue());
    options.progress  = [this](const FlowerSearchPreview& preview)
    {
        // If the UI is busy, skip this snapshot. A newer one will arrive without slowing the search.
        std::unique_lock lock(m_previewMutex, std::try_to_lock);
        if (lock.owns_lock())
        {
            m_pendingPreview = preview;
        }
    };
    m_previewTimer.Start(33);
    // Give the worker its own board; only the UI thread touches the controls.
    m_worker = std::jthread(
        [this, board = m_board, options](const std::stop_token stop)
        {
            FlowerSolveResult result;
            try
            {
                result = solveFlower(board, options, stop);
            }
            catch (const std::exception&)
            {
                result.status = FlowerSolveStatus::Error;
            }

            auto* event = new wxThreadEvent(wxEVT_THREAD);
            event->SetPayload(std::move(result));
            wxQueueEvent(this, event);
        });
}

void FlowerPanel::finishSolve(wxThreadEvent& event)
{
    m_previewTimer.Stop();
    if (m_worker.joinable())
    {
        m_worker.join();
    }

    // Joining first lets us clear the shared snapshot without racing another preview update.
    const auto result = event.GetPayload<FlowerSolveResult>();
    m_pendingPreview.reset();
    m_grid->setPaths({});
    setSolving(false);
    switch (result.status)
    {
    case FlowerSolveStatus::Solved:
        m_grid->setPaths(result.paths);
        m_instructions->ChangeValue(formatPaths(result.paths));
        m_copy->Enable();
        m_status->SetLabel("Solved. Follow the colored paths.");
        break;
    case FlowerSolveStatus::NoSolution:
        m_status->SetLabel("No solution. The paths would share cells.");
        break;
    case FlowerSolveStatus::Invalid:
        m_status->SetLabel(wxString::FromUTF8(result.message));
        break;
    case FlowerSolveStatus::Cancelled:
        m_status->SetLabel("Search cancelled.");
        break;
    case FlowerSolveStatus::LimitReached:
        m_status->SetLabel("Search limit reached. Try a longer time limit.\nA solution may still exist.");
        break;
    case FlowerSolveStatus::Error:
        m_status->SetLabel("Search failed. Try again.");
        break;
    }
}

void FlowerPanel::showPreview()
{
    if (!m_solving)
    {
        return;
    }

    std::optional<FlowerSearchPreview> preview;
    {
        // Take the snapshot, then release the mutex before asking the grid to paint it.
        std::lock_guard lock(m_previewMutex);
        preview.swap(m_pendingPreview);
    }

    if (preview)
    {
        m_grid->setPreview(*preview);
        m_status->SetLabel(wxString::Format("Searching: %llu states tested.\nShowing tentative paths.",
            static_cast<unsigned long long>(preview->visitedNodes)));
    }
}

void FlowerPanel::copySolution()
{
    if (!wxTheClipboard->Open())
    {
        m_status->SetLabel("Clipboard busy. Try again.");
        return;
    }

    const auto text =
        wxString::Format("Flower (%d x %d)\n", m_board.rows(), m_board.columns()) + m_instructions->GetValue();
    const bool copied = wxTheClipboard->SetData(new wxTextDataObject(text));
    if (copied)
    {
        // Keep the copied text available after the application closes.
        wxTheClipboard->Flush();
    }

    wxTheClipboard->Close();
    m_status->SetLabel(copied ? "Solution copied." : "Could not copy. Try again.");
}
