[![pl](https://img.shields.io/badge/lang-pl-green.svg)](https://github.com/baziorek/STC_editor/tree/master/README_pl.md)

# STC Editor
A simple text editor built with Qt to streamline the insertion of [STC](https://cpp0x.pl/kursy/Kurs-STC/169) markup tags used by the [cpp0x.pl](https://cpp0x.pl/) platform.

## Overview
This editor is designed to simplify working with STC markup for creating content on cpp0x.pl. It provides a straightforward Qt-based interface with a text widget and buttons for inserting STC tags, along with specialized features to enhance productivity.

![Screenshot](https://github.com/user-attachments/assets/17571fce-a89c-4c99-a38a-fd72ee65c7d5)

## Dedicated Features
1. **Syntax Highlighting**: Colorizes STC markup for better readability.
2. **Tag Closure Validation**: Checks if all STC tags are properly closed.
3. **Text Transformation**: Convert selected text to lowercase, uppercase, camelCase to snake_case, or vice versa.
4. **Document Context View**:
   - Detects tags (e.g., `[h1]Header[/h1]`), their line numbers, and content (e.g., "Header").
   - Allows jumping to specific document positions based on context.
   - Filters specific tags (e.g., only `[h1]` headers).
   - Tracks cursor position within the document context in real-time.
5. **Image Preview**: Hover over `[img src="path/to/image.png"]` to preview images (requires valid paths).
6. **Tag Removal**: Right-click inside tags (e.g., `[b]Bold text[/b]`) to remove the tags, leaving only the content (e.g., `Bold text`).
7. **C++ Code Formatting**: Right-click inside `[cpp]...[/cpp]` tags to format code using `clang-format` (requires `clang-format` installed).
8. **C++ Code Compilation**: Right-click inside `[cpp]...[/cpp]` tags to compile code using `g++` (requires `g++` installed).
9. **File Statistics**: Displays STC-specific statistics, such as tag usage, alongside standard editor metrics.
10. **Breadcrumb Navigation**: A dynamically updated breadcrumb bar showing the current position in the STC document structure, with clickable navigation.
11. **Change Tracking**: Tracks modified lines using the [pydifflib-cpp](https://github.com/dominicprice/pydifflib-cpp) library.

## General Editor Features
1. **File Operations**: Load and save files with UTF-8 encoding support.
2. **Recent Files**: Remembers recently opened files for quick access.
3. **External Change Detection**: Notifies when a file is modified externally.
4. **Line Numbering**: Add numbering to selected lines via the context menu.
5. **Line Joining**: Combine multiple lines into a single line separated by spaces.
6. **File Path Copying**: Copy the file's basename or absolute path to the clipboard.
7. **Indentation Controls**: `Tab` indents selected text; `Shift+Tab` unindents.
8. **Shortcut List**: Accessible via the application menu.
9. **Text Search**:
   - Search with options for case sensitivity and whole-word matching.
   - Displays occurrence counts:
     - Case-sensitive matches.
     - Case-insensitive matches.
     - Case-sensitive whole-word matches.
     - Case-insensitive whole-word matches.
10. **Font Scaling**: Increase/decrease font size with `Ctrl++` and `Ctrl+-`.
11. **Status Bar**: Shows the number of unsaved changed lines, time of last edit, and last save (visible only when there are unsaved changes).

## Planned Features
### Before First Release
- **Fixes**:
  - Resolve issue where the editor prompts to overwrite changes when no changes exist during sequential file opening.
  - Fix multi-line code section formatting issues.
  - Fix window geometry saving (currently only position is saved, not size).
- **Features**:
  - Add `Ctrl+R` for replace functionality with an option to toggle it.
  - Validate if `[run]` tags are nested within `[pkt]` tags.
  - Add checking for unclosed tags.
  - Support bookmarks for quick navigation to specific code locations.
  - Track TODOs within the document.
  - Enable `Alt+Left` and `Alt+Right` for navigating backward/forward through code positions.
  - Support opening multiple files simultaneously.
  - Implement a side-by-side view for comparing files.

### Future Ideas
- Compile C++ code directly from the editor.
- Export code blocks to external files.
- Consolidate images into a single directory with path updates in STC tags.
- Integrate a C++ syntax analyzer (e.g., [flex](https://github.com/westes/flex)).
- Add syntax highlighting for C++ and Python using [QCXXHighlighter](https://github.com/Megaxela/QCodeEditor) (MIT license).
- Create a table generator for STC tables.
- Display real-time line change statistics (added, modified, deleted).
- Replace image URL prefixes for server-hosted images.
- Maintain an input history for undo/redo (`Ctrl+Z`).
- Support multi-word search within the same line.
- Add Polish spellchecking (e.g., using [nuspell](https://github.com/nuspell/nuspell) or Qt's spellchecker).
- Support multiple file encodings with automatic detection.
- Provide a web preview of STC content.
- Add plugin support, possibly with Lua scripting.
- Integrate cppreference documentation lookup (similar to cppman or QtCreator).
- Record and replay macros.
- Replace the context list with a tree widget for better visualization.
- Add icons to menu and context menu actions.
- Create a dedicated list for `[cpp]` code blocks.
- Show character-by-character line diffs.
- Optimize editor performance for faster typing.
- Adjust line numbering size to match font size.
- Scan documents in a separate thread for better performance.
- Highlight the current line for better cursor visibility.

## Collaboration
Contributions are welcome! Feel free to submit pull requests or suggest improvements to make this editor more user-friendly.

## Usefulness
This tool has been invaluable for creating articles on cpp0x.pl, such as the [C++ vs. Python comparison](https://cpp0x.pl/artykuly/Inne-artykuly/Porownanie-C++-i-Python-roznice-w-skladni-i-podejsciu-programistycznym/99). It eliminates the tedious task of manually inserting STC tags and debugging unclosed tags.

## Useful Links
- [Online STC Interpreter](https://cpp0x.pl/stc/)
- [Icon Source: Material Design Icons](https://pictogrammers.com/library/mdi/)

## Disclaimer
This editor is a simple tool and not thoroughly tested for reliability. Use it at your own risk, as it may not preserve critical content. That said, it has not failed the developer yet.
