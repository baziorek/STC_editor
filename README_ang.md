[![pl](https://img.shields.io/badge/lang-pl-green.svg)](https://github.com/baziorek/STC_editor/tree/master/README.md)
![Build Status: Linux](https://github.com/baziorek/STC_editor/actions/workflows/release-ubuntu22.04.yml/badge.svg)
![Build Status: Windows](https://github.com/baziorek/STC_editor/actions/workflows/release-windows.yml/badge.svg)
![Building & running tests](https://github.com/baziorek/STC_editor/actions/workflows/test-linux.yml/badge.svg)

<a href="https://en.wikipedia.org/wiki/Totus_Tuus"><img width="10%" alt="Totus Tuus" src="https://github.com/user-attachments/assets/02c77754-5d69-4688-b99b-496995de4cfe" /></a>

# STC Editor

A simple text editor built with Qt to streamline the insertion of [STC](https://cpp0x.pl/kursy/Kurs-STC/169) markup tags used by the [cpp0x.pl](https://cpp0x.pl/) platform.

## Overview

This editor is designed to simplify working with STC markup for creating content on [cpp0x.pl](https://cpp0x.pl/). It provides a straightforward Qt-based interface with a text widget and buttons for inserting STC tags, along with specialized features to enhance productivity.

![Screenshot](screens/applicationSample.png)

## Dedicated Features

If you want to use this editor for writing content for [cpp0x.pl](https://cpp0x.pl/) (which I encourage):

1. **Syntax Highlighting**: Colorizes STC markup for better readability.
2. **Tag Closure Validation**: Checks if all STC tags are properly closed.
3. **Text Transformation**: Convert selected text to lowercase, uppercase, camelCase to snake_case, or vice versa.
4. **Document Context View**:
   - Detects tags (e.g., `[h1]Header[/h1]`), their line numbers, and content (e.g., "Header").
   - Allows jumping to specific document positions based on context.
   - Filters specific tags (e.g., only `[h1]` headers).
   - Tracks cursor position within the document context in real-time.
5. **Image Preview**: Hover over `[img src="path/to/image.png"]` to preview images, including web-hosted images (requires valid paths).
6. **Tag Removal**: Right-click inside tags (e.g., `[b]Bold text[/b]`) to remove the tags, leaving only the content (e.g., `Bold text`).
7. **C++ Code Formatting**: Right-click inside `[cpp]...[/cpp]` tags to format code using `clang-format` (requires `clang-format` installed). If a `.clang-format` file is present near the text file, it will be used; otherwise, the default "LLVM" style is applied.
8. **C++ Code Compilation**: Right-click inside `[cpp]...[/cpp]` tags to compile code using `g++` (requires `g++` installed).
9. **C++ Comment Removal**: Right-click inside `[cpp]...[/cpp]` tags to remove all C++ comments from the code (uses the [StripCppComments library](https://github.com/wtwhite/StripCppComments)). It also cleans up excessive empty lines, leaving a maximum of two consecutive empty lines.
10. **Clean Up Empty Lines**: Right-click inside any text to remove excessive empty lines, leaving a maximum of two consecutive empty lines. This is useful for cleaning up text after removing comments or for general text cleanup.
10. **File Statistics**: Displays STC-specific statistics, such as tag usage, alongside standard editor metrics.
10. **Breadcrumb Navigation**: A dynamically updated breadcrumb bar showing the current position in the STC document structure, with clickable navigation.
11. **Change Tracking**: Tracks modified lines using the [pydifflib-cpp](https://github.com/dominicprice/pydifflib-cpp) library.
12. **Code Block Listing**: A separate widget that tracks the positions of `[cpp]` and other code blocks in real-time.
13. **Dedicated Drag-and-Drop**: Drag files into the application for automatic handling:
    - Image file paths are wrapped in `[img src="path/to/dragged/image.png"]` tags.
    - Text files with C/C++ extensions are inserted and wrapped in `[cpp]...[/cpp]` tags.
    - Other text files are inserted and wrapped in `[code]...[/code]` tags.
14. **Real-Time Web Preview**: Uses the [STC-to-HTML conversion backend](https://cpp0x.pl/stc/) to preview content in real-time. Statistics (data sent/received) are shown on hover over the rendering area.
15. **TODO Tracking**: Displays `TODO:` comments in the document context view, clickable to jump to their position.
16. **Clickable Links**: Hold `Ctrl` and left-click inside `[a href="..."]` or `[a href="..." name="..."]` tags to open the link.
17. **Smart Link Pasting**: Pasting a URL via `Ctrl+V` wraps it in `[a href="link"]` tags:
    - After a moment, the page title is fetched and added as a `name` attribute (e.g., `[a href="link" name="Page title"]`).
    - If text is selected, it becomes the link's name (e.g., `[a href="link" name="Selected text"]`).
    - If pasted inside `[img src=""]`, the URL is inserted as plain text.
18. **Table Pasting**: Pasting tabular data wraps it in `[csv]...[/csv]` tags with cells separated by semicolons. If the text contains tags, cells are wrapped in `[run]` tags, and the table becomes `[csv ext]`.
19. **Rich Text Pasting**: Pasting HTML from websites or Office applications partially converts it to STC tags (e.g., links, bold text).
20. **Tag Attribute Management**: Right-click on tags with optional attributes to add/remove them via the context menu.
21. **Smart STC Tag Buttons**: Buttons next to the text area support most STC tags. Clicking a button either inserts a tag at the cursor or wraps selected text:
    - The `a href` button adapts to selected text, inserting an empty tag if none is selected or detecting a link to wrap appropriately with both link and name.
    - The `img` button adapts similarly, detecting image paths and wrapping with source and description.
    - For tags like `[a href=...]` or `[img src=...]`, smart selection splits links/paths and text for proper attribute assignment.
    - If selected text is already tagged, re-tagging with the same tag removes the tags if the selection includes or is within them.
22. **Spellchecking**: Uses the [nuspell](https://nuspell.github.io/) library to check text against a Polish dictionary, underlining non-existent words. Text within STC tags is skipped unless attributes (e.g., image descriptions) require checking.
    - Context menu offers suggestions for correcting typos.

## General Editor Features

The editor is also suitable for general document editing. Here are some standout features not commonly found in other editors:

1. **File Operations**: Load, save, and reload files, with the ability to rename open files.
2. **Recent Files**: Remembers recently opened files and their last cursor positions, including the date of last access.
3. **External Change Detection**: Notifies when a file is modified externally.
4. **Smart Context Menu for Text**:
    - **Line Numbering**: Add or remove numbering for selected lines, with options to fix inconsistent numbering.
    - **Add Bullets**: Add bullet points to each selected line.
    - **Sort Lines**: Sort selected lines, ignoring case.
    - **Join Lines**: Combine multiple selected lines into one, separated by spaces.
    - **Case Conversion**: Convert selected text to lowercase, uppercase, or between camelCase and snake_case.
5. **File Path Copying**: Copy the file's basename or absolute path to the clipboard.
6. **Indentation Controls**: `Tab` indents selected text; `Shift+Tab` unindents.
7. **Shortcut List**: Accessible via the application menu.
8. **Text Search**:
    - Search with options for case sensitivity and whole-word matching.
    - Displays occurrence counts for:
      - Case-sensitive matches.
      - Case-insensitive matches.
      - Case-sensitive whole-word matches.
      - Case-insensitive whole-word matches.
    - `Enter` and `Shift+Enter` navigate to the next/previous match.
    - Up/down arrows navigate matches without shifting focus to the text editor.
    - All matches are highlighted in the text.
9. **Font Scaling**: Increase/decrease font size with `Ctrl++`, `Ctrl+-`, or `Ctrl+Mouse Scroll`.
10. **Status Bar**: Shows the open file's name, number of unsaved changed lines, last edit time, and last save time (visible only with unsaved changes).
11. **Current Line Highlighting**: Tracks the cursor's position within the current line.
12. **Detailed Unsaved Changes**: When exiting with unsaved changes, a diff table shows line-by-line differences, with options to restore specific lines from the disk version. Character-level diffs highlight subtle changes.
13. **Hideable Widgets**: All non-text-editor widgets can be hidden for faster editing.
14. **Multiple File Encodings**: Supports various encodings (not just UTF-8) using the [uchardet](https://gitlab.freedesktop.org/uchardet/uchardet) library.
15. **Work Timer**: Tracks both the time the editor is open and active editing time (pauses when no keypresses are detected).

## ⬇️ Downloads (Latest Automatically Built Version)

You can download the latest compiled version of **STC_editor** from the most recent successful GitHub Actions build (requires a GitHub account):

👉 [⬇️ Download the compiled AppImage version (for Linux)](https://github.com/baziorek/STC_editor/actions/workflows/release-ubuntu22.04.yml?query=branch%3Amaster)  
👉 [⬇️ Download the compiled Windows version](https://github.com/baziorek/STC_editor/actions/workflows/release-windows.yml?query=branch%3Amaster)

> To download:
> 1. Click on the latest successful workflow.
> 2. Scroll to the bottom of the page.
> 3. Download the artifact named `STC_editor-x86_64.AppImage` or `STC_editor-win64.zip`.

📦 [⬇️ Download the latest release](https://github.com/baziorek/STC_editor/releases/latest)  
*No published releases are currently available. They will appear here when released.*

## Planned Features

1. Built-in STC course within the application.
2. Context menu HELP section.
3. Multi-word search on the same line, regardless of order.
4. Search and Replace: `Ctrl+R` with the ability to skip specific matches.
5. Conditional Replace: Replace only if the text doesn't already match (e.g., replace `cout` with `std::cout` only if not already `std::cout`).
6. Integration of a C++ syntax analyzer (e.g., [flex](https://github.com/westes/flex)).
7. Dedicated C++ code formatting.
8. C++ and Python syntax highlighting using [QCXXHighlighter](https://github.com/Megaxela/QCodeEditor) (MIT license).
9. Real-time change statistics (line count, character count, file size, cursor line/column).
10. Input history for undo/redo (`Ctrl+Z`).
11. Faster application exit without prompting for unchanged states.
12. Integration of cppreference documentation (similar to `cppman` or QtCreator).
13. FindWidget: Update match positions in real-time when adding/removing lines.
14. Search only within code blocks.
15. Refactor CodeEditor to avoid being a God Object, possibly with AI assistance.
16. Pasting images from the clipboard (`Ctrl+V`) with a prompt to save them to a directory.
17. Application translations using QLinguist.
18. Use a single library (e.g., [diff-match-patch](https://github.com/google/diff-match-patch)) for both line and character diffs.
19. Integration with AI models (e.g., [Ollama](https://ollama.com)).
20. Display font size information during `Ctrl+Scroll` (like QtCreator).
21. Table generator for STC tables.
22. PreviewWidget: Track positions between the source document and HTML preview.
23. Line change history.
24. Precompiled headers for `codeEditor.h` and module support.
25. Integrate IWYU with CMake.
26. Code and header folding (like in IDEs).
27. Automatic content backups.
28. Use `QTextBlockUserData` for optimization (e.g., `block.setUserData(data)`).
29. Optimize editor performance for fast typing.
30. Terminal preview using [qtermwidget](https://github.com/lxqt/qtermwidget) (check GPL compatibility).
31. Replace context list with a tree widget.
32. Gradient-based line numbering to indicate recently edited lines.
33. Hide tags (except surrounding ones) for a rich text editor experience.
34. Toggle specific syntax highlighting aspects.
35. Next/Previous Change: Buttons to navigate through document changes.
36. Features inspired by other editors, e.g., [Scribe-Text-Editor](https://github.com/AleksandrHovhannisyan/Scribe-Text-Editor).
37. Webpage preview on hover.
38. Markdown support based on [Qt-Widgets/notes](https://github.com/Qt-Widgets/notes).
39. Additional widgets, e.g., [SlidingStackedWidget](https://github.com/Qt-Widgets/SlidingStackedWidget-1) or [Qwt](https://qwt.sourceforge.io/index.html).
40. Document scanning in a separate thread for better performance.
41. Macro recording and playback.
42. Plugin support, possibly with Lua scripting.
43. Replace image URL prefixes for server-hosted images.
44. Text dictation using [Whisper](https://github.com/openai/whisper).
45. `Alt+Left` and `Alt+Right` for navigating backward/forward through code positions.
46. Open multiple files simultaneously.
47. Side-by-side view for comparing files.
48. Export code blocks to external files.
49. Consolidate images into a single directory with path updates in STC tags.
50. Save individual lines to disk when comparing unsaved changes.
51. Show diff for external file changes.
52. Fix scrolling area appearing for long lines in diff views.
53. Display command results in the editor.
54. Validation Checks:
    - Ensure `[run]` tags are nested within `[pkt]` tags.
    - Verify all tags are closed (e.g., check for new tags after line edits).
    - Validate tag attributes (e.g., proper quoting, allowed attributes).
    - Ensure only valid STC tags are used.
    - Check if links exist.
    - Prevent closing unopened tags.
    - Ensure `[run]` tags are within `[csv]` or `[pkt]`.

## Mini Bugs

1. Match line numbering size to font size.
2. Font size changes interfere with new tags (e.g., `H1` tags get smaller fonts than the rest of the text).
3. Font size changes should affect line numbering font.
4. Pasting issues: Inserts "strange" characters.

## Collaboration

Contributions are welcome! Feel free to submit pull requests or suggest improvements to make this editor more user-friendly.

### Found Bugs?

Please provide detailed information about any bugs:
1. Steps to reproduce the issue.
2. STC code that caused the problem.
3. Expected behavior.

## Usefulness

This tool has been invaluable for creating articles on [cpp0x.pl](https://cpp0x.pl/), such as the [C++ vs. Python comparison](https://cpp0x.pl/artykuly/Inne-artykuly/Porownanie-C++-i-Python-roznice-w-skladni-i-podejsciu-programistycznym/99). It eliminates the tedious task of manually inserting STC tags and debugging unclosed tags.

## Useful Links

- [Online STC Interpreter](https://cpp0x.pl/stc/)
- [Icon Source: Material Design Icons](https://pictogrammers.com/library/mdi/)

## External Libraries Used

1. [pydifflib-cpp](https://github.com/dominicprice/pydifflib-cpp) - For detecting line differences (added, removed, modified). License: PSF.
2. [diff-match-patch-cpp-stl](https://github.com/leutloff/diff-match-patch-cpp-stl/) - For character-level differences within matching lines. License: Apache 2.0.
3. [uchardet](https://gitlab.freedesktop.org/uchardet/uchardet) - Supports multiple file encodings (not just UTF-8). License: Mozilla Public License.
4. [nuspell](https://nuspell.github.io/) - Spellchecking library using [Hunspell](https://hunspell.github.io/) dictionaries.
5. [StripCppComments](https://github.com/wtwhite/StripCppComments) - For removing C++ comments. License: MIT.

### Dictionaries (Polish)

The application uses [Hunspell](https://hunspell.github.io/) [dictionaries](https://github.com/nuspell/nuspell/wiki/Dictionaries-and-Contacts) ([pl_PL.aff](https://cgit.freedesktop.org/libreoffice/dictionaries/plain/pl_PL/pl_PL.aff), [pl_PL.dic](https://cgit.freedesktop.org/libreoffice/dictionaries/plain/pl_PL/pl_PL.dic)) from the LibreOffice project.

These dictionaries are licensed under the [Mozilla Public License v2.0 (MPL-2.0)](https://www.mozilla.org/MPL/2.0/).  
Original sources are available from [LibreOffice repositories](https://cgit.freedesktop.org/libreoffice/dictionaries/tree/pl_PL).

Author and license details are in:
- [README_pl.txt](dictionaries/pl/README_pl.txt)
- [README_en.txt](dictionaries/pl/README_en.txt)

## Disclaimer

This editor is a simple tool and not thoroughly tested for reliability. Use it at your own risk, as it may not preserve critical content. That said, it has not failed the developer yet.
