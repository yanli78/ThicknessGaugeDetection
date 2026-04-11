#include "mainwindow.h"
#include "ui_mainwindow.h"

int key_flag = 0;

int COL_SERIAL = 0;       // A列：序号
int COL_CODE = 0;         // B列：编号
int COL_SPEC = 0;         // C列：规格
int COL_DATA_START = 4;   // D列：数据起始列
int COL_ROW_AVG = 0;      // I列：单行平均值
int COL_ROW_MIN = 0;      // K列：单行最小值
int COL_MERGE_L = 0;      // L列：单行合并起始列
int COL_MERGE_M = 0;      // M列：单行合并结束列
int COL_SINGLE_CONCL = 0; // N列：单行结论
int COL_GROUP_AVG = 0;    // J列：分组平均值
int COL_GROUP_CONCL = 0;  // O列：分组结论
int DETECT_COL_START = 1; // 检测行合并起始列（A）
int DETECT_COL_END = 12;  // 检测行合并结束列（S）

// 行配置
int START_ROW = 9;               // 数据起始行（必须≥1）
int DATA_PER_ROW = 5;            // 每行5个数据
int GROUP_SIZE = 3;              // 每3行一组
int MAX_SEARCH_ROW = 500;        // 续填最大查找行
double avgPASS_THRESHOLD = 90.0; // 平均合格阈值
double minPASS_THRESHOLD = 90.0; // 最小合格阈值

// 路径配置
QString TEMPLATE_NAME = "1.xlsx";
QString SAVE_NAME = "测量数据.xlsx";
QString sheetName;

const int FONT_COLOR_RED = 255; // 红色字体（不合格）
const int FONT_COLOR_BLACK = 0; // 黑色字体（默认/合格）
const int ALIGN_GENERAL = -4107;
const int ALIGN_CENTER = -4108;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->dateEdit->setDate(QDate::currentDate());

    connect(ui->pushButton_save, &QPushButton::clicked, this, &MainWindow::saveToFile);
    connect(ui->pushButton_deleteRow, &QPushButton::clicked, this, &MainWindow::deleteSelectedRow);
    QTimer::singleShot(1000, this, [this]()
                       { on_pushButton_clear_clicked(); });
    serial = new QSerialPort(this);
    statusLabel = new QLabel(this);
    this->statusBar()->addWidget(statusLabel);
    ui->radioButton->setEnabled(false);
    // ui->comboBox_2->setCurrentIndex(2);
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::dataReceive);
    ui->tableWidget->setHorizontalHeaderLabels({"测量结果"});
    ui->tableWidget->setColumnCount(1);
    ui->label_max->setText("最大值：");
    ui->label_min->setText("最小值:");
    ui->label_avg->setText("平均值");
    ui->radioButton_3->setChecked(true);
    connect(ui->pushButton_clear, &QPushButton::clicked, this, &MainWindow::on_pushButton_clear_clicked);
    // 注意：把MainWindow换成centralWidget，同时用相对路径
    ui->centralwidget->setAttribute(Qt::WA_TranslucentBackground, true);
    ui->centralwidget->setAutoFillBackground(false);

    // 初始化背景图片（相对路径/绝对路径均可）
    QString validBgPath = findValidBgImage();
    if (!validBgPath.isEmpty())
    {
        m_bgPixmap.load(validBgPath); // 加载找到的有效图片
    }
    // 触发重绘
    this->update();

        // TCP服务器初始化（成员变量，不再是局部变量）
    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection, this, &MainWindow::onNewConnection);
    // 监听8080端口
    if (!tcpServer->listen(QHostAddress::Any, 8080))
    {
        statusLabel->setText(QString("监听失败：%1").arg(tcpServer->errorString()));
    }
    else
    {
        statusLabel->setText("状态：监听中 (端口8080)");
    }
    // serial->setPortName("COM5");
    serial->setBaudRate(QSerialPort::Baud9600);       // 设置波特率
    serial->setDataBits(QSerialPort::Data8);            // 设置数据位
    serial->setParity(QSerialPort::NoParity);           // 设置校验位
    serial->setStopBits(QSerialPort::OneStop);          // 设置停止位
    serial->setFlowControl(QSerialPort::NoFlowControl); // 设置流控制

    this->setFocusPolicy(Qt::StrongFocus);

    m_udpListener = new QUdpSocket(this);
    // 绑定 UDP 9999 端口，允许其他地址绑定 (ShareAddress)
    if (m_udpListener->bind(QHostAddress::Any, 9999, QUdpSocket::ShareAddress))
    {
        connect(m_udpListener, &QUdpSocket::readyRead, this, &MainWindow::onUdpBroadcastReceived);
        statusLabel->setText("状态：TCP监听8080 / UDP发现监听9999");
    }
    else
    {
        statusLabel->setText("UDP端口9999绑定失败！");
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

// 核心：重写paintEvent，直接绘制主窗口背景（无控件参与）
void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    // 1. 创建画家对象，绘制到主窗口本身
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform); // 平滑缩放

    if (!m_bgPixmap.isNull())
    {
        // 2. 绘制背景图片，适配窗口大小（保持比例且覆盖整个窗口）
        QPixmap scaledPix = m_bgPixmap.scaled(
            this->size(),
            Qt::KeepAspectRatioByExpanding, // 保持比例，超出窗口部分裁剪
            Qt::SmoothTransformation        // 平滑缩放，避免模糊
        );
        // 居中绘制图片（让图片在窗口中心显示）
        QRect pixRect = scaledPix.rect();
        pixRect.moveCenter(this->rect().center());
        painter.drawPixmap(pixRect, scaledPix);
    }
    else
    {
        // 图片加载失败时，绘制默认背景（可选）
        // painter.fillRect(this->rect(), Qt::white);
        //qWarning() << "背景图片加载失败，请检查路径！";
    }
}

// 自动查找存在的背景图片，返回第一个有效的路径（空字符串表示无有效图片）
QString MainWindow::findValidBgImage()
{
    // 定义候选背景图片路径列表（可按需添加更多格式，如bmp、gif等）
    QList<QString> candidatePaths = {
        "./images/back.png",
        "./images/back.jpg"};

    // 遍历候选列表，检查每个图片是否有效
    for (const QString &path : candidatePaths)
    {
        // 第一步：检查文件是否存在
        if (!QFile::exists(path))
        {
            qDebug() << "图片文件不存在：" << path;
            continue; // 跳过不存在的文件，检查下一个
        }

        // 第二步：检查图片能否成功加载（避免文件存在但损坏/格式错误）
        QPixmap tempPix;
        if (tempPix.load(path))
        {
            qDebug() << "找到有效背景图片，加载：" << path;
            return path; // 找到第一个有效图片，直接返回路径
        }
        else
        {
            qDebug() << "图片文件存在但无法加载（可能损坏）：" << path;
        }
    }

    // 所有候选都无效时返回空字符串
    qWarning() << "⚠️ 所有候选背景图片都不存在/无法加载！";
    return "";
}

void MainWindow::on_pushButton_clicked()
{
    ui->comboBox->clear();
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        QSerialPort _com;
        _com.setPort(info);
        if (_com.portName() == serial->portName() or _com.open(QIODevice::ReadWrite))
        {
            ui->comboBox->addItem(info.portName());
            _com.close();
        }
    }
}

void MainWindow::on_pushButton_2_clicked()
{

    if (!serial->isOpen())
    {
        serialSet();
        if (serial->open(QIODevice::ReadWrite))
        {
            // ui->radioButton->setChecked(true);
            ui->pushButton_2->setText("关闭串口");
        }
        else
        {
            return;
        }
    }
    else
    {
        serial->close();
        // ui->radioButton->setChecked(false);
        ui->pushButton_2->setText("打开串口");
    }
}

void MainWindow::serialSet()
{
    serial->setPortName(ui->comboBox->currentText());
}

