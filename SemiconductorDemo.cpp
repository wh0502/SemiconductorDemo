#include "SemiconductorDemo.h"
#include <QMessageBox>
#include <QRandomGenerator>
#include <QDateTime>

SemiconductorDemo::SemiconductorDemo(QWidget *parent)
    : QMainWindow(parent),tcpServer(nullptr), clientSocket(nullptr),dataCount(0),isAlarmActive(false)
{
    ui.setupUi(this);
    //初始化ui
    initUi();
    //初始化信号与槽
    initConnect();
    //初始化状态机
    machine = new QStateMachine(this);
    idleState = new QState();//空闲状态
    runningState = new QState();//运行状态
    alarmState = new QState();//报警状态

    idleState->addTransition(startStopBtn, &QPushButton::clicked, runningState);//空闲状态下点击启动--》运行状态
    runningState->addTransition(this, &SemiconductorDemo::alarmTriggered, alarmState);//运行状态下自定义信号--》报警状态
    runningState->addTransition(startStopBtn, &QPushButton::clicked, idleState);//运行状态下点击复位--》空闲状态
    alarmState->addTransition(resetBtn, &QPushButton::clicked, idleState);//报警状态下点击复位--》空闲状态

    connect(idleState, &QState::entered, this, [=]()//自带信号，当状态被激活触发,进入状态时触发
        {
            stateLabel->setText(QString::fromLocal8Bit("当前状态: Idle"));
            startStopBtn->setText(QString::fromLocal8Bit("启动运行"));
            resetBtn->setEnabled(false);
            if (clientSocket)
                clientSocket->disconnectFromHost();
            if (tcpServer) {
                tcpServer->close();
                delete tcpServer;
                tcpServer = nullptr;
            }
            appendLog(QString::fromLocal8Bit("设备进入空闲状态"));
        });

    connect(runningState, &QState::entered, this, [=]()
    {
        stateLabel->setText(QString::fromLocal8Bit("当前状态: Running"));
        startStopBtn->setText(QString::fromLocal8Bit("停止"));
        resetBtn->setEnabled(false);
        //启动TCP服务器
        if (!tcpServer) {
            tcpServer = new QTcpServer(this);
            connect(tcpServer, &QTcpServer::newConnection, this, &SemiconductorDemo::onNewConnection);
            if (!tcpServer->listen(QHostAddress::Any, 12345)) {
                appendLog(QString::fromLocal8Bit("TCP 服务器启动失败: ") + tcpServer->errorString(), "ERROR");
            }
            else {
                appendLog(QString::fromLocal8Bit("TCP 服务器已启动，端口 12345，等待设备连接..."));
            }
        }
        //appendLog(QString::fromLocal8Bit("设备进入运行状态，开始接收数据"));
    });

    connect(alarmState, &QState::entered, this, [=]() {
        stateLabel->setText(QString::fromLocal8Bit("当前状态: Alarm"));
        startStopBtn->setEnabled(false);
        resetBtn->setEnabled(true);
        isAlarmActive = true;
        appendLog(QString::fromLocal8Bit("【报警】设备异常，请检查!"), QString::fromLocal8Bit("ALARM"));
        QMessageBox::critical(this, QString::fromLocal8Bit("报警"), QString::fromLocal8Bit("设备触发报警！温度过高或位置超限。"));
        });

    connect(alarmState, &QState::exited, this, [=]() { //离开状态时触发
        startStopBtn->setEnabled(true);
        isAlarmActive = false;
        dataCount = 0;
        tempData.clear();
        posData.clear();
        temperaturePlot->graph(0)->data()->clear();
        positionPlot->graph(0)->data()->clear();
        temperaturePlot->replot();
        positionPlot->replot();
        appendLog(QString::fromLocal8Bit("报警已复位，设备回到空闲状态"));
        });

    machine->addState(idleState);
    machine->addState(runningState);
    machine->addState(alarmState);
    machine->setInitialState(idleState);//默认状态
    machine->start();
    appendLog(QString::fromLocal8Bit("上位机初始化完成，等待启动"));
}

