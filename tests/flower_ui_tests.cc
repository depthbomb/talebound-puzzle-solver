#include "ui_test_support.h"
#include "puzzles/flower/flower_grid.h"
#include "flower_fixtures.h"
#include "ui/main_frame.h"

#include <memory>
#include <wx/choice.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace
{
    void place(wxWindow* frame, const FlowerCell cell, const int color)
    {
        find<wxChoice>(frame, "flower-color")->SetSelection(color);
        auto* grid = find<FlowerGrid>(frame, "flower-grid");
        wxCommandEvent event(wxEVT_FLOWER_EDIT, grid->GetId());
        event.SetEventObject(grid);
        event.SetInt(cell.row * grid->board().columns() + cell.column);
        grid->ProcessWindowEvent(event);
    }

    void resize(wxWindow* frame, const int rows, const int columns)
    {
        find<wxSpinCtrl>(frame, "flower-rows")->SetValue(rows);
        find<wxSpinCtrl>(frame, "flower-columns")->SetValue(columns);
        click(frame, "flower-resize");
    }
}

void testFlowerUi()
{
    const auto frame = std::make_unique<MainFrame>();
    auto* tabs = find<wxNotebook>(frame.get(), "puzzle-tabs");
    require(tabs->GetPageCount() == 3 && tabs->GetPageText(1) == "Flower", "Flower tab is missing.");
    tabs->SetSelection(1);
    auto* grid = find<FlowerGrid>(frame.get(), "flower-grid");
    auto* palette = find<wxChoice>(frame.get(), "flower-color");
    const std::array<wxString, 7> names = {"White", "Red", "Blue", "Orange", "Pink", "Violet", "Purple"};
    require(palette->GetCount() == 8, "Expected seven colors and an eraser.");
    for (std::size_t index = 0; index < names.size(); ++index)
    {
        require(palette->GetString(static_cast<unsigned int>(index + 1)).Contains(names[index]), "Incorrect palette color.");
    }
    require(!find<wxButton>(frame.get(), "flower-solve")->IsEnabled(), "Empty puzzle can be solved.");
    require(!find<wxButton>(frame.get(), "flower-cancel")->IsShown(), "Cancel appears while idle.");

    place(frame.get(), {2, 0}, 1);
    require(!find<wxButton>(frame.get(), "flower-solve")->IsEnabled(), "Incomplete pair can be solved.");
    place(frame.get(), {2, 4}, 1);
    place(frame.get(), {0, 0}, 1);
    require(grid->board().flowers()[0].size() == 2 && grid->board().colorAt({0, 0}) == 0, "Third flower was accepted.");
    const auto original = grid->board();
    click(frame.get(), "flower-solve");
    require(!palette->IsEnabled() && find<wxButton>(frame.get(), "flower-cancel")->IsEnabled(),
        "Search did not lock editing and enable cancellation.");
    require(find<wxButton>(frame.get(), "flower-cancel")->IsShown(), "Cancel is hidden during search.");
    waitForSearch(find<wxSpinCtrl>(frame.get(), "flower-rows"));
    require(!find<wxButton>(frame.get(), "flower-cancel")->IsShown(), "Cancel remains visible after search.");
    require(grid->board() == original && grid->paths()[0].size() == 5, "White path is missing or modified the flowers.");
    auto* instructions = find<wxTextCtrl>(frame.get(), "flower-instructions");
    require(instructions->GetValue().Contains("1 - White") && instructions->GetValue().Contains("(3, 5)"),
        "Solution coordinates or color name are incorrect.");
    require(find<wxButton>(frame.get(), "flower-copy")->IsEnabled(), "Solved paths cannot be copied.");

    place(frame.get(), {0, 0}, 7);
    require(grid->paths()[0].empty() && instructions->GetValue().empty(), "Editing left stale paths visible.");
    require(!find<wxButton>(frame.get(), "flower-copy")->IsEnabled(), "Stale paths can be copied.");
    click(frame.get(), "flower-clear");
    resize(frame.get(), 3, 3);
    place(frame.get(), {0, 0}, 7);
    place(frame.get(), {0, 2}, 7);
    click(frame.get(), "flower-solve");
    waitForSearch(find<wxSpinCtrl>(frame.get(), "flower-rows"));
    require(grid->paths()[6].size() == 3 && instructions->GetValue().Contains("7 - Purple"),
        "Using only the seventh color failed.");

    resize(frame.get(), 2, 2);
    require(grid->board().flowers()[6].size() == 1 && !find<wxButton>(frame.get(), "flower-solve")->IsEnabled(),
        "Resize did not identify the cropped pair.");
    click(frame.get(), "flower-clear");
    place(frame.get(), {0, 0}, 2);
    place(frame.get(), {1, 1}, 2);
    place(frame.get(), {0, 1}, 3);
    place(frame.get(), {1, 0}, 3);
    click(frame.get(), "flower-solve");
    waitForSearch(find<wxSpinCtrl>(frame.get(), "flower-rows"));
    require(find<wxStaticText>(frame.get(), "flower-status")->GetLabel().StartsWith("No solution"),
        "Crossing puzzle was not reported as impossible.");
    require(!find<wxButton>(frame.get(), "flower-copy")->IsEnabled(), "Impossible puzzle permits copying paths.");

    click(frame.get(), "flower-clear");
    resize(frame.get(), 30, 30);
    place(frame.get(), {0, 0}, 4);
    place(frame.get(), {29, 29}, 4);
    click(frame.get(), "flower-solve");
    click(frame.get(), "flower-cancel");
    waitForSearch(find<wxSpinCtrl>(frame.get(), "flower-rows"));
    const auto status = find<wxStaticText>(frame.get(), "flower-status")->GetLabel();
    require(status.StartsWith("Search cancelled") || status.StartsWith("Solved"), "Cancellation produced a false impossibility.");
    require(grid->board().flowers()[3].size() == 2, "Cancellation changed the flowers.");
    if (status.StartsWith("Search cancelled"))
    {
        for (const auto& path : grid->paths())
        {
            require(path.empty(), "Cancellation left tentative paths displayed as a solution.");
        }
    }

    click(frame.get(), "flower-clear");
    resize(frame.get(), 11, 11);
    const auto reported = reportedFlowerPuzzle();
    const auto flowers = reported.flowers();
    for (std::size_t color = 0; color < flowers.size(); ++color)
    {
        for (const auto cell : flowers[color])
        {
            place(frame.get(), cell, static_cast<int>(color) + 1);
        }
    }
    click(frame.get(), "flower-solve");
    waitForSearch(find<wxSpinCtrl>(frame.get(), "flower-rows"));
    require(find<wxStaticText>(frame.get(), "flower-status")->GetLabel().StartsWith("Solved"),
        "The reported 11x11 puzzle was not solved through the UI.");
    require(grid->board() == reported, "Search preview changed the flower placement.");
    for (const auto& path : grid->paths())
    {
        require(path.size() >= 2, "An incomplete preview replaced a finished path.");
    }

    // Closing with a search in flight must join its worker before destroying the event target.
    click(frame.get(), "flower-solve");
}
