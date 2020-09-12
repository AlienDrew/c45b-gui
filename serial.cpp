#include "serial.h"

#include <QTimer>
#include <QTime>
#include <QChar>
#include <QDebug>
#include <QThread>
#include <QtMath>

Serial::Serial(QObject *parent) : QObject(parent)
{
    m_connectTimer = new QTimer(this);
    m_uploadTimer = new QTimer(this);
    m_uploadTimer->setSingleShot(true);
    connect(m_connectTimer, &QTimer::timeout, this, &Serial::on_tryConnectTimeout);
    connect(m_uploadTimer, &QTimer::timeout, this, &Serial::on_uploadTimeout);
    m_port = new QSerialPort(this);
    connect(m_port, &QSerialPort::readyRead, this, &Serial::handleReadyRead);
    connect(m_port, &QSerialPort::bytesWritten, this, &Serial::handleBytesWritten);
    connect(m_port, &QSerialPort::errorOccurred, this, &Serial::handleError);
    connect(this, &Serial::do_parse, this, &Serial::parse, Qt::QueuedConnection);
}

Serial::~Serial()
{

}

bool Serial::tryConnectToBootloader(QString port, qint32 baudRate, int connectionTimeout)
{
    m_connectionTimeout = connectionTimeout;
    m_port->setPortName(port);
    m_port->setBaudRate(baudRate);
    m_port->setFlowControl(QSerialPort::SoftwareControl);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setStopBits(QSerialPort::TwoStop);
    if (!m_port->open(QIODevice::ReadWrite)) {
        qDebug()<<"Could not open port";
        return false;
    }
    m_connectTimer->start(100);
    return true;
}

void Serial::disconnectFromBootloader()
{
    if (m_port->isOpen())
    {
        prepareCommandAndWrite(Commands::Disconnect, "g\n");
        m_connected = false;
        m_activeBootloader = false;
        emit connected(false, "Disconnected from "+m_port->portName());
    }
}

QString Serial::portName() const
{
    return m_port->portName();
}

bool Serial::isOpen() const
{
    return m_port->isOpen();
}

void Serial::clear() const
{
    m_port->clear();
    //m_port->flush();
}

void Serial::close() const
{
    m_port->close();
}

void Serial::program(const HexFile& hexFile, bool doFlash)
{
    QString cmd(doFlash ? "pf\n" : "pe\n");
    m_doFlash = doFlash;
    prepareCommandAndWrite(Commands::Program, cmd.toUtf8(), hexFile);
    // Wait for "pf+\r"
}

void Serial::handleBytesWritten(qint64 bytes)
{
    qDebug()<<"bytes written: "<<bytes;
    qDebug()<<"Data successfully sent to port: "<<m_writeData;
    m_writeData.clear();
    m_currentWriteCommand = Commands::Idle;
//    if (m_port->bytesToWrite() == 0)
//        {
//            //m_bytesWritten = 0;
//            qDebug()<<"Data successfully sent to port: "<<m_writeData;
//            //consoleOutput("PC: "+QString(m_write_data), MsgType::send);
//            m_writeData.clear();
//            m_currentWriteCommand = Commands::Idle;
//        }
}

void Serial::handleReadyRead()
{
    if (m_port->bytesAvailable()<30)
    {
        m_readData.append(m_port->readAll());
        //qDebug()<<m_readData;
        if (m_readData.contains('\r') or m_readData.contains(Serial::XON))
        {
            m_readData.chop(1);
            m_port->clear();
            m_port->flush();
            emit do_parse(m_readData, QPrivateSignal());
            m_readData.clear();
        }
        else if (m_currentCommand == Commands::DownloadLine)
        {
            //m_port->clear();
            //m_port->flush();
            emit do_parse(m_readData, QPrivateSignal());
            m_readData.clear();
        }
    }
}

void Serial::handleError(QSerialPort::SerialPortError serialPortError)
{
    if (serialPortError == QSerialPort::ReadError) {
        qDebug()<<"An I/O error occurred while reading the data from port"<<m_port->portName()<<", error:"<<m_port->errorString();
        //consoleOutput("PC: An I/O error occurred while reading the data from port: "+port->errorString(), MsgType::alert);
    }
    else if (serialPortError == QSerialPort::WriteError)
    {
        qDebug()<<"An I/O error occurred while writing the data to port"<<m_port->portName()<<", error:"<<m_port->errorString();
        //consoleOutput("PC: An I/O error occurred while writing the data to port: "+m_serialPort->errorString(), MsgType::alert);
    }
    else if (serialPortError == QSerialPort::ResourceError)
    {
        if (m_port->isOpen())
        {
            m_connected = false;
            m_activeBootloader = false;
            m_currentCommand = Commands::Idle;
            m_port->close();
        }
        m_count = 0;
        connected(false, "The device "+m_port->portName()+" has been removed from the system");
    }
}

