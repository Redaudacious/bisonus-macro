# Changelog

## 0.7.0

New:

- Each macro is now a short script. The language has variables, loops, if, wait, keydelay and text. The help has a full reference.
- The script editor colors the words and underlines the line of the first error. Open it with "Edit..." or with the code button in the panel.
- Macros made of old JSON steps change into scripts on the first start. The program keeps a copy of your old file.
- Macros can click and move the mouse. The new commands are click, mousemove and wheel. KDE asks you once to allow the keyboard and the mouse.
- You can stop a running macro in four ways. Click "Stop" in the panel, press Pause, use "Stop macro" in the tray menu, or press the key of the macro again.
- The package starts with no macros. The example macros are a template. The empty Macros page has three cards: import a file, start from a template or add a macro. You can also drop a file on it.
- At start, the program tells you what is missing from your session.
- "bisonus-macro --version" works in a terminal without a desktop.
- "bisonus-macro --cleanup" removes the KWin rule, the KWin scripts and the global keys. With "--all" it also removes your settings and macros, after a backup. General has a button for the same job.
- A package for Arch Linux and CachyOS. The installer knows these systems and tells you the AUR and pacman commands.
- The description of the program shows in the language of your system, in the menu and in the software center. English is the default and Romanian is the translation.
- The README is in English and says how to install, stop and remove the program.
- The help shows what is new in each version.

Fixed:

- The panel keeps its scroll position when you collapse or expand a category. Before, it went back to the top.
- "Apply" in the settings keeps the scroll position, the selected row and the collapsed branches of the Macros page. Before, it went back to the top.
- A script with a very deep expression now gives an error. Before, it could freeze the program.
- A comment that does not end, and "nan" or "inf" as a number, now give an error. Before, a script could wait for nothing.
- "keydelay" and "wait" need a number, and "true" and "false" cannot get a value.
