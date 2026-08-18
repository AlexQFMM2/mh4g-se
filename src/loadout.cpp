#include "loadout.hpp"

#include "equipment_validator.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

#include <cstring>
#include <cmath>

namespace
{
const int kArmorTypes[5] = {
    MH3U_Type::HeadType, MH3U_Type::ChestType, MH3U_Type::ArmsType,
    MH3U_Type::WaistType, MH3U_Type::LegsType
};

void setU16(equipment_t &equipment, int offset, int value)
{
    equipment[offset] = (uint8_t)(value & 0xff);
    equipment[offset + 1] = (uint8_t)((value >> 8) & 0xff);
}

void setError(QString *error, const QString &message)
{
    if (error) *error = message;
}

bool jsonInteger(const QJsonObject &object, const char *key, int minimum, int maximum,
                 int *result, QString *error)
{
    const QJsonValue value = object.value(QString::fromLatin1(key));
    if (!value.isDouble() || !std::isfinite(value.toDouble()) ||
        std::floor(value.toDouble()) != value.toDouble() ||
        value.toDouble() < minimum || value.toDouble() > maximum)
    {
        setError(error, QString::fromUtf8("%1 必须是 %2..%3 内的整数。")
            .arg(QString::fromLatin1(key)).arg(minimum).arg(maximum));
        return false;
    }
    *result = (int)value.toDouble();
    return true;
}

QString slotName(loadout_slot_e slot)
{
    static const char *names[] = {"武器", "头", "胸", "腕", "腰", "腿", "护石"};
    return QString::fromUtf8(names[(int)slot]);
}

void addDecorationContributions(const QList<int> &decorations, int column,
                                QMap<int, QVector<int> > &values, int &usedSlots,
                                bool &slotsUnknown, QStringList &diagnostics)
{
    GameDataRepository &repository = GameDataRepository::instance();
    for (int index = 0; index < decorations.size(); ++index)
    {
        const int rawId = decorations.at(index);
        const int id = rawId & 0x7fff;
        if (id == 0) continue;
        decoration_data_t decoration = repository.decoration(id);
        if (!decoration.found || decoration.slotCount < 0)
        {
            slotsUnknown = true;
            diagnostics << QString::fromUtf8("装饰珠 ID %1 的孔位或技能效果未确认。").arg(id);
        }
        else usedSlots += decoration.slotCount;
        const QList<skill_point_data_t> points = repository.decorationSkillPoints(id);
        for (int p = 0; p < points.size(); ++p)
        {
            if (!values.contains(points.at(p).skillTreeId)) values[points.at(p).skillTreeId] = QVector<int>(LoadoutSlotCount, 0);
            values[points.at(p).skillTreeId][column] += points.at(p).points;
        }
    }
}

QJsonValue equipmentJson(const loadout_model_t &model, loadout_slot_e slot)
{
    equipment_t raw;
    if (!LoadoutCalculator::buildEquipment(model, slot, raw, NULL))
        return QJsonValue(QJsonValue::Null);
    QJsonObject object;
    object.insert("save_type", (int)raw[0]);
    object.insert("save_id", (int)(raw[2] | (raw[3] << 8)));
    object.insert("record_hex", QString::fromLatin1(QByteArray((const char *)raw, EQUIPMENT_SIZE).toHex()));
    return object;
}

bool readPiece(const QJsonValue &value, loadout_slot_e slot, loadout_piece_t *piece, QString *error)
{
    *piece = loadout_piece_t();
    if (value.isNull() || value.isUndefined()) return true;
    if (!value.isObject()) { setError(error, slotName(slot) + QString::fromUtf8("记录必须是对象或 null。")); return false; }
    QJsonObject object = value.toObject();
    int saveType = -1;
    int saveId = -1;
    if (!jsonInteger(object, "save_type", 0, 255, &saveType, error) ||
        !jsonInteger(object, "save_id", 0, 65535, &saveId, error)) return false;
    const int expected = LoadoutCalculator::expectedSaveType(slot);
    if (slot != LoadoutWeapon && saveType != expected)
    { setError(error, slotName(slot) + QString::fromUtf8("装备类型与部位不一致。")); return false; }
    if (slot == LoadoutWeapon && (saveType < 7 || saveType > 20))
    { setError(error, QString::fromUtf8("武器类型无效。")); return false; }
    if ((slot == LoadoutCharm && !GameDataRepository::instance().charmClassExists(saveId)) ||
        (slot != LoadoutCharm && !GameDataRepository::instance().candidate(saveType, saveId).found))
    { setError(error, slotName(slot) + QString::fromUtf8("的存档 ID 无法解析。")); return false; }
    const QString recordHex = object.value("record_hex").toString();
    if (recordHex.size() != EQUIPMENT_SIZE * 2 || recordHex != recordHex.toLower() ||
        recordHex.contains(QRegularExpression(QString::fromLatin1("[^0-9a-f]"))))
    { setError(error, slotName(slot) + QString::fromUtf8("的 record_hex 必须是 56 位小写十六进制。")); return false; }
    const QByteArray record = QByteArray::fromHex(recordHex.toLatin1());
    if (record.size() != EQUIPMENT_SIZE || (quint8)record.at(0) != saveType ||
        ((quint8)record.at(2) | ((quint8)record.at(3) << 8)) != saveId)
    { setError(error, slotName(slot) + QString::fromUtf8("的显式类型、ID 与原始记录不一致。")); return false; }
    QList<int> decorations;
    for (int offset = 6; offset <= 10; offset += 2)
    {
        const int jewel = ((quint8)record.at(offset) | ((quint8)record.at(offset + 1) << 8));
        if (jewel) decorations.append(jewel);
    }
    piece->selected = true; piece->saveType = saveType; piece->saveId = saveId;
    piece->decorations = decorations; piece->record = record;
    return true;
}
}

