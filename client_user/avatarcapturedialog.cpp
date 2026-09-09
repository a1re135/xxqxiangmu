#include "avatarcapturedialog.h"

#include <QBuffer>
#include <QCamera>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QImageCapture>
#include <QLabel>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoFrameFormat>
#include <QVideoWidget>

namespace {
// ========== 常量定义 ==========
constexpr int kPreviewSide = 360;        // UI预览窗口像素大小
constexpr int kMaxAvatarBytes = 200 * 1024; // 头像最大文件大小：200KB
constexpr int kMaxEdge = 512;            // 头像图片最大边长
}

/**
 * @brief 构造函数
 */
AvatarCaptureDialog::AvatarCaptureDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("拍照上传头像"));
    setModal(true);          // 模态对话框，阻塞父窗口交互
    setMinimumSize(420, 520);
    setupUi();               // 创建所有UI控件
    setState(State::Idle);   // 初始置为空闲状态
}

AvatarCaptureDialog::~AvatarCaptureDialog()
{
    stopCamera(); // 析构优先关闭摄像头，释放硬件资源
    // 如果没有走到Saved状态，说明用户取消/关闭，清理临时文件
    if (m_state != State::Saved)
        cleanupTempFile();
}

/**
 * @brief 创建全部UI控件、布局
 */
void AvatarCaptureDialog::setupUi()
{
    setStyleSheet(QStringLiteral(R"(
QDialog {
    background: #0E1F33;
}
QLabel {
    color: #E5EFFF;
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
QPushButton#primaryButton {
    background: #2563EB;
}
QPushButton#primaryButton:hover {
    background: #3B82F6;
}
)"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    // QStackedWidget：同一位置切换多个页面，非常适合多状态UI
    m_viewStack = new QStackedWidget(this);
    m_viewStack->setFixedSize(kPreviewSide, kPreviewSide);

    // 页面1：加载提示
    m_loadingLabel = new QLabel(QStringLiteral("正在打开摄像头…"), m_viewStack);
    m_loadingLabel->setAlignment(Qt::AlignCenter);
    m_loadingLabel->setStyleSheet(QStringLiteral("background:#1A1F2B; color:#CFD8E6;"));

    // 页面2：摄像头实时视频画面
    m_videoWidget = new QVideoWidget(m_viewStack);
    // KeepAspectRatioByExpanding：铺满控件，画面会裁切一部分
    m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatioByExpanding);

    // 页面3：拍摄完成定格图片展示
    m_frozenLabel = new QLabel(m_viewStack);
    m_frozenLabel->setAlignment(Qt::AlignCenter);
    m_frozenLabel->setStyleSheet(QStringLiteral("background:#1A1F2B;"));

    // 把3个页面加入堆栈
    m_viewStack->addWidget(m_loadingLabel);
    m_viewStack->addWidget(m_videoWidget);
    m_viewStack->addWidget(m_frozenLabel);
    root->addWidget(m_viewStack, 0, Qt::AlignHCenter);

    // 底部提示信息
    m_hintLabel = new QLabel(this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet(QStringLiteral("color:#8A94A6; font-size:12px;"));
    root->addWidget(m_hintLabel);

    // 按钮行布局
    auto *btnRow = new QHBoxLayout;
    m_shutterBtn = new QPushButton(QStringLiteral("拍摄"), this);
    m_shutterBtn->setObjectName(QStringLiteral("primaryButton"));
    m_shutterBtn->setMinimumHeight(40);
    connect(m_shutterBtn, &QPushButton::clicked, this, &AvatarCaptureDialog::onShutterClicked);

    m_retakeBtn = new QPushButton(QStringLiteral("重拍"), this);
    m_retakeBtn->setMinimumHeight(40);
    connect(m_retakeBtn, &QPushButton::clicked, this, &AvatarCaptureDialog::onRetakeClicked);

    m_confirmBtn = new QPushButton(QStringLiteral("确认使用"), this);
    m_confirmBtn->setObjectName(QStringLiteral("primaryButton"));
    m_confirmBtn->setMinimumHeight(40);
    connect(m_confirmBtn, &QPushButton::clicked, this, &AvatarCaptureDialog::onConfirmClicked);

    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    cancelBtn->setMinimumHeight(40);
    connect(cancelBtn, &QPushButton::clicked, this, &AvatarCaptureDialog::reject);

    btnRow->addWidget(m_shutterBtn);
    btnRow->addWidget(m_retakeBtn);
    btnRow->addWidget(m_confirmBtn);
    btnRow->addWidget(cancelBtn);
    root->addLayout(btnRow);
}