const QByteArray hardcodeHexData = QByteArray::fromHex("2a43181c000000000000000000000000");
void MainWindow::on_pushButton_3_clicked()
{
    ui->lineEdit_result->setText(QString::number(tempResult, 'f', 6));
    if (!serial->isOpen())
    {
        QMessageBox::warning(this, "错误", "串口未打开，请先打开串口！");
        return;
    }
    serial->write(hardcodeHexData);
    // MainWindow::updataStatistics();
}

// 在MainWindow类中定义成员变量（缓存不完整数据）
QByteArray m_dataCache;

void MainWindow::dataReceive()
{
    // ========== 新增1：防护串口空指针（避免崩溃） ==========
    if (!serial)
    {
        ui->textBrowser->append("错误：串口未初始化！");
        return;
    }

    // ========== 原有缓存逻辑：保留但优化 ==========
    m_dataCache.append(serial->readAll()); // 数据存入缓存
    ui->textBrowser->moveCursor(QTextCursor::End);

    // ========== 新增2：初始化表格列（避免列数不足导致setItem失败） ==========
    // 确保表格有至少3列（0:样品名称, 1:测量位置, 2:测量结果）
    if (ui->tableWidget->columnCount() != 1)
    {
        ui->tableWidget->setColumnCount(1);
        // 设置列标题（可选，提升可读性）
        ui->tableWidget->setHorizontalHeaderLabels({"测量结果"});
    }

    bluetoothprotocolparser.onDataReceived(m_dataCache);
    double result = bluetoothprotocolparser.thick;

        // ========== 表格插入数据（原有逻辑保留） ==========
        int row = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(row);

        // 设置“测量结果”列（第2列）
        QTableWidgetItem *resultItem = new QTableWidgetItem(QString::number(result, 'f', 2));
        ui->tableWidget->setItem(row, 0, resultItem);

        QScrollBar *vScrollBar = ui->tableWidget->verticalScrollBar();
        if (vScrollBar)
        { // 防护滚动条空指针
            vScrollBar->setValue(vScrollBar->maximum());
        }

        // ========== 新增4：将有效数据存入统计列表 + 触发统计更新 ==========
        measureValues.append(static_cast<double>(result)); // 转double存入统计列表

        updateStatistics(); // 立即更新最大值/最小值/平均值
}

void MainWindow::on_pushButton_5_clicked()
{
    ui->textBrowser->clear();
}

void MainWindow::on_pushButton_finish_clicked()
{
    ui->lineEdit_result->setText(QString::number(tempResult, 'f', 2));
}

void MainWindow::updateStatistics() // 建议修正拼写为updateStatistics（消除语义警告）
{
    // 1. 定义常量：避免魔法数字，消除“幻数”警告
    const int DECIMAL_PLACES = 2; // 小数位数常量
    const QString LABEL_PREFIX_MAX = "最大值:";
    const QString LABEL_PREFIX_MIN = "最小值:";
    const QString LABEL_PREFIX_AVG = "平均值:";

    // 2. 空列表处理：保持逻辑，但格式规范化
    if (measureValues.isEmpty())
    {
        ui->label_max->setText(LABEL_PREFIX_MAX);
        ui->label_min->setText(LABEL_PREFIX_MIN);
        ui->label_avg->setText(LABEL_PREFIX_AVG);
        return;
    }

    // 3. 单次遍历计算最大/最小/总和（消除重复遍历的性能警告）
    double maxVal = measureValues.first();
    double minVal = measureValues.first();
    double sum = 0.0;                           // 显式初始化为0.0（消除“未初始化”警告）
    const int dataCount = measureValues.size(); // 缓存size，避免多次调用

    for (const double &val : measureValues)
    { // const& 避免拷贝，消除性能警告
        if (val > maxVal)
        {
            maxVal = val;
        }
        if (val < minVal)
        {
            minVal = val;
        }
        sum += val;
    }

    // 4. 除数安全校验（虽isEmpty已过滤，仍显式校验消除“除零”警告）
    double avgVal = 0.0;
    if (dataCount > 0)
    {
        avgVal = sum / static_cast<double>(dataCount); // 显式类型转换，消除精度警告
    }

    // 5. 格式化字符串：规范化参数，消除格式警告
    ui->label_max->setText(QString("%1%2").arg(LABEL_PREFIX_MAX).arg(maxVal, 0, 'f', DECIMAL_PLACES));
    ui->label_min->setText(QString("%1%2").arg(LABEL_PREFIX_MIN).arg(minVal, 0, 'f', DECIMAL_PLACES));
    ui->label_avg->setText(QString("%1%2").arg(LABEL_PREFIX_AVG).arg(avgVal, 0, 'f', DECIMAL_PLACES));

    ui->label_13->setText(QString("测量总数: %1").arg(ui->tableWidget->rowCount()));
    ui->label_2->setText(QString("本行第几个: %1").arg(ui->tableWidget->rowCount() % DATA_PER_ROW));
}

void MainWindow::on_pushButton_clear_clicked()
{
    ui->tableWidget->setRowCount(0);
    measureValues.clear();
    ui->label_max->setText("最大值:");
    ui->label_min->setText("最小值:");
    ui->label_avg->setText("平均值:");
    updateStatistics();
}

void MainWindow::dowork()
{

    // 根据take_flag执行串口写入
    if (key_flag == 1)
    {
        // ui->textBrowser->append("3");
        if (serial->isOpen())
        { // 增加判空，避免异常
            // QMessageBox::warning(this,"错误","1！");

            serial->write(hardcodeHexData);

            // ui->textBrowser->append("2");
            key_flag = 0;
        }
    }
    // ui->textBrowser->append("3");
}

void MainWindow::onNewConnection()
{
    QTcpSocket *clientSocket = tcpServer->nextPendingConnection();
    clientSockets.append(clientSocket);

    // 绑定信号槽
    connect(clientSocket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(clientSocket, &QTcpSocket::disconnected, this, &MainWindow::onClientDisconnected);
    connect(clientSocket, &QTcpSocket::disconnected, clientSocket, &QTcpSocket::deleteLater);

    // 更新状态
    QString clientIp = clientSocket->peerAddress().toString();
    statusLabel->setText(QString("状态：已连接 - %1").arg(clientIp));
    // msgDisplay->append(QString("[%1] 客户端连接：%2").arg(
    // QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"), clientIp));
}

// 接收数据处理
void MainWindow::onReadyRead()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket *>(sender());
    if (!clientSocket)
        return;

    // 读取所有数据（树莓派发送的是ASCII字符串）
    QByteArray data = clientSocket->readAll();
    QString msg = QString::fromUtf8(data).trimmed(); // 去除换行符

    // ui->textBrowser->append(msg);

    if (msg == "BTN1")
    {
        key_flag = 1;
        // QMessageBox::warning(this,"错误","2！");
        qDebug() << "bnt1";
        serial->write(hardcodeHexData);
        // ui->textBrowser->append("3");
        // ui->textBrowser->append("1");
    }
}

// 客户端断开连接处理
void MainWindow::onClientDisconnected()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket *>(sender());
    if (!clientSocket)
        return;

    QString clientIp = clientSocket->peerAddress().toString();
    // msgDisplay->append(QString("[%1] 客户端断开：%2").arg(
    //::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"), clientIp));

    // 从列表中移除
    clientSockets.removeOne(clientSocket);

    // 更新状态
    if (clientSockets.isEmpty())
    {
        statusLabel->setText("状态：等待连接...");
    }
    else
    {
        statusLabel->setText(QString("状态：已连接 - %1个客户端").arg(clientSockets.size()));
    }
}

