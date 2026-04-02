#pragma once

// ── FontAwesome 4.x icon codepoints ──────────────────────────────────────────
//
//  Usage: ImGui::TextUnformatted(FA_FOLDER "  My folder");
//
//  These are UTF-8 encoded Unicode codepoints from the FontAwesome 4.x range
//  (U+F000 – U+F8FF).  The icon font must be merged into the main font via
//  Settings::load_fonts() (done automatically for the Pixel Dark theme).
//
//  Full list: https://fontawesome.com/v4/icons/
//

// Files & folders
#define FA_FILE             "\xef\x82\xb4"   // U+F0F6  fa-file
#define FA_FILE_TEXT        "\xef\x82\xb5"   // U+F0F7  fa-file-text
#define FA_FOLDER           "\xef\x81\xbb"   // U+F07B  fa-folder
#define FA_FOLDER_OPEN      "\xef\x81\xbc"   // U+F07C  fa-folder-open
#define FA_SAVE             "\xef\x83\x87"   // U+F0C7  fa-floppy-o (save)
#define FA_TRASH            "\xef\x87\xb8"   // U+F1F8  fa-trash

// Editing
#define FA_PENCIL           "\xef\x81\x84"   // U+F044  fa-pencil-square-o → use U+F040
#define FA_EDIT             "\xef\x81\x84"   // U+F044  fa-pencil-square-o
#define FA_COPY             "\xef\x83\x85"   // U+F0C5  fa-copy
#define FA_PASTE            "\xef\x83\xaa"   // U+F0EA  fa-clipboard
#define FA_CUT              "\xef\x83\x84"   // U+F0C4  fa-scissors
#define FA_UNDO             "\xef\x83\xa2"   // U+F0E2  fa-undo
#define FA_REDO             "\xef\x80\x9e"   // U+F01E  fa-repeat

// Navigation & layout
#define FA_BARS             "\xef\x83\x89"   // U+F0C9  fa-bars (hamburger)
#define FA_CARET_DOWN       "\xef\x83\x97"   // U+F0D7  fa-caret-down
#define FA_CARET_RIGHT      "\xef\x83\x9a"   // U+F0DA  fa-caret-right
#define FA_CHEVRON_DOWN     "\xef\x84\x98"   // U+F078  fa-chevron-down
#define FA_CHEVRON_RIGHT    "\xef\x84\x9a"   // U+F054  fa-chevron-right  (U+F054)
#define FA_ANGLE_LEFT       "\xef\x84\x93"   // U+F053  fa-angle-left
#define FA_ANGLE_RIGHT      "\xef\x84\x94"   // U+F054  fa-angle-right
#define FA_ARROW_UP         "\xef\x81\xb7"   // U+F077  fa-chevron-up (proxy)
#define FA_HOME             "\xef\x80\x95"   // U+F015  fa-home
#define FA_SEARCH           "\xef\x80\x82"   // U+F002  fa-search

// UI state
#define FA_PLUS             "\xef\x81\xa7"   // U+F067  fa-plus
#define FA_MINUS            "\xef\x81\xa8"   // U+F068  fa-minus
#define FA_TIMES            "\xef\x80\x8d"   // U+F00D  fa-times (X / close)
#define FA_CHECK            "\xef\x80\x8c"   // U+F00C  fa-check
#define FA_CIRCLE           "\xef\x84\x91"   // U+F111  fa-circle
#define FA_DOT_CIRCLE       "\xef\x86\x92"   // U+F192  fa-dot-circle-o
#define FA_SQUARE           "\xef\x85\x89"   // U+F0C8  fa-square (filled)  (U+F096 empty)
#define FA_COG              "\xef\x80\x93"   // U+F013  fa-cog (settings)
#define FA_COG_ALT          "\xef\x82\xad"   // U+F085  fa-cogs
#define FA_INFO             "\xef\x81\x9a"   // U+F05A  fa-info-circle
#define FA_WARNING          "\xef\x81\xb1"   // U+F071  fa-exclamation-triangle
#define FA_BAN              "\xef\x81\x9e"   // U+F05E  fa-ban

// Layers / objects
#define FA_LAYER_GROUP      "\xef\x80\xb0"   // U+F030  fa-camera (approximate — FA4 has no layer icon)
#define FA_OBJECT_GROUP     "\xef\x87\xbf"   // U+F1FF  fa-object-group
#define FA_OBJECT_UNGROUP   "\xef\x88\x80"   // U+F200  fa-object-ungroup
#define FA_EYE              "\xef\x81\xae"   // U+F06E  fa-eye
#define FA_EYE_SLASH        "\xef\x81\xb0"   // U+F070  fa-eye-slash
#define FA_LOCK             "\xef\x80\xa3"   // U+F023  fa-lock
#define FA_UNLOCK           "\xef\x82\x9c"   // U+F09C  fa-unlock

// Media / shader
#define FA_PLAY             "\xef\x81\x8b"   // U+F04B  fa-play
#define FA_PAUSE            "\xef\x81\x8c"   // U+F04C  fa-pause
#define FA_STOP             "\xef\x81\x8d"   // U+F04D  fa-stop
#define FA_CODE             "\xef\x84\xa1"   // U+F121  fa-code
#define FA_TERMINAL         "\xef\x84\xa0"   // U+F120  fa-terminal
#define FA_PAINT_BRUSH      "\xef\x87\xbc"   // U+F1FC  fa-paint-brush
#define FA_MAGIC            "\xef\x83\x90"   // U+F0D0  fa-magic (wand)
#define FA_IMAGE            "\xef\x80\xbe"   // U+F03E  fa-picture-o
#define FA_FILM             "\xef\x80\x88"   // U+F008  fa-film

// 3D / scene
#define FA_CUBE             "\xef\x86\xb2"   // U+F1B2  fa-cube
#define FA_CUBES            "\xef\x86\xb3"   // U+F1B3  fa-cubes

