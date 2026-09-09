#include "albumpickerdialog.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {
constexpr int kThumbSide = 96;      // 缩略图边长
constexpr int kMaxImages = 300;     // 最多扫描的图片数量（避免超大目录卡顿）
constexpr int kScanDepth = 3;       // 递归扫描目录深度
// 注意：QDirIterator 的 nameFilters 是过滤器列表，每个扩展名一个元素，
// 不能写成 "*.png *.jpg" 这种带空格的单个字符串，否则匹配不到任何文件。
const QStringList kImageFilters{
    QStringLiteral("*.png"),
    QStringLiteral("*.jpg"),
    QStringLiteral("*.jpeg"),
    QStringLiteral("*.bmp"),
    QStringLiteral("*.webp")
};
}

AlbumPickerDialog::AlbumPickerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("从相册选择"));
    setModal(true);
    setMinimumSize(560, 480);
    buildUi();
    scanImages();
}

void AlbumPickerDialog::buildUi()
{
    setStyleSheet(QStringLiteral(R"(
QDialog {
    background: #0E1F33;
}
QLabel {
    color: #E5EFFF;
}
QLabel#albumTitle {
    font-size: 18px;
    font-weight: bold;
    color: #FFFFFF;
}
QLabel#albumHint {
    color: #8AA3C0;
    font-size: 12px;
}
QPushButton {
    background: #2563EB;
    color: white;
    border: none;
    border-radius: 8px;
    padding: 8px 16px;
    font-size: 13px;
    min-height: 18px;
}
QPushButton:hover {
    background: #3B82F6;
}
QPushButton:disabled {
    background: #1E4065;
    color: #8AA3C0;
}
QListWidget {
    background: #0A1828;
    border: 1px solid #264B70;
    border-radius: 8px;
    color: #E5EFFF;
    outline: none;
}
QListWidget::item {
    border-radius: 6px;
    padding: 4px;
}
QListWidget::item:hover {
    background: #142C46;
}
QListWidget::item:selected {
    background: #1D4ED8;
}
)"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("从相册选择"), this);
    title->setObjectName(QStringLiteral("albumTitle"));
    layout->addWidget(title);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setObjectName(QStringLiteral("albumHint"));
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    m_grid = new QListWidget(this);
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setIconSize(QSize(kThumbSide, kThumbSide));
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setUniformItemSizes(true);
    m_grid->setSpacing(8);
    m_grid->setSelectionMode(QAbstractItemView::SingleSelection);
    m_grid->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_grid, 1);

    // 双击图片 = 直接选中确认
    connect(m_grid, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        onConfirmClicked();
    });

    // 选中变化时更新提示与确认按钮状态
    connect(m_grid, &QListWidget::itemSelectionChanged, this, [this]() {
        const int row = m_grid->currentRow();
        m_selectedPath = (row >= 0 && row < m_imagePaths.size())
                             ? m_imagePaths.at(row)
                             : QString();
        m_confirmButton->setEnabled(!m_selectedPath.isEmpty());
    });

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();

    m_confirmButton = new QPushButton(QStringLiteral("确认使用"), this);
    m_confirmButton->setEnabled(false);
    connect(m_confirmButton, &QPushButton::clicked,
            this, &AlbumPickerDialog::onConfirmClicked);
    buttonRow->addWidget(m_confirmButton);

    auto *cancelButton = new QPushButton(QStringLiteral("取消"), this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonRow->addWidget(cancelButton);

    layout->addLayout(buttonRow);
}

void AlbumPickerDialog::scanImages()
{
    m_imagePaths.clear();
    m_grid->clear();

    const QStringList roots{
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        QDir::homePath()
    };

    QDirIterator::IteratorFlags flags = QDirIterator::Subdirectories;

    for (const QString &root : roots) {
        if (root.isEmpty() || !QDir(root).exists())
            continue;

        QDirIterator it(root, kImageFilters,
                        QDir::Files | QDir::Readable, flags);

        while (it.hasNext() && m_imagePaths.size() < kMaxImages) {
            const QString path = it.next();
            const QFileInfo info(path);
            const int depth = info.absolutePath().count('/')
                              - QDir(root).absolutePath().count('/');

            if (depth > kScanDepth)
                continue;

            m_imagePaths.append(path);
        }

        if (m_imagePaths.size() >= kMaxImages)
            break;
    }

    for (const QString &path : m_imagePaths) {
        auto *item = new QListWidgetItem(QIcon(path), QString(), m_grid);
        item->setSizeHint(QSize(kThumbSide + 16, kThumbSide + 16));
        item->setToolTip(QDir::toNativeSeparators(path));
    }

    refreshEmptyHint();
}

void AlbumPickerDialog::refreshEmptyHint()
{
    if (m_imagePaths.isEmpty()) {
        m_hintLabel->setText(
            QStringLiteral("系统图片目录中未找到图片，"
                           "请改用「从文件选择」。"));
    } else {
        m_hintLabel->setText(
            QStringLiteral("已找到 %1 张图片，单击选中后点击"
                           "「确认使用」，或直接双击图片。")
                .arg(m_imagePaths.size()));
    }
}

void AlbumPickerDialog::onConfirmClicked()
{
    if (m_selectedPath.isEmpty()) {
        const int row = m_grid->currentRow();
        if (row >= 0 && row < m_imagePaths.size())
            m_selectedPath = m_imagePaths.at(row);
    }

    if (m_selectedPath.isEmpty())
        return;

    accept();
}
