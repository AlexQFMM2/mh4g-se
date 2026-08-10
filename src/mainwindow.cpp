#include "mainwindow.hpp"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <array>

namespace
{
QString knownOrId(const QString &name, int id)
{
    return name.isEmpty() ? QString("#%1").arg(id) : name;
}

std::uint16_t read16(const MH4GSave::Equipment &record, int offset)
{
    return record[offset] | (static_cast<std::uint16_t>(record[offset + 1]) << 8);
}

QString rawHex(const MH4GSave::Equipment &record)
{
    QByteArray bytes(reinterpret_cast<const char *>(record.data()), static_cast<int>(record.size()));
    return QString::fromLatin1(bytes.toHex(' ').toUpper());
}

QString decorationSummary(const MH4GData &data, const MH4GSave::Equipment &record)
{
    QStringList result;
    for (int offset : {6, 8, 10})
    {
        const std::uint16_t raw = read16(record, offset);
        if (raw == 0) continue;
        const bool fixed = (raw & 0x8000) != 0;
        const int id = raw & 0x7fff;
        QString name = knownOrId(data.decorationName(id), id);
        if (fixed) name += " [固定]";
        result << name;
    }
    return result.isEmpty() ? "-" : result.join(", ");
}

void configureTable(QTableWidget *table)
{
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
}

class ItemBoxDialog : public QDialog
{
public:
    ItemBoxDialog(MH4GSave &save, const MH4GData &data, QWidget *parent)
        : QDialog(parent), m_save(save), m_data(data)
    {
        setWindowTitle("MH4G 道具箱");
        resize(960, 720);

        m_table = new QTableWidget(this);
        m_table->setColumnCount(4);
        m_table->setHorizontalHeaderLabels({"格", "名称", "ID", "数量"});
        configureTable(m_table);
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

        m_search = new QLineEdit(this);
        m_search->setPlaceholderText("搜索名称、ID 或格号");
        m_nonEmpty = new QCheckBox("只显示非空", this);
        m_nonEmpty->setChecked(true);

        m_id = new QSpinBox(this);
        m_id->setRange(0, 65535);
        m_count = new QSpinBox(this);
        m_count->setRange(0, 65535);
        m_knownItem = new QComboBox(this);
        m_knownItem->setEditable(true);
        m_knownItem->setInsertPolicy(QComboBox::NoInsert);
        m_knownItem->setMaxVisibleItems(24);
        for (const MH4GNamedValue &item : data.items())
            m_knownItem->addItem(QString("%1 — %2").arg(item.id).arg(item.name), item.id);
        m_knownItem->model()->sort(0);

        QPushButton *apply = new QPushButton("应用到选中格", this);
        QPushButton *clear = new QPushButton("清空选中格", this);
        QPushButton *close = new QPushButton("关闭", this);

        QFormLayout *form = new QFormLayout();
        form->addRow("已知道具", m_knownItem);
        form->addRow("道具 ID", m_id);
        form->addRow("数量", m_count);

        QVBoxLayout *side = new QVBoxLayout();
        side->addWidget(new QLabel("筛选", this));
        side->addWidget(m_search);
        side->addWidget(m_nonEmpty);
        side->addSpacing(16);
        side->addLayout(form);
        side->addWidget(apply);
        side->addWidget(clear);
        side->addStretch();
        side->addWidget(close);

        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->addWidget(m_table, 1);
        layout->addLayout(side);

        connect(m_search, &QLineEdit::textChanged, this, [this] { populate(); });
        connect(m_nonEmpty, &QCheckBox::toggled, this, [this] { populate(); });
        connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] { loadSelection(); });
        connect(m_knownItem, QOverload<int>::of(&QComboBox::activated), this,
            [this](int index) { m_id->setValue(m_knownItem->itemData(index).toInt()); });
        connect(apply, &QPushButton::clicked, this, [this] { applySelection(); });
        connect(clear, &QPushButton::clicked, this, [this] {
            const int slot = selectedSlot();
            if (slot < 0) return;
            m_save.setItem(slot, {});
            populate(slot);
        });
        connect(close, &QPushButton::clicked, this, &QDialog::accept);

        populate();
    }

