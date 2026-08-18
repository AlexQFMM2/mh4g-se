#include "mh4g.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTextStream>
#include <QUuid>

#include <algorithm>
#include <cstring>

#if __has_include(<openssl/blowfish.h>)
#include <openssl/blowfish.h>
#else
extern "C" {
typedef unsigned int BF_LONG;
typedef struct bf_key_st {
    BF_LONG P[18];
    BF_LONG S[4 * 256];
} BF_KEY;
void BF_set_key(BF_KEY *key, int len, const unsigned char *data);
void BF_ecb_encrypt(const unsigned char *in, unsigned char *out, const BF_KEY *key, int enc);
}
#define BF_ENCRYPT 1
#define BF_DECRYPT 0
#endif

namespace
{
const char SaveKey[] = "blowfish key iorajegqmrna4itjeangmb agmwgtobjteowhv9mope";

}

bool MH4GData::load(const QString &language, QString *error)
{
    const QString normalized = language == "en" ? "en" : "cn";
    const QString root = locateDataRoot();
    if (root.isEmpty())
    {
        if (error) *error = "找不到 data 目录。请从程序目录运行，或把 data 放在程序旁边。";
        return false;
    }

    const QString databasePath = QDir(root).filePath("mh4g.sqlite");
    const QString manifestPath = QDir(root).filePath("manifest.json");
    QFile manifestFile(manifestPath);
    QFile databaseFile(databasePath);
    if (!manifestFile.open(QIODevice::ReadOnly) || !databaseFile.open(QIODevice::ReadOnly))
    {
        if (error) *error = "无法读取 mh4g.sqlite 或 manifest.json。";
        return false;
    }
    const QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    const QJsonObject databaseManifest = manifest.value("database").toObject();
    const QByteArray databaseBytes = databaseFile.readAll();
    const QString digest = QString::fromLatin1(QCryptographicHash::hash(databaseBytes, QCryptographicHash::Sha256).toHex());
    if (manifest.value("format").toString() != "mh4g-save-editor-data-manifest-v2" ||
        databaseManifest.value("sha256").toString() != digest ||
        databaseManifest.value("bytes").toVariant().toLongLong() != databaseBytes.size())
    {
        if (error) *error = "MH4G SQLite 与 manifest 校验不一致。";
        return false;
    }
    databaseFile.close();

    QVector<MH4GNamedValue> items, skills, types, decorations;
    QVector<MH4GLookupValue> lookups;
    QHash<int, QVector<MH4GNamedValue>> equipment;
    const QString connectionName = "mh4g-data-" + QUuid::createUuid().toString();
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        database.setDatabaseName(databasePath);
        database.setConnectOptions("QSQLITE_OPEN_READONLY");
        if (!database.open())
        {
            if (error) *error = "无法只读打开 MH4G SQLite：" + database.lastError().text();
            return false;
        }
        const QString nameColumn = normalized == "en" ? "name_en" : "name_cn";
        auto readNamed = [&](const QString &sql, QVector<MH4GNamedValue> &target, bool relic) -> bool {
            QSqlQuery query(database);
            if (!query.exec(sql)) { if (error) *error = query.lastError().text(); return false; }
            while (query.next())
            {
                MH4GNamedValue value;
                value.id = query.value(0).toInt(); value.name = query.value(1).toString();
                value.english = query.value(2).toString(); value.source = query.value(3).toString();
                if (query.record().count() > 4) value.rarity = query.value(4).toInt();
                if (relic && query.record().count() > 5) value.isRelic = query.value(5).toInt() != 0;
                target.push_back(value);
            }
            return true;
        };
        if (!readNamed(QString("SELECT save_id,%1,name_en,source FROM items ORDER BY save_id").arg(nameColumn), items, false) ||
            !readNamed(QString("SELECT id,%1,name_en,source FROM skill_trees ORDER BY id").arg(nameColumn), skills, false) ||
            !readNamed(QString("SELECT save_type,%1,name_en,'' FROM equipment_types ORDER BY save_type").arg(nameColumn), types, false) ||
            !readNamed(QString("SELECT save_id,%1,name_en,source FROM decorations ORDER BY save_id").arg(nameColumn), decorations, false))
            return false;
        for (int saveType = 1; saveType <= 20; ++saveType)
        {
            if (saveType == 6)
            {
                QVector<MH4GNamedValue> values;
                if (!readNamed(QString("SELECT save_id,%1,name_en,source,0,0 FROM charm_classes ORDER BY save_id").arg(nameColumn), values, true)) return false;
                equipment.insert(saveType, values);
            }
            else if (saveType <= 5 || saveType >= 7)
            {
                QVector<MH4GNamedValue> values;
                const QString table = saveType <= 5 ? "armors" : "weapons";
                if (!readNamed(QString("SELECT save_id,%1,name_en,source,rarity,is_relic FROM %2 WHERE save_type=%3 ORDER BY save_id").arg(nameColumn, table).arg(saveType), values, true)) return false;
                equipment.insert(saveType, values);
            }
        }
        QSqlQuery lookup(database);
        if (!lookup.exec(QString("SELECT domain,save_type,variant,code,%1,name_en,source FROM equipment_lookups ORDER BY domain,save_type,variant,code").arg(nameColumn)))
        { if (error) *error = lookup.lastError().text(); return false; }
        while (lookup.next())
            lookups.push_back({lookup.value(0).toString(), lookup.value(1).toInt(), lookup.value(2).toString(),
                               lookup.value(3).toInt(), lookup.value(4).toString(), lookup.value(5).toString(), lookup.value(6).toString()});
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    m_language = normalized;
    m_dataVersion = digest;
    m_items = items;
    m_skills = skills;
    m_types = types;
    m_decorations = decorations;
    m_equipment = equipment;
    m_lookups = lookups;
    return true;
}