loadout_piece_t *loadout_model_t::piece(loadout_slot_e slot)
{
    switch (slot)
    {
        case LoadoutWeapon: return &weapon;
        case LoadoutHead: return &head;
        case LoadoutChest: return &chest;
        case LoadoutArms: return &arms;
        case LoadoutWaist: return &waist;
        case LoadoutLegs: return &legs;
        default: return NULL;
    }
}

const loadout_piece_t *loadout_model_t::piece(loadout_slot_e slot) const
{
    return const_cast<loadout_model_t *>(this)->piece(slot);
}

bool loadout_model_t::complete() const
{
    return weapon.selected && head.selected && chest.selected && arms.selected &&
           waist.selected && legs.selected && charm.selected;
}

void loadout_model_t::clear()
{
    const int currentGender = gender;
    *this = loadout_model_t();
    gender = currentGender;
}

loadout_summary_t::loadout_summary_t()
    : baseDefense(0), maxDefense(0), weaponDefense(0), fireRes(0), waterRes(0), iceRes(0),
      thunderRes(0), dragonRes(0), totalSlots(0), usedSlots(0), invalidCount(0), unknownCount(0),
      defenseUnknown(false), resistanceUnknown(false), slotsUnknown(false) {}

int LoadoutCalculator::expectedSaveType(loadout_slot_e slot)
{
    if (slot >= LoadoutHead && slot <= LoadoutLegs) return kArmorTypes[(int)slot - 1];
    if (slot == LoadoutCharm) return MH3U_Type::CharmType;
    return -1;
}

bool LoadoutCalculator::isRangedWeapon(int saveType)
{
    return saveType == MH3U_Type::LBGType || saveType == MH3U_Type::HBGType || saveType == MH3U_Type::BowType;
}

