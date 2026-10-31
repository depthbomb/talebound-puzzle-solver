#pragma once

#include "test_support.h"

#include <chrono>
#include <thread>
#include <wx/app.h>
#include <wx/button.h>

template<typename T>
T* find(wxWindow* parent, const wxString& name)
{
    auto* control = dynamic_cast<T*>(wxWindow::FindWindowByName(name, parent));
    require(control != nullptr, "Missing control: " + name.ToStdString());
    return control;
}

inline void click(wxWindow* parent, const wxString& name)
{
    auto* button = find<wxButton>(parent, name);
    require(button->IsEnabled(), "Disabled button: " + name.ToStdString());
    wxCommandEvent event(wxEVT_BUTTON, button->GetId());
    event.SetEventObject(button);
    require(button->ProcessWindowEvent(event), "Unhandled button: " + name.ToStdString());
}

inline void waitForSearch(wxWindow* editingControl)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!editingControl->IsEnabled())
    {
        wxTheApp->ProcessPendingEvents();
        require(std::chrono::steady_clock::now() < deadline, "Asynchronous search did not finish.");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void testMushroomUi();
void testFlowerUi();
void testTetrisUi();