void MainWindow::onUdpBroadcastReceived()
{
    while (m_udpListener->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(m_udpListener->pendingDatagramSize());
        QHostAddress senderAddr; // 用来存树莓派的 IP
        quint16 senderPort;      // 用来存树莓派的端口

        // 读取数据
        m_udpListener->readDatagram(datagram.data(), datagram.size(), &senderAddr, &senderPort);
        QString msg = QString::fromUtf8(datagram).trimmed();

        // 如果收到特定的暗号 "DISCOVER_SERVER"
        if (msg == "DISCOVER_SERVER")
        {
            // 获取本机的局域网 IP 地址
            QString localIp;
            QList<QHostAddress> ipList = QNetworkInterface::allAddresses();
            for (const QHostAddress &addr : ipList)
            {
                // 找 IPv4 且不是 127.0.0.1 的地址
                if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback())
                {
                    // 这里可以简单过滤一下，通常局域网是 192.168 或 10 开头
                    // 为了通用性，我们直接取第一个非本地回环地址，或者你可以根据需要筛选
                    localIp = addr.toString();
                    break;
                }
            }

            if (localIp.isEmpty())
                localIp = "127.0.0.1";

            // 构造回复消息，格式例如："SERVER_IP:192.168.1.105:8080"
            QString replyMsg = QString("SERVER_IP:%1:8080").arg(localIp);

            // 把这个消息发回给树莓派
            m_udpListener->writeDatagram(replyMsg.toUtf8(), senderAddr, senderPort);
        }
    }
}

QStringList MainWindow::getTableThirdColumn(QTableWidget *tableWidget)
{
    QStringList thirdColData;
    if (!tableWidget)
    {
        qDebug() << "TableWidget指针为空！";
        return thirdColData;
    }

    // 遍历所有行（行索引从0开始）
    int rowCount = tableWidget->rowCount();
    for (int row = 0; row < rowCount; ++row)
    {
        QTableWidgetItem *item = tableWidget->item(row, 0); // 第三列=索引2
        if (item && !item->text().isEmpty())
        { // 过滤空数据
            thirdColData.append(item->text());
        }
        else
        {
            thirdColData.append(""); // 空数据补空字符串，保持索引一致
        }
    }
    return thirdColData;
}

void MainWindow::deleteSelectedRow()
{
    QTableWidget *table = ui->tableWidget;
    if (table == nullptr)
    {
        QMessageBox::warning(this, "错误", "表格控件不存在！");
        return;
    }

    // 1. 获取当前选中的行（QTableWidget的选中行是QList<int>类型）
    QList<int> selectedRows;
    // 遍历所有选中的项，收集唯一的行号（避免同一行被多次选中）
    for (const QTableWidgetItem *item : table->selectedItems())
    {
        int row = item->row();
        if (!selectedRows.contains(row))
        {
            selectedRows.append(row);
        }
    }

    // 2. 若没有选中行，提示用户
    if (selectedRows.isEmpty())
    {
        QMessageBox::information(this, "提示", "请先选中要删除的行！");
        return;
    }

    // 3. 倒序删除行（避免删除前面的行后，后面的行号错位）
    std::sort(selectedRows.begin(), selectedRows.end(), std::greater<int>());
    for (int row : selectedRows)
    {
        // 从表格中删除行
        table->removeRow(row);
        // 同时从measureValues列表中删除对应的数据（如果需要同步）
        if (row < measureValues.size())
        {
            measureValues.removeAt(row);
        }
    }

    // 4. 删除后更新统计信息（比如最大值、最小值、平均值）
    updateStatistics();
}

