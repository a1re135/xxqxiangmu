#pragma once

#include <QDialog>
#include <QStringList>

class QListWidget;
class QLabel;
class QPushButton;

// 简易相册选择对话框（桌面端替代方案）：
// 递归扫描系统图片目录（QStandardPaths::PicturesLocation）下的图片，
// 网格展示，选中后通过 selectedFilePath() 返回。目录为空时提示改用
// "从文件选择"。
class AlbumPickerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AlbumPickerDialog(QWidget *parent = nullptr);

    QString selectedFilePath() const { return m_selectedPath; }

private slots:
    void onConfirmClicked();

private:
    void buildUi();
    void scanImages();
    void refreshEmptyHint();

    QStringList m_imagePaths;
    QString m_selectedPath;

    QListWidget *m_grid = nullptr;
    QLabel *m_hintLabel = nullptr;
    QPushButton *m_confirmButton = nullptr;
};
