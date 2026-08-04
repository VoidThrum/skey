#ifndef SKEY_SETTINGS_APPEARANCE_TAB_H
#define SKEY_SETTINGS_APPEARANCE_TAB_H

#include <QColor>
#include <QString>
#include <QWidget>
#include <vector>

class QGridLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;
struct SKeyConfig;

class AppearanceTab : public QWidget {
    Q_OBJECT
public:
    explicit AppearanceTab(QWidget *parent = nullptr);

    void loadFromConfig(const SKeyConfig &cfg);
    SKeyConfig collectConfig() const;   // fills iconTheme
    void setDefaults();

private slots:
    void onTileClicked();
    void onAddCustom();
    void onDeleteCustom(const QString &filename);

private:
    void setupUI();
    void rebuildGrid();
    void updateSelection();
    QPushButton *makeTile(const QString &iconPath, const QString &themeKey,
                          const QString &tooltip, bool isPreset,
                          const QColor &bgColor = QColor(0x2d, 0x2d, 0x2d));
    QPushButton *makeAddTile();

    QGridLayout *grid_;
    QVBoxLayout *mainLayout_;
    QPushButton *addTile_;

    QString selectedTheme_;   // current selected theme key (preset name or custom filename)

    // Cache of known custom icon filenames (without path)
    std::vector<std::string> customIcons_;
};

#endif // SKEY_SETTINGS_APPEARANCE_TAB_H
