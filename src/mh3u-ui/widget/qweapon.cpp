#include "qweapon.hpp"

#include "../../mh4g_equipment_values.hpp"

#include <QCheckBox>
#include <QColor>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegExp>
#include <QSpinBox>
#include <QVBoxLayout>

namespace
{
QString hexByte(int value)
{
    return QString("0x%1").arg(value & 0xff, 2, 16, QChar('0')).toUpper();
}

bool isKnownOverflow(const QString &domain, const QString &variant, int value)
{
    if (domain == "attack_tier") return value > 0x14;
    if (domain == "sharpness" && variant == "melee") return value > 0x15;
    if (domain == "attribute_value") return value > 0x19;
    if (domain == "status_value") return value > 0x11;
    return false;
}

QString sharpnessHtml(int code, const MH4GSharpnessResult &sharpness)
{
    static const char *names[] = {"红", "橙", "黄", "绿", "蓝", "白", "紫"};
    static const char *colors[] = {
        "#ed1c24", "#fca31d", "#fff200", "#00b83f", "#3f48cc", "#ffffff", "#b346b3",
    };

    QString details;
    QString cells;
    for (int index = 0; index < 7; ++index)
    {
        if (!details.isEmpty()) details += "　";
        details += QString::fromUtf8(names[index]) + QString::number(sharpness.lengths[index]);
        if (sharpness.lengths[index] > 0)
        {
            const int width = qMax(2, qRound(sharpness.lengths[index] * 0.8));
            cells += QString("<td width=\"%1\" bgcolor=\"%2\">&nbsp;</td>")
                         .arg(width).arg(QString::fromLatin1(colors[index]));
        }
    }
    const int unused = qMax(0, 450 - sharpness.total());
    if (unused > 0)
        cells += QString("<td width=\"%1\" bgcolor=\"#c3c3c3\">&nbsp;</td>")
                     .arg(qMax(2, qRound(unused * 0.8)));

    const QString evidence = sharpness.provisional
        ? " · 推定映射，待实机确认"
        : " · MH4U 图表反向";
    return QString("<div>方案 %1 · %2 · 合计 %3/450%4</div>"
                   "<table cellspacing=\"0\" cellpadding=\"0\" border=\"1\"><tr>%5</tr></table>")
        .arg(hexByte(code), details).arg(sharpness.total()).arg(evidence, cells);
}
}

