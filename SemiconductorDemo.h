#pragma once

#include <QtWidgets/QMainWindow>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QStateMachine>
#include <QState>
#include <QTcpServer>
#include <QTcpSocket>
#include "qcustomplot.h"
#include "ui_SemiconductorDemo.h"

class SemiconductorDemo : public QMainWindow
{
    Q_OBJECT

public:
    SemiconductorDemo(QWidget *parent = nullptr);
    ~SemiconductorDemo();

signals:
    void alarmTriggered();//触发报警的信号
private slots:
    void onStartStopButton();
    void onAlarmResetButton();
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();
    void checkAlarm(double temperature, double position);
    void appendLog(const QString& msg, const QString& level = "INFO");

private:
    void initUi();
    void initConnect();
private:
    Ui::SemiconductorDemoClass ui;

    //状态机
    QStateMachine* machine;
    QState* idleState;
    QState* runningState;
    QState* alarmState;

    // 通信
    QTcpServer* tcpServer;
    QTcpSocket* clientSocket;

    //控件
    QPushButton* startStopBtn;
    QPushButton* resetBtn;
    QLabel* stateLabel;
    QTextEdit* logEdit;
    QCustomPlot* temperaturePlot;
    QCustomPlot* positionPlot;

    //数据存储
    bool isAlarmActive;
    int dataCount;
    QVector<double> tempData;
    QVector<double> posData;


};

