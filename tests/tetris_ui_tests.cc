#include "ui_test_support.h"
#include "puzzles/tetris/tetris_grid.h"
#include "ui/main_frame.h"

#include <memory>
#include <wx/choice.h>
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

    void selectVariant(wxWindow* frame, const int selection)
    {
        auto* choice = find<wxChoice>(frame, "tetris-variant");
        require(choice->IsEnabled(), "Variant selection is disabled.");
        choice->SetSelection(selection);
        wxCommandEvent event(wxEVT_CHOICE, choice->GetId());
        event.SetEventObject(choice);
        event.SetInt(selection);
        choice->ProcessWindowEvent(event);
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
    auto* variant = find<wxChoice>(frame.get(), "tetris-variant");
    require(variant->GetCount() == 2 && grid->board().variant() == TetrisVariant::Rectangle, "Missing Tetris variants.");
    require(solve->IsEnabled() && !copy->IsEnabled() && grid->placements().empty(), "Empty puzzle is not empty.");
    require(!find<wxButton>(frame.get(), "tetris-cancel")->IsShown(), "Cancel appears while idle.");
    click(frame.get(), "tetris-solve");
    require(status->GetLabel().StartsWith("Not enough pieces."), "Empty inventory did not show an error.");
    setCount(frame.get(), 1, 7);
    click(frame.get(), "tetris-solve");
    require(status->GetLabel().StartsWith("Not enough pieces.") && status->GetLabel().Contains("32 cells"),
        "Seven pieces did not show the rectangle's missing-piece error.");
    setCount(frame.get(), 1, 9);
    require(find<wxSpinCtrl>(frame.get(), "tetris-count-1")->GetValue() == 9, "Piece input is still capped at eight.");
    click(frame.get(), "tetris-solve");
    require(status->GetLabel().StartsWith("Too many pieces.") && variant->IsEnabled()
        && !find<wxButton>(frame.get(), "tetris-cancel")->IsShown(), "Excess pieces started a search.");
    setCount(frame.get(), 1, 8);
    require(solve->IsEnabled() && find<wxStaticText>(frame.get(), "tetris-total")->GetLabel() == "8 pieces",
        "Eight-piece inventory was not accepted.");
    click(frame.get(), "tetris-solve");
    require(!find<wxSpinCtrl>(frame.get(), "tetris-count-1")->IsEnabled()
        && !variant->IsEnabled() && find<wxButton>(frame.get(), "tetris-cancel")->IsEnabled(),
        "Search did not disable editing and enable cancellation.");
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
    require(solve->IsEnabled() && find<wxStaticText>(frame.get(), "tetris-total")->GetLabel() == "0 pieces",
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
    require(solve->IsEnabled() && !copy->IsEnabled() && grid->placements().empty(),
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

    selectVariant(frame.get(), 1);
    require(grid->board().rows() == 8 && grid->board().cellCount() == 48 && !grid->board().hasCell({0, 0})
        && grid->board().hasCell({7, 5}), "Variant did not switch the grid to a plus.");
    require(grid->placements().empty() && instructions->GetValue().empty() && !copy->IsEnabled(),
        "Variant change left a stale solution.");
    click(frame.get(), "tetris-solve");
    require(status->GetLabel().StartsWith("Not enough pieces.") && status->GetLabel().Contains("48 cells"),
        "Plus board still accepts eight pieces.");
    click(frame.get(), "tetris-clear");
    setCount(frame.get(), 1, 13);
    click(frame.get(), "tetris-solve");
    require(status->GetLabel().StartsWith("Too many pieces."), "Plus board accepted thirteen pieces.");
    setCount(frame.get(), 1, 12);
    click(frame.get(), "tetris-solve");
    require(!variant->IsEnabled(), "Variant can change during a plus search.");
    waitForSearch(find<wxSpinCtrl>(frame.get(), "tetris-count-0"));
    require(status->GetLabel().StartsWith("Solved") && grid->placements().size() == 12 && copy->IsEnabled()
        && variant->IsEnabled(), "Twelve squares did not solve the plus board.");
    require(instructions->GetValue().Contains("L - 2 x 2 (O)") && instructions->GetValue().Contains("(8, 6)"),
        "Plus solution omitted its final piece or bottom arm.");
    std::uint64_t covered = 0;
    for (const auto& placement : grid->placements())
    {
        for (const auto cell : placement.cells)
        {
            require(grid->board().hasCell(cell), "Displayed piece covers a missing corner.");
            const auto bit = std::uint64_t{1} << (cell.row * 8 + cell.column);
            require((covered & bit) == 0, "Displayed pieces overlap.");
            covered |= bit;
        }
    }
    require(covered == grid->board().mask(), "Displayed solution leaves holes in the plus board.");

    selectVariant(frame.get(), 0);
    require(grid->board().rows() == 4 && grid->placements().empty() && !copy->IsEnabled(),
        "Switching back retained plus placements.");
    require(find<wxSpinCtrl>(frame.get(), "tetris-count-1")->GetValue() == 12, "Variant change discarded entered counts.");
    click(frame.get(), "tetris-solve");
    require(status->GetLabel().StartsWith("Too many pieces.") && status->GetLabel().Contains("32 cells"),
        "Rectangle validation retained the plus board's size.");
    setCount(frame.get(), 1, 8);
    click(frame.get(), "tetris-solve");
    waitForSearch(find<wxSpinCtrl>(frame.get(), "tetris-count-0"));
    require(status->GetLabel().StartsWith("Solved") && grid->placements().size() == 8, "Rectangle no longer solves.");

    // Destruction must stop and join a worker before its event target is destroyed.
    selectVariant(frame.get(), 1);
    setCount(frame.get(), 1, 12);
    click(frame.get(), "tetris-solve");
}