void Serial::parse(const QByteArray readData)
{
    switch (m_currentCommand) {
    case Commands::Connect:
    {
        if (readData.contains("c45b2"))
        {
            m_connected = true;
            qDebug() << "Connected";
        }
        else if (readData.contains(QString("%1-\n\r>").arg(QChar(Serial::XOFF)).toStdString().c_str()))
        {
            m_connected = true;
            m_activeBootloader = true;
            qDebug() << "Found already activated bootloader";
        }

        if (!m_activeBootloader && !readData.contains("c45b2"))
        {
            connected(false, "Error: Wrong bootloader version: "+readData);
        }

        if (m_activeBootloader)
        {
            connected(true, "Warning: bootloader was already active - could not check for compatible version");
        }
        else
        {
            connected(true, "===WELCOME TO Bootloader "+readData.mid(5).simplified()+"===");
        }
        m_currentCommand = Commands::Idle;
        break;
    }
    case Commands::Program:
    {
        QString reply = readData;
        reply = reply.replace(QChar(0x13), "").trimmed();
        m_cmd.chop(1);
        QString expected = m_cmd + QString("+");
        if (!reply.startsWith(expected))
        {
            qDebug() << "Error: Bootloader did not respond to '" << m_cmd << "' command";
            //verbose
            qDebug() << "Reply: " << reply;
            break;
        }

        // Send to bootloader
        qDebug() << "Programming " << (m_cmd == "pf" ? "flash" : "EEPROM") << " memory...";

        QStringList hexFileLines = m_hexFile.getHexFile();
        quint32 lineNr = 0;
        qDebug()<<"lines in hex: "<<hexFileLines.count();
        foreach(QString line, hexFileLines)
        {
            ++lineNr;
            if (!prepareCommandAndWrite(Commands::DownloadLine, line.toUtf8()))
            {
                qDebug() << "Error: Failed to download line " << lineNr;
                break;
            }
            //emit uploadedProgress(qRound(double(lineNr/hexFileLines.count()*100)));
        }
        m_currentCommand = Commands::DownloadLine;//Commands::Idle;
        break;
    }
    case Commands::DownloadLine:
    {
        if( readData.contains('-') )
        {
            m_uploadTimer->stop();
            qDebug() << "Something went wrong during programming";
            m_count = 0;
            m_port->clear();
            m_port->flush();
            emit firmwareUploaded(false, "Something went wrong during programming :(");
            m_currentCommand = Commands::Idle;
            break;
        }
        if (!readData.contains('.') && !readData.contains('*'))
        {
            m_uploadTimer->stop();
            if (readData.isEmpty())
                qDebug() << "Timeout";
            else if (readData.contains('\r'))
            {
                m_count = 0;
                m_port->clear();
                m_port->flush();
                emit firmwareUploaded(true);
                m_currentCommand = Commands::Idle;
                break;
            }
            else
                qDebug() << "Reply: " << readData;
            m_count = 0;
            m_port->clear();
            m_port->flush();
            emit firmwareUploaded(false);
            m_currentCommand = Commands::Idle;
            break;
        }
        // ...and with '*' on page write
        //uploadedProgress(qRound(double(readData.count('*')*128*2/30120*100)));
        if (readData.contains('*'))
        {
            m_uploadTimer->start(1000);
            m_count++;
            double size = qCeil(m_hexFile.size()/(m_doFlash ? 128.0 : 16.0));
            uploadedProgress(qRound(m_count/size*100));
            //qDebug() << "hex file size" << m_hexFile.size();
            qDebug() << m_count << "out of" << size<< ":" << qRound(m_count/size*100) << "%";
        }
        if (readData.contains('\r'))
        {
            m_uploadTimer->stop();
            m_count = 0;
            m_port->clear();
            m_port->flush();
            emit firmwareUploaded(true);
            m_currentCommand = Commands::Idle;
        }
        break;
    }
    case Commands::Disconnect:
    {
        if (readData.contains("g+"))
        {
            m_port->close();
            m_currentCommand = Commands::Idle;
        }
        break;
    }
    default:
        break;
    }
}

bool Serial::prepareCommandAndWrite(const Commands command, const QString values, HexFile hexFile)
{
    if (m_writeData.size() > 0 || values.size() == 0)
        return false;
    m_writeData.append(values);
    m_currentCommand = command;
    m_currentWriteCommand = command;
    m_cmd = values;
    if (m_currentCommand == Commands::Program)
    {
        m_hexFile = hexFile;
    }
    qint64 ret = m_port->write(m_writeData);
    if (command == Commands::DownloadLine)
    {
        m_writeData.clear();
        m_currentWriteCommand = Commands::Idle;
    }
    return (ret != -1) ? true : false;
}

void Serial::on_tryConnectTimeout()
{
    static QTime t;
    static bool started = false;
    if (!started)
    {
        started = true;
        t.start();
    }
    if (t.elapsed() >= m_connectionTimeout)
    {
        started = false;
        m_connectTimer->stop();
        if (!m_connected)
        {
            m_port->close();
            connected(false, "Error: No initial reply from bootloader");
            return;
        }
    }
    if (!m_connected)
    {
        QString cmd = "UUUU\n";
        prepareCommandAndWrite(Commands::Connect, cmd);
    }
    else if (m_connected)
    {
        started = false;
        m_connectTimer->stop();
        //emit connected(true);
    }
}

void Serial::on_uploadTimeout()
{
    m_count = 0;
    m_port->clear();
    m_port->flush();
    emit firmwareUploaded(false, "Upload timeout: probably you have less flash/eeprom size available than you specified...");
    m_currentCommand = Commands::Idle;
}