bool LoadoutCalculator::buildEquipment(const loadout_model_t &model, loadout_slot_e slot,
                                       equipment_t &equipment, QString *error)
{
    std::memset(equipment, 0, sizeof(equipment_t));
    if (slot == LoadoutCharm)
    {
        const loadout_charm_t &charm = model.charm;
        if (!charm.selected) { setError(error, QString::fromUtf8("护石尚未选择。")); return false; }
        if (!GameDataRepository::instance().charmCandidate(charm.classId, charm.slotCount, charm.skill1Id,
            charm.skill1Points, charm.skill2Id, charm.skill2Points).found)
        { setError(error, QString::fromUtf8("护石组合无法解析。")); return false; }
        if (charm.decorations.size() > 3 || charm.classId < 0 || charm.classId > 65535 ||
            charm.slotCount < 0 || charm.slotCount > 3 || charm.skill1Id < 0 || charm.skill1Id > 65535 ||
            charm.skill2Id < 0 || charm.skill2Id > 65535 || charm.skill1Points < -32768 || charm.skill1Points > 32767 ||
            charm.skill2Points < -32768 || charm.skill2Points > 32767)
        { setError(error, QString::fromUtf8("护石字段超出存档范围。")); return false; }
        if (charm.record.size() == EQUIPMENT_SIZE) std::memcpy(equipment, charm.record.constData(), EQUIPMENT_SIZE);
        equipment[0] = MH3U_Type::CharmType; equipment[1] = (uint8_t)charm.slotCount;
        setU16(equipment, 2, charm.classId);
        for (int offset = 6; offset <= 10; offset += 2) setU16(equipment, offset, 0);
        for (int i = 0; i < charm.decorations.size(); ++i) setU16(equipment, 6 + i * 2, charm.decorations.at(i));
        setU16(equipment, 12, charm.skill1Id); setU16(equipment, 14, (uint16_t)(int16_t)charm.skill1Points);
        setU16(equipment, 16, charm.skill2Id); setU16(equipment, 18, (uint16_t)(int16_t)charm.skill2Points);
        return true;
    }
    const loadout_piece_t *piece = model.piece(slot);
    if (!piece || !piece->selected) { setError(error, slotName(slot) + QString::fromUtf8("尚未选择。")); return false; }
    if (slot != LoadoutWeapon && piece->saveType != expectedSaveType(slot))
    { setError(error, slotName(slot) + QString::fromUtf8("装备类型与部位不一致。")); return false; }
    if (slot == LoadoutWeapon && (piece->saveType < 7 || piece->saveType > 20))
    { setError(error, QString::fromUtf8("武器类型无效。")); return false; }
    if (!GameDataRepository::instance().candidate(piece->saveType, piece->saveId).found ||
        piece->saveId < 0 || piece->saveId > 65535 || piece->decorations.size() > 3)
    { setError(error, slotName(slot) + QString::fromUtf8("装备 ID 或珠子记录无法安全编码。")); return false; }
    if (piece->record.size() == EQUIPMENT_SIZE) std::memcpy(equipment, piece->record.constData(), EQUIPMENT_SIZE);
    equipment[0] = (uint8_t)piece->saveType; setU16(equipment, 2, piece->saveId);
    for (int offset = 6; offset <= 10; offset += 2) setU16(equipment, offset, 0);
    for (int i = 0; i < piece->decorations.size(); ++i)
    {
        if (!GameDataRepository::instance().decoration(piece->decorations.at(i) & 0x7fff).found)
        { setError(error, QString::fromUtf8("装饰珠 ID %1 无法解析。").arg(piece->decorations.at(i))); return false; }
        setU16(equipment, 6 + i * 2, piece->decorations.at(i));
    }
    return true;
}

