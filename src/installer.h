/*
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) The OpenTyrian Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef INSTALLER_H
#define INSTALLER_H

// Tyrian 2000 data installer core.
//
// Detects, downloads, verifies, extracts and installs the Tyrian 2000 data, or
// installs it from a local tyrian2000.zip or an existing folder (GOG included).
// Local zips use the same pinned size and SHA-256 as downloads. Repacked or
// non-canonical data can be imported as a folder, subject to the provider's
// structural checks; the result and differing file names are logged.
// It has no access to the game state: the launcher, a headless CLI
// (--install-2000=...) and the tests all drive the same small API.
//
// Nothing is ever half-installed.  A download goes to <parent>/tyrian2000.zip.part,
// an extraction or copy to <parent>/data-tyrian2000.tmp, and only a validated
// tree is renamed into the install location.  A failed, cancelled or interrupted
// run leaves the previous install (if any) untouched, and a retry starts clean.
// An OS-held lock prevents overlapping installers; after a process interruption,
// a retry restores any complete .old backup before clearing the staging area.
// Nothing from the archive or a chosen folder is ever modified or deleted.
//
// ---- How the launcher uses it ------------------------------------------------
//
//   InstallerStatus status;
//   installerDetect(&status);                 // cheap; call on entry and after an install
//   if (!status.installed) show "INSTALL", status.message, status.installDirectory
//
//   installerBeginDownload();                 // or ...FromZip(path) / ...FromFolder(path)
//   every frame:
//       InstallerProgress p;
//       bool working = installerPoll(&p);     // never blocks; safe to call every frame
//       draw p.message, and a bar of p.bytesDone / p.bytesTotal
//       if (!working) break;                  // p.state is DONE, FAILED or CANCELLED
//   Esc/B while working: installerCancel();   // still poll until it reports CANCELLED
//
// The work runs in one SDL thread.  All functions are for the UI thread only.
// After DONE the data is at p.installedPath and installerDetect() reports it,
// so gameDataLocate() finds it with no further step.  The last result stays
// readable through installerPoll() until the next Begin call.
//
// p.message is always a short, user-facing sentence, safe to draw as is; on a
// verify failure it is exactly INSTALLER_MESSAGE_VERIFY_FAILED.  When curl is
// missing, p.error is INSTALLER_ERROR_NO_CURL and the UI should offer the manual
// install (a zip or a folder).  installerSuggestFolders() lists likely folders
// to offer, and any user-given path is always accepted.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define INSTALLER_PATH_MAX 1024
#define INSTALLER_MESSAGE_MAX 320
#define INSTALLER_SUGGESTIONS_MAX 12

// The one official source (the author's site), and what it must contain.
#define INSTALLER_DOWNLOAD_URL "https://www.camanis.net/tyrian/tyrian2000.zip"
#define INSTALLER_EXPECTED_FILENAME "tyrian2000.zip"
#define INSTALLER_EXPECTED_SIZE 5051363u
#define INSTALLER_EXPECTED_SHA256 "348bc76e73514e452279b8730cf217daf0f70a282f07b6b94af653d87e921667"

#define INSTALLER_MESSAGE_VERIFY_FAILED "The downloaded Tyrian 2000 data could not be verified. Please try again or install the data manually."

typedef enum
{
	INSTALLER_IDLE,           // nothing started since launch
	INSTALLER_DOWNLOADING,    // bytesDone / bytesTotal is the download
	INSTALLER_VERIFYING,      // size and SHA-256 of the archive
	INSTALLER_EXTRACTING,     // unpacking or copying; bytesDone / bytesTotal in bytes
	INSTALLER_VALIDATING,     // manifest and structure of the new files
	INSTALLER_INSTALLING,     // the atomic rename
	INSTALLER_DONE,           // installed; installedPath is set
	INSTALLER_FAILED,         // nothing installed; error and message say why
	INSTALLER_CANCELLED       // installerCancel() took effect; nothing installed
} InstallerState;

typedef enum
{
	INSTALLER_OK,
	INSTALLER_ERROR_NO_CURL,        // curl could not be started: offer the manual install
	INSTALLER_ERROR_NETWORK,        // the download failed
	INSTALLER_ERROR_VERIFY,         // size / SHA-256 / manifest of a download did not match
	INSTALLER_ERROR_BAD_ARCHIVE,    // not a usable zip (corrupt, unsafe entries, too large)
	INSTALLER_ERROR_NOT_FOUND,      // the path, or the Tyrian 2000 files in it, do not exist
	INSTALLER_ERROR_WRONG_VARIANT,  // Tyrian 2.x / 1.x data given as Tyrian 2000
	INSTALLER_ERROR_INVALID_DATA,   // Tyrian 2000 data, but incomplete or malformed
	INSTALLER_ERROR_IO,             // disk full, no permission, no install location
	INSTALLER_ERROR_BUSY            // installerBegin*() while another install is running
} InstallerError;

typedef struct
{
	bool installed;                            // valid Tyrian 2000 data is where the game will look
	char path[INSTALLER_PATH_MAX];             // that directory (empty when not installed)
	char installDirectory[INSTALLER_PATH_MAX]; // where an install would go (empty: no user location)
	char message[INSTALLER_MESSAGE_MAX];       // one line: what was found, or why not
} InstallerStatus;

typedef struct
{
	InstallerState state;
	InstallerError error;                      // INSTALLER_OK unless state is FAILED
	uint64_t bytesDone, bytesTotal;            // 0 / 0 when the state has no byte count
	bool nonCanonical;                         // DONE: valid, but not the original release's files
	char message[INSTALLER_MESSAGE_MAX];
	char installedPath[INSTALLER_PATH_MAX];    // DONE only
} InstallerProgress;

typedef struct
{
	char path[INSTALLER_PATH_MAX];
	bool hasData;                              // Tyrian 2000 files were found in or below it
} InstallerSuggestion;

// Where the installer puts the data (the game's default search reads it too):
//   Windows        %APPDATA%/OpenTyrian/data-tyrian2000
//   macOS          ~/Library/Application Support/OpenTyrian/data-tyrian2000
//   Linux/SteamOS  $XDG_DATA_HOME/opentyrian/data-tyrian2000, else ~/.local/share/...
//   portable mode  data-tyrian2000/ beside the executable (opentyrian.cfg there)
// Returns false (and an empty string) when the platform has no such location.
bool installerInstallDirectory(char *out, size_t size);

// Is valid Tyrian 2000 data where the game's default search would load it?
void installerDetect(InstallerStatus *status);

// Existing folders where a Tyrian 2000 install (GOG, Steam, ...) commonly lives,
// most likely first.  Returns how many were written to out (at most max).
// Only a hint for a picker; any path can be given to installerBeginFromFolder().
int installerSuggestFolders(InstallerSuggestion *out, int max);

// Start an install.  Each returns false, leaving the current state alone, if
// another install is still running (installerPoll() to see it) or the argument
// is missing; otherwise the state moves to a working one and the call returns
// at once.
bool installerBeginDownload(void);
bool installerBeginFromZip(const char *path);
bool installerBeginFromFolder(const char *path);

// Copies the current state.  Returns true while the install is still working;
// false when it is IDLE or has finished (DONE, FAILED or CANCELLED).
bool installerPoll(InstallerProgress *progress);

// Asks the running install to stop.  Kills a running curl, removes the partial
// download and temporary files, installs nothing.  A no-op when nothing runs.
void installerCancel(void);

// Cancels and waits for the worker thread; call before the process exits.
void installerShutdown(void);

// ---- headless / tests ----------------------------------------------------------

// Reads the option's value out of argv: --install-2000=download|detect|selftest|<zip or folder>
// (also "--install-2000 VALUE"), and --install-2000-spec=FILE.  NULL when absent.
const char *installerCliArgument(int argc, char *argv[]);
const char *installerCliSpecArgument(int argc, char *argv[]);

// Runs the option's request to completion, printing progress lines, and returns
// true on success (exit 0) or false (exit 1).  "detect" succeeds when Tyrian
// 2000 is installed; "selftest" runs the SHA-256 / cksum known-answer tests.
bool installerRunCli(const char *request, const char *spec);

// Test-only: replaces the canonical archive size, SHA-256 and manifest with those
// in a text file ("archive-size N", "archive-sha256 HEX", then "size crc name"
// lines) so the suite can install synthetic data without any real game file.
// Only --install-2000-spec reaches this; the launcher never does.
bool installerLoadTestSpec(const char *path);

#endif // INSTALLER_H
