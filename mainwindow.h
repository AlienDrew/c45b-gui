#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QTimer;
class Serial;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event);

private:
    enum class MsgType : quint8
    {
        Ok,
        Alert,
    };

    void updatePorts();
    void initBaudRates();
    void consoleOutput(QString line, MsgType type = MsgType::Ok);
    void on_baudRateCustom(const QString &baudRate);
    void on_connect();
    void on_connected(bool, const QString &msg);
    void on_program_click();

    QTimer* m_port_timer;
    Serial* m_port;
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
