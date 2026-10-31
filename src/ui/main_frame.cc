#include "ui/main_frame.h"

#include "metadata.h"
#include "puzzles/flower/flower_panel.h"
#include "puzzles/mushroom/mushroom_panel.h"
#include "puzzles/tetris/tetris_panel.h"

#include <wx/hyperlink.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>

MainFrame::MainFrame() : wxFrame(nullptr, wxID_ANY, AppName)
{
    auto* content = new wxPanel(this);
    auto* tabs    = new wxNotebook(content, wxID_ANY);
    tabs->SetName("puzzle-tabs");
    tabs->AddPage(new MushroomPanel(tabs), "Mushroom", true);
    tabs->AddPage(new FlowerPanel(tabs), "Flower");
    tabs->AddPage(new TetrisPanel(tabs), "Tetris");

    auto* source = new wxHyperlinkCtrl(content, wxID_ANY, "Source", AppSourceUrl);
    source->SetName("source-link");
    source->SetToolTip(AppSourceUrl);

    auto* layout = new wxBoxSizer(wxVERTICAL);
    layout->Add(tabs, 1, wxEXPAND);
    layout->Add(source, 0, wxALIGN_RIGHT | wxALL, FromDIP(8));
    content->SetSizer(layout);

    auto* frameLayout = new wxBoxSizer(wxVERTICAL);
    frameLayout->Add(content, 1, wxEXPAND);
    SetSizerAndFit(frameLayout);
    SetMinSize(GetSize());
    Centre();
}