QWeapon::QWeapon(weapon_t *weapon, QWidget *parent) : QEquipment(NULL, parent), weapon(weapon)
{
    m_equipmentType = new QComboBox(this);
    m_equipmentType->addItem(uiText("(None)"), 0);
    for (uint32_t i = 0; i < MH3U_DS::equipmentTypes()->size(); i++)
        m_equipmentType->addItem(QString(MH3U_DS::equipmentTypes()->at(i).identifier.c_str()),
                                 MH3U_DS::equipmentTypes()->at(i).count);
    configureSearchableComboBox(m_equipmentType);
    m_equipmentType->setEnabled(false);

    m_identifier = new QComboBox(this);
    configureSearchableComboBox(m_identifier);
    m_onlyRelicWeapons = new QCheckBox("只看发掘武器", this);
    m_onlyRelicWeapons->setToolTip("只筛选当前武器类型中明确标记为 is_relic=1 的武器。筛选不会自动替换当前装备。");

    m_firstJewelIdentifier = new QComboBox(this);
    m_secondJewelIdentifier = new QComboBox(this);
    m_thirdJewelIdentifier = new QComboBox(this);
    for (QComboBox *combo : {m_firstJewelIdentifier, m_secondJewelIdentifier, m_thirdJewelIdentifier})
    {
        combo->addItem(uiText("(None)"), 0);
        for (uint32_t i = 0; i < MH3U_DS::jewels()->size(); i++)
            combo->addItem(QString(MH3U_DS::jewels()->at(i).identifier.c_str()),
                           MH3U_DS::jewels()->at(i).count);
        configureSearchableComboBox(combo);
    }
    m_firstJewelFixed = new QCheckBox("固定/内置", this);
    m_secondJewelFixed = new QCheckBox("固定/内置", this);
    m_thirdJewelFixed = new QCheckBox("固定/内置", this);

    QGroupBox *basicGroup = new QGroupBox("基础", this);
    QGridLayout *basic = new QGridLayout(basicGroup);
    basic->addWidget(new QLabel(uiText("Equipment type"), basicGroup), 0, 0);
    basic->addWidget(new QLabel(uiText("Identifier"), basicGroup), 0, 1);
    basic->addWidget(new QLabel(uiText("First Jewel's Identifier"), basicGroup), 0, 2);
    basic->addWidget(new QLabel(uiText("Second Jewel's Identifier"), basicGroup), 0, 3);
    basic->addWidget(new QLabel(uiText("Third Jewel's Identifier"), basicGroup), 0, 4);
    basic->addWidget(m_equipmentType, 1, 0);
    basic->addWidget(m_identifier, 1, 1);
    basic->addWidget(m_firstJewelIdentifier, 1, 2);
    basic->addWidget(m_secondJewelIdentifier, 1, 3);
    basic->addWidget(m_thirdJewelIdentifier, 1, 4);
    basic->addWidget(m_onlyRelicWeapons, 2, 1);
    basic->addWidget(m_firstJewelFixed, 2, 2);
    basic->addWidget(m_secondJewelFixed, 2, 3);
    basic->addWidget(m_thirdJewelFixed, 2, 4);

    m_relicStatus = new QLabel(this);
    m_relicStatus->setWordWrap(true);

    m_advancedGroup = new QGroupBox("发掘武器高级自定义", this);
    QGridLayout *advanced = new QGridLayout(m_advancedGroup);
    advanced->addWidget(new QLabel("字段", m_advancedGroup), 0, 0);
    advanced->addWidget(new QLabel("档位选择", m_advancedGroup), 0, 1);
    advanced->addWidget(new QLabel("真实值", m_advancedGroup), 0, 2);
    advanced->addWidget(new QLabel("面板值", m_advancedGroup), 0, 3);

    m_attackTier = new QComboBox(m_advancedGroup);
    m_attackTrueValue = new QLabel("-", m_advancedGroup);
    m_attackPanelValue = new QLabel("-", m_advancedGroup);
    advanced->addWidget(new QLabel("攻击档", m_advancedGroup), 1, 0);
    advanced->addWidget(m_attackTier, 1, 1);
    advanced->addWidget(m_attackTrueValue, 1, 2);
    advanced->addWidget(m_attackPanelValue, 1, 3);

    m_attributeType = new QComboBox(m_advancedGroup);
    advanced->addWidget(new QLabel("属性类型", m_advancedGroup), 2, 0);
    advanced->addWidget(m_attributeType, 2, 1, 1, 3);

    m_attributeValue = new QComboBox(m_advancedGroup);
    m_attributeTrueValue = new QLabel("-", m_advancedGroup);
    m_attributePanelValue = new QLabel("-", m_advancedGroup);
    advanced->addWidget(new QLabel("属性值", m_advancedGroup), 3, 0);
    advanced->addWidget(m_attributeValue, 3, 1);
    advanced->addWidget(m_attributeTrueValue, 3, 2);
    advanced->addWidget(m_attributePanelValue, 3, 3);

    m_sharpness = new QComboBox(m_advancedGroup);
    m_sharpnessValue = new QLabel("七色长度", m_advancedGroup);
    m_sharpnessValue->setTextFormat(Qt::RichText);
    m_sharpnessValue->setMinimumWidth(380);
    advanced->addWidget(new QLabel("斩味/弹匣/瓶配置", m_advancedGroup), 4, 0);
    advanced->addWidget(m_sharpness, 4, 1);
    advanced->addWidget(m_sharpnessValue, 4, 2, 1, 2);

    m_upgrade = new QComboBox(m_advancedGroup);
    m_weaponSpecial = new QComboBox(m_advancedGroup);
    m_slots = new QComboBox(m_advancedGroup);
    m_rarity = new QComboBox(m_advancedGroup);
    m_polishRequirement = new QComboBox(m_advancedGroup);
    m_honing = new QComboBox(m_advancedGroup);
    const bool isBowgun = weapon->equipmentType == MH4G_Type::LBGType ||
                          weapon->equipmentType == MH4G_Type::HBGType;
    const bool isInsectGlaive = weapon->equipmentType == MH4G_Type::IGType;
    m_levelOrModificationLabel = new QLabel(isInsectGlaive ? "猎虫等级 (0x01)" :
        (isBowgun ? "弩改造位 (0x01)" : "等级/改造原值 (0x01)"), m_advancedGroup);
    m_levelOrModification = new QSpinBox(m_advancedGroup);
    m_levelOrModification->setRange(0, 0xff);
    m_levelOrModification->setDisplayIntegerBase(16);
    m_levelOrModification->setPrefix("0x");
    m_levelOrModification->setEnabled(isBowgun || isInsectGlaive);
    m_levelOrModification->setToolTip(isBowgun
        ? "保留完整字节；下方开关只修改 bit5/bit3，其余位不变。"
        : "猎虫等级允许 00–FF，不校验正常养成上限。");
    m_bowgunRawSummary = new QLabel(m_advancedGroup);
    int row = 5;
    advanced->addWidget(m_levelOrModificationLabel, row, 0);
    advanced->addWidget(m_levelOrModification, row, 1);
    advanced->addWidget(m_bowgunRawSummary, row++, 2, 1, 2);

    m_bowgunLimitBreakBit = new QCheckBox("bit5 · 0x20（限制解除待实机确认）", m_advancedGroup);
    m_bowgunAttachmentBit = new QCheckBox("bit3 · 0x08（弩配件待实机确认）", m_advancedGroup);
    m_bowgunLimitBreakBit->setVisible(isBowgun);
    m_bowgunAttachmentBit->setVisible(isBowgun);
    advanced->addWidget(m_bowgunLimitBreakBit, row, 0, 1, 2);
    advanced->addWidget(m_bowgunAttachmentBit, row++, 2, 1, 2);

    advanced->addWidget(new QLabel("发掘升级", m_advancedGroup), row, 0);
    advanced->addWidget(m_upgrade, row, 1);
    advanced->addWidget(new QLabel("武器专属值", m_advancedGroup), row, 2);
    advanced->addWidget(m_weaponSpecial, row++, 3);
    advanced->addWidget(new QLabel("孔数", m_advancedGroup), row, 0);
    advanced->addWidget(m_slots, row, 1);
    advanced->addWidget(new QLabel("动态稀有度", m_advancedGroup), row, 2);
    advanced->addWidget(m_rarity, row++, 3);
    advanced->addWidget(new QLabel("研磨要求", m_advancedGroup), row, 0);
    advanced->addWidget(m_polishRequirement, row, 1);
    advanced->addWidget(new QLabel("极限强化", m_advancedGroup), row, 2);
    advanced->addWidget(m_honing, row++, 3);

    m_unpolished = new QCheckBox("未研磨/锈蚀", m_advancedGroup);
    m_glow = new QCheckBox("发光", m_advancedGroup);
    advanced->addWidget(m_unpolished, row, 0, 1, 2);
    advanced->addWidget(m_glow, row++, 2, 1, 2);

    m_calculationNote = new QLabel("面板值为武器自身换算，不包含防具技能、护符/爪、猫饭、药物和任务内增益。攻击极限强化按真攻击 +20；发掘升级暂按每级 +10 真攻击的参考换算计入，并用※标记。", m_advancedGroup);
    m_calculationNote->setWordWrap(true);
    advanced->addWidget(m_calculationNote, row++, 0, 1, 4);

    m_kinsectInstanceGroup = new QGroupBox("猎虫实例原始值 (0x14–0x1B)", m_advancedGroup);
    QGridLayout *kinsectLayout = new QGridLayout(m_kinsectInstanceGroup);
    const char *kinsectNames[] = {"力量", "耐力", "速度", "火", "水", "雷", "冰", "龙"};
    for (int index = 0; index < 8; ++index)
    {
        m_kinsectValues[index] = new QSpinBox(m_kinsectInstanceGroup);
        m_kinsectValues[index]->setRange(0, 0xff);
        m_kinsectValues[index]->setDisplayIntegerBase(16);
        m_kinsectValues[index]->setPrefix("0x");
        m_kinsectValues[index]->setToolTip("允许 00–FF；不校验猎虫养成合法性。");
        kinsectLayout->addWidget(new QLabel(QString::fromUtf8(kinsectNames[index]), m_kinsectInstanceGroup),
                                 (index / 4) * 2, index % 4);
        kinsectLayout->addWidget(m_kinsectValues[index], (index / 4) * 2 + 1, index % 4);
    }
    m_kinsectInstanceGroup->setVisible(isInsectGlaive);
    advanced->addWidget(m_kinsectInstanceGroup, row, 0, 1, 4);
    if (isBowgun)
    {
        m_attributeType->setEnabled(false);
        m_attributeValue->setEnabled(false);
        m_attributeType->setToolTip("轻重弩不使用发掘属性档；0x04–0x05 原值保持不变。");
        m_attributeValue->setToolTip("轻重弩不使用发掘属性档；0x04–0x05 原值保持不变。");
    }
    advanced->setColumnStretch(1, 1);
    advanced->setColumnStretch(3, 1);

    for (QComboBox *combo : {m_attributeType, m_attributeValue, m_attackTier, m_sharpness,
                             m_upgrade, m_weaponSpecial, m_slots, m_rarity,
                             m_polishRequirement, m_honing})
    {
        configureSearchableComboBox(combo);
        combo->setToolTip("可从预设选择，也可直接输入两位原始十六进制值，例如 0xFF。");
        connect(combo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCalculatedValues()));
        if (combo->lineEdit() != NULL)
            connect(combo->lineEdit(), SIGNAL(editingFinished()), this, SLOT(updateCalculatedValues()));
    }
    connect(m_attributeType, SIGNAL(currentIndexChanged(int)), this, SLOT(updateAttributeValueChoices()));
    connect(m_weaponSpecial, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCalculatedValues()));
    connect(m_levelOrModification, SIGNAL(valueChanged(int)), this, SLOT(levelOrModificationChanged(int)));
    connect(m_bowgunLimitBreakBit, SIGNAL(toggled(bool)), this, SLOT(bowgunFlagsChanged()));
    connect(m_bowgunAttachmentBit, SIGNAL(toggled(bool)), this, SLOT(bowgunFlagsChanged()));

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Save)->setText("保存");
    buttons->button(QDialogButtonBox::Close)->setText("关闭");
    connect(buttons, SIGNAL(accepted()), this, SLOT(saveAndAccept()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(basicGroup);
    layout->addWidget(m_relicStatus);
    layout->addWidget(m_advancedGroup);
    layout->addWidget(buttons);
    setLayout(layout);
    setWindowTitle(uiText("Single weapon editor"));
    resize(980, 680);

    connect(m_onlyRelicWeapons, SIGNAL(toggled(bool)), this, SLOT(populateIdentifiers()));
    connect(m_identifier, SIGNAL(currentIndexChanged(int)), this, SLOT(updateRelicState()));

    load();
}

void QWeapon::closeEvent(QCloseEvent *)
{
    weapon = NULL;
}

const dataset_t *QWeapon::weaponDataset() const
{
    return MH3U_DS::equipment(weapon->equipmentType);
}

void QWeapon::populateIdentifiers()
{
    int current = identifierValue();
    if (current <= 0) current = weapon->identifier;
    const bool onlyRelic = m_onlyRelicWeapons->isChecked();
    const dataset_t *values = weaponDataset();

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
                name = QString("%1（当前普通武器，保留）").arg(displayNameWithoutSearchSuffix(name));
            m_identifier->addItem(name, value.count);
            currentFound = currentFound || value.count == static_cast<uint32_t>(current);
        }
    }
    if (current > 0 && !currentFound)
        m_identifier->addItem(QString("未知装备 #%1（保留原值）").arg(current), current);
    m_identifier->setCurrentIndex(m_identifier->findData(current));
    m_identifier->blockSignals(false);
    updateRelicState();
}

