#ifndef SKEY_SETTINGS_MACRO_TAB_H
#define SKEY_SETTINGS_MACRO_TAB_H

#include <QWidget>
#include <string>
#include <vector>

class QLineEdit;
class QTableWidget;
class QCheckBox;
class QPushButton;

struct MacroTabData {
    bool enableMacro     = true;
    bool capitalizeMacro = true;
    bool macroInOffMode  = false;
    std::vector<std::pair<std::string, std::string>> entries;
};

class MacroTab : public QWidget {
    Q_OBJECT
public:
    explicit MacroTab(QWidget *parent = nullptr);

    void loadFromConfig(const MacroTabData &data);
    MacroTabData collectConfig() const;
    void setDefaults();

private slots:
    void onAdd();
    void onDelete();

private:
    void setupUI();
    void addRow(const std::string &key, const std::string &value);

    QCheckBox    *enableCheck_;
    QCheckBox    *capitalizeCheck_;
    QCheckBox    *offModeCheck_;
    QTableWidget *table_;
    QLineEdit    *keyEdit_;
    QLineEdit    *valueEdit_;
    QPushButton  *addButton_;
};

#endif // SKEY_SETTINGS_MACRO_TAB_H
