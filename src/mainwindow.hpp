#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include "mh4g.hpp"

#include <QWidget>

class QLabel;
class QPushButton;
class QComboBox;

class MainWindow : public QWidget
{
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void loadData(const QString &language);
    void loadSave();
    void saveAs();
    void openItems();
    void openEquipment();
    void refresh();

    MH4GData m_data;
    MH4GSave m_save;
    QLabel *m_status = nullptr;
    QPushButton *m_itemsButton = nullptr;
    QPushButton *m_equipmentButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QComboBox *m_language = nullptr;
};

#endif