QString QWeapon::lookupVariant(const QString &domain) const
{
    const int type = weapon->equipmentType;
    if (domain == "attack_tier") return MH4GEquipmentValues::isRangedType(type) ? "ranged" : "melee";
    if (domain == "sharpness")
    {
        if (type == MH4G_Type::LBGType) return "light_bowgun";
        if (type == MH4G_Type::HBGType) return "heavy_bowgun";
        if (type == MH4G_Type::BowType) return "bow";
        return "melee";
    }
    if (domain == "attribute_value" || domain == "status_value")
    {
        if (type == MH4G_Type::GSType || type == MH4G_Type::HHType) return "high";
        if (type == MH4G_Type::SNSType || type == MH4G_Type::BowType ||
            type == MH4G_Type::DBType || type == MH4G_Type::IGType) return "low";
        return "medium";
    }
    if (domain == "upgrade" || domain == "honing") return "weapon";
    if (domain == "attribute_type") return "relic";
    if (domain == "slots" || domain == "polish_requirement") return "all";
    if (domain == "rarity") return "relic";
    if (domain == "weapon_special")
    {
        switch (type)
        {
            case MH4G_Type::GSType:
            case MH4G_Type::HType: return "attack_boost";
            case MH4G_Type::SNSType:
            case MH4G_Type::LType: return "defense_boost";
            case MH4G_Type::LSType:
            case MH4G_Type::DBType: return "affinity_boost";
            case MH4G_Type::SAType:
            case MH4G_Type::CBType: return "relic_phial";
            case MH4G_Type::GLType: return "shelling";
            case MH4G_Type::LBGType:
            case MH4G_Type::HBGType: return "available_shots";
            case MH4G_Type::HHType: return "notes";
            case MH4G_Type::BowType: return "reserved";
            default: return QString();
        }
    }
    if (domain == "kinsect") return "type";
    return QString();
}