void SemiconductorDemo::initUi()
{
    //创建中心部件
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    //状态控制区域
    QGroupBox* controlGroup = new QGroupBox("Device Control");
    QHBoxLayout* controlLayout = new QHBoxLayout(controlGroup);
    startStopBtn = new QPushButton(QString::fromLocal8Bit("启动运行"));
    resetBtn = new QPushButton(QString::fromLocal8Bit("复位"));
    resetBtn->setEnabled(false);
    stateLabel = new QLabel(QString::fromLocal8Bit("当前状态: Idle"));
    controlLayout->addWidget(startStopBtn);
    controlLayout->addWidget(resetBtn);
    controlLayout->addWidget(stateLabel);
    controlLayout->addStretch();
    mainLayout->addWidget(controlGroup);

    //实时曲线区域
    QGroupBox* plotGroup = new QGroupBox("realTime data");
    QVBoxLayout* plotLayout = new QVBoxLayout(plotGroup);

    temperaturePlot = new QCustomPlot();
    temperaturePlot->addGraph();
    temperaturePlot->graph(0)->setPen(QPen(Qt::red));
    temperaturePlot->xAxis->setLabel("number of sampling points");
    temperaturePlot->yAxis->setLabel("temperature");
    temperaturePlot->xAxis->setRange(0, 100);
    temperaturePlot->yAxis->setRange(0, 150);

    positionPlot = new QCustomPlot();
    positionPlot->addGraph();
    positionPlot->graph(0)->setPen(QPen(Qt::blue));
    positionPlot->xAxis->setLabel("number of sampling points");
    positionPlot->yAxis->setLabel("position");
    positionPlot->xAxis->setRange(0, 100);
    positionPlot->yAxis->setRange(-50, 50);

    plotLayout->addWidget(temperaturePlot);
    plotLayout->addWidget(positionPlot);
    mainLayout->addWidget(plotGroup);

    //日志区域
    QGroupBox* logGroup = new QGroupBox("Logs and Al");
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    logEdit = new QTextEdit();
    logEdit->setReadOnly(true);
    logLayout->addWidget(logEdit);
    logGroup->setLayout(logLayout);
    mainLayout->addWidget(logGroup);
    logGroup->setMinimumHeight(100);   // 保证日志区域最少100像素高

    // ---------- 设置拉伸因子 ----------
    mainLayout->setStretchFactor(controlGroup, 0);
    mainLayout->setStretchFactor(plotGroup, 3);
    mainLayout->setStretchFactor(logGroup, 1);
}
void SemiconductorDemo::initConnect()
{
    connect(startStopBtn, &QPushButton::clicked, this, &SemiconductorDemo::onStartStopButton);//启动
    connect(resetBtn, &QPushButton::clicked, this, &SemiconductorDemo::onAlarmResetButton);//复位
}

void SemiconductorDemo::onStartStopButton()
{
    if (runningState->active()) {
        // 当前是运行状态，将要切换到空闲，手动关闭服务器
        if (tcpServer) {
            tcpServer->close();
            delete tcpServer;
            tcpServer = nullptr;
        }
        if (clientSocket) {
            clientSocket->disconnectFromHost();
            clientSocket = nullptr;
        }
        //appendLog(QString::fromLocal8Bit("停止运行，TCP服务器已关闭"));
    }
}
void SemiconductorDemo::onAlarmResetButton()
{

}
SemiconductorDemo::~SemiconductorDemo()
{
    if (tcpServer) tcpServer->close();
}

void SemiconductorDemo::onNewConnection()
{
    if (!tcpServer) return;
    clientSocket = tcpServer->nextPendingConnection();
    connect(clientSocket, &QTcpSocket::readyRead, this, &SemiconductorDemo::onReadyRead);
    connect(clientSocket, &QTcpSocket::disconnected, this, &SemiconductorDemo::onClientDisconnected);
    appendLog(QString::fromLocal8Bit("新设备已连接: ") + clientSocket->peerAddress().toString());
}

void SemiconductorDemo::onReadyRead() {
    if (!clientSocket) return;
    QByteArray data = clientSocket->readAll();
    // 模拟解析协议：假设数据格式 "TEMP:25.5,POS:10.2"
    QString msg = QString::fromUtf8(data).trimmed();
    appendLog(QString::fromLocal8Bit("收到数据: ") + msg, QString::fromLocal8Bit("DEBUG"));

    //解析
    double temperature = 0.0, position = 0.0;
    if (msg.contains("TEMP:")) {
        int idx = msg.indexOf("TEMP:") + 5;//查找出现的下标
        int end = msg.indexOf(",", idx);
        if (end == -1) end = msg.length();
        temperature = msg.mid(idx, end - idx).toDouble();
    }
    if (msg.contains("POS:")) {
        int idx = msg.indexOf("POS:") + 4;
        int end = msg.indexOf(",", idx);
        if (end == -1) end = msg.length();
        position = msg.mid(idx, end - idx).toDouble();
    }
    tempData.append(temperature);
    posData.append(position);
    dataCount++;

    if (tempData.size() > 100) {
        tempData.removeFirst();
        posData.removeFirst();
    }

    QVector<double> x(tempData.size());
    for (int i = 0; i < tempData.size(); ++i) x[i] = i;
    temperaturePlot->graph(0)->setData(x, tempData);//x和y轴坐标
    temperaturePlot->rescaleAxes(true);
    temperaturePlot->replot();
    positionPlot->graph(0)->setData(x, posData);
    positionPlot->rescaleAxes(true);
    positionPlot->replot();

    checkAlarm(temperature, position);
}
void SemiconductorDemo::onClientDisconnected() {
    //appendLog(QString::fromLocal8Bit("设备断开连接"));
    if (clientSocket) {
        clientSocket->deleteLater();
        clientSocket = nullptr;
    }
}

//判断当前接收到的温度和位置数据是否超出安全阈值，如果超出且尚未处于报警状态，则触发报警信号，让状态机切换到 Alarm 状态。
void SemiconductorDemo::checkAlarm(double temperature,double position)
{
    if (!isAlarmActive && (temperature > 100.0 || qAbs(position) > 40.0)) {
        emit alarmTriggered();
    }
}
void SemiconductorDemo::appendLog(const QString& msg, const QString& level)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString coloredMsg;
    if (level == "ERROR") coloredMsg = QString("<font color='red'>[%1]</font>").arg(level);
    else if (level == "ALARM") coloredMsg = QString("<font color='#FF6600'>[%1]</font>").arg(level);
    else if (level == "DEBUG") coloredMsg = QString("<font color='gray'>[%1]</font>").arg(level);
    else coloredMsg = QString("[%1]").arg(level);
    logEdit->append(QString("[%1] %2 %3").arg(timestamp, coloredMsg, msg));
}