private:
    int selectedSlot() const
    {
        const int row = m_table->currentRow();
        if (row < 0 || !m_table->item(row, 0)) return -1;
        return m_table->item(row, 0)->data(Qt::UserRole).toInt();
    }

    void populate(int selectSlot = -1)
    {
        if (selectSlot < 0) selectSlot = selectedSlot();
        const QString query = m_search->text().trimmed();
        m_table->setUpdatesEnabled(false);
        m_table->setRowCount(0);
        int selectedRow = -1;
        for (int slot = 0; slot < MH4GSave::ItemCount; ++slot)
        {
            const MH4GSave::Item item = m_save.item(slot);
            if (m_nonEmpty->isChecked() && item.id == 0 && item.count == 0) continue;
            const QString name = item.id == 0 ? "空" : knownOrId(m_data.itemName(item.id), item.id);
            const QString searchable = QString("%1 %2 %3 %4").arg(slot + 1).arg(name).arg(item.id).arg(item.count);
            if (!query.isEmpty() && !searchable.contains(query, Qt::CaseInsensitive)) continue;

            const int row = m_table->rowCount();
            m_table->insertRow(row);
            QTableWidgetItem *slotItem = new QTableWidgetItem(QString::number(slot + 1));
            slotItem->setData(Qt::UserRole, slot);
            m_table->setItem(row, 0, slotItem);
            m_table->setItem(row, 1, new QTableWidgetItem(name));
            m_table->setItem(row, 2, new QTableWidgetItem(QString::number(item.id)));
            m_table->setItem(row, 3, new QTableWidgetItem(QString::number(item.count)));
            if (slot == selectSlot) selectedRow = row;
        }
        m_table->setUpdatesEnabled(true);
        if (selectedRow >= 0) m_table->selectRow(selectedRow);
        else if (m_table->rowCount()) m_table->selectRow(0);
        else loadSelection();
    }

    void loadSelection()
    {
        const int slot = selectedSlot();
        if (slot < 0)
        {
            m_id->setValue(0);
            m_count->setValue(0);
            return;
        }
        const MH4GSave::Item item = m_save.item(slot);
        m_id->setValue(item.id);
        m_count->setValue(item.count);
        const int combo = m_knownItem->findData(item.id);
        if (combo >= 0) m_knownItem->setCurrentIndex(combo);
        else m_knownItem->setEditText(QString("未知 ID %1").arg(item.id));
    }

    void applySelection()
    {
        const int slot = selectedSlot();
        if (slot < 0) return;
        m_save.setItem(slot, {static_cast<std::uint16_t>(m_id->value()),
                              static_cast<std::uint16_t>(m_count->value())});
        populate(slot);
    }

    MH4GSave &m_save;
    const MH4GData &m_data;
    QTableWidget *m_table = nullptr;
    QLineEdit *m_search = nullptr;
    QCheckBox *m_nonEmpty = nullptr;
    QComboBox *m_knownItem = nullptr;
    QSpinBox *m_id = nullptr;
    QSpinBox *m_count = nullptr;
};