QString QWeapon::localizedLookupName(const QString &domain, uint8_t value, const QString &name) const
{
    if (domain == "attribute_type")
    {
        static const char *names[] = {"无", "火", "水", "雷", "龙", "冰", "毒", "麻痹", "睡眠", "爆破"};
        if (value <= 9) return QString::fromUtf8(names[value]);
    }
    if (domain == "honing")
    {
        if (value == 0x00) return "无";
        if (value == 0x40) return "攻击";
        if (value == 0x80) return "防御";
        if (value == 0xC0) return "生命吸收";
    }
    return name;
}

void QWeapon::populateByteCombo(QComboBox *combo, const QString &domain, const QString &variant,
                                uint8_t currentValue)
{
    combo->blockSignals(true);
    combo->clear();
    const lookup_dataset_t *values = MH3U_DS::lookups();
    bool found = false;
    if (values != NULL)
    {
        for (const lookupitem_t &value : *values)
        {
            if (QString::fromStdString(value.domain) != domain || value.equipmentType != weapon->equipmentType ||
                (!variant.isEmpty() && QString::fromStdString(value.variant) != variant))
                continue;
            QString name = localizedLookupName(domain, value.value, QString::fromStdString(value.identifier));
            QString prefix = isKnownOverflow(domain, variant, value.value) ? "越界 · " : QString();
            combo->addItem(QString("%1%2 · %3").arg(prefix, hexByte(value.value), name), value.value);
            if (!prefix.isEmpty())
                combo->setItemData(combo->count() - 1, QColor("#b42318"), Qt::ForegroundRole);
            combo->setItemData(combo->count() - 1, QString::fromStdString(value.identifier), Qt::UserRole + 1);
            found = found || value.value == currentValue;
        }
    }
    if (!found)
    {
        combo->addItem(QString("保留原值 %1（未识别）").arg(hexByte(currentValue)), currentValue);
        combo->setItemData(combo->count() - 1, QString(), Qt::UserRole + 1);
    }
    combo->setCurrentIndex(combo->findData(currentValue));
    combo->blockSignals(false);
}

