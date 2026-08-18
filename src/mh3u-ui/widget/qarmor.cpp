#include "qarmor.hpp"

#include "../../mh4g_equipment_values.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegExp>
#include <QVBoxLayout>

namespace
{
QString armorHexByte(int value)
{
    return QString("0x%1").arg(value & 0xff, 2, 16, QChar('0')).toUpper();
}
}

QArmor::QArmor(armor_t *armor, QWidget *parent) : QEquipment(NULL, parent), armor(armor)
{
    m_equipmentType = new QComboBox(this);
    m_equipmentType->addItem(uiText("(None)"), 0);
    for (const dataitem_t &type : *MH3U_DS::equipmentTypes())
        m_equipmentType->addItem(QString::fromStdString(type.identifier), type.count);
    configureSearchableComboBox(m_equipmentType);
    m_equipmentType->setEnabled(false);

    m_upgradeLevel = new QSpinBox(this);
    m_upgradeLevel->setRange(0x00, 0xff);

    m_identifier = new QComboBox(this);
    m_identifier->addItem(uiText("(None)"), 0);
    populateEquipmentIdentifierComboBox(m_identifier, MH3U_DS::equipment(armor->equipmentType), armor->identifier);
    configureSearchableComboBox(m_identifier);
    m_onlyRelicArmors = new QCheckBox("只看发掘防具", this);
    m_onlyRelicArmors->setToolTip("只筛选当前部位中明确标记为 is_relic=1 的防具。筛选不会自动替换当前装备。");

    m_firstJewelIdentifier = new QComboBox(this);
    m_secondJewelIdentifier = new QComboBox(this);
    m_thirdJewelIdentifier = new QComboBox(this);
    for (QComboBox *combo : {m_firstJewelIdentifier, m_secondJewelIdentifier, m_thirdJewelIdentifier})
    {
        combo->addItem(uiText("(None)"), 0);
        for (const dataitem_t &jewel : *MH3U_DS::jewels())
            combo->addItem(QString::fromStdString(jewel.identifier), jewel.count);
        configureSearchableComboBox(combo);
    }
    m_firstJewelFixed = new QCheckBox("固定/内置", this);
    m_secondJewelFixed = new QCheckBox("固定/内置", this);
    m_thirdJewelFixed = new QCheckBox("固定/内置", this);

    QGroupBox *basicGroup = new QGroupBox("基础", this);
    QGridLayout *basic = new QGridLayout(basicGroup);
    basic->addWidget(new QLabel(uiText("Equipment type"), basicGroup), 0, 0);
    basic->addWidget(new QLabel(uiText("Upgrade level"), basicGroup), 0, 1);
    basic->addWidget(new QLabel(uiText("Identifier"), basicGroup), 0, 2);
    basic->addWidget(new QLabel(uiText("First Jewel's Identifier"), basicGroup), 0, 3);
    basic->addWidget(new QLabel(uiText("Second Jewel's Identifier"), basicGroup), 0, 4);
    basic->addWidget(new QLabel(uiText("Third Jewel's Identifier"), basicGroup), 0, 5);
    basic->addWidget(m_equipmentType, 1, 0);
    basic->addWidget(m_upgradeLevel, 1, 1);
    basic->addWidget(m_identifier, 1, 2);
    basic->addWidget(m_firstJewelIdentifier, 1, 3);
    basic->addWidget(m_secondJewelIdentifier, 1, 4);
    basic->addWidget(m_thirdJewelIdentifier, 1, 5);
    basic->addWidget(m_firstJewelFixed, 2, 3);
    basic->addWidget(m_secondJewelFixed, 2, 4);
    basic->addWidget(m_thirdJewelFixed, 2, 5);
    basic->addWidget(m_onlyRelicArmors, 2, 2);

    m_enableAdvanced = new QCheckBox("启用发掘防具高级参数", this);
    m_enableAdvanced->setToolTip("显式发掘 ID 会自动启用；普通外观也可手动启用，不校验掉落合法性。");

    m_advancedGroup = new QGroupBox("发掘防具高级自定义", this);
    QGridLayout *advanced = new QGridLayout(m_advancedGroup);
    advanced->addWidget(new QLabel("字段", m_advancedGroup), 0, 0);
    advanced->addWidget(new QLabel("档位选择", m_advancedGroup), 0, 1);
    advanced->addWidget(new QLabel("真实值", m_advancedGroup), 0, 2);
    advanced->addWidget(new QLabel("面板值", m_advancedGroup), 0, 3);

    m_defense = new QComboBox(m_advancedGroup);
    m_defenseTrueValue = new QLabel("-", m_advancedGroup);
    m_defensePanelValue = new QLabel("-", m_advancedGroup);
    advanced->addWidget(new QLabel("防御档", m_advancedGroup), 1, 0);
    advanced->addWidget(m_defense, 1, 1);
    advanced->addWidget(m_defenseTrueValue, 1, 2);
    advanced->addWidget(m_defensePanelValue, 1, 3);

    m_resistance = new QComboBox(m_advancedGroup);
    m_resistanceValue = new QLabel("-", m_advancedGroup);
    advanced->addWidget(new QLabel("五属性抗性档", m_advancedGroup), 2, 0);
    advanced->addWidget(m_resistance, 2, 1);
    advanced->addWidget(m_resistanceValue, 2, 2, 1, 2);

    m_relicUpgrade = new QComboBox(m_advancedGroup);
    m_slots = new QComboBox(m_advancedGroup);
    m_rarity = new QComboBox(m_advancedGroup);
    m_polishRequirement = new QComboBox(m_advancedGroup);
    advanced->addWidget(new QLabel("发掘升级", m_advancedGroup), 3, 0);
    advanced->addWidget(m_relicUpgrade, 3, 1);
    advanced->addWidget(new QLabel("孔数", m_advancedGroup), 3, 2);
    advanced->addWidget(m_slots, 3, 3);
    advanced->addWidget(new QLabel("动态稀有度", m_advancedGroup), 4, 0);
    advanced->addWidget(m_rarity, 4, 1);
    advanced->addWidget(new QLabel("研磨要求", m_advancedGroup), 4, 2);
    advanced->addWidget(m_polishRequirement, 4, 3);

    m_unpolished = new QCheckBox("未研磨/锈蚀", m_advancedGroup);
    m_glow = new QCheckBox("发光", m_advancedGroup);
    advanced->addWidget(m_unpolished, 5, 0, 1, 2);
    advanced->addWidget(m_glow, 5, 2, 1, 2);

    for (QComboBox *combo : {m_defense, m_resistance, m_relicUpgrade, m_slots, m_rarity, m_polishRequirement})
    {
        configureSearchableComboBox(combo);
        combo->setToolTip("可选择预设，也可直接输入 00–FF 原始值。");
        connect(combo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCalculatedValues()));
        if (combo->lineEdit()) connect(combo->lineEdit(), SIGNAL(editingFinished()), this, SLOT(updateCalculatedValues()));
    }

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Save)->setText("保存");
    buttons->button(QDialogButtonBox::Close)->setText("关闭");
    connect(buttons, SIGNAL(accepted()), this, SLOT(saveAndAccept()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(basicGroup);
    layout->addWidget(m_enableAdvanced);
    layout->addWidget(m_advancedGroup);
    layout->addWidget(buttons);
    setLayout(layout);
    setWindowTitle(uiText("Single armor editor"));
    resize(1020, 600);

    connect(m_enableAdvanced, SIGNAL(toggled(bool)), this, SLOT(updateAdvancedState()));
    connect(m_onlyRelicArmors, SIGNAL(toggled(bool)), this, SLOT(populateIdentifiers()));
    connect(m_identifier, SIGNAL(currentIndexChanged(int)), this, SLOT(identifierChanged()));
    load();
}

void QArmor::closeEvent(QCloseEvent *) { armor = NULL; }

void QArmor::populateByteCombo(QComboBox *combo, const QString &domain, const QString &variant,
                               uint8_t currentValue)
{
    combo->clear();
    bool found = false;
    for (const lookupitem_t &value : *MH3U_DS::lookups())
    {
        if (QString::fromStdString(value.domain) != domain || value.equipmentType != armor->equipmentType ||
            QString::fromStdString(value.variant) != variant) continue;
        const QString display = QString::fromStdString(value.identifier);
        combo->addItem(QString("%1 · %2").arg(armorHexByte(value.value), display), value.value);
        combo->setItemData(combo->count() - 1, display, Qt::UserRole + 1);
        found = found || value.value == currentValue;
    }
    if (!found)
    {
        combo->addItem(QString("保留原值 %1（未识别）").arg(armorHexByte(currentValue)), currentValue);
        combo->setItemData(combo->count() - 1, QString(), Qt::UserRole + 1);
    }
    combo->setCurrentIndex(combo->findData(currentValue));
}

int QArmor::byteComboValue(QComboBox *combo, bool *ok) const
{
    bool localOk = false;
    int value = searchableComboBoxCurrentData(combo).toInt(&localOk);
    if (!localOk || combo->currentIndex() < 0)
    {
        QRegExp hex("^(?:0x)?([0-9A-Fa-f]{1,2})$");
        const QString text = combo->currentText().trimmed();
        if (hex.exactMatch(text)) value = hex.cap(1).toInt(&localOk, 16);
        else value = text.toInt(&localOk, 10);
    }
    localOk = localOk && value >= 0 && value <= 255;
    if (ok) *ok = localOk;
    return localOk ? value : -1;
}

QString QArmor::comboLookupDisplay(QComboBox *combo) const
{
    return combo->currentIndex() < 0 ? QString() : combo->itemData(combo->currentIndex(), Qt::UserRole + 1).toString();
}

void QArmor::updateAdvancedState()
{
    m_advancedGroup->setEnabled(m_enableAdvanced->isChecked());
}

void QArmor::identifierChanged()
{
    const int identifier = searchableComboBoxCurrentData(m_identifier).toInt();
    if (identifier > 0 && MH3U_DS::isRelicEquipment(armor->equipmentType, identifier))
        m_enableAdvanced->setChecked(true);
}

void QArmor::populateIdentifiers()
{
    int current = searchableComboBoxCurrentData(m_identifier).toInt();
    if (current <= 0) current = armor->identifier;
    const bool onlyRelic = m_onlyRelicArmors->isChecked();
    const dataset_t *values = MH3U_DS::equipment(armor->equipmentType);

    m_identifier->blockSignals(true);
    m_identifier->clear();
    m_identifier->addItem(uiText("(None)"), 0);
    bool currentFound = current == 0;
    if (values != NULL)
    {
        for (const dataitem_t &value : *values)
        {
            if (value.count == 0 || (onlyRelic && !value.isRelic && value.count != static_cast<uint32_t>(current)))
                continue;
            QString name = QString::fromStdString(value.identifier);
            if (isPlaceholderEquipmentName(name))
            {
                if (value.count != static_cast<uint32_t>(current)) continue;
                name = preservedPlaceholderEquipmentName(value.count);
            }
            if (onlyRelic && !value.isRelic)
                name = QString("%1（当前普通防具，保留）").arg(displayNameWithoutSearchSuffix(name));
            m_identifier->addItem(name, value.count);
            currentFound = currentFound || value.count == static_cast<uint32_t>(current);
        }
    }
    if (current > 0 && !currentFound)
        m_identifier->addItem(QString("未知装备 #%1（保留原值）").arg(current), current);
    m_identifier->setCurrentIndex(m_identifier->findData(current));
    m_identifier->blockSignals(false);
    identifierChanged();
}

void QArmor::updateCalculatedValues()
{
    const MH4GLookupNumber defense = MH4GEquipmentValues::parseLookupNumber(comboLookupDisplay(m_defense));
    if (defense.known)
    {
        m_defenseTrueValue->setText(QString::number(defense.value));
        m_defensePanelValue->setText(QString::number(defense.value));
    }
    else
    {
        m_defenseTrueValue->setText("未知");
        m_defensePanelValue->setText("未知");
    }
    QString resistance = comboLookupDisplay(m_resistance);
    resistance.replace("Fi", "火").replace("Wa", "水").replace("Th", "雷").replace("Ic", "冰").replace("Dr", "龙");
    m_resistanceValue->setText(resistance.isEmpty() ? "未知" : resistance);
}

void QArmor::load()
{
    m_equipmentType->setCurrentIndex(m_equipmentType->findData(armor->equipmentType));
    m_upgradeLevel->setValue(armor->upgradeLevel);
    m_identifier->setCurrentIndex(m_identifier->findData(armor->identifier));
    selectOrPreserveComboBoxValue(m_firstJewelIdentifier, armor->firstJewelIdentifier & 0x7fff, "装饰珠");
    selectOrPreserveComboBoxValue(m_secondJewelIdentifier, armor->secondJewelIdentifier & 0x7fff, "装饰珠");
    selectOrPreserveComboBoxValue(m_thirdJewelIdentifier, armor->thirdJewelIdentifier & 0x7fff, "装饰珠");
    m_firstJewelFixed->setChecked((armor->firstJewelIdentifier & 0x8000) != 0);
    m_secondJewelFixed->setChecked((armor->secondJewelIdentifier & 0x8000) != 0);
    m_thirdJewelFixed->setChecked((armor->thirdJewelIdentifier & 0x8000) != 0);

    populateByteCombo(m_resistance, "relic_resistance", "armor", armor->raw[12]);
    populateByteCombo(m_defense, "relic_defense", "armor", armor->raw[13]);
    populateByteCombo(m_relicUpgrade, "upgrade", "armor", armor->raw[14]);
    populateByteCombo(m_slots, "slots", "all", (armor->raw[16] >> 2) & 0x03);
    populateByteCombo(m_rarity, "rarity", "relic", armor->raw[17]);
    populateByteCombo(m_polishRequirement, "polish_requirement", "all", armor->raw[18]);
    m_unpolished->setChecked((armor->raw[16] & 0x01) != 0);
    m_glow->setChecked((armor->raw[16] & 0x02) != 0);
    bool hasAdvanced = MH3U_DS::isRelicEquipment(armor->equipmentType, armor->identifier);
    for (int offset = 12; offset <= 19; ++offset) hasAdvanced = hasAdvanced || armor->raw[offset] != 0;
    m_enableAdvanced->setChecked(hasAdvanced);
    updateAdvancedState();
    updateCalculatedValues();
}

void QArmor::save()
{
    armor->equipmentType = static_cast<uint8_t>(searchableComboBoxCurrentData(m_equipmentType).toInt());
    armor->upgradeLevel = static_cast<uint8_t>(m_upgradeLevel->value());
    armor->identifier = static_cast<uint16_t>(searchableComboBoxUnsignedValue(m_identifier, 0xffff));
    armor->firstJewelIdentifier = (m_firstJewelFixed->isChecked() ? 0x8000 : 0) |
        static_cast<uint16_t>(searchableComboBoxUnsignedValue(m_firstJewelIdentifier, 0x7fff));
    armor->secondJewelIdentifier = (m_secondJewelFixed->isChecked() ? 0x8000 : 0) |
        static_cast<uint16_t>(searchableComboBoxUnsignedValue(m_secondJewelIdentifier, 0x7fff));
    armor->thirdJewelIdentifier = (m_thirdJewelFixed->isChecked() ? 0x8000 : 0) |
        static_cast<uint16_t>(searchableComboBoxUnsignedValue(m_thirdJewelIdentifier, 0x7fff));
    if (!m_enableAdvanced->isChecked()) return;

    armor->raw[12] = static_cast<uint8_t>(byteComboValue(m_resistance));
    armor->raw[13] = static_cast<uint8_t>(byteComboValue(m_defense));
    armor->raw[14] = static_cast<uint8_t>(byteComboValue(m_relicUpgrade));
    uint8_t flags = armor->raw[16] & 0xf0;
    if (m_unpolished->isChecked()) flags |= 0x01;
    if (m_glow->isChecked()) flags |= 0x02;
    flags |= static_cast<uint8_t>((byteComboValue(m_slots) & 0x03) << 2);
    armor->raw[16] = flags;
    armor->raw[17] = static_cast<uint8_t>(byteComboValue(m_rarity));
    armor->raw[18] = static_cast<uint8_t>(byteComboValue(m_polishRequirement));
}

bool QArmor::validate()
{
    const uint8_t equipmentType = static_cast<uint8_t>(searchableComboBoxCurrentData(m_equipmentType).toInt());
    bool identifierOk = false;
    const uint16_t identifier = static_cast<uint16_t>(
        searchableComboBoxUnsignedValue(m_identifier, 0xffff, &identifierOk));
    if (equipmentType == MH3U_Type::NoneType)
    {
        QMessageBox::warning(this, windowTitle(), "装备类型不能为“无”。");
        return false;
    }
    if (MH3U_Armory::convertSubtype(static_cast<equipment_type_e>(equipmentType)) != MH3U_Type::ArmorSubtype)
    {
        QMessageBox::warning(this, windowTitle(), "当前窗口只能保存防具类型。");
        return false;
    }
    if (!identifierOk || identifier == 0)
    {
        QMessageBox::warning(this, windowTitle(), "编号不能为“无”。请先选择具体防具。");
        return false;
    }
    for (QComboBox *combo : {m_firstJewelIdentifier, m_secondJewelIdentifier, m_thirdJewelIdentifier})
    {
        bool ok = false;
        searchableComboBoxUnsignedValue(combo, 0x7fff, &ok);
        if (!ok)
        {
            QMessageBox::warning(this, windowTitle(), "装饰珠 ID 必须是 0–32767；固定标记请使用下方勾选框。");
            return false;
        }
    }
    if (m_enableAdvanced->isChecked())
    {
        for (QComboBox *combo : {m_resistance, m_defense, m_relicUpgrade, m_slots, m_rarity, m_polishRequirement})
        {
            bool ok = false;
            byteComboValue(combo, &ok);
            if (!ok)
            {
                QMessageBox::warning(this, windowTitle(), "高级字段必须是 00–FF 范围内的值。");
                return false;
            }
        }
    }
    return true;
}

void QArmor::saveAndAccept()
{
    if (!validate()) return;
    save();
    accept();
}