QString MH4GData::language() const { return m_language; }
QString MH4GData::dataVersion() const { return m_dataVersion; }
const QVector<MH4GNamedValue> &MH4GData::items() const { return m_items; }
const QVector<MH4GNamedValue> &MH4GData::skills() const { return m_skills; }
const QVector<MH4GNamedValue> &MH4GData::equipmentTypes() const { return m_types; }
const QVector<MH4GNamedValue> &MH4GData::decorations() const { return m_decorations; }
const QVector<MH4GLookupValue> &MH4GData::lookups() const { return m_lookups; }

const QVector<MH4GNamedValue> &MH4GData::equipment(int type) const
{
    static const QVector<MH4GNamedValue> empty;
    const auto it = m_equipment.constFind(type);
    return it == m_equipment.constEnd() ? empty : it.value();
}

QString MH4GData::itemName(int id) const { return nameFor(m_items, id); }
QString MH4GData::equipmentTypeName(int type) const { return nameFor(m_types, type); }
QString MH4GData::equipmentName(int type, int id) const { return nameFor(equipment(type), id); }
QString MH4GData::decorationName(int id) const { return nameFor(m_decorations, id & 0x7fff); }

QVector<MH4GLookupValue> MH4GData::lookupValues(const QString &domain, int equipmentType,
                                                 const QString &variant) const
{
    QVector<MH4GLookupValue> result;
    for (const MH4GLookupValue &value : m_lookups)
    {
        if (value.domain == domain && value.equipmentType == equipmentType &&
            (variant.isEmpty() || value.variant == variant))
            result.push_back(value);
    }
    return result;
}

bool MH4GData::isRelicEquipment(int type, int id) const
{
    const QVector<MH4GNamedValue> &values = equipment(type);
    auto it = std::lower_bound(values.constBegin(), values.constEnd(), id,
        [](const MH4GNamedValue &value, int wanted) { return value.id < wanted; });
    return it != values.constEnd() && it->id == id && it->isRelic;
}

QString MH4GData::locateDataRoot() const
{
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath()).filePath("data"),
        QDir(QCoreApplication::applicationDirPath()).filePath("../data"),
        QDir::current().filePath("data"),
    };
    for (const QString &candidate : candidates)
    {
        if (QFile::exists(QDir(candidate).filePath("manifest.json")))
            return QDir(candidate).absolutePath();
    }
    return QString();
}

QString MH4GData::nameFor(const QVector<MH4GNamedValue> &values, int id)
{
    auto it = std::lower_bound(values.constBegin(), values.constEnd(), id,
        [](const MH4GNamedValue &value, int wanted) { return value.id < wanted; });
    if (it != values.constEnd() && it->id == id) return it->name;
    return QString();
}

bool MH4GSave::load(const QString &fileName)
{
    m_error.clear();
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        m_error = QString("无法打开存档：%1").arg(file.errorString());
        return false;
    }
    const QByteArray encrypted = file.readAll();
    QByteArray decrypted;
    if (!decrypt(encrypted, decrypted, &m_error)) return false;
    m_decrypted = decrypted;
    m_fileName = fileName;
    return true;
}

bool MH4GSave::save(const QString &fileName)
{
    m_error.clear();
    if (!loaded())
    {
        m_error = "没有已打开的存档。";
        return false;
    }
    QByteArray encrypted;
    if (!encrypt(m_decrypted, encrypted, &m_error)) return false;
    QSaveFile file(fileName);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly))
    {
        m_error = QString("无法写入存档：%1").arg(file.errorString());
        return false;
    }
    if (file.write(encrypted) != encrypted.size() || !file.commit())
    {
        m_error = QString("存档没有完整写入：%1").arg(file.errorString());
        return false;
    }
    m_fileName = fileName;
    return true;
}