loadout_summary_t LoadoutCalculator::calculate(const loadout_model_t &model, save_format_e platform)
{
    loadout_summary_t summary;
    GameDataRepository &repository = GameDataRepository::instance();
    QMap<int, QVector<int> > values;
    for (int slotIndex = 0; slotIndex < LoadoutSlotCount; ++slotIndex)
    {
        loadout_slot_e slot = (loadout_slot_e)slotIndex;
        if (slot == LoadoutCharm)
        {
            if (!model.charm.selected) continue;
            summary.totalSlots += model.charm.slotCount;
            if (model.charm.skill1Id > 0)
            { if (!values.contains(model.charm.skill1Id)) values[model.charm.skill1Id] = QVector<int>(LoadoutSlotCount, 0);
              values[model.charm.skill1Id][slotIndex] += model.charm.skill1Points; }
            if (model.charm.skill2Id > 0)
            { if (!values.contains(model.charm.skill2Id)) values[model.charm.skill2Id] = QVector<int>(LoadoutSlotCount, 0);
              values[model.charm.skill2Id][slotIndex] += model.charm.skill2Points; }
            addDecorationContributions(model.charm.decorations, slotIndex, values, summary.usedSlots,
                                       summary.slotsUnknown, summary.diagnostics);
        }
        else
        {
            const loadout_piece_t *piece = model.piece(slot);
            if (!piece || !piece->selected) continue;
            loadout_candidate_t detail = repository.candidate(piece->saveType, piece->saveId);
            if (!detail.found) { summary.unknownCount++; summary.diagnostics << slotName(slot) + QString::fromUtf8("数据无法解析。"); continue; }
            if (detail.isRelic)
            {
                equipment_t instance;
                QString instanceError;
                if (buildEquipment(model, slot, instance, &instanceError))
                {
                    detail.slotCount = (instance[0x10] >> 2) & 3;
                    if (slot == LoadoutWeapon)
                    {
                        if (!repository.relicWeaponValues(piece->saveType, instance[0x0d], &detail.attack, &detail.affinity, &detail.defense))
                            summary.diagnostics << QString::fromUtf8("发掘武器攻击档未收录，相关数值显示为未知。");
                    }
                    else
                    {
                        detail.skillPoints.clear();
                        if (!repository.relicArmorDefense(instance[0x0d], &detail.baseDefense))
                            detail.baseDefense = -1;
                        detail.maxDefense = -1;
                        if (!repository.relicArmorResistance(instance[0x0c], &detail.fireRes, &detail.waterRes,
                                &detail.thunderRes, &detail.iceRes, &detail.dragonRes))
                            summary.resistanceUnknown = true;
                    }
                }
            }
            if (detail.slotCount < 0) summary.slotsUnknown = true; else summary.totalSlots += detail.slotCount;
            QMap<int, int>::const_iterator point = detail.skillPoints.constBegin();
            for (; point != detail.skillPoints.constEnd(); ++point)
            { if (!values.contains(point.key())) values[point.key()] = QVector<int>(LoadoutSlotCount, 0);
              values[point.key()][slotIndex] += point.value(); }
            addDecorationContributions(piece->decorations, slotIndex, values, summary.usedSlots,
                                       summary.slotsUnknown, summary.diagnostics);
            if (slot == LoadoutWeapon) { summary.weaponDefense = detail.defense; summary.baseDefense += detail.defense; summary.maxDefense += detail.defense; }
            else
            {
                if (detail.baseDefense < 0) summary.defenseUnknown = true;
                else summary.baseDefense += detail.baseDefense;
                if (detail.maxDefense < 0) summary.defenseUnknown = true;
                else summary.maxDefense += detail.maxDefense;
                if (!detail.confirmed) summary.resistanceUnknown = true;
                summary.fireRes += detail.fireRes; summary.waterRes += detail.waterRes; summary.iceRes += detail.iceRes;
                summary.thunderRes += detail.thunderRes; summary.dragonRes += detail.dragonRes;
            }
        }
        equipment_t raw;
        QString buildError;
        if (buildEquipment(model, slot, raw, &buildError))
        {
            equipment_validation_t validation = EquipmentValidator::validate(raw, platform, model.gender);
            if (validation.status == EquipmentInvalid) summary.invalidCount++;
            else if (validation.status == EquipmentUnknown) summary.unknownCount++;
            if (validation.status != EquipmentValid) summary.diagnostics << slotName(slot) + QString::fromUtf8("：") + validation.details();
        }
    }
    if (model.weapon.selected)
    {
        const int requiredCombat = isRangedWeapon(model.weapon.saveType) ? 2 : 1;
        for (int index = LoadoutHead; index <= LoadoutLegs; ++index)
        {
            const loadout_piece_t *piece = model.piece((loadout_slot_e)index);
            if (!piece || !piece->selected) continue;
            loadout_candidate_t armor = repository.candidate(piece->saveType, piece->saveId);
            if (armor.combat > 0 && armor.combat != requiredCombat)
            { summary.invalidCount++; summary.diagnostics << slotName((loadout_slot_e)index) + QString::fromUtf8("与当前武器的近战/远程类型不适用。"); }
            if (armor.gender > 0 && armor.gender != model.gender + 1)
            { summary.invalidCount++; summary.diagnostics << slotName((loadout_slot_e)index) + QString::fromUtf8("与当前配装性别不适用。"); }
        }
    }
    if (values.contains(1))
    {
        const QVector<int> torsoMarkers = values.value(1);
        const QVector<int> chestValuesTemplate = QVector<int>();
        Q_UNUSED(chestValuesTemplate);
        for (int column = LoadoutHead; column <= LoadoutLegs; ++column)
        {
            if (column == LoadoutChest || torsoMarkers.value(column) <= 0) continue;
            QList<int> keys = values.keys();
            for (int k = 0; k < keys.size(); ++k)
            {
                if (keys.at(k) == 1) continue;
                values[keys.at(k)][column] += values[keys.at(k)].value(LoadoutChest);
            }
        }
    }
    const QList<skill_tree_data_t> treeRows = repository.skillTreesDetailed();
    QMap<int, skill_tree_data_t> trees;
    for (int i = 0; i < treeRows.size(); ++i) trees.insert(treeRows.at(i).id, treeRows.at(i));
    QList<int> skillIds = values.keys();
    for (int i = 0; i < skillIds.size(); ++i)
    {
        const int id = skillIds.at(i);
        loadout_skill_row_t row;
        row.skillTreeId = id; row.name = trees.value(id).name; row.columns = values.value(id);
        row.total = 0; for (int c = 0; c < row.columns.size(); ++c) row.total += row.columns.at(c);
        row.distanceToNext = 0; row.positiveActive = false; row.negativeActive = false;
        const QList<active_skill_data_t> active = repository.activeSkills(id);
        int chosenPositive = -2147483647;
        int chosenNegative = 2147483647;
        int nextPositive = 2147483647;
        int nextNegative = -2147483647;
        QString nextPositiveName;
        QString nextNegativeName;
        for (int a = 0; a < active.size(); ++a)
        {
            const int threshold = active.at(a).points;
            if (threshold > 0 && row.total >= threshold && threshold > chosenPositive)
            { chosenPositive = threshold; row.activeSkill = active.at(a).name; row.positiveActive = true; }
            if (threshold < 0 && row.total <= threshold && threshold < chosenNegative)
            { chosenNegative = threshold; row.activeSkill = active.at(a).name; row.negativeActive = true; row.positiveActive = false; }
            if (threshold > row.total && threshold > 0 && threshold < nextPositive)
            { nextPositive = threshold; nextPositiveName = active.at(a).name; }
            if (threshold < row.total && threshold < 0 && threshold > nextNegative)
            { nextNegative = threshold; nextNegativeName = active.at(a).name; }
        }
        const int positiveDistance = nextPositive < 2147483647 ? nextPositive - row.total : 2147483647;
        const int negativeDistance = nextNegative > -2147483647 ? row.total - nextNegative : 2147483647;
        if (positiveDistance <= negativeDistance)
        {
            row.distanceToNext = positiveDistance;
            row.nextSkill = nextPositiveName;
        }
        else
        {
            row.distanceToNext = negativeDistance;
            row.nextSkill = nextNegativeName;
        }
        if (row.distanceToNext == 2147483647) row.distanceToNext = 0;
        summary.skills.append(row);
    }
    return summary;
}

