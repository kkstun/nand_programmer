/*  Copyright (C) 2017 Bogdan Bogush <bogdan.s.bogush@gmail.com>
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "main_window.h"
#include "ui_main_window.h"
#include "chip_db.h"
#include "logger.h"
#include <QDebug>
#include <QFileDialog>
#include <QFile>
#include <QStringList>
#include <memory>

#define HEADER_ADDRESS_WIDTH 80
#define HEADER_HEX_WIDTH 340
#define BUFFER_ROW_HEIGHT 20

#define START_ADDRESS 0x00000000

static void addChipDB(QComboBox *chipSelectComboBox)
{
    ChipInfo *db;
    uint32_t size = getChipDB(db);

    for (uint32_t i = 0; i < size; i++)
        chipSelectComboBox->addItem(db[i].name);
}

void MainWindow::initBufTable()
{
    buffer = nullptr;
    bufferSize = 0;

    ui->bufferTableView->setModel(&bufferTableModel);
    QHeaderView *verticalHeader = ui->bufferTableView->verticalHeader();
    verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader->setDefaultSectionSize(BUFFER_ROW_HEIGHT);
    ui->bufferTableView->setColumnWidth(HEADER_ADDRESS_COL,
        HEADER_ADDRESS_WIDTH);
    ui->bufferTableView->setColumnWidth(HEADER_HEX_COL, HEADER_HEX_WIDTH);
}

void MainWindow::resetBufTable()
{
    bufferTableModel.setBuffer(nullptr, 0);
    bufferSize = 0;
    delete buffer;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    Logger *logger = Logger::getInstance();

    ui->setupUi(this);

    logger->setTextEdit(ui->logTextEdit);

    initBufTable();

    prog = new Programmer(this);

    addChipDB(ui->chipSelectComboBox);
    connect(ui->chipSelectComboBox, SIGNAL(currentIndexChanged(int)),
        this, SLOT(slotSelectChip(int)));

    connect(ui->actionOpen, SIGNAL(triggered()), this,
        SLOT(slotFileOpen()));
    connect(ui->actionConnect, SIGNAL(triggered()), this,
        SLOT(slotProgConnect()));
    connect(ui->actionDetect_Device, SIGNAL(triggered()), this,
        SLOT(slotProgReadDeviceId()));
    connect(ui->actionErase, SIGNAL(triggered()), this,
        SLOT(slotProgErase()));
    connect(ui->actionRead, SIGNAL(triggered()), this,
        SLOT(slotProgRead()));
    connect(ui->actionWrite, SIGNAL(triggered()), this,
        SLOT(slotProgWrite()));
}

MainWindow::~MainWindow()
{
    delete buffer;
    Logger::putInstance();
    delete ui;
}

void MainWindow::slotFileOpen()
{
    qint64 ret, fileSize;
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), ".",
        tr("Binary Files (*)"));

    if (fileName.isNull())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        qCritical() << "Failed to open file:" << fileName << ", error:" <<
            file.errorString();
        return;
    }

    resetBufTable();
    fileSize = file.size();
    buffer = new (std::nothrow) uint8_t[fileSize];
    if (!buffer)
    {
        qCritical() << "Failed to allocate memory for read buffer";
        goto Exit;
    }

    ret = file.read((char *)buffer, fileSize);
    if (ret < 0)
    {
        qCritical() << "Failed to read file:" << fileName << ", error:" <<
            file.errorString();
        goto Exit;
    }

    if (ret != fileSize)
    {
        qCritical() << "File was partially read, length" << ret;
        goto Exit;
    }

    bufferSize = fileSize;
    bufferTableModel.setBuffer(buffer, fileSize);

Exit:
    file.close();
}

void MainWindow::slotProgConnect()
{
    if (!prog->isConnected())
    {
        if (!prog->connect())
            qInfo() << "Connected to programmer";
        else
            return;

        ui->actionConnect->setText(tr("Disconnect"));
    }
    else
    {
        prog->disconnect();
        ui->actionConnect->setText(tr("Connect"));
        qInfo() << "Disconnected from programmer";
    }
}

void MainWindow::readChipIdCb(ChipId id)
{
    QString idStr;

    idStr.sprintf("0x%02X 0x%02X 0x%02X 0x%02X", id.makerId, id.deviceId,
        id.thirdId, id.fourthId);
    ui->deviceValueLabel->setText(idStr);
}

void MainWindow::slotProgReadDeviceId()
{
    prog->readChipId(std::bind(&MainWindow::readChipIdCb, this,
        std::placeholders::_1));
}

void MainWindow::eraseChipCb()
{
    qInfo() << "Chip has been erased successfully";
}

void MainWindow::slotProgErase()
{
    QByteArray ba = ui->chipSelectComboBox->currentText().toLatin1();
    ChipInfo *chipInfo = getChipInfoByName(ba.data());

    prog->eraseChip(std::bind(&MainWindow::eraseChipCb, this), START_ADDRESS,
        chipInfo->size);
}

void MainWindow::readChipCb(int status)
{
    QByteArray ba = ui->chipSelectComboBox->currentText().toLatin1();
    ChipInfo *chipInfo = getChipInfoByName(ba.data());
    uint32_t readSize = chipInfo->size;

    if (status)
    {
        delete buffer;
        return;
    }

    qInfo() << "Data has been successfully read";
    bufferTableModel.setBuffer(buffer, readSize);
}

void MainWindow::slotProgRead()
{
    QByteArray ba = ui->chipSelectComboBox->currentText().toLatin1();
    ChipInfo *chipInfo = getChipInfoByName(ba.data());
    uint32_t readSize = chipInfo->size;

    resetBufTable();
    buffer = new (std::nothrow) uint8_t[readSize];
    if (!buffer)
    {
        qCritical() << "Failed to allocate memory for read buffer";
        return;
    }

    prog->readChip(std::bind(&MainWindow::readChipCb, this,
        std::placeholders::_1), buffer, START_ADDRESS, readSize);
}

void MainWindow::writeChipCb(int status)
{
    if (!status)
        qInfo() << "Data has been successfully written";
}

void MainWindow::slotProgWrite()
{
    if (!bufferSize)
    {
        qInfo() << "Write buffer is empty";
        return;
    }

    prog->writeChip(std::bind(&MainWindow::writeChipCb, this,
        std::placeholders::_1), buffer, START_ADDRESS, bufferSize);
}

void MainWindow::selectChipCb()
{
    qInfo() << "Chip has been selected successfully";
}

void MainWindow::slotSelectChip(int selectedChipNum)
{
    prog->selectChip(std::bind(&MainWindow::selectChipCb, this),
        selectedChipNum);
}
