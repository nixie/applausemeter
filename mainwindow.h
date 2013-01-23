#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QPixmap>
#include <QWidget>
#include <QVector>
#include <QObject>
#include <QMainWindow>
#include <QPushButton>
#include <QComboBox>
#include <QByteArray>

#include <qaudioinput.h>


class AudioInfo : public QIODevice
{
    Q_OBJECT
public:
    AudioInfo(const QAudioFormat &format, QObject *parent);
    ~AudioInfo();

    void start();
    void stop();

    qreal level() const { return m_level; }

    qint64 readData(char *data, qint64 maxlen);
    qint64 writeData(const char *data, qint64 len);

private:
    const QAudioFormat m_format;
    quint16 m_maxAmplitude;
    qreal m_level; // 0.0 <= m_level <= 1.0
    

signals:
    void update();
};



namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
private:
    void initializeAudio();
    void initializeWindow();
    void createAudioInput();

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void setLevel(qreal value){m_level = value;};

    


private slots:
    void refreshDisplay();
    void notified();
    void readMore();
    void toggleMode();
    void toggleSuspend();
    void stateChanged(QAudio::State state);
    void deviceChanged(int index);
    void resetMax();



private:
    Ui::MainWindow *ui;
    
    QAudioDeviceInfo m_device;
    AudioInfo *m_audioInfo;
    QAudioFormat m_format;
    QAudioInput *m_audioInput;
    QIODevice *m_input;
    bool m_pullMode;
    QByteArray m_buffer;
    qreal m_level;
    qreal m_max;


    QVector<double> *buf;
    int m_idx=0;
    const int BUFS=20;

     // Owned by layout
     QPushButton *m_modeButton;
     QPushButton *m_suspendResumeButton;
     QComboBox *m_deviceBox;

     static const QString PushModeLabel;
     static const QString PullModeLabel;
     static const QString SuspendLabel;
     static const QString ResumeLabel;


};

#endif // MAINWINDOW_H