QByteArray LoadoutFile::serialize(const loadout_model_t &model)
{
    QJsonObject root;
    root.insert("schema", "MH_LOADOUT"); root.insert("schema_version", 1); root.insert("game", "mh4g");
    root.insert("data_version", GameDataRepository::instance().dataVersion()); root.insert("name", model.name);
    root.insert("gender", model.gender == 1 ? "female" : "male"); root.insert("weapon", equipmentJson(model, LoadoutWeapon));
    QJsonObject armor; armor.insert("head", equipmentJson(model, LoadoutHead)); armor.insert("chest", equipmentJson(model, LoadoutChest));
    armor.insert("arms", equipmentJson(model, LoadoutArms)); armor.insert("waist", equipmentJson(model, LoadoutWaist)); armor.insert("legs", equipmentJson(model, LoadoutLegs));
    root.insert("armor", armor);
    if (model.charm.selected)
    {
        root.insert("charm", equipmentJson(model, LoadoutCharm));
    }
    else root.insert("charm", QJsonValue(QJsonValue::Null));
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool LoadoutFile::save(const QString &path, const loadout_model_t &model, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { setError(error, file.errorString()); return false; }
    QByteArray bytes = serialize(model);
    if (file.write(bytes) != bytes.size() || !file.commit()) { setError(error, file.errorString()); return false; }
    return true;
}

bool LoadoutFile::load(const QString &path, loadout_model_t *model, bool *versionWarning, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { setError(error, file.errorString()); return false; }
    return deserialize(file.readAll(), model, versionWarning, error);
}

bool LoadoutFile::deserialize(const QByteArray &bytes, loadout_model_t *model, bool *versionWarning, QString *error)
{
    if (!model) { setError(error, QString::fromUtf8("配装输出为空。")); return false; }
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (document.isNull() || !document.isObject()) { setError(error, parseError.errorString()); return false; }
    QJsonObject root = document.object();
    if (root.value("schema").toString() != "MH_LOADOUT" || root.value("schema_version").toInt() != 1 ||
        root.value("game").toString() != "mh4g")
    { setError(error, QString::fromUtf8("不是受支持的 MH4G 配装文件。")); return false; }
    loadout_model_t parsed; parsed.name = root.value("name").toString();
    const QString gender = root.value("gender").toString();
    if (gender != "male" && gender != "female") { setError(error, QString::fromUtf8("配装性别无效。")); return false; }
    parsed.gender = gender == "female" ? 1 : 0;
    if (!readPiece(root.value("weapon"), LoadoutWeapon, &parsed.weapon, error)) return false;
    if (!root.value("armor").isObject()) { setError(error, QString::fromUtf8("armor 必须是对象。")); return false; }
    QJsonObject armor = root.value("armor").toObject();
    if (!readPiece(armor.value("head"), LoadoutHead, &parsed.head, error) ||
        !readPiece(armor.value("chest"), LoadoutChest, &parsed.chest, error) ||
        !readPiece(armor.value("arms"), LoadoutArms, &parsed.arms, error) ||
        !readPiece(armor.value("waist"), LoadoutWaist, &parsed.waist, error) ||
        !readPiece(armor.value("legs"), LoadoutLegs, &parsed.legs, error)) return false;
    QJsonValue charmValue = root.value("charm");
    if (!charmValue.isNull() && !charmValue.isUndefined())
    {
        loadout_piece_t charmPiece;
        if (!readPiece(charmValue, LoadoutCharm, &charmPiece, error)) return false;
        parsed.charm.record = charmPiece.record;
        parsed.charm.classId = charmPiece.saveId;
        parsed.charm.slotCount = (quint8)charmPiece.record.at(1);
        parsed.charm.decorations = charmPiece.decorations;
        parsed.charm.skill1Id = (quint8)charmPiece.record.at(12) | ((quint8)charmPiece.record.at(13) << 8);
        parsed.charm.skill1Points = (qint16)((quint8)charmPiece.record.at(14) | ((quint8)charmPiece.record.at(15) << 8));
        parsed.charm.skill2Id = (quint8)charmPiece.record.at(16) | ((quint8)charmPiece.record.at(17) << 8);
        parsed.charm.skill2Points = (qint16)((quint8)charmPiece.record.at(18) | ((quint8)charmPiece.record.at(19) << 8));
        parsed.charm.selected = true;
    }
    if (versionWarning) *versionWarning = root.value("data_version").toString() != GameDataRepository::instance().dataVersion();
    *model = parsed;
    return true;
}

bool LoadoutSaveBridge::appendCompleteLoadout(const loadout_model_t &model, save_t *save,
                                              QList<int> *writtenIndexes, QString *error)
{
    if (!save) { setError(error, QString::fromUtf8("尚未读取存档。")); return false; }
    if (!model.complete())
    {
        QStringList missing;
        for (int slot = LoadoutWeapon; slot <= LoadoutLegs; ++slot)
        {
            const loadout_piece_t *piece = model.piece((loadout_slot_e)slot);
            if (!piece || !piece->selected) missing << slotName((loadout_slot_e)slot);
        }
        if (!model.charm.selected) missing << slotName(LoadoutCharm);
        setError(error, QString::fromUtf8("请先选择：%1。").arg(missing.join(QString::fromUtf8("、"))));
        return false;
    }
    equipment_t records[LoadoutSlotCount];
    for (int slot = 0; slot < LoadoutSlotCount; ++slot)
        if (!LoadoutCalculator::buildEquipment(model, (loadout_slot_e)slot, records[slot], error)) return false;
    QList<int> empty;
    for (int index = 0; index < 1500 && empty.size() < LoadoutSlotCount; ++index)
    {
        equipment_t &equipment = save->box[index / 100][index % 100];
        const int id = equipment[2] | (equipment[3] << 8);
        if (equipment[0] == 0 && id == 0) empty.append(index);
    }
    if (empty.size() != LoadoutSlotCount) { setError(error, QString::fromUtf8("装备箱不足七个空格。")); return false; }
    save_t staged = *save;
    for (int slot = 0; slot < LoadoutSlotCount; ++slot)
        std::memcpy(staged.box[empty.at(slot) / 100][empty.at(slot) % 100], records[slot], EQUIPMENT_SIZE);
    *save = staged;
    if (writtenIndexes) *writtenIndexes = empty;
    return true;
}
