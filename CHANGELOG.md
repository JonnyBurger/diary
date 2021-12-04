## [Unreleased]

### Changed
* New s shortcut logic: s no longer jumps to specific day (search). f shortcut is used instead to "find" date (drop-in replacement). s shortcut is used to "sync" entries to Google calendar (CalDAV sync).

### Added
* CalDAV sync: s/S shortcuts to sync entries to Google calendar.
* Import (#76, #77): i shortcut to import entries from .ics
* Export (#78): E shortcut to export entries to .ics
* External text format command (#68, #80) --fmt-cmd
* Mouse support for selecting dates (#60)
* Documentation for build pipelines (CI/CD) and Open Build Service (OBS) jobs (`docs/CI.md` and `docs/OBS.md`)
* Documentation of testing procedures (`docs/TESTING.md`)

## [0.4] - 2020-10-17

### Changed
* Fix install on OSX (#41)
* Improve Makefile and build instructions

## [0.3] - 2016-12-28

### Changed
* Escape characters and utf-8 fixes
* First day of week according to locale

### Added
* Header file
* Constant parameters
* Jump to nearest entry
* Man page

## [0.2] - 2016-11-27

### Changed
* Fixes trailing '/' in entry reading

### Added
* Remove entries
* Search entries, jump to date

## [0.1] - 2016-11-24

### Notes
* There is still a bug which appends '/' to the diary directory even if the trailing '/' is already given.
* Removing entries not yet possible