void MainWindow::on_comboBox_sampleName_currentIndexChanged(int index)
{
    QString customText;
    QPixmap pixmap;
    pixmap = QPixmap();
    COL_SERIAL = 0;       // A列：序号
    COL_CODE = 0;         // B列：编号
    COL_SPEC = 0;         // C列：规格
    COL_DATA_START = 4;   // D列：数据起始列
    COL_ROW_AVG = 0;      // I列：单行平均值
    COL_ROW_MIN = 0;      // K列：单行最小值
    COL_MERGE_L = 0;      // L列：单行合并起始列
    COL_MERGE_M = 0;      // M列：单行合并结束列
    COL_SINGLE_CONCL = 0; // N列：单行结论
    COL_GROUP_AVG = 0;    // J列：分组平均值
    COL_GROUP_CONCL = 0;  // O列：分组结论
    DETECT_COL_START = 1; // 检测行合并起始列（A）
    DETECT_COL_END = 12;  // 检测行合并结束列（S）

    // 行配置
    START_ROW = 9;    // 数据起始行（必须≥1）
    DATA_PER_ROW = 5; // 每行5个数据
    GROUP_SIZE = 3;   // 每3行一组
    switch (index)
    {
    case 1:
    {

        sheetName = "Sheet1";
        COL_SERIAL = 1;
        COL_CODE = 0;
        COL_SPEC = 2;
        COL_DATA_START = 3;
        START_ROW = 9;
        COL_ROW_AVG = 0;
        COL_ROW_MIN = 0;
        COL_MERGE_L = 8;
        COL_MERGE_M = 9;
        COL_GROUP_AVG = 10;
        COL_GROUP_CONCL = 11;
        DETECT_COL_START = 1;
        DETECT_COL_END = 11;
        customText = QString("35KV及以上螺栓镀锌层测量位置：\n\n螺栓螺母随机取样\n\n在下图位置1所示的测量面上进行。\n\n至少取5个测量点测厚，样品数量3个为1组");
        pixmap.load("./source/1.png");
        break;
    }
    case 2:
    {

        sheetName = "Sheet2";
        COL_SERIAL = 1;
        COL_CODE = 2;
        COL_SPEC = 3;
        START_ROW = 5;
        COL_ROW_AVG = 9;
        COL_GROUP_AVG = 10;
        COL_GROUP_CONCL = 12;
        DETECT_COL_END = 12;
        customText = QString("35-500KV金具镀锌层测量位置：\n \n随机均布于整个试品的锌层表面，在制件尺寸允许的情况下，测量不应在离边缘小于10mm的区域或火焰切割面进行，至少取 5 个测量点测厚");
        pixmap.load("./source/2.png");
        break;
    }
    case 3:
    {

        sheetName = "Sheet3";
        COL_SERIAL = 1;
        COL_CODE = 2;
        COL_SPEC = 3;
        COL_DATA_START = 4;
        START_ROW = 6;
        DATA_PER_ROW = 12;
        COL_ROW_AVG = 16;
        COL_ROW_MIN = 17;
        DETECT_COL_END = 19;
        COL_SINGLE_CONCL = 19;
        customText = QString("10kV塔材钢镀锌层测量位置：\n\n钢管构件在两端（离边缘距离不小于 100 mm）和中间任意位置各环向均匀测量4点;\n\n角钢试样每面3 处各1点，4面共12点；\n\n钢板试样每面6处各1点，2面共12点；\n\n水泥杆法兰盘锌层每面3 处各1点，4面共12点;\n\n测试时测点应均匀分布");
        pixmap.load("./source/3.png");
        break;
    }
    case 4:
    {
        sheetName = "Sheet4";
        COL_SERIAL = 1;
        COL_CODE = 2;
        COL_DATA_START = 3;
        START_ROW = 9;
        COL_MERGE_L = 8;
        COL_MERGE_M = 9;
        COL_GROUP_AVG = 10;
        COL_GROUP_CONCL = 11;
        DETECT_COL_END = 11;
        customText = QString("10KV及以下螺栓镀锌层测量位置：\n\n螺栓螺母随机取样\n\n在下图位置1所示的测量面上进行。\n\n至少取5个测量点测厚，样品数量3个为1组");
        pixmap.load("./source/1.png");
        break;
    }
    case 5:
    {
        sheetName = "Sheet5";
        COL_SERIAL = 1;
        COL_CODE = 2;
        COL_SPEC = 3;
        COL_DATA_START = 4;
        START_ROW = 5;
        DATA_PER_ROW = 5;
        COL_ROW_AVG = 9;
        COL_GROUP_AVG = 10;
        COL_GROUP_CONCL = 12;
        DETECT_COL_END = 12;
        customText = QString("10KV及以下金具镀锌层测量位置：\n\n 随机均布于整个试品的锌层表面，在制件尺寸允许的情况下，测量不应在离边缘小于10mm的区域或火焰切割面进行，至少取 5 个测量点测厚");
        pixmap.load("./source/2.png");
        break;
    }
    case 6:
    {
        sheetName = "Sheet6";
        COL_SERIAL = 1;
        COL_CODE = 2;
        COL_SPEC = 3;
        COL_DATA_START = 4;
        START_ROW = 6;
        DATA_PER_ROW = 12;
        COL_ROW_AVG = 16;
        COL_ROW_MIN = 17;
        DETECT_COL_END = 19;
        COL_SINGLE_CONCL = 19;
        customText = QString("35kV及以上塔材钢镀锌层测量位置：\n\n钢管构件在两端（离边缘距离不小于 100 mm）和中间任意位置各环向均匀测量4点;\n\n角钢试样每面3 处各1点，4面共12点；\n\n钢板试样每面6处各1点，2面共12点；\n\n水泥杆法兰盘锌层每面3 处各1点，4面共12点;\n\n测试时测点应均匀分布");
        pixmap.load("./source/3.png");
        break;
    }
    case 7:
    {
        sheetName = "Sheet7";
        COL_SERIAL = 1;
        COL_SPEC = 2;
        COL_DATA_START = 3;
        COL_MERGE_L = 8;
        COL_MERGE_M = 9;
        COL_GROUP_AVG = 10;
        COL_GROUP_CONCL = 11;
        DETECT_COL_END = 11;
        customText = QString("35KV及以上螺栓镀锌层测量位置：\n\n螺栓螺母随机取样\n\n在下图位置1所示的测量面上进行。\n\n至少取5个测量点测厚，样品数量3个为1组 ");
        pixmap.load("./source/1.png");
        break;
    }
    case 8:
    {
        sheetName = "Sheet8";
        COL_SERIAL = 1;
        COL_CODE = 2;
        COL_SPEC = 3;
        START_ROW = 5;
        COL_ROW_AVG = 9;
        COL_GROUP_AVG = 10;
        COL_GROUP_CONCL = 12;
        DETECT_COL_END = 12;
        customText = QString("35-500KV金具镀锌层测量位置：\n\n随机均布于整个试品的锌层表面，在制件尺寸允许的情况下，测量不应在离边缘小于10mm的区域或火焰切割面进行，至少取 5 个测量点测厚");
        pixmap.load("./source/2.png");
        break;
    }
    case 9:
    {
        sheetName = "Sheet9";
        COL_SERIAL = 1;
        COL_CODE = 2;
        COL_SPEC = 3;
        COL_DATA_START = 4;
        START_ROW = 6;
        DATA_PER_ROW = 12;
        COL_ROW_AVG = 16;
        COL_ROW_MIN = 17;
        DETECT_COL_END = 19;
        COL_SINGLE_CONCL = 19;
        customText = QString("35kV及以上塔材钢镀锌层测量位置：\n\n钢管构件在两端（离边缘距离不小于 100 mm）和中间任意位置各环向均匀测量4点;\n\n角钢试样每面3 处各1点，4面共12点；\n\n钢板试样每面6处各1点，2面共12点；\n\n水泥杆法兰盘锌层每面3 处各1点，4面共12点;\n\n测试时测点应均匀分布");
        pixmap.load("./source/3.png");
        break;
    }
    case 10:
    {
        sheetName = "Sheet10";
        COL_SERIAL = 1;
        COL_SPEC = 2;
        COL_DATA_START = 3;
        START_ROW = 9;
        COL_MERGE_L = 8;
        COL_MERGE_M = 9;
        COL_GROUP_AVG = 10;
        COL_GROUP_CONCL = 11;
        DETECT_COL_END = 11;
        customText = QString("10KV及以下螺栓镀锌层测量位置：\n\n螺栓螺母随机取样\n\n在下图位置1所示的测量面上进行。\n\n至少取5个测量点测厚，样品数量3个为1组");
        pixmap.load("./source/1.png");
        break;
    }
    case 11:
    {
        sheetName = "Sheet11";
        COL_SERIAL = 1;
        COL_CODE = 2;
        COL_SPEC = 3;
        START_ROW = 5;
        COL_ROW_AVG = 9;
        COL_GROUP_AVG = 10;
        COL_GROUP_CONCL = 12;
        DETECT_COL_END = 12;
        customText = QString("10KV及以下金具镀锌层测量位置：\n\n随机均布于整个试品的锌层表面，在制件尺寸允许的情况下，测量不应在离边缘小于10mm的区域或火焰切割面进行，至少取 5 个测量点测厚");
        pixmap.load("./source/2.png");
        break;
    }
    case 12:
    {
        sheetName = "Sheet12";
        COL_SERIAL = 1;
        COL_CODE = 2;
        COL_SPEC = 3;
        COL_DATA_START = 4;
        START_ROW = 6;
        DATA_PER_ROW = 12;
        COL_ROW_AVG = 16;
        COL_ROW_MIN = 17;
        DETECT_COL_END = 19;
        COL_SINGLE_CONCL = 19;
        customText = QString("35kV及以上塔材铁镀锌层测量位置：\n\n钢管构件在两端（离边缘距离不小于 100 mm）和中间任意位置各环向均匀测量4点;\n\n角钢试样每面3 处各1点，4面共12点；\n\n钢板试样每面6处各1点，2面共12点；\n\n水泥杆法兰盘锌层每面3 处各1点，4面共12点;\n\n测试时测点应均匀分布");
        pixmap.load("./source/3.png");
        break;
    }
    case 13:
    {
        sheetName = "Sheet13";
        COL_SERIAL = 1;
        COL_CODE = 2;
        COL_SPEC = 3;
        COL_DATA_START = 4;
        START_ROW = 6;
        DATA_PER_ROW = 12;
        COL_ROW_AVG = 16;
        COL_ROW_MIN = 17;
        DETECT_COL_END = 19;
        COL_SINGLE_CONCL = 19;
        customText = QString("35kV及以上塔材铁镀锌层测量位置：\n\n钢管构件在两端（离边缘距离不小于 100 mm）和中间任意位置各环向均匀测量4点;\n\n角钢试样每面3 处各1点，4面共12点；\n\n钢板试样每面6处各1点，2面共12点；\n\n水泥杆法兰盘锌层每面3 处各1点，4面共12点;\n\n测试时测点应均匀分布");
        pixmap.load("./source/3.png");
        break;
    }
    case 14:
    {

        sheetName = "Sheet14";
        COL_SERIAL = 1;
        COL_CODE = 2;
        COL_SPEC = 3;
        COL_DATA_START = 4;
        START_ROW = 6;
        DATA_PER_ROW = 12;
        COL_ROW_AVG = 16;
        COL_ROW_MIN = 17;
        DETECT_COL_END = 19;
        COL_SINGLE_CONCL = 19;
        customText = QString("10kV塔材铁镀锌层测量位置：\n\n钢管构件在两端（离边缘距离不小于 100 mm）和中间任意位置各环向均匀测量4点;\n\n角钢试样每面3 处各1点，4面共12点；\n\n钢板试样每面6处各1点，2面共12点；\n\n水泥杆法兰盘锌层每面3 处各1点，4面共12点;\n\n测试时测点应均匀分布");
        pixmap.load("./source/3.png");
        break;
    }
    default:
    {
        customText = QString("没有提示！");
        break;
    }
    }
    ui->textBrowser_2->setText(customText);
    ui->label_14->setPixmap(pixmap.scaled(ui->label_14->size(),       // 适配Label尺寸
                                          Qt::KeepAspectRatio,        // 保持宽高比
                                          Qt::SmoothTransformation)); // 平滑缩放
}

void MainWindow::on_pushButton_4_clicked()
{

    Setting *setting = new Setting(this);

    setting->setWindowTitle("设置");
    setting->exec();
}