int QWeapon::byteComboValue(QComboBox *combo, bool *ok) const
{
    bool localOk = false;
    int value = searchableComboBoxCurrentData(combo).toInt(&localOk);
    if (!localOk || combo->currentIndex() < 0)
    {
        QString text = combo->currentText().trimmed();
        QRegExp hex("^(?:0x)?([0-9A-Fa-f]{1,2})$");
        if (hex.exactMatch(text)) value = hex.cap(1).toInt(&localOk, 16);
        else value = text.toInt(&localOk, 10);
    }
    localOk = localOk && value >= 0 && value <= 255;
    if (ok != NULL) *ok = localOk;
    return localOk ? value : -1;
}

int QWeapon::identifierValue(bool *ok) const
{
    bool localOk = false;
    int value = searchableComboBoxCurrentData(m_identifier).toInt(&localOk);
    if (!localOk || m_identifier->currentIndex() < 0)
    {
        const QString text = m_identifier->currentText().trimmed();
        QRegExp hex("^0x([0-9A-Fa-f]{1,4})$");
        if (hex.exactMatch(text)) value = hex.cap(1).toInt(&localOk, 16);
        else value = text.toInt(&localOk, 10);
    }
    localOk = localOk && value >= 0 && value <= 0xffff;
    if (ok != NULL) *ok = localOk;
    return localOk ? value : -1;
}

QString QWeapon::comboLookupDisplay(QComboBox *combo) const
{
    if (combo->currentIndex() < 0) return QString();
    return combo->itemData(combo->currentIndex(), Qt::UserRole + 1).toString();
}