/**
 * @brief 窗口显示事件，弹窗之后再初始化摄像头
 * 避免后台占用硬件；窗口弹出才启用设备。
 */
void AvatarCaptureDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (!m_cameraReady)
        setupCamera();
}

/**
 * @brief 重写取消逻辑：释放硬件、清理临时文件
 */
void AvatarCaptureDialog::reject()
{
    stopCamera();
    cleanupTempFile();
    m_savedPath.clear();
    m_frozenImage = QImage();
    setState(State::Idle);
    QDialog::reject();
}

/**
 * @brief 初始化摄像头相关多媒体对象，Qt6多媒体标准流程
 */
void AvatarCaptureDialog::setupCamera()
{
    // 获取系统可用摄像头列表
    const auto cameras = QMediaDevices::videoInputs();
    if (cameras.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("拍照"),
                             QStringLiteral(
                                 "未检测到可用摄像头。\n\n"
                                 "请检查：\n"
                                 "1. 摄像头是否已连接/已开启；\n"
                                 "2. 虚拟机（VMware/VirtualBox）需在设置中"
                                 "把宿主机摄像头分配给虚拟机；\n"
                                 "3. Linux 下执行 ls /dev/video* 查看是否有"
                                 "摄像头设备，并确认当前用户有读取权限\n"
                                 "   （可执行：sudo usermod -a -G video $USER"
                                 " 后重新登录）；\n"
                                 "4. 是否缺少 Qt Multimedia 运行插件"
                                 "（Ubuntu 可安装 qt6-multimedia-plugins 或"
                                 " gstreamer 后端）。\n\n"
                                 "也可以改用「从文件选择」上传头像。"));
        reject();
        return;
    }

    // 创建媒体会话
    m_session = new QMediaCaptureSession(this);
    m_imageCapture = new QImageCapture(this);
    m_camera = new QCamera(cameras.first(), this); // 使用第一个摄像头

    // 选择输出格式：优先 MJPEG 压缩格式（带宽占用小），分辨率不超过
    // 720p，规避 USB 2.0 直通带宽不足导致画面闪烁/只有半幅/丢帧的问题。
    const auto formats = cameras.first().videoFormats();
    QCameraFormat chosenFormat;

    for (const QCameraFormat &fmt : formats) {
        const QSize res = fmt.resolution();
        if (res.width() > 1280 || res.height() > 720)
            continue;

        const bool jpeg = (fmt.pixelFormat()
                           == QVideoFrameFormat::Format_Jpeg);
        const bool chosenJpeg =
            !chosenFormat.isNull()
            && chosenFormat.pixelFormat()
                   == QVideoFrameFormat::Format_Jpeg;

        if (chosenFormat.isNull()
            // MJPEG 优先于未压缩格式
            || (jpeg && !chosenJpeg)
            // 同为 MJPEG 或同为未压缩时，优先更大分辨率
            || (jpeg == chosenJpeg
                && res.width() * res.height()
                       > chosenFormat.resolution().width()
                             * chosenFormat.resolution().height())) {
            chosenFormat = fmt;
        }
    }

    if (!chosenFormat.isNull())
        m_camera->setCameraFormat(chosenFormat);

    // 绑定会话：摄像头、画面输出控件、拍照组件
    m_session->setCamera(m_camera);
    m_session->setVideoOutput(m_videoWidget);
    m_session->setImageCapture(m_imageCapture);

    // 绑定错误信号
    connect(m_camera, &QCamera::errorOccurred, this,
            [this](QCamera::Error error, const QString &msg) {
                onCameraError(static_cast<int>(error), msg);
            });

    // 拍照成功信号
    connect(m_imageCapture, &QImageCapture::imageCaptured,
            this, &AvatarCaptureDialog::onImageCaptured);

    // 拍照出错信号
    connect(m_imageCapture, &QImageCapture::errorOccurred, this,
            [this](int id, QImageCapture::Error error, const QString &msg) {
                onCaptureError(id, static_cast<int>(error), msg);
            });

    // 摄像头active状态变化：摄像头真正启动成功后切换UI
    connect(m_camera, &QCamera::activeChanged, this, [this](bool active) {
        if (active && m_state == State::Previewing) {
            m_viewStack->setCurrentWidget(m_videoWidget);
            m_hintLabel->setText(QStringLiteral("对准后点击「拍摄」"));
            m_shutterBtn->setEnabled(true);
        }
    });

    // 启动超时诊断：8 秒仍未激活则提示排查权限/后端插件，
    // 避免界面一直停在"正在打开摄像头…"。
    QTimer::singleShot(8000, this, [this]() {
        if (m_state == State::Previewing
            && m_camera && !m_camera->isActive()) {
            m_hintLabel->setText(
                QStringLiteral(
                    "摄像头未能在 8 秒内启动。请检查：\n"
                    "1. 设备权限：ls -l /dev/video*\n"
                    "2. 当前用户是否在 video 组（groups）\n"
                    "3. Qt Multimedia 后端插件是否完整"));
        }
    });

    setState(State::Previewing);
    m_camera->start(); // 启动摄像头硬件
    m_cameraReady = true;
}

