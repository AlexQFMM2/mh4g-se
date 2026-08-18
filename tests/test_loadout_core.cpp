#include "loadout.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <cstring>
#include <iostream>

namespace
{
void fail(const QString &message)
{
    std::cerr << message.toStdString() << std::endl;
    std::exit(1);
}

loadout_candidate_t firstCandidate(int type)
{
    equipment_query_t query;
    query.confirmedOnly = false;
    query.limit = 1;
    const QList<loadout_candidate_t> values = GameDataRepository::instance().queryCandidates(type, query);
    if (values.isEmpty()) fail(QString("no candidate for type %1").arg(type));
    return values.first();
}

QByteArray recordFor(int type, int id, int marker)
{
    QByteArray record(EQUIPMENT_SIZE, '\0');
    record[0] = (char)type;
    record[2] = (char)(id & 0xff);
    record[3] = (char)((id >> 8) & 0xff);
    record[27] = (char)marker;
    return record;
}

void setPiece(loadout_piece_t &piece, int type, int marker)
{
    const loadout_candidate_t value = firstCandidate(type);
    piece.selected = true;
    piece.saveType = type;
    piece.saveId = value.saveId;
    piece.record = recordFor(type, value.saveId, marker);
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const QDir applicationDirectory(QCoreApplication::applicationDirPath());
    QString database = QFileInfo(applicationDirectory.filePath("../../data/mh4g.sqlite")).absoluteFilePath();
    if (!QFileInfo::exists(database))
        database = QFileInfo(applicationDirectory.filePath("../../../data/mh4g.sqlite")).absoluteFilePath();
    if (!GameDataRepository::instance().open(database)) fail(GameDataRepository::instance().errorString());

    equipment_query_t relicQuery;
    relicQuery.onlyRelic = true;
    relicQuery.confirmedOnly = false;
    relicQuery.limit = 500;
    const QList<loadout_candidate_t> relicWeapons = GameDataRepository::instance().queryCandidates(-1, relicQuery);
    if (relicWeapons.isEmpty()) fail("relic-only query returned no weapons");
    for (int index = 0; index < relicWeapons.size(); ++index)
        if (!relicWeapons.at(index).isRelic) fail("relic-only query returned a normal weapon");

    equipment_query_t relicGreatSwordsQuery;
    relicGreatSwordsQuery.onlyRelic = true;
    relicGreatSwordsQuery.confirmedOnly = false;
    relicGreatSwordsQuery.weaponType = MH3U_Type::GSType;
    relicGreatSwordsQuery.limit = 500;
    const QList<loadout_candidate_t> relicGreatSwords = GameDataRepository::instance().queryCandidates(-1, relicGreatSwordsQuery);
    if (relicGreatSwords.isEmpty()) fail("relic great-sword query returned no weapons");
    for (int index = 0; index < relicGreatSwords.size(); ++index)
        if (!relicGreatSwords.at(index).isRelic || relicGreatSwords.at(index).saveType != MH3U_Type::GSType)
            fail("relic great-sword query did not combine its filters");

    equipment_query_t relicHeadQuery;
    relicHeadQuery.onlyRelic = true;
    relicHeadQuery.confirmedOnly = false;
    relicHeadQuery.limit = 500;
    const QList<loadout_candidate_t> relicHeadArmors = GameDataRepository::instance().queryCandidates(MH3U_Type::HeadType, relicHeadQuery);
    if (relicHeadArmors.isEmpty()) fail("relic head-armor query returned no armors");
    for (int index = 0; index < relicHeadArmors.size(); ++index)
        if (!relicHeadArmors.at(index).isRelic || relicHeadArmors.at(index).saveType != MH3U_Type::HeadType)
            fail("relic head-armor query returned a normal or wrong-slot armor");

    loadout_model_t original;
    original.name = QString::fromUtf8("28 字节回归");
    original.gender = 0;
    setPiece(original.weapon, 7, 0xa1);
    setPiece(original.head, 5, 0xa2);
    setPiece(original.chest, 1, 0xa3);
    setPiece(original.arms, 2, 0xa4);
    setPiece(original.waist, 3, 0xa5);
    setPiece(original.legs, 4, 0xa6);
    const loadout_candidate_t charm = firstCandidate(6);
    original.charm.selected = true;
    original.charm.classId = charm.classId;
    original.charm.record = recordFor(6, charm.classId, 0xa7);
    original.charm.skill1Points = -300;
    original.charm.record[14] = (char)0xd4;
    original.charm.record[15] = (char)0xfe;

    const QByteArray payload = LoadoutFile::serialize(original);
    loadout_model_t decoded;
    QString error;
    if (!LoadoutFile::deserialize(payload, &decoded, NULL, &error)) fail(error);
    for (int slot = 0; slot < LoadoutSlotCount; ++slot)
    {
        equipment_t left, right;
        if (!LoadoutCalculator::buildEquipment(original, (loadout_slot_e)slot, left, &error) ||
            !LoadoutCalculator::buildEquipment(decoded, (loadout_slot_e)slot, right, &error)) fail(error);
        if (std::memcmp(left, right, EQUIPMENT_SIZE) != 0) fail(QString("slot %1 did not round trip").arg(slot));
    }

    save_t save;
    std::memset(&save, 0, sizeof(save));
    for (int index = 0; index < 1493; ++index) save.box[index / 100][index % 100][0] = 1;
    QList<int> written;
    if (!LoadoutSaveBridge::appendCompleteLoadout(decoded, &save, &written, &error)) fail(error);
    if (written.size() != 7 || written.first() != 1493 || written.last() != 1499) fail("did not use all 1500 equipment slots");

    save_t full = save;
    full.box[14][99][0] = 1;
    const save_t before = full;
    if (LoadoutSaveBridge::appendCompleteLoadout(decoded, &full, NULL, &error)) fail("import unexpectedly succeeded without seven empty slots");
    if (std::memcmp(&before, &full, sizeof(save_t)) != 0) fail("failed import changed the equipment box");

    GameDataRepository::instance().close();
    return 0;
}
