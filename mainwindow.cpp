#include <QTimer>
#include <QDebug>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QFileDialog>

#include "hexfile.h"
#include "hexutils.h"
#include "serial.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::updatePorts()
{
    QString curr = ui->cmbPort->currentText();
    //ui->cmbPort->clear();
    foreach (const QSerialPortInfo &serialPortInfo, QSerialPortInfo::availablePorts())
    {
        if (ui->cmbPort->findText(serialPortInfo.portName()) == -1)
            ui->cmbPort->addItem(serialPortInfo.portName());
    }

    for (auto i=0; i<ui->cmbPort->count(); i++)
    {
        bool found = false;
        foreach (const QSerialPortInfo &serialPortInfo, QSerialPortInfo::availablePorts())
        {
            if (ui->cmbPort->itemText(i) == serialPortInfo.portName())
                found = true;
        }
        if (found == false)
        {
            ui->cmbPort->removeItem(i);
        }
    }
    if (ui->cmbPort->findText(curr) != -1)
        ui->cmbPort->setCurrentText(curr);
}

void MainWindow::initBaudRates()
{
    QStringList baudList = {
        "230400",
        "115200",
        "76800",
        "57600",
        "38400",
        "28800",
        "19200",
        "14400",
        "9600",
        "4800",
        "2400",
        "1200",
        "300",
        "150",
        "100",
    };
    ui->baudRateBox->addItems(baudList);
}

void MainWindow::consoleOutput(QString line, MsgType type)
{
    QTextCursor cursor = ui->console->textCursor();
    switch(type)
    {
        case MsgType::Alert: ui->console->setTextColor(Qt::darkRed); break;
        case MsgType::Ok: ui->console->setTextColor(Qt::darkGreen); break;
        default: ui->console->setTextColor(Qt::black); break;
    }
    ui->console->append(line);
    cursor.movePosition(QTextCursor::End);
    ui->console->setTextCursor(cursor);
    statusBar()->showMessage(line);
    qDebug()<<line;
}

void MainWindow::on_baudRateCustom(const QString &baudRate)
{
//    if (!baudRate.toInt())
//        QMessageBox::warning(this, "Error", "Wrong baudrate.");
}

void MainWindow::on_connect()
{
    if (!m_port->isOpen())
    {
        int baudRate = ui->baudRateBox->currentText().toInt();
        if (baudRate == 0)
        {
            QMessageBox::warning(this, "Error", "Wrong baudrate");
            return;
        }
        if (!m_port->tryConnectToBootloader(this->ui->cmbPort->currentText(), baudRate, ui->timeoutBox->value()))
        {
            QMessageBox::warning(this, "Error", tr("Could not connect to port"));
            return;
        }
        m_port_timer->stop();
        ui->cmbPort->setEnabled(false);
        ui->baudRateBox->setEnabled(false);
        ui->connectButton->setText("Connecting...");
        ui->connectButton->setEnabled(false);
        ui->timeoutBox->setEnabled(false);
        consoleOutput(tr("Connecting to ")+m_port->portName()+"...");
    }
    else
    {
        m_port->disconnectFromBootloader();
    }
}

void MainWindow::on_connected(bool is_connected, const QString &msg)
{
    if (is_connected)
    {
        ui->connectButton->setText("Disconnect");
        ui->connectButton->setEnabled(true);
        ui->timeoutBox->setEnabled(false);
        ui->programButton->setEnabled(true);
        ui->programEepromButton->setEnabled(true);
        consoleOutput(tr("Connected to ")+m_port->portName());
        consoleOutput(msg);
    }
    else
    {
        ui->connectButton->setText("Connect");
        ui->connectButton->setEnabled(true);
        ui->timeoutBox->setEnabled(true);
        m_port_timer->start();
        ui->cmbPort->setEnabled(true);
        ui->baudRateBox->setEnabled(true);
        ui->programButton->setEnabled(false);
        ui->programEepromButton->setEnabled(false);
        if (msg.isEmpty())
            consoleOutput(tr("Unable to connect to ")+m_port->portName(), MsgType::Alert);
        consoleOutput(msg, MsgType::Alert);
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setFixedSize(this->width(),this->height());
    m_port_timer = new QTimer(this);
    connect(m_port_timer, &QTimer::timeout, this, &MainWindow::updatePorts);
    m_port_timer->start(400);
    foreach (const QSerialPortInfo &serialPortInfo, QSerialPortInfo::availablePorts())
    {
            ui->cmbPort->addItem(serialPortInfo.portName());
    }
    m_port = new Serial(this);
    connect(m_port, &Serial::connected, this, &MainWindow::on_connected);
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::on_connect);
    connect(ui->baudRateBox, &QComboBox::editTextChanged, this, &MainWindow::on_baudRateCustom);
    connect(ui->displayConsole, &QCheckBox::toggled, [=](bool toggled){
        ui->console->setVisible(toggled);
        ui->consoleLabel->setVisible(toggled);
        ui->consoleClearButton->setVisible(toggled);
    });
    connect(ui->consoleClearButton, &QPushButton::clicked, [=](){
        ui->console->clear();
    });
    connect(ui->selectHexButton, &QPushButton::clicked, [=](){
        ui->hexFilePath->setText(
                    QFileDialog::getOpenFileName(
                        this,
                        tr("Select Flash Hex File"), "", tr("Hex Files (*.hex)")
                    )
        );
    });
    connect(ui->selectEepromButton, &QPushButton::clicked, [=](){
        ui->eepromFilePath->setText(
                    QFileDialog::getOpenFileName(
                        this,
                        tr("Select EEPROM Hex File"), "", tr("Hex Files (*.eep)")
                    )
        );
    });
    connect(ui->programButton, &QPushButton::clicked, [=](){
        if (ui->hexFilePath->text().isEmpty())
        {
            QMessageBox::warning(this, "Error", tr("Flash hex file is not specified"));
            return;
        }
        HexFile hexfile;
        hexfile.load(ui->hexFilePath->text(), true);
        m_port->program(hexfile, true);
    });
    connect(ui->programEepromButton, &QPushButton::clicked, [=](){
        if (ui->eepromFilePath->text().isEmpty())
        {
            QMessageBox::warning(this, "Error", tr("EEPROM file is not specified"));
            return;
        }
        HexFile hexfile;
        hexfile.load(ui->eepromFilePath->text(), true);
        m_port->program(hexfile, false);
    });
    initBaudRates();
}

MainWindow::~MainWindow()
{
    delete ui;
}

