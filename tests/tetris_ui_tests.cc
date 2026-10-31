#include "ui_test_support.h"
#include "puzzles/tetris/tetris_grid.h"
#include "ui/main_frame.h"

#include <memory>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace
{
    void setCount(wxWindow* frame, const int piece, const int count)
    {
        auto* input = find<wxSpinCtrl>(frame, wxString::Format("tetris-count-%d", piece));
        require(input->IsEnabled(), "Piece editing is disabled.");
        input->SetValue(count);
        wxSpinEvent event(wxEVT_SPINCTRL, input->GetId());
        event.SetEventObject(input);
        event.SetPosition(count);
        input->ProcessWindowEvent(event);
    }
}

void testTetrisUi()
{
    const auto frame = std::make_unique<MainFrame>();
    auto* tabs = find<wxNotebook>(frame.get(), "puzzle-tabs");
    require(tabs->GetPageCount() == 3 && tabs->GetPageText(2) == "Tetris", "Tetris tab is missing.");
    tabs->SetSelection(2);
    auto* grid = find<TetrisGrid>(frame.get(), "tetris-grid");
    auto* instructions = find<wxTextCtrl>(frame.get(), "tetris-instructions");
    auto* solve = find<wxButton>(frame.get(), "tetris-solve");
    auto* copy = find<wxButton>(frame.get(), "tetris-copy");
    auto* status = find<wxStaticText>(frame.get(), "tetris-status");
    require(!solve->IsEnabled() && !copy->IsEnabled() && grid->placements().empty(), "Empty puzzle is not empty.");
    require(!find<wxButton>(frame.get(), "tetris-cancel")->IsShown(), "Cancel appears while idle.");
    setCount(frame.get(), 1, 7);
    require(!solve->IsEnabled(), "Seven pieces can be solved.");
    setCount(frame.get(), 0, 2);
    require(!solve->IsEnabled(), "Nine pieces can be solved.");
    setCount(frame.get(), 0, 0);
    setCount(frame.get(), 1, 8);
    require(solve->IsEnabled() && find<wxStaticText>(frame.get(), "tetris-total")->GetLabel().Contains("8 / 8"),
        "Eight-piece inventory was not accepted.");
    click(frame.get(), "tetris-solve");
    require(!find<wxSpinCtrl>(frame.get(), "tetris-count-1")->IsEnabled()
        && find<wxButton>(frame.get(), "tetris-cancel")->IsEnabled(), "Search did not disable editing and enable cancellation.");
    require(find<wxButton>(frame.get(), "tetris-cancel")->IsShown(), "Cancel is hidden during search.");
    waitForSearch(find<wxSpinCtrl>(frame.get(), "tetris-count-0"));
    require(!find<wxButton>(frame.get(), "tetris-cancel")->IsShown(), "Cancel remains visible after search.");
    require(status->GetLabel().StartsWith("Solved") && grid->placements().size() == 8 && copy->IsEnabled(),
        "Eight squares were not solved.");
    require(instructions->GetValue().Contains("A - 2 x 2 (O)") && instructions->GetValue().Contains("H - 2 x 2 (O)")
        && instructions->GetValue().Contains("(4, 8)"), "Individual piece labels or one-based coordinates are missing.");
    setCount(frame.get(), 1, 7);
    require(grid->placements().empty() && instructions->GetValue().empty() && !copy->IsEnabled(),
        "Changing a count left stale placements visible.");
    setCount(frame.get(), 0, 1);
    click(frame.get(), "tetris-solve");
    waitForSearch(find<wxSpinCtrl>(frame.get(), "tetris-count-0"));
    require(status->GetLabel().StartsWith("No solution") && !copy->IsEnabled() && grid->placements().empty(),
        "Impossible inventory produced a solution.");

    click(frame.get(), "tetris-clear");
    require(!solve->IsEnabled() && find<wxStaticText>(frame.get(), "tetris-total")->GetLabel().StartsWith("0 / 8"),
        "Clear did not reset the inventory.");
    const TetrisInventory mixed = {2, 1, 1, 1, 1, 2};
    for (int type = 0; type < static_cast<int>(mixed.size()); ++type)
    {
        setCount(frame.get(), type, mixed[type]);
    }

    click(frame.get(), "tetris-solve");
    waitForSearch(find<wxSpinCtrl>(frame.get(), "tetris-count-0"));
    require(status->GetLabel().StartsWith("Solved"), "Mixed inventory was not solved.");
    TetrisInventory used = {};
    for (const auto& placement : grid->placements())
    {
        ++used[static_cast<std::size_t>(placement.piece)];
    }

    require(used == mixed, "UI passed incorrect piece counts to the solver.");
    require(instructions->GetValue().Contains("Mirrored L (J)") && instructions->GetValue().Contains("Mirrored S (Z)"),
        "Mirrored piece names are missing.");

    click(frame.get(), "tetris-clear");
    const int line = static_cast<int>(TetrisPiece::Line);
    auto* preview = find<wxWindow>(frame.get(), wxString::Format("tetris-preview-%d", line));
    require(preview->GetClientSize().y >= preview->FromDIP(68), "The line preview clips its fourth block.");
    setCount(frame.get(), line, 8);
    click(frame.get(), "tetris-solve");
    waitForSearch(find<wxSpinCtrl>(frame.get(), "tetris-count-0"));
    require(status->GetLabel().StartsWith("Solved") && grid->placements().size() == 8 && copy->IsEnabled(),
        "Eight line pieces were not solved.");
    for (const auto& placement : grid->placements())
    {
        require(placement.piece == TetrisPiece::Line, "A line piece was replaced with another shape.");
    }

    require(instructions->GetValue().Contains("A - Line (I)") && instructions->GetValue().Contains("H - Line (I)"),
        "Line piece names are missing from the solution.");
    setCount(frame.get(), line, 4);
    require(!solve->IsEnabled() && !copy->IsEnabled() && grid->placements().empty(),
        "Changing the line count left a stale solution.");
    setCount(frame.get(), 1, 4);
    click(frame.get(), "tetris-solve");
    waitForSearch(find<wxSpinCtrl>(frame.get(), "tetris-count-0"));
    require(status->GetLabel().StartsWith("Solved"), "Mixed squares and lines were not solved.");
    used = {};
    for (const auto& placement : grid->placements())
    {
        ++used[static_cast<std::size_t>(placement.piece)];
    }

    require(used[1] == 4 && used[line] == 4, "Mixed squares and lines used the wrong counts.");
    click(frame.get(), "tetris-solve");
    click(frame.get(), "tetris-cancel");
    waitForSearch(find<wxSpinCtrl>(frame.get(), "tetris-count-0"));
    require(status->GetLabel().StartsWith("Search cancelled") || status->GetLabel().StartsWith("Solved"),
        "Cancellation was reported as impossibility.");
    if (status->GetLabel().StartsWith("Search cancelled"))
    {
        require(grid->placements().empty() && !copy->IsEnabled(), "Cancelled search left partial placements.");
    }

    // Destruction must stop and join a worker before its event target is destroyed.
    click(frame.get(), "tetris-solve");
}
