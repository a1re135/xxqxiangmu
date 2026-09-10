#pragma once

#include <QDialog>
#include <QImage>

class QLabel;
class QPushButton;
class QStackedWidget;
class QVideoWidget;
class QCamera;
class QImageCapture;
class QMediaCaptureSession;
class QShowEvent;

// 拍照对话框（参考 CamFunction/AvatarCaptureDialog 实现）：
// 确认后通过 capturedFilePath() 取临时 jpg 路径；取消/关闭不落盘。
// 挂接到个人主页头像更换：确认后由调用方复制到应用数据目录并调
// UserService::updateAvatar。
class AvatarCaptureDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AvatarCaptureDialog(QWidget *parent = nullptr);
    ~AvatarCaptureDialog() override;

    QString capturedFilePath() const { return m_savedPath; }

protected:
    void showEvent(QShowEvent *event) override;
    void reject() override;

private slots:
    void onShutterClicked();
    void onRetakeClicked();
    void onConfirmClicked();
    void onImageCaptured(int id, const QImage &image);
    void onCameraError(int error, const QString &msg);
    void onCaptureError(int id, int error, const QString &msg);

private:
    enum class State { Idle, Previewing, Frozen, Saved };

    void setupUi();
    void setupCamera();
    void stopCamera();
    void setState(State state);
    void cleanupTempFile();
    QString saveAndCompress(const QImage &raw);

    State m_state = State::Idle;
    QString m_savedPath;
    QImage m_frozenImage;
    bool m_cameraReady = false;

    QStackedWidget *m_viewStack = nullptr;
    QLabel *m_loadingLabel = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
    QLabel *m_frozenLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QPushButton *m_shutterBtn = nullptr;
    QPushButton *m_retakeBtn = nullptr;
    QPushButton *m_confirmBtn = nullptr;

    QCamera *m_camera = nullptr;
    QMediaCaptureSession *m_session = nullptr;
    QImageCapture *m_imageCapture = nullptr;
};