bool MH4GSave::loaded() const { return m_decrypted.size() == FileSize; }
QString MH4GSave::fileName() const { return m_fileName; }
QString MH4GSave::lastError() const { return m_error; }

MH4GSave::Item MH4GSave::item(int slot) const
{
    if (!loaded() || slot < 0 || slot >= ItemCount) return {};
    const int offset = DataOffset + ItemOffset + slot * ItemSize;
    return {read16(m_decrypted, offset), read16(m_decrypted, offset + 2)};
}

void MH4GSave::setItem(int slot, Item value)
{
    if (!loaded() || slot < 0 || slot >= ItemCount) return;
    const int offset = DataOffset + ItemOffset + slot * ItemSize;
    write16(m_decrypted, offset, value.id);
    write16(m_decrypted, offset + 2, value.count);
}

MH4GSave::Character MH4GSave::character() const
{
    Character value;
    if (!loaded()) return value;

    QString name;
    for (int index = 0; index < 12; ++index)
    {
        const std::uint16_t codeUnit = read16(m_decrypted, DataOffset + index * 2);
        if (codeUnit == 0) break;
        name.append(QChar(codeUnit));
    }
    value.name = name;
    value.sex = static_cast<std::uint8_t>(m_decrypted.at(DataOffset + 24));
    value.hair = static_cast<std::uint8_t>(m_decrypted.at(DataOffset + 25));
    value.underwear = static_cast<std::uint8_t>(m_decrypted.at(DataOffset + 26));
    value.voice = static_cast<std::uint8_t>(m_decrypted.at(DataOffset + 27));
    value.hunterRank = read32(m_decrypted, DataOffset + 44);
    value.money = read32(m_decrypted, DataOffset + 52);
    return value;
}

void MH4GSave::setCharacter(const Character &value)
{
    if (!loaded()) return;
    for (int index = 0; index < 12; ++index)
        write16(m_decrypted, DataOffset + index * 2, 0);
    const QString name = value.name.left(11);
    for (int index = 0; index < name.size(); ++index)
        write16(m_decrypted, DataOffset + index * 2, name.at(index).unicode());
    m_decrypted[DataOffset + 24] = static_cast<char>(value.sex);
    m_decrypted[DataOffset + 25] = static_cast<char>(value.hair);
    m_decrypted[DataOffset + 26] = static_cast<char>(value.underwear);
    m_decrypted[DataOffset + 27] = static_cast<char>(value.voice);
    write32(m_decrypted, DataOffset + 44, value.hunterRank);
    write32(m_decrypted, DataOffset + 52, value.money);
}

MH4GSave::Equipment MH4GSave::equipment(int slot) const
{
    Equipment value{};
    if (!loaded() || slot < 0 || slot >= EquipmentCount) return value;
    const int offset = DataOffset + EquipmentOffset + slot * EquipmentSize;
    std::memcpy(value.data(), m_decrypted.constData() + offset, EquipmentSize);
    return value;
}

void MH4GSave::setEquipment(int slot, const Equipment &value)
{
    if (!loaded() || slot < 0 || slot >= EquipmentCount) return;
    const Equipment oldValue = equipment(slot);
    if (oldValue == value) return;
    const int offset = DataOffset + EquipmentOffset + slot * EquipmentSize;
    std::memcpy(m_decrypted.data() + offset, value.data(), EquipmentSize);
    synchronizeEquippedCopy(slot, oldValue, value);
}

void MH4GSave::synchronizeEquippedCopy(int equipmentSlot, const Equipment &oldValue,
                                       const Equipment &newValue)
{
    for (int equippedSlot = 0; equippedSlot < EquippedEquipmentCount; ++equippedSlot)
    {
        const int indexOffset = DataOffset + EquippedEquipmentIndexOffset + equippedSlot * 2;
        if (read16(m_decrypted, indexOffset) != equipmentSlot) continue;
        const int copyOffset = DataOffset + EquippedEquipmentOffset + equippedSlot * EquipmentSize;
        if (std::memcmp(m_decrypted.constData() + copyOffset, oldValue.data(), EquipmentSize) == 0)
            std::memcpy(m_decrypted.data() + copyOffset, newValue.data(), EquipmentSize);
    }
}

void MH4GSave::patchEquipmentBasic(int slot, std::uint8_t type, std::uint8_t levelOrSlots,
                                   std::uint16_t identifier,
                                   const std::array<std::uint16_t, 3> &decorations)
{
    Equipment record = equipment(slot);
    record[0] = type;
    record[1] = levelOrSlots;
    record[2] = static_cast<std::uint8_t>(identifier & 0xff);
    record[3] = static_cast<std::uint8_t>((identifier >> 8) & 0xff);
    for (int index = 0; index < 3; ++index)
    {
        const int offset = 6 + index * 2;
        record[offset] = static_cast<std::uint8_t>(decorations[index] & 0xff);
        record[offset + 1] = static_cast<std::uint8_t>((decorations[index] >> 8) & 0xff);
    }
    setEquipment(slot, record);
}

