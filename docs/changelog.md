# Changelog

All notable changes to Vibe Coder are documented in this file.

## [0.6.0] - 2026-03-12

### Added
- **Changes Monitor** — real-time file change tracker as bottom tab
  - QFileSystemWatcher + periodic scan (3s) detects modifications, new files, deletions
  - Split view: file list with timestamps + diff preview with syntax highlighting
  - Revert button: `git checkout -- <file>` to restore git version
  - Open button: opens changed file in editor tab
  - Badge count on tab: "Changes (N)"
  - Skips .git, .LLM, node_modules, __pycache__ directories
  - Watches up to 2000 files / 500 directories
- **Git ignored file coloring** — ignored files shown in dimmed color (dark: #5a5a5a, light: #b0b0b0) in file browser
- Git status now includes `--ignored` flag for full status visibility
- **Tabbed Settings dialog** — sections reorganized into tabs (Terminal, Editor, File Browser, Prompt, Diff Viewer, Changes Monitor)
- **Diff Viewer settings** — configurable font family, font size, background and text colors
- **Changes Monitor settings** — configurable font family, font size, background and text colors
- Global theme `applyThemeDefaults()` now cascades to diff viewer and changes monitor colors

### Changed
- Settings dialog default color scheme changed from Light to Dark
- Default prompt colors changed to dark theme (#1e1e1e bg, #d4d4d4 text)
- Settings dialog minimum size increased to 460×480 for tabbed layout
- Editor splitter wraps main tab widget (enables proper split view)

### Removed
- OutputParser and OutputPanel (AI terminal output parsing) — removed from codebase

## [0.5.0] - 2026-03-12

### Performance
- sshfs mount is now fully async — UI never blocks during SSH connect
- SSH health check runs async sequentially (one profile at a time, non-blocking)
- Git commit runs async via chained QProcess callbacks (no UI freeze)
- `which rsync` result cached — called only once per session
- Single rehighlight pass when opening files (setLanguage builds rules, setDarkTheme triggers rehighlight)
- Git status polling interval increased from 2s to 5s to reduce CPU usage
- Terminal shell start deferred until widget is placed in layout

### Security
- SSH passwords never appear in process argument lists (no sshpass -p)
- Transfers use sshfs mount (cp/rsync locally) — no SSH password needed
- Tunnels use SSH_ASKPASS mechanism with temporary script for password auth
- SSH user/host validated against shell metacharacters before use (command injection prevention)
- Git commit auto-adds .env, *.pem, *.key, credentials.json, id_rsa, id_ed25519 to .gitignore

### Memory
- Tab close properly deletes CodeEditor widget via deleteLater() (was leaking on every tab close)

## [0.4.0] - 2026-03-12

### Added
- SSH reconnect on connection loss: health timer (5s) with stat-based mount check, up to 3 auto-retry attempts
- Multiple simultaneous SSH profiles with profile switcher combo in status bar
- SFTP upload/download with rsync progress bar in status bar
- SSH port forwarding management: Local (-L) and Remote (-R) tunnels via SshTunnelDialog
- SshManager class: centralized SSH profile, health, transfer, and tunnel management
- SshTunnelDialog: create/view/remove active tunnels with QTableWidget

### Added (Prompts)
- Saved/recurring prompts per project (stored as IDs in instructions.json)
- Shift+Enter (or Ctrl+Shift+Enter) = send + save prompt
- Save Prompt button + delete (X) button for saved prompts
- Saved prompts combo with selection to fill prompt editor

### Added (SSH)
- SSH password support (masked input, never persisted)
- Saved SSH connections via QSettings (password excluded)
- Terminals auto-cd when opening remote files
- Path bar shows and accepts remote paths

### Added (File Browser)
- Dual model: QFileSystemModel (local) + QStandardItemModel (SSH)
- SSH file browsing via synchronous QDir reads over sshfs (reliable over FUSE)
- Lazy-load subdirectories on expand (dummy placeholder → real contents)
- sshfs mounts remote root (/) so all remote paths are accessible

### Added (UI)
- Bottom tab widget with Prompt and Terminal tabs
- AI-terminal (top) + Terminal (bottom) dual terminal layout

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
