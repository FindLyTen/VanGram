// VanGram: account tags sync between devices via VPS receiver.
#pragma once

#include "base/basic_types.h"

#include <QtCore/QObject>
#include <QtCore/QJsonObject>
#include <QtNetwork/QNetworkAccessManager>

class QNetworkReply;

namespace Ayu {
namespace TagsSync {

// Push local tags map to the server (merge server-side by updated_at).
// Returns true if the request was scheduled.
bool Push(const QJsonObject &tags);

// Pull remote tags, merge with the local file (LWW by updated_at),
// save and return merged map through the callback.
void Pull(Fn<void(QJsonObject)> done);

// Convenience: serialize the on-disk tags file and push it.
bool PushFromFile();

} // namespace TagsSync
} // namespace Ayu
