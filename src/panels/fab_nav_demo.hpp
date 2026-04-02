#pragma once

#include "../ui/fab_nav.hpp"

namespace shadebug::panels {

class FabNavDemo {
public:
    FabNavDemo()
        : fab_nav_("MainFab", ImVec2(40, 100), ui::FabNav::Position::TopRight) {
        setup_file_menu();
        setup_view_menu();
        setup_debug_menu();
    }

    void draw() {
        fab_nav_.draw();
    }

    ui::FabNav& get() { return fab_nav_; }

private:
    ui::FabNav fab_nav_;

    void setup_file_menu() {
        fab_nav_.add_button("file", "\xef\x81\x85", "File");  // FA_FILE
        fab_nav_.add_submenu("file", "new",  "\xef\x80\x9c", "New");
        fab_nav_.add_submenu("file", "open", "\xef\x81\x82", "Open");
        fab_nav_.add_submenu("file", "save", "\xef\x80\x9d", "Save");
    }

    void setup_view_menu() {
        fab_nav_.add_button("view", "\xef\x80\xae", "View");  // FA_EYE
        fab_nav_.add_submenu("view", "layers",    "\xef\x81\xa2", "Layers");
        fab_nav_.add_submenu("view", "props",     "\xef\x80\x89", "Properties");
        fab_nav_.add_submenu("view", "console",   "\xef\x83\xa8", "Console");
    }

    void setup_debug_menu() {
        fab_nav_.add_button("debug", "\xef\x81\xb0", "Debug");  // FA_BUG
        fab_nav_.add_submenu("debug", "log",      "\xef\x80\xac", "Logs");
        fab_nav_.add_submenu("debug", "profile",  "\xef\x81\xb1", "Profile");
        fab_nav_.add_submenu("debug", "stats",    "\xef\x80\xad", "Stats");
    }
};

} // namespace shadebug::panels

