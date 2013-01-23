#include <stdlib.h>
#include <math.h>
#include <QDebug>
#include <QPainter>
#include <QVBoxLayout>
#include <QAudioDeviceInfo>
#include <QAudioInput>
#include <QtCore/qendian.h>
#include <iostream>


#include "mainwindow.h"
#include "ui_mainwindow.h"

const QString MainWindow::PushModeLabel(tr("Enable push mode"));
const QString MainWindow::PullModeLabel(tr("Enable pull mode"));
const QString MainWindow::SuspendLabel(tr("Stop"));
const QString MainWindow::ResumeLabel(tr("Start"));



const int BufferSize = 4096;

AudioInfo::AudioInfo(const QAudioFormat &format, QObject *parent)
    :   QIODevice(parent)
    ,   m_format(format)
    ,   m_maxAmplitude(0)
    ,   m_level(0.0)

{
    switch (m_format.sampleSize()) {
    case 8:
        switch (m_format.sampleType()) {
        case QAudioFormat::UnSignedInt:
            m_maxAmplitude = 255;
            break;
        case QAudioFormat::SignedInt:
            m_maxAmplitude = 127;
            break;
        default: ;
        }
        break;
    case 16:
        switch (m_format.sampleType()) {
        case QAudioFormat::UnSignedInt:
            m_maxAmplitude = 65535;
            break;
        case QAudioFormat::SignedInt:
            m_maxAmplitude = 32767;
            break;
        default: ;
        }
        break;
    }
}

AudioInfo::~AudioInfo()
{
}

void AudioInfo::start()
{
    open(QIODevice::WriteOnly);
}

void AudioInfo::stop()
{
    close();
}

qint64 AudioInfo::readData(char *data, qint64 maxlen)
{
    Q_UNUSED(data)
    Q_UNUSED(maxlen)

    return 0;
}

qint64 AudioInfo::writeData(const char *data, qint64 len)
{
    if (m_maxAmplitude) {
        Q_ASSERT(m_format.sampleSize() % 8 == 0);
        const int channelBytes = m_format.sampleSize() / 8;
        const int sampleBytes = m_format.channels() * channelBytes;
        Q_ASSERT(len % sampleBytes == 0);
        const int numSamples = len / sampleBytes;

        quint16 maxValue = 0;
        const unsigned char *ptr =reinterpret_cast<const unsigned char *>(data);

        for (int i = 0; i < numSamples; ++i) {
            for(int j = 0; j < m_format.channels(); ++j) {
                quint16 value = 0;

                if (m_format.sampleSize() == 8 && m_format.sampleType() == QAudioFormat::UnSignedInt) {
                    value = *reinterpret_cast<const quint8*>(ptr);
                } else if (m_format.sampleSize() == 8 && m_format.sampleType() == QAudioFormat::SignedInt) {
                    value = qAbs(*reinterpret_cast<const qint8*>(ptr));
                } else if (m_format.sampleSize() == 16 && m_format.sampleType() == QAudioFormat::UnSignedInt) {
                    if (m_format.byteOrder() == QAudioFormat::LittleEndian)
                        value = qFromLittleEndian<quint16>(ptr);
                    else
                        value = qFromBigEndian<quint16>(ptr);
                } else if (m_format.sampleSize() == 16 && m_format.sampleType() == QAudioFormat::SignedInt) {
                    if (m_format.byteOrder() == QAudioFormat::LittleEndian)
                        value = qAbs(qFromLittleEndian<qint16>(ptr));
                    else
                        value = qAbs(qFromBigEndian<qint16>(ptr));
                }

                maxValue = qMax(value, maxValue);
                ptr += channelBytes;
            }
        }

        maxValue = qMin(maxValue, m_maxAmplitude);
        m_level = qreal(maxValue) / m_maxAmplitude;
    }

    emit update();
    return len;
}



MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initializeWindow();
    buf = new QVector<double>(BUFS, 0.0);
    initializeAudio();
    deviceChanged(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initializeWindow()
{                          
                           
                           
    m_deviceBox = new QComboBox(this);
    QList<QAudioDeviceInfo> devices =
        QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    for(int i = 0; i < devices.size(); ++i)
        m_deviceBox->addItem(devices.at(i).deviceName(),
                qVariantFromValue(devices.at(i)));
                           
    connect(m_deviceBox, SIGNAL(activated(int)), SLOT(deviceChanged(int)));
    ui->verticalLayout_2->addWidget(m_deviceBox);
                            
    m_modeButton = new QPushButton(this);
    m_modeButton->setText(PushModeLabel);
    connect(m_modeButton, SIGNAL(clicked()), SLOT(toggleMode()));
    ui->verticalLayout_2->addWidget(m_modeButton);
                           
    m_suspendResumeButton = new QPushButton(this);
    m_suspendResumeButton->setText(SuspendLabel);

    QFont font;
    font.setPointSize(60);
    m_suspendResumeButton->setFont(font);

    connect(m_suspendResumeButton, SIGNAL(clicked()), SLOT(toggleSuspend()));
    ui->verticalLayout_2->addWidget(m_suspendResumeButton);
                    
    connect(ui->pushButton_2, SIGNAL(clicked()), SLOT(resetMax()));
    //window->setLayout(layout.data());
    //layout.take(); // ownership transferred
                           
   // setCentralWidget(window.data());
   // QWidget *const windowPtr = window.take(); // ownership transferred
    //windowPtr->show();      
}                      
                         
void MainWindow::initializeAudio()
{                          
    m_pullMode = true;     
                           
    m_format.setFrequency(8000);
    m_format.setChannels(1);
    m_format.setSampleSize(16);
    m_format.setSampleType(QAudioFormat::SignedInt);
    m_format.setByteOrder(QAudioFormat::LittleEndian);
    m_format.setCodec("audio/pcm");
                            
    QAudioDeviceInfo info(QAudioDeviceInfo::defaultInputDevice());
    if (!info.isFormatSupported(m_format)) {
        qWarning() << "Default format not supported - trying to use nearest";
        m_format = info.nearestFormat(m_format);
    }                      
                           
    m_audioInfo  = new AudioInfo(m_format, this);
    connect(m_audioInfo, SIGNAL(update()), SLOT(refreshDisplay()));
                             
    createAudioInput();     
}                            
                    
void MainWindow::createAudioInput()
{                           
    m_audioInput = new QAudioInput(m_device, m_format, this);
    connect(m_audioInput, SIGNAL(notify()), SLOT(notified()));
    connect(m_audioInput, SIGNAL(stateChanged(QAudio::State)), SLOT(stateChanged(QAudio::State)));
    m_audioInfo->start();   
    m_audioInput->start(m_audioInfo);
}                           
                            
void MainWindow::notified()  
{                           
    qWarning() << "bytesReady = " << m_audioInput->bytesReady()
               << ", " << "elapsedUSecs = " <<m_audioInput->elapsedUSecs()
               << ", " << "processedUSecs = "<<m_audioInput->processedUSecs();
}                                  
void MainWindow::readMore()  
{                           
    if(!m_audioInput)       
        return;             
    qint64 len = m_audioInput->bytesReady();
    if(len > 4096)          
        len = 4096;         
    qint64 l = m_input->read(m_buffer.data(), len);
    if(l > 0) {             
        m_audioInfo->write(m_buffer.constData(), l);
    }                       
}                           

void MainWindow::resetMax()  
{                           
    m_max= 0.0;
    buf->fill(0.0);
    refreshDisplay();
}                                    
                            
void MainWindow::toggleMode()
{                           
    // Change bewteen pull and push modes
    m_audioInput->stop();   
                            
    if (m_pullMode) {       
        m_modeButton->setText(PullModeLabel);
        m_input = m_audioInput->start();
        connect(m_input, SIGNAL(readyRead()), SLOT(readMore()));
        m_pullMode = false; 
    } else {                
        m_modeButton->setText(PushModeLabel);
        m_pullMode = true;  
        m_audioInput->start(m_audioInfo);
    }                       
                            
    m_suspendResumeButton->setText(SuspendLabel);
}                           
                            
void MainWindow::toggleSuspend()
{                           
    // toggle suspend/resume
    if(m_audioInput->state() == QAudio::SuspendedState) {
        qWarning() << "status: Suspended, resume()";
        m_audioInput->resume();
        m_suspendResumeButton->setText(SuspendLabel);
    } else if (m_audioInput->state() == QAudio::ActiveState) {
        qWarning() << "status: Active, suspend()";
        m_audioInput->suspend();
        m_suspendResumeButton->setText(ResumeLabel);
    } else if (m_audioInput->state() == QAudio::StoppedState) {
        qWarning() << "status: Stopped, resume()";
        m_audioInput->resume();
        m_suspendResumeButton->setText(SuspendLabel);
    } else if (m_audioInput->state() == QAudio::IdleState) {
        qWarning() << "status: IdleState";
    }                       
}                           
                            
void MainWindow::stateChanged(QAudio::State state)
{                           
    qWarning() << "state = " << state;
}                           
                            
void MainWindow::refreshDisplay()
{            
    
    (*buf)[m_idx++]=m_audioInfo->level();
    if (m_idx == BUFS){
        m_idx=0;
    }

    qreal sum=0.0;
    for (unsigned int i=0; i < BUFS; i++){
        sum += (*buf)[i];
    }

    sum = sum/BUFS;
    std::cout << m_audioInfo->level()*100 << " " << int(sum*100) << std::endl;

    ui->progressBar->setValue(int(sum*100));

    if (sum*100 > m_max){
        m_max = sum*100;
    }

    char tbuff[20];
    sprintf(tbuff, "%4.2f\n%4.2f", m_max, sum*100);
    ui->label->setText(QString(tbuff));
        

}                           
                            
void MainWindow::deviceChanged(int index)
{                           
    m_audioInfo->stop();    
    m_audioInput->stop();   
    m_audioInput->disconnect(this);
    delete m_audioInput;    
                            
    m_device = m_deviceBox->itemData(index).value<QAudioDeviceInfo>();
    createAudioInput();     
}                           
                            

