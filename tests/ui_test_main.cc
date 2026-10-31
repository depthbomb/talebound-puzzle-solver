#include "ui_test_support.h"

class TestApp final : public wxApp
{
public:
    bool OnInit() override
    {
        SetAppName("Talebound Puzzle Solver tests");
        return true;
    }
};

wxIMPLEMENT_APP_NO_MAIN(TestApp);

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Expected a workflow: mushroom, flower, or tetris.\n";
        return 1;
    }

    const std::string workflow = argv[1];
    if (!wxEntryStart(argc, argv))
    {
        return 1;
    }

    const int result = runTests([&]
    {
        require(wxTheApp->CallOnInit(), "wxWidgets initialization failed.");
        if (workflow == "mushroom")
        {
            testMushroomUi();
        }
        else if (workflow == "flower")
        {
            testFlowerUi();
        }
        else if (workflow == "tetris")
        {
            testTetrisUi();
        }
        else
        {
            throw std::runtime_error("Unknown workflow.");
        }
    });

    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