/**
 * @brief 停止摄像头，释放硬件资源
 */
void AvatarCaptureDialog::stopCamera()
{
    if (m_camera && m_camera->isActive())
        m_camera->stop();
}

/**
 * @brief 状态机核心函数，根据状态自动控制界面显示、按钮可用性
 */
void AvatarCaptureDialog::setState(State state)
{
    m_state = state;
    const bool previewing = (state == State::Previewing);
    const bool frozen = (state == State::Frozen);

    // 控制按钮显示隐藏
    m_shutterBtn->setVisible(previewing);
    m_shutterBtn->setEnabled(previewing && m_camera && m_camera->isActive());
    m_retakeBtn->setVisible(frozen);
    m_retakeBtn->setEnabled(frozen);
    m_confirmBtn->setVisible(frozen);
    m_confirmBtn->setEnabled(frozen);

    if (state == State::Idle || state == State::Previewing) {
        m_viewStack->setCurrentWidget(m_loadingLabel);
        m_hintLabel->setText(QStringLiteral("正在打开摄像头…"));
        // 如果摄像头已经active，切到视频预览页
        if (m_camera && m_camera->isActive()) {
            m_viewStack->setCurrentWidget(m_videoWidget);
            m_hintLabel->setText(QStringLiteral("对准后点击「拍摄」"));
            m_shutterBtn->setEnabled(true);
        }
    } else if (frozen) {
        // 定格状态，显示拍摄后的图片页面
        m_viewStack->setCurrentWidget(m_frozenLabel);
        m_hintLabel->setText(QStringLiteral("可重拍或确认使用"));
    }
}

/**
 * @brief 点击拍摄按钮
 */
void AvatarCaptureDialog::onShutterClicked()
{
    if (m_state != State::Previewing || !m_imageCapture)
        return;
    // 检查拍照组件是否就绪，防止重复点击
    if (!m_imageCapture->isReadyForCapture())
        return;

    m_shutterBtn->setEnabled(false);
    m_hintLabel->setText(QStringLiteral("正在拍摄…"));
    m_imageCapture->capture(); // 触发拍照
}

/**
 * @brief 重拍按钮：清空图片，重新打开摄像头预览
 */
void AvatarCaptureDialog::onRetakeClicked()
{
    m_frozenImage = QImage();
    m_frozenLabel->clear();
    cleanupTempFile();
    // 如果摄像头停了，重新启动
    if (m_camera && !m_camera->isActive())
        m_camera->start();
    setState(State::Previewing);
}

/**
 * @brief 确认使用照片，执行图片压缩保存，关闭对话框返回accept
 */
