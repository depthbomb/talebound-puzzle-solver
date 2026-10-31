#include "ui_test_support.h"
#include "ui/main_frame.h"
#include "puzzles/mushroom/board.h"

#include <array>
#include <memory>
#include <vector>
#include <wx/choice.h>
#include <wx/notebook.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/tglbtn.h>

namespace
{
    MushroomBoard readGrid(wxWindow* frame, const MushroomVariant variant = MushroomVariant::Standard)
    {
        MushroomBoard board(variant);
        for (const auto position : board.cells())
        {
            auto* cell = find<wxToggleButton>(frame, wxString::Format("cell-%d-%d", position.row + 1, position.column + 1));
            board.setCell(position, cell->GetValue());
            require(cell->GetLabel() == (cell->GetValue() ? "Thorns (on)" : "Mushroom (off)"),
                "Cell label and state disagree.");
            require(cell->HasFlag(wxBU_NOTEXT) && cell->GetBitmap().IsOk() && cell->GetBitmapDisabled().IsOk(),
                "Cell icons are missing in editing or preview mode.");
        }
        return board;
    }

    void verifyDisplayedSolution(wxWindow* frame, MushroomBoard board)
    {
        for (const auto cell : board.cells())
        {
            const auto* step = find<wxStaticText>(frame, wxString::Format("solution-%d-%d", cell.row + 1, cell.column + 1));
            if (!step->GetLabel().empty())
            {
                board.applyClick(cell);
            }
        }
        require(board.isClear(), "The displayed solution does not clear the current grid.");
    }

    void selectVariant(wxWindow* frame, const int selection)
    {
        auto* choice = find<wxChoice>(frame, "puzzle-variant");
        require(choice->IsEnabled(), "Variant selector is disabled.");
        choice->SetSelection(selection);
        wxCommandEvent event(wxEVT_CHOICE, choice->GetId());
        event.SetInt(selection);
        event.SetEventObject(choice);
        choice->GetEventHandler()->ProcessEvent(event);
    }

    void turnOn(wxWindow* frame, const MushroomCell position)
    {
        auto* cell = find<wxToggleButton>(frame, wxString::Format("cell-%d-%d", position.row + 1, position.column + 1));
        cell->SetValue(true);
        wxCommandEvent event(wxEVT_TOGGLEBUTTON, cell->GetId());
        event.SetEventObject(cell);
        event.SetInt(1);
        cell->GetEventHandler()->ProcessEvent(event);
    }
}