bool QWeapon::selectedWeaponIsRelic() const
{
    const int identifier = identifierValue();
    return identifier > 0 && MH3U_DS::isRelicWeapon(weapon->equipmentType, static_cast<uint16_t>(identifier));
}

void QWeapon::updateRelicState()
{
    const bool relic = selectedWeaponIsRelic();
    m_advancedGroup->setEnabled(relic);
    if (relic)
    {
        m_relicStatus->setText("已识别为发掘武器：高级随机参数可以编辑；不校验掉落合法性。");
        m_relicStatus->setStyleSheet("color:#176b35;");
    }
    else
    {
        m_relicStatus->setText("普通武器：高级随机参数只读并保留原始字节。勾选“只看发掘武器”可快速选择发掘外观。");
        m_relicStatus->setStyleSheet("color:#7a5a00;");
    }
    updateCalculatedValues();
}

void QWeapon::updateAttributeValueChoices()
{
    bool ok = false;
    const int attributeType = byteComboValue(m_attributeType, &ok);
    const int current = byteComboValue(m_attributeValue);
    const QString domain = ok && attributeType >= 6 ? "status_value" : "attribute_value";
    populateByteCombo(m_attributeValue, domain, lookupVariant(domain),
                      static_cast<uint8_t>(current >= 0 ? current : weapon->raw[4]));
    updateCalculatedValues();
}

void QWeapon::updateCalculatedValues()
{
    bool attackOk = false;
    bool specialOk = false;
    bool upgradeOk = false;
    bool honingOk = false;
    const int attackTier = byteComboValue(m_attackTier, &attackOk);
    const int special = byteComboValue(m_weaponSpecial, &specialOk);
    const int upgrade = byteComboValue(m_upgrade, &upgradeOk);
    const int honing = byteComboValue(m_honing, &honingOk);
    const MH4GAttackResult attack = MH4GEquipmentValues::attack(
        weapon->equipmentType,
        static_cast<uint8_t>(attackOk ? attackTier : 0xff),
        static_cast<uint8_t>(specialOk ? special : 0xff),
        static_cast<uint8_t>(upgradeOk ? upgrade : 0xff),
        static_cast<uint8_t>(honingOk ? honing : 0xff));
    if (attack.known)
    {
        const QString estimated = attack.upgradeBonusEstimated ? QString::fromUtf8("※") : QString();
        const QString detail = QString("%1（基础%2 + 专属%3 + 升级%4%5 + 极限%6）")
            .arg(attack.trueRaw).arg(attack.baseTrueRaw).arg(attack.weaponBonus)
            .arg(attack.upgradeBonus).arg(estimated).arg(attack.honingBonus);
        m_attackTrueValue->setText(attack.modifiersKnown
            ? detail
            : detail + "（含未知原值，仅已知部分）");
        m_attackPanelValue->setText(QString("%1（%2×%3）")
            .arg(attack.panelValue).arg(attack.trueRaw).arg(attack.multiplier, 0, 'f', 1));
    }
    else
    {
        m_attackTrueValue->setText("未知");
        m_attackPanelValue->setText("未知");
    }

    const bool isBowgun = weapon->equipmentType == MH4G_Type::LBGType ||
                          weapon->equipmentType == MH4G_Type::HBGType;
    const MH4GLookupNumber attribute = MH4GEquipmentValues::parseLookupNumber(comboLookupDisplay(m_attributeValue));
    if (isBowgun)
    {
        m_attributeTrueValue->setText("不适用");
        m_attributePanelValue->setText("原值保留");
    }
    else if (attribute.known)
    {
        const QString value = QString::number(attribute.value);
        m_attributeTrueValue->setText(attribute.awakened ? value + "（觉醒）" : value);
        m_attributePanelValue->setText(attribute.awakened ? "需觉醒 · " + value : value);
    }
    else
    {
        m_attributeTrueValue->setText("未知");
        m_attributePanelValue->setText("未知");
    }

    bool sharpOk = false;
    const int sharp = byteComboValue(m_sharpness, &sharpOk);
    if (!sharpOk) m_sharpnessValue->setText("未知原始值（允许保存）");
    else if (MH4GEquipmentValues::usesMeleeSharpness(weapon->equipmentType))
    {
        const MH4GSharpnessResult value = MH4GEquipmentValues::sharpness(static_cast<uint8_t>(sharp));
        m_sharpnessValue->setText(value.known
            ? sharpnessHtml(sharp, value)
            : QString("方案 %1 · 未知七色长度（原值可保存）").arg(hexByte(sharp)));
    }
    else
        m_sharpnessValue->setText(QString("配置 %1 · 原值模式").arg(hexByte(sharp)));
}