class EquipmentBoxDialog : public QDialog
{
public:
    EquipmentBoxDialog(MH4GSave &save, const MH4GData &data, QWidget *parent)
        : QDialog(parent), m_save(save), m_data(data)
    {
        setWindowTitle("MH4G 装备箱（基础编辑）");
        resize(1260, 780);

        m_table = new QTableWidget(this);
        m_table->setColumnCount(7);
        m_table->setHorizontalHeaderLabels({"格", "类型", "名称", "ID", "等级/孔数", "装饰品", "原始 28 字节"});
        configureTable(m_table);
        for (int column : {0, 3, 4})
            m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
        m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

        m_search = new QLineEdit(this);
        m_search->setPlaceholderText("搜索名称、类型、ID、格号或原始值");
        m_nonEmpty = new QCheckBox("只显示非空", this);
        m_nonEmpty->setChecked(true);
        m_typeFilter = new QComboBox(this);
        m_typeFilter->addItem("全部类型", -1);
        for (const MH4GNamedValue &type : data.equipmentTypes())
            m_typeFilter->addItem(QString("%1 — %2").arg(type.id).arg(type.name), type.id);

        m_type = new QComboBox(this);
        for (const MH4GNamedValue &type : data.equipmentTypes())
            m_type->addItem(QString("%1 — %2").arg(type.id).arg(type.name), type.id);
        for (int type = 0; type <= 255; ++type)
            if (m_type->findData(type) < 0) m_type->addItem(QString("%1 — 未知类型").arg(type), type);

        m_equipmentId = new QSpinBox(this);
        m_equipmentId->setRange(0, 65535);
        m_level = new QSpinBox(this);
        m_level->setRange(0, 255);
        for (int index = 0; index < 3; ++index)
        {
            m_decorations[index] = new QSpinBox(this);
            m_decorations[index]->setRange(0, 65535);
            m_decorationLabels[index] = new QLabel(this);
        }
        m_raw = new QLineEdit(this);
        m_raw->setPlaceholderText("56 个十六进制数字，可用空格分隔");

        QPushButton *apply = new QPushButton("应用基础字段", this);
        QPushButton *applyRaw = new QPushButton("替换完整原始记录", this);
        QPushButton *clear = new QPushButton("清空选中格", this);
        QPushButton *close = new QPushButton("关闭", this);

        QFormLayout *form = new QFormLayout();
        form->addRow("装备类型", m_type);
        form->addRow("装备 ID", m_equipmentId);
        form->addRow("等级 / 孔数", m_level);
        for (int index = 0; index < 3; ++index)
        {
            QWidget *row = new QWidget(this);
            QHBoxLayout *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->addWidget(m_decorations[index]);
            rowLayout->addWidget(m_decorationLabels[index], 1);
            form->addRow(QString("装饰品 %1 原始值").arg(index + 1), row);
        }
        form->addRow("原始 28 字节", m_raw);

        QLabel *notice = new QLabel(
            "“应用基础字段”只修改 00、01、02–03、06–0B，其他字节保持原样。\n"
            "装饰品原始值的 0x8000 位是固定/内置标志。\n"
            "完整原始记录属于高级操作，不检查装备组合是否合法。", this);
        notice->setWordWrap(true);

        QVBoxLayout *side = new QVBoxLayout();
        side->addWidget(new QLabel("筛选", this));
        side->addWidget(m_search);
        side->addWidget(m_typeFilter);
        side->addWidget(m_nonEmpty);
        side->addSpacing(12);
        side->addLayout(form);
        side->addWidget(notice);
        side->addWidget(apply);
        side->addWidget(applyRaw);
        side->addWidget(clear);
        side->addStretch();
        side->addWidget(close);

        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->addWidget(m_table, 1);
        layout->addLayout(side);

        connect(m_search, &QLineEdit::textChanged, this, [this] { populate(); });
        connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { populate(); });
        connect(m_nonEmpty, &QCheckBox::toggled, this, [this] { populate(); });
        connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] { loadSelection(); });
        for (QSpinBox *decoration : m_decorations)
            connect(decoration, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { updateDecorationLabels(); });
        connect(apply, &QPushButton::clicked, this, [this] { applyBasic(); });
        connect(applyRaw, &QPushButton::clicked, this, [this] { this->applyRaw(); });
        connect(clear, &QPushButton::clicked, this, [this] {
            const int slot = selectedSlot();
            if (slot < 0) return;
            m_save.setEquipment(slot, {});
            populate(slot);
        });
        connect(close, &QPushButton::clicked, this, &QDialog::accept);

        populate();
    }