void testMushroomUi()
{
    const auto frame = std::make_unique<MainFrame>();
    auto* tabs = find<wxNotebook>(frame.get(), "puzzle-tabs");
    require(tabs->GetPageCount() == 3 && tabs->GetPageText(0) == "Mushroom" && tabs->GetPageText(1) == "Flower",
        "Expected separate Mushroom and Flower tabs.");
    const auto* variants = find<wxChoice>(frame.get(), "puzzle-variant");
    require(variants->GetCount() == 3, "Expected three variants.");
    require(variants->GetString(0) == "1" && variants->GetString(1) == "2" && variants->GetString(2) == "3",
        "Variants should be named only by number.");
    require(readGrid(frame.get()) == MushroomBoard::example(), "Initial board does not match the user's example.");
    auto* coordinates = find<wxTextCtrl>(frame.get(), "solution-coordinates");
    require(coordinates->GetValue().Contains("Row 1: columns 1, 2, 5, 6"), "Incorrect original solution text.");
    require(find<wxStaticText>(frame.get(), "solution-status")->GetLabel().StartsWith("18 clicks"),
        "Original solution count is incorrect.");
    verifyDisplayedSolution(frame.get(), readGrid(frame.get()));
    require(!find<wxButton>(frame.get(), "next-move")->IsShown()
        && !find<wxButton>(frame.get(), "back-to-editing")->IsShown(), "Preview-only controls appear while editing.");

    auto* firstCell = find<wxToggleButton>(frame.get(), "cell-1-1");
    turnOn(frame.get(), {0, 0});
    auto expected = MushroomBoard::example();
    expected.setCell({0, 0}, true);
    require(readGrid(frame.get()) == expected, "Editing toggled neighbors instead of just the selected cell.");
    verifyDisplayedSolution(frame.get(), expected);

    click(frame.get(), "clear-board");
    require(readGrid(frame.get()).isClear(), "Clear did not clear the board.");
    require(coordinates->GetValue().Contains("No clicks needed"), "Empty solution message is missing.");
    require(!find<wxButton>(frame.get(), "preview-solution")->IsEnabled(), "Empty board permits a preview.");
    verifyDisplayedSolution(frame.get(), {});

    click(frame.get(), "all-on");
    require(readGrid(frame.get()).countOn() == 36, "All on did not fill the grid.");
    verifyDisplayedSolution(frame.get(), readGrid(frame.get()));

    click(frame.get(), "load-example");
    const auto originalText = coordinates->GetValue();
    click(frame.get(), "preview-solution");
    require(!find<wxButton>(frame.get(), "preview-solution")->IsShown()
        && find<wxButton>(frame.get(), "next-move")->IsShown()
        && find<wxButton>(frame.get(), "back-to-editing")->IsShown(), "Preview controls did not switch modes.");
    require(!firstCell->IsEnabled(), "Preview allows changes to the starting grid.");
    require(!find<wxButton>(frame.get(), "clear-board")->IsEnabled(), "Preview permits clearing the starting grid.");
    click(frame.get(), "next-move");
    expected = MushroomBoard::example();
    expected.applyClick({0, 0});
    require(readGrid(frame.get()) == expected, "Preview did not apply the first move and its neighbors.");

    click(frame.get(), "back-to-editing");
    require(find<wxButton>(frame.get(), "preview-solution")->IsShown()
        && !find<wxButton>(frame.get(), "next-move")->IsShown(), "Editing did not restore the preview action.");
    require(firstCell->IsEnabled(), "Returning to editing did not enable cells.");
    require(readGrid(frame.get()) == MushroomBoard::example(), "Partial preview changed the starting grid.");
    click(frame.get(), "preview-solution");
    for (int step = 0; step < 18; ++step)
    {
        click(frame.get(), "next-move");
    }
    require(readGrid(frame.get()).isClear(), "Full preview did not turn everything off.");
    require(!find<wxButton>(frame.get(), "next-move")->IsEnabled(), "Preview can move past the final click.");
    require(find<wxStaticText>(frame.get(), "board-status")->GetLabel().Contains("All cells are off"),
        "Completion message is missing.");
    require(coordinates->GetValue() == originalText, "Preview changed the solution instructions.");

    click(frame.get(), "back-to-editing");
    require(readGrid(frame.get()) == MushroomBoard::example(), "Completed preview did not restore the starting grid.");

    struct VariantCase
    {
        MushroomVariant variant;
        MushroomCell impossibleCell;
        std::vector<const char*> missingControls;
        std::size_t cellCount;
    };
    const std::array<VariantCase, 2> cases = {{
        {MushroomVariant::Small, {0, 1},
            {"cell-1-1", "cell-1-5", "cell-3-1", "cell-3-5", "cell-4-2", "cell-2-6"}, 11},
        {MushroomVariant::MiddleGaps, {0, 0},
            {"cell-2-2", "cell-2-4", "solution-2-2", "solution-2-4"}, 13}
    }};
    for (const auto& test : cases)
    {
        selectVariant(frame.get(), static_cast<int>(test.variant));
        require(readGrid(frame.get(), test.variant).isClear(), "Variant should start empty.");
        for (const auto* name : test.missingControls)
        {
            require(wxWindow::FindWindowByName(name, frame.get()) == nullptr, "Grid contains a missing object.");
        }
        require(!find<wxButton>(frame.get(), "load-example")->IsEnabled(), "Large example is enabled in a small variant.");
        turnOn(frame.get(), test.impossibleCell);
        require(find<wxStaticText>(frame.get(), "solution-status")->GetLabel().Contains("No solution"),
            "An impossible pattern was presented as solvable.");
        require(!find<wxButton>(frame.get(), "copy-solution")->IsEnabled(), "Impossible pattern permits copying.");
        require(!find<wxButton>(frame.get(), "preview-solution")->IsEnabled(), "Impossible pattern permits preview.");

        click(frame.get(), "all-on");
        require(readGrid(frame.get(), test.variant).countOn() == test.cellCount, "All on did not respect the shape.");
        click(frame.get(), "clear-board");
        require(readGrid(frame.get(), test.variant).isClear(), "Clear did not clear the board.");
        MushroomBoard board(test.variant);
        board.applyClick({1, 2});
        for (const auto cell : board.cells())
        {
            if (board.isOn(cell))
            {
                turnOn(frame.get(), cell);
            }
        }
        verifyDisplayedSolution(frame.get(), board);
        require(coordinates->GetValue().Contains("Row 2: columns 3"), "Solution uses incorrect coordinates.");
        require(!coordinates->GetValue().Contains("Row 4"), "Solution includes a nonexistent row.");
        click(frame.get(), "preview-solution");
        require(!find<wxChoice>(frame.get(), "puzzle-variant")->IsEnabled(), "Preview permits variant changes.");
        click(frame.get(), "next-move");
        require(readGrid(frame.get(), test.variant).isClear(), "Preview did not clear the board.");
        click(frame.get(), "back-to-editing");
        require(readGrid(frame.get(), test.variant) == board, "Preview changed its starting grid.");
    }

    selectVariant(frame.get(), 0);
    require(readGrid(frame.get()) == MushroomBoard::example(), "Variant switching lost the original starting grid.");
    for (const auto& test : cases)
    {
        selectVariant(frame.get(), static_cast<int>(test.variant));
        MushroomBoard board(test.variant);
        board.applyClick({1, 2});
        require(readGrid(frame.get(), test.variant) == board, "Variant switching lost its starting grid.");
    }
}