void AvatarCaptureDialog::onConfirmClicked()
{
    if (m_state != State::Frozen || m_frozenImage.isNull())
        return;

    m_confirmBtn->setEnabled(false);
    m_retakeBtn->setEnabled(false);

    // 图片处理、压缩保存到临时文件
    const QString path = saveAndCompress(m_frozenImage);
    if (path.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("拍照"),
                             QStringLiteral("头像保存失败"));
        m_confirmBtn->setEnabled(true);
        m_retakeBtn->setEnabled(true);
        return;
    }
    m_savedPath = path;
    stopCamera();
    m_state = State::Saved;
    accept(); // 对话框返回QDialog::Accepted
}

/**
 * @brief 拍照成功回调，拿到拍摄出来的QImage
 */
void AvatarCaptureDialog::onImageCaptured(int, const QImage &image)
{
    if (image.isNull()) {
        m_hintLabel->setText(QStringLiteral("拍摄失败，请重试"));
        m_shutterBtn->setEnabled(true);
        return;
    }
    m_frozenImage = image;
    // 缩放图片用于UI预览显示
    const QPixmap pix = QPixmap::fromImage(image).scaled(
        kPreviewSide, kPreviewSide,
        Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    m_frozenLabel->setPixmap(pix);
    setState(State::Frozen); // 切换到定格预览状态
}

/**
 * @brief 摄像头硬件出错
 */
void AvatarCaptureDialog::onCameraError(int, const QString &msg)
{
    QMessageBox::warning(this, QStringLiteral("拍照"),
                         msg.isEmpty() ? QStringLiteral("未检测到可用摄像头")
                                       : msg);
    reject();
}

/**
 * @brief 拍照动作出错
 */
void AvatarCaptureDialog::onCaptureError(int, int, const QString &msg)
{
    m_hintLabel->setText(msg.isEmpty() ? QStringLiteral("拍摄失败") : msg);
    m_shutterBtn->setEnabled(m_state == State::Previewing);
}

/**
 * @brief 删除临时文件，避免磁盘残留垃圾
 */
void AvatarCaptureDialog::cleanupTempFile()
{
    if (!m_savedPath.isEmpty()) {
        QFile::remove(m_savedPath);
        m_savedPath.clear();
    }
}

/**
 * @brief 头像图片处理逻辑
 * 1. 将原图裁剪成正方形（头像一般都是正方形）
 * 2. 限制最大边长
 * 3. 循环降低jpeg质量，保证文件不超过 kMaxAvatarBytes
 * 4. 质量降到最低还不行，则进一步缩小分辨率
 * @param raw 摄像头输出原图
 * @return 临时文件路径，失败返回空
 */
QString AvatarCaptureDialog::saveAndCompress(const QImage &raw)
{
    if (raw.isNull())
        return {};

    // 1.居中裁剪正方形：取宽高较小值，从中间抠图
    const int side = qMin(raw.width(), raw.height());
    const int x = (raw.width() - side) / 2;
    const int y = (raw.height() - side) / 2;
    QImage square = raw.copy(x, y, side, side);

    // 限制最大边长
    if (square.width() > kMaxEdge) {
        square = square.scaled(kMaxEdge, kMaxEdge,
                               Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    // 生成临时文件名称，使用毫秒时间戳避免重名
    const QString path = QDir::temp().filePath(
        QStringLiteral("ncs_avatar_%1.jpg").arg(QDateTime::currentMSecsSinceEpoch()));

    // 局部lambda：把图片按指定quality输出JPG，先写到内存Buffer校验大小，再落盘
    auto writeJpeg = [&](const QImage &img, int quality) -> bool {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        if (!buffer.open(QIODevice::WriteOnly))
            return false;
        // 先保存到内存，不直接写磁盘，方便判断文件大小
        if (!img.save(&buffer, "JPG", quality))
            return false;
        // 判断是否超出最大字节限制
        if (bytes.size() > kMaxAvatarBytes)
            return false;

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        return file.write(bytes) == bytes.size();
    };

    // 策略：优先降低图片质量，85→75→65…40
    for (int quality = 85; quality >= 40; quality -= 10) {
        if (writeJpeg(square, quality))
            return path;
    }

    // 质量压到40依然超大小，进一步降低分辨率到256
    square = square.scaled(256, 256, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (writeJpeg(square, 70))
        return path;

    // 全部策略失败，删除残留临时文件，返回空
    QFile::remove(path);
    return {};
}
