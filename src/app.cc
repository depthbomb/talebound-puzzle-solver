#include "metadata.h"
#include "ui/main_frame.h"

#include <wx/app.h>

class PuzzleSolverApp final : public wxApp
{
public:
    bool OnInit() override
    {
        if (!wxApp::OnInit())
        {
            return false;
        }

        SetAppName(AppName);
        SetAppDisplayName(AppName);
        SetVendorName(AppCompany);

        auto* frame = new MainFrame;
        SetTopWindow(frame);
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(PuzzleSolverApp);
