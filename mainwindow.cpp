#include <QTimer>
#include <QDebug>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QInputDialog>

#include "common/hexfile.h"
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
        ui->eraseFlashButton->setEnabled(true);
        ui->eraseEepromButton->setEnabled(true);
        ui->progressBar->setEnabled(true);
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
        ui->hexFilePath->setEnabled(true);
        ui->eepromFilePath->setEnabled(true);
        ui->selectHexButton->setEnabled(true);
        ui->selectEepromButton->setEnabled(true);
        ui->programButton->setEnabled(false);
        ui->programEepromButton->setEnabled(false);
        ui->eraseFlashButton->setEnabled(false);
        ui->eraseEepromButton->setEnabled(false);
        ui->progressBar->setEnabled(false);
        ui->progressBar->setValue(0);

        if (msg.isEmpty())
            consoleOutput(tr("Unable to connect to ")+m_port->portName(), MsgType::Alert);
        consoleOutput(msg, MsgType::Alert);
    }
}

void MainWindow::on_program_click()
{
    bool ok = false;
    int size = 0;
    HexFile hexfile;
    QObject* caller = QObject::sender();
    if (caller == ui->programButton)
    {
        if (ui->hexFilePath->text().isEmpty())
        {
            QMessageBox::warning(this, "Error", tr("Flash hex file is not specified"));
            return;
        }
        hexfile.load(ui->hexFilePath->text(), true);
        ok = true;
    }
    else if (caller == ui->programEepromButton)
    {
        if (ui->eepromFilePath->text().isEmpty())
        {
            QMessageBox::warning(this, "Error", tr("EEPROM file is not specified"));
            return;
        }
        hexfile.load(ui->eepromFilePath->text(), true);
        ok = true;
    }
    else if (caller == ui->eraseFlashButton)
    {
        size = QInputDialog::getInt(this, tr("Flash size"),
                                         tr("Enter available flash size in kb:"), 0, 0, 256, 1, &ok);

        if (ok)
        {
            for (int i = 0; i < (size)*1024; i++)
            {
                hexfile.append(0xFF);
            }
        }
    }
    else if (caller == ui->eraseEepromButton)
    {
        size = QInputDialog::getInt(this, tr("Eeprom size"),
                                         tr("Enter EEPROM size in bytes:"), 0, 0, 4096, 1, &ok);

        if (ok)
        {
            for (int i = 0; i < size; i++)
            {
                hexfile.append(0xFF);
            }
        }
    }
    if (!hexfile.errorString().isEmpty())
    {
        consoleOutput(hexfile.errorString(), MsgType::Alert);
        return;
    }

    if (ok)
    {
        ui->hexFilePath->setEnabled(false);
        ui->eepromFilePath->setEnabled(false);
        ui->selectHexButton->setEnabled(false);
        ui->selectEepromButton->setEnabled(false);

        ui->programButton->setEnabled(false);
        ui->programEepromButton->setEnabled(false);
        ui->eraseFlashButton->setEnabled(false);
        ui->eraseEepromButton->setEnabled(false);
        ui->connectButton->setEnabled(false);

        if (caller == ui->programButton)
        {
            consoleOutput("Uploading firmware to the chip...");
            m_port->program(hexfile, true);
        }
        else if (caller == ui->programEepromButton)
        {
            if (!hexfile.errorString().isEmpty())
            {
                consoleOutput(hexfile.errorString(), MsgType::Alert);
                return;
            }
            consoleOutput("Uploading eeprom to the chip...");
            m_port->program(hexfile, false);
        }
        else if (caller == ui->eraseFlashButton)
        {
            consoleOutput("Erasing the chip...");
            m_port->program(hexfile, true);
        }
        else if (caller == ui->eraseEepromButton)
        {
            consoleOutput("Erasing EEPROM...");
            m_port->program(hexfile, false);
        }
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

        static quint16 height = this->height();
        if (!toggled)
        {
            height = this->height();
            this->setFixedHeight(this->height()-ui->console->height());
        }
        else
        {
            this->setFixedHeight(height);
            height = this->height();
        }
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
    connect(ui->programButton, &QPushButton::clicked, this, &MainWindow::on_program_click);
    connect(ui->programEepromButton, &QPushButton::clicked, this, &MainWindow::on_program_click);
    connect(ui->eraseFlashButton, &QPushButton::clicked, this, &MainWindow::on_program_click);
    connect(ui->eraseEepromButton, &QPushButton::clicked, this, &MainWindow::on_program_click);
    connect(m_port, &Serial::uploadedProgress, [=](int val){
        ui->progressBar->setValue(val);
        consoleOutput(QString::number(val)+"%");
    });
    connect(m_port, &Serial::firmwareUploaded, [=](bool val, const QString &msg){
        ui->hexFilePath->setEnabled(true);
        ui->eepromFilePath->setEnabled(true);
        ui->selectHexButton->setEnabled(true);
        ui->selectEepromButton->setEnabled(true);

        ui->programButton->setEnabled(true);
        ui->programEepromButton->setEnabled(true);
        ui->eraseFlashButton->setEnabled(true);
        ui->eraseEepromButton->setEnabled(true);
        ui->connectButton->setEnabled(true);

        if (val)
            consoleOutput("Finished! "+msg);
        else
        {
            ui->progressBar->setValue(0);
            if (msg.isEmpty())
                consoleOutput("Some error occured during upload :(", MsgType::Alert);
            else
                consoleOutput(msg, MsgType::Alert);
        }
    });
    initBaudRates();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_port->isOpen())
    {
        consoleOutput("Please disconnect from the device first!", MsgType::Alert);
        event->ignore();
    }
    else
        event->accept();
}