void QWeapon::levelOrModificationChanged(int value)
{
    m_bowgunLimitBreakBit->blockSignals(true);
    m_bowgunAttachmentBit->blockSignals(true);
    m_bowgunLimitBreakBit->setChecked((value & 0x20) != 0);
    m_bowgunAttachmentBit->setChecked((value & 0x08) != 0);
    m_bowgunLimitBreakBit->blockSignals(false);
    m_bowgunAttachmentBit->blockSignals(false);
    if (weapon->equipmentType == MH4G_Type::IGType)
        m_bowgunRawSummary->setText(QString("猎虫等级原始值 %1").arg(hexByte(value)));
    else if (weapon->equipmentType == MH4G_Type::LBGType || weapon->equipmentType == MH4G_Type::HBGType)
        m_bowgunRawSummary->setText(QString("完整原值 %1；未确认位保持不变").arg(hexByte(value)));
    else
        m_bowgunRawSummary->setText(QString("只读保留 %1").arg(hexByte(value)));
}

void QWeapon::bowgunFlagsChanged()
{
    int value = m_levelOrModification->value();
    value = m_bowgunLimitBreakBit->isChecked() ? (value | 0x20) : (value & ~0x20);
    value = m_bowgunAttachmentBit->isChecked() ? (value | 0x08) : (value & ~0x08);
    m_levelOrModification->setValue(value);
}

void QWeapon::load()
{
    m_equipmentType->setCurrentIndex(m_equipmentType->findData(weapon->equipmentType));
    populateIdentifiers();
    selectOrPreserveComboBoxValue(m_firstJewelIdentifier, weapon->firstJewelIdentifier & 0x7fff, "装饰珠");
    selectOrPreserveComboBoxValue(m_secondJewelIdentifier, weapon->secondJewelIdentifier & 0x7fff, "装饰珠");
    selectOrPreserveComboBoxValue(m_thirdJewelIdentifier, weapon->thirdJewelIdentifier & 0x7fff, "装饰珠");
    m_firstJewelFixed->setChecked((weapon->firstJewelIdentifier & 0x8000) != 0);
    m_secondJewelFixed->setChecked((weapon->secondJewelIdentifier & 0x8000) != 0);
    m_thirdJewelFixed->setChecked((weapon->thirdJewelIdentifier & 0x8000) != 0);
    m_levelOrModification->setValue(weapon->levelOrModification);
    levelOrModificationChanged(weapon->levelOrModification);
    for (int index = 0; index < 8; ++index)
        m_kinsectValues[index]->setValue(weapon->raw[20 + index]);

    populateByteCombo(m_attributeType, "attribute_type", lookupVariant("attribute_type"), weapon->raw[5]);
    populateByteCombo(m_attackTier, "attack_tier", lookupVariant("attack_tier"), weapon->raw[13]);
    populateByteCombo(m_sharpness, "sharpness", lookupVariant("sharpness"), weapon->raw[12]);
    populateByteCombo(m_upgrade, "upgrade", lookupVariant("upgrade"), weapon->raw[14]);
    const QString specialDomain = weapon->equipmentType == MH4G_Type::IGType ? "kinsect" : "weapon_special";
    populateByteCombo(m_weaponSpecial, specialDomain, lookupVariant(specialDomain), weapon->raw[15]);
    populateByteCombo(m_slots, "slots", lookupVariant("slots"), (weapon->raw[16] >> 2) & 0x03);
    populateByteCombo(m_rarity, "rarity", lookupVariant("rarity"), weapon->raw[17]);
    populateByteCombo(m_polishRequirement, "polish_requirement", lookupVariant("polish_requirement"), weapon->raw[18]);
    populateByteCombo(m_honing, "honing", lookupVariant("honing"), weapon->raw[19]);
    m_unpolished->setChecked((weapon->raw[16] & 0x01) != 0);
    m_glow->setChecked((weapon->raw[16] & 0x02) != 0);
    updateAttributeValueChoices();
    updateRelicState();
}