private:
    int selectedSlot() const
    {
        const int row = m_table->currentRow();
        if (row < 0 || !m_table->item(row, 0)) return -1;
        return m_table->item(row, 0)->data(Qt::UserRole).toInt();
    }

    QString typeName(int type) const
    {
        return knownOrId(m_data.equipmentTypeName(type), type);
    }

    QString equipmentName(const MH4GSave::Equipment &record) const
    {
        const int type = record[0];
        const int id = read16(record, 2);
        if (type == 0 && id == 0) return "空";
        return knownOrId(m_data.equipmentName(type, id), id);
    }

    void populate(int selectSlot = -1)
    {
        if (selectSlot < 0) selectSlot = selectedSlot();
        const QString query = m_search->text().trimmed();
        const int filterType = m_typeFilter->currentData().toInt();
        m_table->setUpdatesEnabled(false);
        m_table->setRowCount(0);
        int selectedRow = -1;
        for (int slot = 0; slot < MH4GSave::EquipmentCount; ++slot)
        {
            const MH4GSave::Equipment record = m_save.equipment(slot);
            const int type = record[0];
            const int id = read16(record, 2);
            if (m_nonEmpty->isChecked() && type == 0 && id == 0) continue;
            if (filterType >= 0 && type != filterType) continue;
            const QString name = equipmentName(record);
            const QString typeText = typeName(type);
            const QString raw = rawHex(record);
            const QString decorations = decorationSummary(m_data, record);
            const QString searchable = QString("%1 %2 %3 %4 %5 %6")
                .arg(slot + 1).arg(typeText).arg(name).arg(id).arg(decorations).arg(raw);
            if (!query.isEmpty() && !searchable.contains(query, Qt::CaseInsensitive)) continue;

            const int row = m_table->rowCount();
            m_table->insertRow(row);
            QTableWidgetItem *slotItem = new QTableWidgetItem(QString::number(slot + 1));
            slotItem->setData(Qt::UserRole, slot);
            m_table->setItem(row, 0, slotItem);
            m_table->setItem(row, 1, new QTableWidgetItem(typeText));
            m_table->setItem(row, 2, new QTableWidgetItem(name));
            m_table->setItem(row, 3, new QTableWidgetItem(QString::number(id)));
            m_table->setItem(row, 4, new QTableWidgetItem(QString::number(record[1])));
            m_table->setItem(row, 5, new QTableWidgetItem(decorations));
            m_table->setItem(row, 6, new QTableWidgetItem(raw));
            if (slot == selectSlot) selectedRow = row;
        }
        m_table->setUpdatesEnabled(true);
        if (selectedRow >= 0) m_table->selectRow(selectedRow);
        else if (m_table->rowCount()) m_table->selectRow(0);
        else loadSelection();
    }

    void loadSelection()
    {
        const int slot = selectedSlot();
        if (slot < 0) return;
        const MH4GSave::Equipment record = m_save.equipment(slot);
        const int typeIndex = m_type->findData(record[0]);
        if (typeIndex >= 0) m_type->setCurrentIndex(typeIndex);
        m_equipmentId->setValue(read16(record, 2));
        m_level->setValue(record[1]);
        for (int index = 0; index < 3; ++index)
            m_decorations[index]->setValue(read16(record, 6 + index * 2));
        m_raw->setText(rawHex(record));
        updateDecorationLabels();
    }

    void updateDecorationLabels()
    {
        for (int index = 0; index < 3; ++index)
        {
            const int raw = m_decorations[index]->value();
            const int id = raw & 0x7fff;
            QString text = id == 0 ? "空" : knownOrId(m_data.decorationName(id), id);
            if (raw & 0x8000) text += " [固定]";
            m_decorationLabels[index]->setText(text);
        }
    }

    void applyBasic()
    {
        const int slot = selectedSlot();
        if (slot < 0) return;
        std::array<std::uint16_t, 3> decorations{};
        for (int index = 0; index < 3; ++index)
            decorations[index] = static_cast<std::uint16_t>(m_decorations[index]->value());
        m_save.patchEquipmentBasic(
            slot,
            static_cast<std::uint8_t>(m_type->currentData().toInt()),
            static_cast<std::uint8_t>(m_level->value()),
            static_cast<std::uint16_t>(m_equipmentId->value()),
            decorations);
        populate(slot);
    }

    void applyRaw()
    {
        const int slot = selectedSlot();
        if (slot < 0) return;
        QString text = m_raw->text();
        text.remove(' ');
        text.remove('\t');
        text.remove('\r');
        text.remove('\n');
        if (text.size() != MH4GSave::EquipmentSize * 2)
        {
            QMessageBox::critical(this, windowTitle(), "原始记录必须正好包含 28 字节（56 个十六进制数字）。");
            return;
        }
        for (const QChar ch : text)
        {
            if (!ch.isDigit() && (ch.toUpper() < 'A' || ch.toUpper() > 'F'))
            {
                QMessageBox::critical(this, windowTitle(), "原始记录包含非十六进制字符。");
                return;
            }
        }
        const QByteArray bytes = QByteArray::fromHex(text.toLatin1());
        if (bytes.size() != MH4GSave::EquipmentSize) return;
        MH4GSave::Equipment record{};
        std::copy(bytes.constBegin(), bytes.constEnd(), reinterpret_cast<char *>(record.data()));
        m_save.setEquipment(slot, record);
        populate(slot);
    }

    MH4GSave &m_save;
    const MH4GData &m_data;
    QTableWidget *m_table = nullptr;
    QLineEdit *m_search = nullptr;
    QCheckBox *m_nonEmpty = nullptr;
    QComboBox *m_typeFilter = nullptr;
    QComboBox *m_type = nullptr;
    QSpinBox *m_equipmentId = nullptr;
    QSpinBox *m_level = nullptr;
    std::array<QSpinBox *, 3> m_decorations{};
    std::array<QLabel *, 3> m_decorationLabels{};
    QLineEdit *m_raw = nullptr;
};
}

