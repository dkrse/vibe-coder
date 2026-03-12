# Changelog

All notable changes to Vibe Coder are documented in this file.

## [0.3.0] - 2026-03-12

### Performance
- Git status polling is now fully async (non-blocking QProcess pipeline)
- Replaced `-uall` with `-unormal` in git status for faster execution on large repos
- Directory git status uses precomputed cache — O(1) lookup instead of O(n) iteration
- File open warns on files larger than 5MB to prevent UI slowdown

### Security
- `.LLM/` directory automatically added to `.gitignore` on project creation
- Path traversal protection in New File, New Directory, and Rename operations
- Delete confirmation now shows full file path
- Delete dialog defaults to "No"

### Fixed
- File save (Ctrl+S) now shows error message on failure (permissions, disk full)
- File save confirms success in status bar
- Context menu respects file browser Dark/Light theme
- Prompt numbering: each prompt in instructions.json now has sequential `id` field

## [0.2.0] - 2026-03-12

### Added
- File browser Dark/Light theme setting (replaced raw color pickers)
- Editable text editor (was read-only)
- Ctrl+S to save current file
- Smart file reload: skips if content matches (avoids overwriting user edits)
- File browser Zed editor-style rendering with git status colors
- Git status colors propagate to parent directories
- Context menu: New File, New Directory, Rename, Delete
- Session persistence: window geometry, splitter sizes, open files, active tab

### Changed
- File browser settings use theme combo instead of color buttons

## [0.1.0] - 2026-03-12

### Added
- Initial release
- Qt6 Widgets application with horizontal/vertical splitter layout
- File browser (left) with QFileSystemModel, shows hidden files
- Terminal emulator (QTermWidget) supporting interactive apps (claude, mc, vim)
- Code editor tabs with syntax highlighting (C/C++, Python, JS/TS, Rust)
- Line number area with dark/light theme support
- Prompt input window with Send and Commit buttons
- Configurable send key: Ctrl+Enter or Enter
- Hamburger menu: Create Project, Edit Project, Settings
- Project system: .LLM/instructions.json with prompt history
- Git commit integration: auto-init, add -A, commit with timestamp
- Comprehensive Settings dialog: Terminal, Editor, File Browser, Prompt
- Terminal follows file browser directory changes