// ===================== 核心工具函数 =====================
// 仅设置外边框
void setOnlyOuterBorder(QAxObject *range)
{
    if (!range)
        return;
    QAxObject *borders = range->querySubObject("Borders");
    if (!borders)
        return;

    // 仅外边框（左、右、上、下）
    borders->querySubObject("Item(int)", 1)->setProperty("LineStyle", 1);
    borders->querySubObject("Item(int)", 2)->setProperty("LineStyle", 1);
    borders->querySubObject("Item(int)", 3)->setProperty("LineStyle", 1);
    borders->querySubObject("Item(int)", 4)->setProperty("LineStyle", 1);
    // 清空内边框
    borders->querySubObject("Item(int)", 5)->setProperty("LineStyle", -4142);
    borders->querySubObject("Item(int)", 6)->setProperty("LineStyle", -4142);
    // 边框粗细
    borders->setProperty("Weight", 2);
    delete borders;
}

// 取消指定行的所有合并（关键：确保数据能填入）
void unmergeRowAllColumns(QAxObject *ws, int targetRow)
{
    if (!ws || targetRow < 1)
        return;
    try
    {
        // 拼接该行的全部列范围（A-S）
        QString rangeStr = QString("A%1:S%1").arg(targetRow);
        QAxObject *range = ws->querySubObject("Range(const QString&)", rangeStr);
        if (range)
        {
            range->dynamicCall("UnMerge()"); // 取消该行所有合并
            delete range;
        }
    }
    catch (...)
    {
        qDebug() << "取消行合并异常：row=" << targetRow;
    }
}

// 单行指定列合并（分步拼接+严格校验）
bool mergeSingleRowColumns(QAxObject *ws, int targetRow, int col1, int col2, const QVariant &value)
{
    // 1. 先取消该行所有合并
    unmergeRowAllColumns(ws, targetRow);

    // 2. 严格校验参数
    if (!ws || targetRow < 1 || col1 >= col2 || col1 < 1 || col2 < 1)
    {
        qDebug() << "合并参数非法：targetRow=" << targetRow << " col1=" << col1 << " col2=" << col2;
        return false;
    }

    // 3. 分步拼接范围（绝对正确）
    QString col1Char = QString(QChar('A' + col1 - 1));
    QString col2Char = QString(QChar('A' + col2 - 1));
    QString cell1 = col1Char + QString::number(targetRow);
    QString cell2 = col2Char + QString::number(targetRow);
    QString rangeStr = cell1 + ":" + cell2;
    qDebug() << "合并范围：" << rangeStr;

    try
    {
        QAxObject *range = ws->querySubObject("Range(const QString&)", rangeStr);
        if (!range)
        {
            qDebug() << "获取Range失败：" << rangeStr;
            return false;
        }
        range->dynamicCall("Merge()");
        range->setProperty("Value", value);
        range->setProperty("HorizontalAlignment", -4108);
        setOnlyOuterBorder(range);

        delete range;
        return true;
    }
    catch (...)
    {
        qDebug() << "Excel合并异常：范围=" << rangeStr;
        return false;
    }
}

// 辅助：拆分编号
void MainWindow::splitCode(const QString &code, QString &prefix, int &num)
{
    int dashIdx = code.lastIndexOf('-');
    if (dashIdx > 0)
    {
        prefix = code.left(dashIdx + 1);
        num = code.mid(dashIdx + 1).toInt();
    }
    else
    {
        prefix = "NB-";
        num = 1001;
    }
}

// 【核心修复】精准查找最后一条业务数据行（排除检测行）
int MainWindow::findLastDataRow(QAxObject *ws, int col)
{
    if (!ws)
        return START_ROW;
    int lastRow = START_ROW - 1; // 初始为起始行前一行

    // 从最大行往起始行找，排除检测行
    for (int row = MAX_SEARCH_ROW; row >= START_ROW; row--)
    {
        // 先取消该行合并（避免读取值失败）
        unmergeRowAllColumns(ws, row);

        // 读取A列值（序号）
        QAxObject *cellSerial = ws->querySubObject("Cells(int, int)", row, COL_SERIAL);
        QString valSerial = cellSerial ? cellSerial->property("Value").toString().trimmed() : "";
        if (cellSerial)
            delete cellSerial;

        // 读取任意数据列值（判断是否为业务行）
        QAxObject *cellData = ws->querySubObject("Cells(int, int)", row, COL_DATA_START);
        QString valData = cellData ? cellData->property("Value").toString().trimmed() : "";
        if (cellData)
            delete cellData;

        // 读取检测行标识（排除检测行）
        QAxObject *cellDetect = ws->querySubObject("Cells(int, int)", row, DETECT_COL_START);
        QString valDetect = cellDetect ? cellDetect->property("Value").toString().trimmed() : "";
        if (cellDetect)
            delete cellDetect;

        // 判定条件：有序号+有数据+不是检测行
        if (!valSerial.isEmpty() && !valData.isEmpty() && !valDetect.contains("检测："))
        {
            lastRow = row;
            break; // 找到最后一行业务数据，停止查找
        }
    }

    // 确保返回行号≥1，若没找到则返回起始行-1（续填时从起始行开始）
    return qMax(lastRow, START_ROW - 1);
}

// 辅助：设置单元格值（填充前取消合并）
// 辅助：设置单元格值（填充前取消合并+取消居中，新增字体颜色控制）
bool MainWindow::setCellValue(QAxObject *ws, int row, int col, const QVariant &val)
{
    if (!ws || row < 1 || col < 1)
        return false;

    // 先取消当前行合并+取消居中对齐
    unmergeRowAllColumns(ws, row);

    try
    {
        QAxObject *cell = ws->querySubObject("Cells(int, int)", row, col);
        if (!cell)
            return false;

        // 1. 设置单元格值
        cell->setProperty("Value", val);

        // 2. 新增：判断是否为结论列，且内容是不合格 → 设为红色
        bool isSingleConclusion = (col == COL_SINGLE_CONCL); // 单行结论列（N列）
        bool isUnPass = (val.toString() == "不合格");
        if (isSingleConclusion && isUnPass)
        {
            setFontColor(cell, FONT_COLOR_RED); // 不合格→红色
        }
        else
        {
            setFontColor(cell, FONT_COLOR_BLACK); // 合格/其他→黑色
        }

        // 3. 常规对齐+外边框（原有逻辑）
        cell->setProperty("HorizontalAlignment", ALIGN_GENERAL);
        cell->setProperty("VerticalAlignment", ALIGN_GENERAL);
        setOnlyOuterBorder(cell);

        delete cell;
        return true;
    }
    catch (...)
    {
        qDebug() << "设置单元格异常：row=" << row << " col=" << col;
        return false;
    }
}

// 辅助：多行单列合并
// 辅助：多行单列合并（仅合并区域居中，新增字体颜色控制）
bool MainWindow::mergeMultiRowSingleCol(QAxObject *ws, int sRow, int eRow, int col, const QVariant &val)
{
    if (!ws || sRow < 1 || eRow < 1 || sRow > eRow || col < 1)
        return false;
    try
    {
        QString colChar = QString(QChar('A' + col - 1));
        QString rangeStr = QString("%1%2:%1%3").arg(colChar).arg(sRow).arg(eRow);
        QAxObject *range = ws->querySubObject("Range(const QString&)", rangeStr);
        if (!range)
            return false;

        // 1. 合并+设值+居中（原有逻辑）
        range->dynamicCall("Merge()");
        range->setProperty("Value", val);
        range->setProperty("HorizontalAlignment", ALIGN_CENTER);
        range->setProperty("VerticalAlignment", ALIGN_CENTER);
        setOnlyOuterBorder(range);

        // 2. 新增：判断是否为分组结论列，且内容是组不合格 → 设为红色
        bool isGroupConclusion = (col == COL_GROUP_CONCL); // 分组结论列（O列）
        bool isGroupUnPass = (val.toString() == "组不合格");
        if (isGroupConclusion && isGroupUnPass)
        {
            setFontColor(range, FONT_COLOR_RED); // 组不合格→红色
        }
        else
        {
            setFontColor(range, FONT_COLOR_BLACK); // 组合格→黑色
        }

        delete range;
        return true;
    }
    catch (...)
    {
        // qDebug() << "多行合并异常：范围=" << rangeStr;
        return false;
    }
}

