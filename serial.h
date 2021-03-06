#ifndef SERIAL_H
#define SERIAL_H

#include <QObject>
#include <QSerialPort>

#include "commands.h"
#include "common/hexfile.h"

class QTimer;

class Serial : public QObject
{
    Q_OBJECT
public:
    explicit Serial(QObject *parent = nullptr);
    ~Serial();
    bool tryConnectToBootloader(QString port, qint32 baudRate, int connectionTimeout=2000);
    void disconnectFromBootloader();
    QString portName() const;
    bool isOpen() const;
    void clear() const;
    void close() const;

    void program(const HexFile &hexFile, bool doFlash);

    static const char XON  = 0x11;
    static const char XOFF = 0x13;

signals:
    void do_parse(const QByteArray readData, QPrivateSignal);
    void connected(bool, const QString &msg="");
    void uploadedProgress(int progress);
    void firmwareUploaded(bool, const QString &msg="");

private slots:
    void on_tryConnectTimeout();
    void on_uploadTimeout();

private:
    void handleBytesWritten(qint64 bytes);
    void handleReadyRead();
    void handleError(QSerialPort::SerialPortError serialPortError);
    void parse(const QByteArray readData);
    //bool downloadLine(QString s);
    bool prepareCommandAndWrite(const Commands command, const QString values, HexFile hexFile = HexFile());

    QSerialPort* m_port;
    bool m_connected = false;
    bool m_activeBootloader = false;
    int m_connectionTimeout = 2000;
    QTimer* m_connectTimer;
    QTimer* m_uploadTimer;
    QByteArray m_readData;
    QByteArray m_writeData;

    int m_count = 0;
    bool m_doFlash;
    QString m_cmd;
    HexFile m_hexFile;
    Commands m_currentCommand = Commands::Idle;
    Commands m_currentWriteCommand = Commands::Idle;
};

#endif // SERIAL_H