MainWindow::MainWindow(QWidget *parent) : QWidget(parent)
{
    setWindowTitle("MH4G - 存档修改器 v0.1");
    resize(460, 330);

    QLabel *title = new QLabel("MH4G 存档修改器", this);
    QFont font = title->font();
    font.setPointSize(font.pointSize() + 5);
    font.setBold(true);
    title->setFont(font);

    QPushButton *loadButton = new QPushButton("打开 user 存档", this);
    m_saveButton = new QPushButton("另存为", this);
    m_itemsButton = new QPushButton("道具箱（1400 格）", this);
    m_equipmentButton = new QPushButton("装备箱（1500 格）", this);
    m_language = new QComboBox(this);
    m_language->addItem("简体中文", "cn");
    m_language->addItem("English", "en");
    m_status = new QLabel(this);
    m_status->setWordWrap(true);

    QLabel *warning = new QLabel("修改前请备份完整存档目录。第一版不单独解释发掘装备，但会保留全部原始字段。", this);
    warning->setWordWrap(true);

    QHBoxLayout *fileButtons = new QHBoxLayout();
    fileButtons->addWidget(loadButton);
    fileButtons->addWidget(m_saveButton);

    QFormLayout *options = new QFormLayout();
    options->addRow("数据语言", m_language);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(m_status);
    layout->addSpacing(8);
    layout->addWidget(m_itemsButton);
    layout->addWidget(m_equipmentButton);
    layout->addSpacing(8);
    layout->addLayout(options);
    layout->addLayout(fileButtons);
    layout->addWidget(warning);

    connect(loadButton, &QPushButton::clicked, this, [this] { loadSave(); });
    connect(m_saveButton, &QPushButton::clicked, this, [this] { saveAs(); });
    connect(m_itemsButton, &QPushButton::clicked, this, [this] { openItems(); });
    connect(m_equipmentButton, &QPushButton::clicked, this, [this] { openEquipment(); });
    connect(m_language, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this] { loadData(m_language->currentData().toString()); });

    loadData("cn");
    refresh();
}

void MainWindow::loadData(const QString &language)
{
    QString error;
    if (!m_data.load(language, &error))
        QMessageBox::critical(this, windowTitle(), error);
}

void MainWindow::loadSave()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this, "打开 MH4G 存档", QString(), "MH4G user 存档 (user1 user2 user3);;所有文件 (*)");
    if (fileName.isEmpty()) return;
    if (!m_save.load(fileName))
    {
        QMessageBox::critical(this, "打开失败", m_save.lastError());
        return;
    }
    refresh();
}

void MainWindow::saveAs()
{
    const QString suggested = m_save.fileName().isEmpty() ? "user1" : m_save.fileName();
    const QString fileName = QFileDialog::getSaveFileName(
        this, "另存 MH4G 存档", suggested, "MH4G user 存档 (user1 user2 user3);;所有文件 (*)");
    if (fileName.isEmpty()) return;
    if (!m_save.save(fileName))
    {
        QMessageBox::critical(this, "保存失败", m_save.lastError());
        return;
    }
    QMessageBox::information(this, windowTitle(), "存档已重新计算校验和并加密保存。");
    refresh();
}

void MainWindow::openItems()
{
    ItemBoxDialog dialog(m_save, m_data, this);
    dialog.exec();
}

void MainWindow::openEquipment()
{
    EquipmentBoxDialog dialog(m_save, m_data, this);
    dialog.exec();
}

void MainWindow::refresh()
{
    const bool loaded = m_save.loaded();
    m_itemsButton->setEnabled(loaded);
    m_equipmentButton->setEnabled(loaded);
    m_saveButton->setEnabled(loaded);
    m_status->setText(loaded
        ? QString("已打开：%1\n校验和有效，格式：MH4G 3DS 加密 user 存档（81408 字节）").arg(m_save.fileName())
        : "尚未打开存档");
}