// 辅助：清空指定行内容（覆盖检测行前先清空）
void MainWindow::clearRowContent(QAxObject *ws, int targetRow)
{
    if (!ws || targetRow < 1)
        return;
    try
    {
        QString rangeStr = QString("A%1:S%1").arg(targetRow);
        QAxObject *range = ws->querySubObject("Range(const QString&)", rangeStr);
        if (range)
        {
            range->dynamicCall("ClearContents()"); // 清空内容（保留格式）
            delete range;
        }
    }
    catch (...)
    {
        qDebug() << "清空行内容异常：row=" << targetRow;
    }
}

// 辅助：清空起始行后所有数据（重填用）
void MainWindow::clearStartRowData(QAxObject *ws)
{
    if (!ws)
        return;
    try
    {
        QString rangeStr = QString("A%1:S%2")
                               .arg(START_ROW)
                               .arg(MAX_SEARCH_ROW);
        QAxObject *range = ws->querySubObject("Range(const QString&)", rangeStr);
        if (range)
        {
            range->dynamicCall("ClearContents()");
            delete range;
        }
    }
    catch (...)
    {
        // qDebug() << "清空数据异常：范围=" << rangeStr;
    }
}

// 辅助：单行结论
QString MainWindow::getSingleConclusion(double avg, double min)
{
    return (avg >= avgPASS_THRESHOLD && min >= minPASS_THRESHOLD) ? "合格" : "不合格";
}

// 辅助：分组结论
// 辅助：分组结论（分组均值+最小值双重判断）
QString MainWindow::getGroupConclusion(QMap<int, double> &avgMap, int sRow, int eRow)
{
    // 1. 收集分组内所有有效的单行平均值
    QList<double> groupAvgList;
    for (int row = sRow; row <= eRow; row++)
    {
        if (avgMap.contains(row))
        {
            groupAvgList.append(avgMap[row]);
        }
    }

    // 2. 无数据直接判定不合格
    if (groupAvgList.isEmpty())
    {
        return "不合格";
    }

    // 3. 计算分组均值
    double sum = std::accumulate(groupAvgList.begin(), groupAvgList.end(), 0.0);
    double groupAvg = sum / groupAvgList.size();

    // 4. 计算分组最小值
    double groupMin = *std::min_element(groupAvgList.begin(), groupAvgList.end());

    // 5. 判定规则（可按需修改）
    bool isPass = (groupAvg >= avgPASS_THRESHOLD) && (groupMin >= minPASS_THRESHOLD);
    return isPass ? "合格" : "不合格";
}

void MainWindow::setvalue(QAxObject *ws, int row, int col, const QVariant &val)
{
    QAxObject *cell = ws->querySubObject("Cells(int, int)", row, col);
    // if (!cell) return false;

    // 1. 设置单元格值
    cell->setProperty("Value", val);
    delete cell;
}