void QWeapon::save()
{
    weapon->equipmentType = static_cast<uint8_t>(searchableComboBoxCurrentData(m_equipmentType).toInt());
    weapon->identifier = static_cast<uint16_t>(identifierValue());
    weapon->firstJewelIdentifier = (m_firstJewelFixed->isChecked() ? 0x8000 : 0) |
        static_cast<uint16_t>(searchableComboBoxUnsignedValue(m_firstJewelIdentifier, 0x7fff));
    weapon->secondJewelIdentifier = (m_secondJewelFixed->isChecked() ? 0x8000 : 0) |
        static_cast<uint16_t>(searchableComboBoxUnsignedValue(m_secondJewelIdentifier, 0x7fff));
    weapon->thirdJewelIdentifier = (m_thirdJewelFixed->isChecked() ? 0x8000 : 0) |
        static_cast<uint16_t>(searchableComboBoxUnsignedValue(m_thirdJewelIdentifier, 0x7fff));

    if (!MH3U_DS::isRelicWeapon(weapon->equipmentType, weapon->identifier)) return;

    if (weapon->equipmentType == MH4G_Type::IGType || weapon->equipmentType == MH4G_Type::LBGType ||
        weapon->equipmentType == MH4G_Type::HBGType)
        weapon->levelOrModification = static_cast<uint8_t>(m_levelOrModification->value());
    if (weapon->equipmentType != MH4G_Type::LBGType && weapon->equipmentType != MH4G_Type::HBGType)
    {
        weapon->raw[4] = static_cast<uint8_t>(byteComboValue(m_attributeValue));
        weapon->raw[5] = static_cast<uint8_t>(byteComboValue(m_attributeType));
    }
    weapon->raw[12] = static_cast<uint8_t>(byteComboValue(m_sharpness));
    weapon->raw[13] = static_cast<uint8_t>(byteComboValue(m_attackTier));
    weapon->raw[14] = static_cast<uint8_t>(byteComboValue(m_upgrade));
    weapon->raw[15] = static_cast<uint8_t>(byteComboValue(m_weaponSpecial));
    uint8_t flags = weapon->raw[16] & 0xf0;
    if (m_unpolished->isChecked()) flags |= 0x01;
    if (m_glow->isChecked()) flags |= 0x02;
    flags |= static_cast<uint8_t>((byteComboValue(m_slots) & 0x03) << 2);
    weapon->raw[16] = flags;
    weapon->raw[17] = static_cast<uint8_t>(byteComboValue(m_rarity));
    weapon->raw[18] = static_cast<uint8_t>(byteComboValue(m_polishRequirement));
    weapon->raw[19] = static_cast<uint8_t>(byteComboValue(m_honing));
    if (weapon->equipmentType == MH4G_Type::IGType)
    {
        for (int index = 0; index < 8; ++index)
            weapon->raw[20 + index] = static_cast<uint8_t>(m_kinsectValues[index]->value());
    }
}

bool QWeapon::validate()
{
    const uint8_t equipmentType = static_cast<uint8_t>(searchableComboBoxCurrentData(m_equipmentType).toInt());
    bool identifierOk = false;
    const int identifier = identifierValue(&identifierOk);
    if (equipmentType == MH3U_Type::NoneType)
    {
        QMessageBox::warning(this, windowTitle(), "装备类型不能为“无”。");
        return false;
    }
    if (MH3U_Armory::convertSubtype(static_cast<equipment_type_e>(equipmentType)) != MH3U_Type::WeaponSubtype)
    {
        QMessageBox::warning(this, windowTitle(), "当前窗口只能保存武器类型。");
        return false;
    }
    if (!identifierOk || identifier == 0)
    {
        QMessageBox::warning(this, windowTitle(), "编号不能为“无”。请先选择具体武器。");
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

    if (selectedWeaponIsRelic())
    {
        for (QComboBox *combo : {m_attributeType, m_attributeValue, m_attackTier, m_sharpness,
                                 m_upgrade, m_weaponSpecial, m_slots, m_rarity,
                                 m_polishRequirement, m_honing})
        {
            bool ok = false;
            byteComboValue(combo, &ok);
            if (!ok)
            {
                QMessageBox::warning(this, windowTitle(),
                    QString("高级字段“%1”不是 00–FF 范围内的十六进制或十进制值。")
                        .arg(combo->currentText()));
                return false;
            }
        }
    }
    return true;
}

void QWeapon::saveAndAccept()
{
    if (!validate()) return;
    save();
    accept();
}
