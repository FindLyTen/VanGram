// VanGram: automatic backup on app quit.
//
// If the last successful backup is older than the configured interval,
// the app, when quitting, runs the regular backup flow (PowerShell helper:
// zip the app dir incl. tdata -> upload to the VPS receiver -> Mega)
// and relaunches itself afterwards. Quitting again within a short
// grace period does not trigger another backup.
#pragma once

namespace Ayu::AutoBackup {

// Returns true if an auto-backup was started; the caller should not
// perform its own quit logic (the helper relaunches the app).
[[nodiscard]] bool maybeBackupOnQuit();

// Seconds since the last backup marker was written (never negative).
// Returns -1 if there is no marker yet.
[[nodiscard]] qint64 secondsSinceLastBackup();

} // namespace Ayu::AutoBackup