void MainWindow::saveToFile()
{

    // 1. 路径处理
    QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString templatePath = QString("./%1").arg(TEMPLATE_NAME);
    QString savePath = QString("%1/%2").arg(desktop).arg(SAVE_NAME);

    // 2. 前置检查
    bool isNewFile = !QFile::exists(savePath);
    if (isNewFile && !QFile::exists(templatePath))
    {
        QMessageBox::critical(this, "错误", QString("软件不完整"));
        return;
    }
    int selIdx = ui->comboBox_sampleName->currentIndex();
    if (selIdx < 1)
    {
        QMessageBox::information(this, "提示", "请选择样品类型");
        return;
    }

    avgPASS_THRESHOLD = revalue(selIdx, 0);
    minPASS_THRESHOLD = revalue(selIdx, 1);

    // 4. 仅首次复制模板
    if (isNewFile)
    {
        QFile::copy(templatePath, savePath);
        LPCWSTR winPath = (LPCWSTR)savePath.utf16();
        DWORD attr = GetFileAttributesW(winPath);
        SetFileAttributesW(winPath, attr & ~FILE_ATTRIBUTE_HIDDEN);
    }

    // 5. 生成测试数据
    QStringList dataList = getTableThirdColumn(ui->tableWidget);
    for (int i = 0; i < 23; i++){
        dataList.append(QString::number(48.0 + i * 0.1, 'f', 1));
    }
    if (dataList.isEmpty())
    {
        QMessageBox::warning(this, "提示", "无数据");
        return;
    }

    // 6. Excel初始化
    QAxObject *excel = nullptr;
    QAxObject *workbook = nullptr;
    QAxObject *ws = nullptr;
    try
    {
        excel = new QAxObject("Excel.Application");
        if (!excel)
        {
            QMessageBox::critical(this, "错误", "Excel初始化失败");
            return;
        }
        excel->setProperty("Visible", false);
        excel->setProperty("DisplayAlerts", false);

        QAxObject *workbooks = excel->querySubObject("Workbooks");
        workbook = workbooks->querySubObject("Open(const QString&)", savePath);
        workbooks->deleteLater();
        if (!workbook)
        {
            QMessageBox::critical(this, "错误", "打开Excel文件失败");
            delete excel;
            return;
        }

        ws = workbook->querySubObject("Worksheets(const QString&)", sheetName);
        if (!ws)
        {
            QMessageBox::critical(this, "错误", QString("Sheet%1不存在").arg(sheetName));
            workbook->dynamicCall("Close(bool)", false);
            delete workbook;
            delete excel;
            return;
        }
        ws->dynamicCall("Activate()");
    }
    catch (...)
    {
        QMessageBox::critical(this, "Excel初始化异常", "无法初始化Excel，请检查是否安装Excel或ActiveQt配置");
        if (ws)
            delete ws;
        if (workbook)
            delete workbook;
        if (excel)
            delete excel;
        return;
    }

    // 7. 重填/续填逻辑（核心：覆盖检测行）
    bool isNewFill = ui->radioButton_3->isChecked();
    int currRow;
    int serialNum;
    int codeNum = 1001;

    if (isNewFill)
    {
        // 重填：清空起始行后所有数据，从起始行开始
        clearStartRowData(ws);
        // QMessageBox::information(this, "提示", "已清空所有数据，开始从头填充");
        currRow = START_ROW;
        serialNum = 1;
        codeNum = 1001;
    }
    else
    {
        // 续填：精准找到最后业务数据行，从下一行开始（覆盖检测行）
        int lastDataRow = findLastDataRow(ws, COL_SERIAL);
        currRow = lastDataRow + 1;
        // 确保行号≥1
        currRow = qMax(currRow, 1);

        // 读取最后序号/编号（续用）
        if (lastDataRow >= START_ROW)
        {
            QAxObject *lastSerial = ws->querySubObject("Cells(int,int)", lastDataRow, COL_SERIAL);
            if (lastSerial && !lastSerial->isNull())
            {
                serialNum = lastSerial->property("Value").toInt() + 1;
                delete lastSerial;
            }

            QAxObject *lastCode = ws->querySubObject("Cells(int,int)", lastDataRow, COL_CODE);
            if (lastCode && !lastCode->isNull())
            {
                QString prefix;
                splitCode(lastCode->property("Value").toString().trimmed(), prefix, codeNum);
                codeNum += 1;
                delete lastCode;
            }
        }
        else
        {
            // 若没找到历史数据，从起始行开始
            currRow = START_ROW;
            serialNum = 1;
            codeNum = 1001;
        }

        // 续填前：清空目标行（检测行）内容，确保覆盖
        clearRowContent(ws, currRow);
    }
    QString content = ui->lineEdit_2->text();
    QString content2 = ui->lineEdit_3->text();

    // 8. 填充数据（核心：覆盖检测行）
    int dataIdx = 0;
    QMap<int, bool> rowDataEnoughMap;
    QMap<int, double> rowAvgMap;
    QList<int> filledRows;

    while (dataIdx < dataList.size())
    {
        if (currRow < 1)
            currRow = 1;

        // 8.1 清空当前行（确保无残留数据）
        clearRowContent(ws, currRow);

        // 8.2 基础信息（序号、编号、规格）
        if (COL_SERIAL)
            setCellValue(ws, currRow, COL_SERIAL, serialNum);
        if (COL_CODE && !content2.isEmpty())
            setCellValue(ws, currRow, COL_CODE, QString("%1-%2").arg(content2).arg(codeNum));
        if (COL_SPEC)
            setCellValue(ws, currRow, COL_SPEC, content);

        // 8.3 业务数据（D列开始）
        QList<double> rowVals;
        int dataCount = 0;
        for (int i = 0; i < DATA_PER_ROW && dataIdx < dataList.size(); i++)
        {
            int col = COL_DATA_START + i;
            double val = dataList[dataIdx].toDouble();
            setCellValue(ws, currRow, col, val);
            rowVals.append(val);
            dataIdx++;
            dataCount++;
        }

        // 8.4 计算统计值（平均值、最小值）
        bool isDataEnough = (dataCount >= DATA_PER_ROW);
        rowDataEnoughMap[currRow] = isDataEnough;

        double rowAvg = 0, rowMin = 0;
        if (!rowVals.isEmpty())
        {
            rowAvg = std::accumulate(rowVals.begin(), rowVals.end(), 0.0) / rowVals.size();
            rowMin = *std::min_element(rowVals.begin(), rowVals.end());
            rowAvgMap[currRow] = rowAvg;
        }

        filledRows.append(currRow);

        // 8.5 单行统计列（平均值、最小值、结论）
        if (COL_ROW_AVG > 0 && rowVals.size() >= 1)
            setCellValue(ws, currRow, COL_ROW_AVG, QString::number(rowAvg, 'f', 1));
        if (COL_ROW_MIN > 0 && rowVals.size() >= 1)
            setCellValue(ws, currRow, COL_ROW_MIN, QString::number(rowMin, 'f', 1));

        // 6. 单行结论（仅数据充足时显示）
        if (COL_SINGLE_CONCL > 0 && isDataEnough && rowVals.size() >= 1)
        {
            QString singleConcl = getSingleConclusion(rowAvg, rowMin);
            if (!singleConcl.isEmpty())
            {
                setCellValue(ws, currRow, COL_SINGLE_CONCL, singleConcl);
            }
        }

        // 8.6 LM列合并（当前行）
        if (COL_MERGE_L && COL_MERGE_M && !rowVals.isEmpty())
            mergeSingleRowColumns(ws, currRow, COL_MERGE_L, COL_MERGE_M, QString::number(rowAvg, 'f', 1));

        // 8.7 递增行号/序号/编号
        currRow++;
        serialNum++;
        codeNum++;
    }

    // 9. 分组统计
    for (int i = 0; i < filledRows.size(); i += GROUP_SIZE)
    {
        int groupS = filledRows[i];
        int groupE = (i + GROUP_SIZE - 1) < filledRows.size() ? filledRows[i + GROUP_SIZE - 1] : filledRows.last();
        double groupAvg = 0;
        int cnt = 0;
        for (int row = groupS; row <= groupE; row++)
        {
            if (rowAvgMap.contains(row))
            {
                groupAvg += rowAvgMap[row];
                cnt++;
            }
        }
        groupAvg = cnt > 0 ? groupAvg / cnt : 0;

        // 第二步：无条件显示分组平均值（只要有数据就显示）
        if (COL_GROUP_AVG && cnt > 0)
            mergeMultiRowSingleCol(ws, groupS, groupE, COL_GROUP_AVG, QString::number(groupAvg, 'f', 1));

        // 仅双条件满足时输出分组结论
        bool isGroupFull = ((i + GROUP_SIZE - 1) < filledRows.size());
        bool isAllRowEnough = true;
        for (int row = groupS; row <= groupE; row++)
        {
            if (!rowDataEnoughMap.contains(row) || !rowDataEnoughMap[row])
            {
                isAllRowEnough = false;
                break;
            }
        }
        if (COL_GROUP_CONCL > 0 && isGroupFull && isAllRowEnough && cnt > 0)
        {
            QString groupConcl = getGroupConclusion(rowAvgMap, groupS, groupE);
            if (!groupConcl.isEmpty())
            {
                mergeMultiRowSingleCol(ws, groupS, groupE, COL_GROUP_CONCL, groupConcl);
            }
        }
    }

    // 10. 检测行（覆盖原有检测行，不跳过）
    if (currRow < 1)
        currRow = 1;
    // 先清空检测行内容+取消合并
    clearRowContent(ws, currRow);
    unmergeRowAllColumns(ws, currRow);

    QString detectText = ui->lineEdit->text().trimmed();
    QDate selectedDate = ui->dateEdit->date();
    QString dateStr = selectedDate.toString("yyyy年MM月dd日");
    QString detectRowText = QString("检测：%1  日期：%2").arg(detectText).arg(dateStr);

    // 检测行合并（A-S）
    mergeSingleRowColumns(ws, currRow, DETECT_COL_START, DETECT_COL_END, detectRowText);

    QString ManText = ui->lineEdit_4->text().trimmed();
    switch (selIdx)
    {
    case 1:
    {
        setvalue(ws, 2, 8, ManText);
        setvalue(ws, 3, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(ws, 4, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 2:
    {
        setvalue(ws, 2, 10, ManText);
        setvalue(ws, 4, 9, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(ws, 4, 10, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 3:
    {
        setvalue(ws, 2, 16, ManText);
        setvalue(ws, 4, 16, QString("δ＜5mm:≥%1").arg(revalue(selIdx, 0)));
        setvalue(ws, 4, 17, QString("δ＜5mm :≥%1").arg(revalue(selIdx, 0)));
        setvalue(ws, 5, 16, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        setvalue(ws, 5, 17, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        break;
    }
    case 4:
    {
        setvalue(ws, 2, 8, ManText);
        setvalue(ws, 3, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(ws, 4, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 5:
    {
        setvalue(ws, 2, 10, ManText);
        setvalue(ws, 4, 9, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(ws, 4, 10, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 6:
    {
        setvalue(ws, 2, 16, ManText);
        setvalue(ws, 4, 16, QString("δ＜5mm:≥%1").arg(revalue(selIdx, 0)));
        setvalue(ws, 4, 17, QString("δ＜5mm :≥%1").arg(revalue(selIdx, 0)));
        setvalue(ws, 5, 16, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        setvalue(ws, 5, 17, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        break;
    }
    case 7:
    {
        setvalue(ws, 2, 8, ManText);
        setvalue(ws, 3, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(ws, 4, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 8:
    {
        setvalue(ws, 2, 10, ManText);
        setvalue(ws, 4, 9, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(ws, 4, 10, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 9:
    {
        setvalue(ws, 2, 16, ManText);
        setvalue(ws, 4, 16, QString("δ＜5mm:≥%1").arg(revalue(selIdx, 0)));
        setvalue(ws, 4, 17, QString("δ＜5mm :≥%1").arg(revalue(selIdx, 0)));
        setvalue(ws, 5, 16, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        setvalue(ws, 5, 17, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        break;
    }
    case 10:
    {
        setvalue(ws, 2, 8, ManText);
        setvalue(ws, 3, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(ws, 4, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 11:
    {
        setvalue(ws, 2, 10, ManText);
        setvalue(ws, 4, 9, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(ws, 4, 10, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 12:
    {
        setvalue(ws, 2, 16, ManText);
        setvalue(ws, 4, 16, QString("δ＜5mm:≥%1").arg(revalue(selIdx, 0)));
        setvalue(ws, 4, 17, QString("δ＜5mm:≥%1").arg(revalue(selIdx, 0)));
        setvalue(ws, 5, 16, QString("δ≥5mm:≥%1").arg(revalue(selIdx, 1)));
        setvalue(ws, 5, 17, QString("δ≥5mm:≥%1").arg(revalue(selIdx, 1)));
    }
    case 13:
    {
        setvalue(ws, 2, 16, ManText);
        setvalue(ws, 4, 16, QString("δ＜5mm:≥%1").arg(revalue(selIdx, 0)));
        setvalue(ws, 4, 17, QString("δ＜5mm :≥%1").arg(revalue(selIdx, 0)));
        setvalue(ws, 5, 16, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        setvalue(ws, 5, 17, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        break;
    }
    case 14:
    {
        setvalue(ws, 2, 16, ManText);
        setvalue(ws, 4, 16, QString("δ＜5mm:≥%1").arg(revalue(selIdx, 0)));
        setvalue(ws, 4, 17, QString("δ＜5mm :≥%1").arg(revalue(selIdx, 0)));
        setvalue(ws, 5, 16, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        setvalue(ws, 5, 17, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        break;
    }
    }

    // 11. 保存释放
    try
    {
        workbook->dynamicCall("Save()");
        workbook->dynamicCall("Close(bool)", true);
        excel->dynamicCall("Quit()");
    }
    catch (...)
    {
        qDebug() << "保存关闭Excel异常";
    }

    // 释放资源
    if (ws)
        delete ws;
    if (workbook)
        delete workbook;
    if (excel)
        delete excel;

    QMessageBox::information(this, "成功", QString("保存完成！"));
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // 静态计数器，初始值设为0（修正计数逻辑）
    static int key_scan = 0;

    // 仅检测字母F键（核心：所有逻辑都在这个if内）
    if (event->key() == Qt::Key_F)
    {
        qDebug() << "你按下了 F 键";
        key_scan++; // 按一次F，计数器+1
        qDebug() << "当前计数：" << key_scan;

        // 核心：把弹窗判断放在F键的if内，仅F键触发
        if (key_scan % 5 == 0)
        {
            qDebug() << "按满10次F键，弹出About窗口";
            About *about = new About(this);            // 父窗口设为主窗口，辅助内存管理
            about->setAttribute(Qt::WA_DeleteOnClose); // 关闭窗口时自动释放内存
            about->setWindowTitle("彩蛋");
            about->exec(); // 改用模态窗口，避免多窗口叠加（推荐）
            // 如果需要非模态：about->show(); 但必须保留WA_DeleteOnClose
        }
    }

    // 保留父类原有键盘事件行为（比如Tab切换焦点）
    QMainWindow::keyPressEvent(event);
}

void MainWindow::setFontColor(QAxObject *range, int color)
{
    if (!range)
        return;
    try
    {
        QAxObject *font = range->querySubObject("Font");
        if (font)
        {
            font->setProperty("ColorIndex", color); // 设置字体颜色
            delete font;
        }
    }
    catch (...)
    {
        qDebug() << "设置字体颜色异常";
    }
}

void BluetoothProtocolParser::onDataReceived(const QByteArray &newData) {
    m_buffer.append(newData);

    // 最小包长：Flag(1) + CMD(1) + Size(2) + CRC(1) = 5字节
    while (m_buffer.size() >= 5) {
        // 1. 寻找包头标志 0xBB [cite: 4, 42]
        if (static_cast<uint8_t>(m_buffer.at(0)) != 0xBB) {
            m_buffer.remove(0, 1);
            continue;
        }

        // 2. 读取 Size 字段 (小端模式)
        uint16_t size = static_cast<uint8_t>(m_buffer.at(2)) |
                        (static_cast<uint8_t>(m_buffer.at(3)) << 8);

        // 根据公式：包总长 = Flag(1) + Size + CRC(1) [cite: 3, 4]
        int expectedPacketLength = 1 + size + 1;

        // 检查缓存中是否有完整的一个包（处理半包等待）
        if (m_buffer.size() < expectedPacketLength) {
            break; // 跳出循环，等待下一次 onDataReceived 数据拼接
        }

        // 3. 提取用于 CRC 校验的数据段 (CMD + SIZE + DATA) [cite: 18]
        QByteArray crcData = m_buffer.mid(1, size);
        uint8_t calculatedCrc = crc8(reinterpret_cast<const uint8_t*>(crcData.constData()), size);
        uint8_t receivedCrc = static_cast<uint8_t>(m_buffer.at(expectedPacketLength - 1));

        // 4. 校验 CRC [cite: 18]
        if (calculatedCrc != receivedCrc) {
            qDebug() << "CRC 校验错误，丢弃包头尝试重新同步！";
            m_buffer.remove(0, 1); // 丢弃0xBB，继续寻找下一个有效包头
            continue;
        }

        // 5. CRC 通过，开始解析数据
        uint8_t cmd = static_cast<uint8_t>(m_buffer.at(1));
        QByteArray payloadData = m_buffer.mid(4, size - 3); // 提取纯 Data(n字节) 载荷

        if (cmd == 0x02) {
            parseRealTimeData(payloadData);
        } else if (cmd == 0x03) {
            // parseDataGroup(payloadData); // 预留数据组 0x03 的解析接口
            qDebug() << "接收到数据组 0x03 (524字节)";
        }

        // 6. 将已处理的整包数据从缓存中移除
        m_buffer.remove(0, expectedPacketLength);
    }
}


uint8_t BluetoothProtocolParser::crc8(const uint8_t *data, uint16_t length) {
    uint8_t crc = 0;
    for (uint16_t len = 0; len < length; ++len) {
        crc ^= data[len];
        for (int i = 0; i < 8; i++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}

// 解析 0x02 实时测量数据
void BluetoothProtocolParser::parseRealTimeData(const QByteArray &data) {
    // 至少需要 5 字节 (4字节数据 + 1字节材料) [cite: 8]
    if (data.size() < 5) return;

    // 1. 读取 4 字节测量数据 (小端模式拼接) [cite: 12]
    uint32_t rawData = static_cast<uint8_t>(data.at(0)) |
                       (static_cast<uint8_t>(data.at(1)) << 8) |
                       (static_cast<uint8_t>(data.at(2)) << 16) |
                       (static_cast<uint8_t>(data.at(3)) << 24);

    // 2. 处理原码符号位逻辑 (最高位为1表示负数)
    bool isNegative = (rawData & 0x80000000) != 0;
    uint32_t magnitude = rawData & 0x7FFFFFFF; // 提取除最高位外的数值

    // 3. 计算实际值 (除以 1000)
    double actualValue = magnitude / 1000.0;
    if (isNegative) {
        actualValue = -actualValue;
    }

    // 4. 解析材料类型
    uint8_t materialCode = static_cast<uint8_t>(data.at(4));
    QString materialStr;
    if (materialCode == 0x00) materialStr = "NFE";
    else if (materialCode == 0x01) materialStr = "FE";
    else if (materialCode == 0x02) materialStr = "空 (Empty)";
    else materialStr = "未知";

    // 5. 解析组名 (如果存在的话) [cite: 8]
    QString groupName = "无";
    if (data.size() >= 21) {
        // 将 16 字节的组名转为字符串，去掉多余的空字符
        groupName = QString::fromLocal8Bit(data.mid(5, 16)).trimmed();
    }

    // 打印或通过信号发给 UI
    qDebug() << QString("解析成功 -> 测量值: %1 µm, 材料: %2, 组名: %3")
                    .arg(actualValue, 0, 'f', 2)
                    .arg(materialStr)
                    .arg(groupName);
    thick = actualValue;
}












