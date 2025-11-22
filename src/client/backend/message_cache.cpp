#include "message_cache.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

auto MessageCache::setUserId(QString const& userId) -> void
{
    user_id_ = userId;
}

auto MessageCache::userId() const -> QString
{
    return user_id_;
}

auto MessageCache::writeMessages(QString const& conversationId, QJsonArray const& messages) -> qint64
{
    if(user_id_.isEmpty()) {
        return 0;
    }

    auto const path = cacheFilePath(conversationId);

    // 计算本页中最大 seq，作为该会话最新 seq。
    auto last_seq = qint64{};
    for(auto const& item : messages) {
        auto const obj = item.toObject();
        auto const seq = static_cast<qint64>(obj.value(QStringLiteral("seq")).toDouble(0.0));
        if(seq > last_seq) {
            last_seq = seq;
        }
    }

    QJsonObject root;
    root.insert(QStringLiteral("conversationId"), conversationId);
    root.insert(QStringLiteral("messages"), messages);
    root.insert(QStringLiteral("lastSeq"), last_seq);

    auto const doc = QJsonDocument{ root };

    QFile file{ path };
    if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return 0;
    }

    file.write(doc.toJson(QJsonDocument::Compact));
    file.close();

    return last_seq;
}

auto MessageCache::appendMessage(
    QString const& conversationId,
    QString const& senderId,
    QString const& senderDisplayName,
    QString const& content,
    QString const& msgType,
    qint64 serverTimeMs,
    qint64 seq
) -> void
{
    if(user_id_.isEmpty()) {
        return;
    }

    auto const path = cacheFilePath(conversationId);

    QJsonArray messages;

    {
        QFile file{ path };
        if(file.exists() && file.open(QIODevice::ReadOnly)) {
            auto const data = file.readAll();
            file.close();

            auto const doc = QJsonDocument::fromJson(data);
            if(doc.isObject()) {
                auto const obj = doc.object();
                messages = obj.value(QStringLiteral("messages")).toArray();
            }
        }
    }

    QJsonObject message;
    message.insert(QStringLiteral("senderId"), senderId);
    message.insert(QStringLiteral("senderDisplayName"), senderDisplayName);
    message.insert(QStringLiteral("content"), content);
    message.insert(QStringLiteral("msgType"), msgType);
    message.insert(QStringLiteral("serverTimeMs"), serverTimeMs);
    message.insert(QStringLiteral("seq"), seq);

    messages.append(message);

    writeMessages(conversationId, messages);
}

auto MessageCache::loadMessages(QString const& conversationId) -> QPair<QJsonArray, qint64>
{
    if(user_id_.isEmpty()) {
        return { QJsonArray{}, 0 };
    }

    auto const path = cacheFilePath(conversationId);
    QFile file{ path };
    if(!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return { QJsonArray{}, 0 };
    }

    auto const data = file.readAll();
    file.close();

    auto const doc = QJsonDocument::fromJson(data);
    if(!doc.isObject()) {
        return { QJsonArray{}, 0 };
    }

    auto const obj = doc.object();
    auto const messages = obj.value(QStringLiteral("messages")).toArray();

    // 从缓存中恢复 lastSeq。
    auto last_seq = static_cast<qint64>(obj.value(QStringLiteral("lastSeq")).toDouble(0.0));
    if(last_seq <= 0) {
        for(auto const& item : messages) {
            auto const m = item.toObject();
            auto const seq = static_cast<qint64>(m.value(QStringLiteral("seq")).toDouble(0.0));
            if(seq > last_seq) {
                last_seq = seq;
            }
        }
    }

    return { messages, last_seq };
}

auto MessageCache::cacheBasePath() const -> QString
{
    auto dir = QDir{ QCoreApplication::applicationDirPath() };
    dir.cdUp();

    if(!dir.exists(QStringLiteral("cache"))) {
        dir.mkdir(QStringLiteral("cache"));
    }
    dir.cd(QStringLiteral("cache"));

    if(!user_id_.isEmpty()) {
        auto const sub = QStringLiteral("user_%1").arg(user_id_);
        if(!dir.exists(sub)) {
            dir.mkdir(sub);
        }
        dir.cd(sub);
    }

    return dir.absolutePath();
}

auto MessageCache::cacheFilePath(QString const& conversationId) const -> QString
{
    auto const base = cacheBasePath();
    return QDir{ base }.filePath(QStringLiteral("conv_%1.json").arg(conversationId));
}