QByteArray MH4GSave::decryptedBytes() const { return m_decrypted; }

bool MH4GSave::decrypt(const QByteArray &encrypted, QByteArray &decrypted, QString *error)
{
    if (encrypted.size() != FileSize)
    {
        if (error) *error = QString("存档大小错误：%1 字节，应为 %2 字节。").arg(encrypted.size()).arg(FileSize);
        return false;
    }
    QByteArray swapped = encrypted;
    swapDwords(swapped);
    if (!cryptBlowfish(swapped, decrypted, false, error)) return false;
    swapDwords(decrypted);
    const std::uint16_t seed = read16(decrypted, 2);
    xorPayload(decrypted, seed);
    if (read16(decrypted, 0) != 0x10)
    {
        if (error) *error = "解密后的存档头无效；文件可能不是 MH4G user 存档。";
        return false;
    }
    std::uint32_t actual = 0;
    for (int index = DataOffset; index < decrypted.size(); ++index)
        actual += static_cast<unsigned char>(decrypted.at(index));
    const std::uint32_t stored = read32(decrypted, 4);
    if (stored != actual)
    {
        if (error) *error = QString("存档校验和错误：记录值 %1，实际值 %2。").arg(stored).arg(actual);
        return false;
    }
    return true;
}

bool MH4GSave::encrypt(const QByteArray &decrypted, QByteArray &encrypted, QString *error)
{
    if (decrypted.size() != FileSize || read16(decrypted, 0) != 0x10)
    {
        if (error) *error = "待保存的 MH4G 数据大小或文件头无效。";
        return false;
    }
    QByteArray prepared = decrypted;
    std::uint32_t checksum = 0;
    for (int index = DataOffset; index < prepared.size(); ++index)
        checksum += static_cast<unsigned char>(prepared.at(index));
    write32(prepared, 4, checksum);
    const std::uint16_t seed = read16(prepared, 2);
    xorPayload(prepared, seed);
    swapDwords(prepared);
    if (!cryptBlowfish(prepared, encrypted, true, error)) return false;
    swapDwords(encrypted);
    return true;
}

std::uint16_t MH4GSave::read16(const QByteArray &data, int offset)
{
    return static_cast<unsigned char>(data.at(offset)) |
        (static_cast<std::uint16_t>(static_cast<unsigned char>(data.at(offset + 1))) << 8);
}

std::uint32_t MH4GSave::read32(const QByteArray &data, int offset)
{
    return read16(data, offset) | (static_cast<std::uint32_t>(read16(data, offset + 2)) << 16);
}

void MH4GSave::write16(QByteArray &data, int offset, std::uint16_t value)
{
    data[offset] = static_cast<char>(value & 0xff);
    data[offset + 1] = static_cast<char>((value >> 8) & 0xff);
}

void MH4GSave::write32(QByteArray &data, int offset, std::uint32_t value)
{
    write16(data, offset, static_cast<std::uint16_t>(value & 0xffff));
    write16(data, offset + 2, static_cast<std::uint16_t>(value >> 16));
}

void MH4GSave::swapDwords(QByteArray &data)
{
    for (int offset = 0; offset < data.size(); offset += 4)
    {
        const char first = data.at(offset);
        const char second = data.at(offset + 1);
        data[offset] = data.at(offset + 3);
        data[offset + 1] = data.at(offset + 2);
        data[offset + 2] = second;
        data[offset + 3] = first;
    }
}

void MH4GSave::xorPayload(QByteArray &data, std::uint16_t seed)
{
    std::uint32_t key = seed;
    for (int offset = 4; offset < data.size(); offset += 2)
    {
        if (key == 0) key = 1;
        key = key * 0xB0u % 0xFF53u;
        write16(data, offset, read16(data, offset) ^ static_cast<std::uint16_t>(key));
    }
}

bool MH4GSave::cryptBlowfish(const QByteArray &input, QByteArray &output, bool encrypting, QString *error)
{
    if (input.size() % 8 != 0)
    {
        if (error) *error = "Blowfish 输入长度不是 8 字节的整数倍。";
        return false;
    }
    BF_KEY key;
    BF_set_key(&key, static_cast<int>(sizeof(SaveKey) - 1),
        reinterpret_cast<const unsigned char *>(SaveKey));
    output.resize(input.size());
    for (int offset = 0; offset < input.size(); offset += 8)
    {
        BF_ecb_encrypt(
            reinterpret_cast<const unsigned char *>(input.constData() + offset),
            reinterpret_cast<unsigned char *>(output.data() + offset),
            &key,
            encrypting ? BF_ENCRYPT : BF_DECRYPT);
    }
    return true;
}
