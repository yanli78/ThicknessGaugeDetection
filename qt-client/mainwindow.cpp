#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QColor>
#include "setting.h"

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
    btserial = new QSerialPort(this);
    statusLabel = new QLabel(this);
    this->statusBar()->addWidget(statusLabel);
    ui->radioButton->setEnabled(false);
    // ui->comboBox_2->setCurrentIndex(2);
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::dataReceive);
    connect(btserial, &QSerialPort::readyRead, this, &MainWindow::btReceive);
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

    // serial->setPortName("COM5");
    serial->setBaudRate(QSerialPort::Baud115200);       // 设置波特率
    serial->setDataBits(QSerialPort::Data8);            // 设置数据位
    serial->setParity(QSerialPort::NoParity);           // 设置校验位
    serial->setStopBits(QSerialPort::OneStop);          // 设置停止位
    serial->setFlowControl(QSerialPort::NoFlowControl); // 设置流控制

    btserial->setBaudRate(QSerialPort::Baud115200);       // 设置波特率
    btserial->setDataBits(QSerialPort::Data8);            // 设置数据位
    btserial->setParity(QSerialPort::NoParity);           // 设置校验位
    btserial->setStopBits(QSerialPort::OneStop);          // 设置停止位
    btserial->setFlowControl(QSerialPort::NoFlowControl); // 设置流控制

    this->setFocusPolicy(Qt::StrongFocus);
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
    // ====================== 处理 comboBox (CH340) ======================
    ui->comboBox->clear();
    int ch340Index = -1; // 记录CH340端口的索引
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        QSerialPort _com;
        _com.setPort(info);
        if (_com.portName() == serial->portName() or _com.open(QIODevice::ReadWrite))
        {
            QString portName = info.portName();
            ui->comboBox->addItem(portName);

            // 检测是否为CH340 (描述中包含"CH340"字符串，不区分大小写)
            if (info.description().contains("CH340", Qt::CaseInsensitive))
            {
                ch340Index = ui->comboBox->count() - 1; // 记录当前索引
            }
            _com.close();
        }
    }
    // 自动选中CH340
    if (ch340Index != -1)
    {
        ui->comboBox->setCurrentIndex(ch340Index);
    }

    // ====================== 处理 comboBox_2 (蓝牙串口) ======================
    ui->comboBox_2->clear();
    int bluetoothIndex = -1; // 记录蓝牙串口的索引
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        QSerialPort _com;
        _com.setPort(info);
        if (_com.portName() == serial->portName() or _com.open(QIODevice::ReadWrite))
        {
            QString portName = info.portName();
            ui->comboBox_2->addItem(portName);

            // 检测是否为蓝牙串口 (描述中包含"Bluetooth"或"蓝牙")
            if (info.description().contains("Bluetooth", Qt::CaseInsensitive) ||
                info.description().contains("蓝牙", Qt::CaseInsensitive))
            {
                bluetoothIndex = ui->comboBox_2->count() - 1; // 记录当前索引
            }
            _com.close();
        }
    }
    // 自动选中蓝牙串口
    if (bluetoothIndex != -1)
    {
        ui->comboBox_2->setCurrentIndex(bluetoothIndex);
    }
}

void MainWindow::on_pushButton_2_clicked()
{

    if (!serial->isOpen()&& !btserial->isOpen())
    {
        serial->setPortName(ui->comboBox->currentText());
        btserial->setPortName(ui->comboBox_2->currentText());
        if (serial->open(QIODevice::ReadWrite) && btserial->open(QIODevice::ReadWrite))
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
        btserial->close();
        // ui->radioButton->setChecked(false);
        ui->pushButton_2->setText("打开串口");
    }
}

void MainWindow::on_pushButton_3_clicked()
{
    btn = 1 ;
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
    tmpresult = bluetoothprotocolparser.thick;



}

QByteArray m_buffer;
void MainWindow::btReceive()
{
    // 1. 读取所有当前可用的数据
    QByteArray data = btserial->readAll();

    // 2. 将新数据追加到全局缓冲区
    m_buffer.append(data);

    // 3. 判断接收信息 (核心逻辑)
    // 这里假设协议是：以换行符 \n 作为一条信息的结束
    while (m_buffer.contains('\n')) {
        // 找到换行符的位置
        int index = m_buffer.indexOf('\n');

        // 从缓冲区中截取一条完整的信息 (0 到 index 之间)
        QByteArray packet = m_buffer.left(index);

        // 移除已处理的数据 (包括换行符本身，index+1)
        m_buffer = m_buffer.mid(index + 1);

        // 4. 处理这条完整的信息
        //qDebug() << "收到完整信息:" << packet;

        // 在这里添加你的业务逻辑，比如解析数据、更新UI等
        processPacket(packet);
    }
}


// 自定义处理数据包的函数
void MainWindow::processPacket(const QByteArray &packet)
{
    // 示例：如果收到 "ON"，做某事；收到 "OFF"，做另一件事
    if (packet == "BTN1\r") {
        qDebug() << "收到完整信息:";
        btn = 1;
        qDebug() <<tmpresult;
    }
}


void MainWindow::on_pushButton_5_clicked()
{
    ui->textBrowser->clear();
}

void MainWindow::on_pushButton_finish_clicked()
{
    ui->lineEdit_result->setText(QString::number(tmpresult, 'f', 2));
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
    if(btn == 1){
        int row = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(row);
        //qDebug() << tmpresult;

        // 设置“测量结果”列（第2列）
        QTableWidgetItem *resultItem = new QTableWidgetItem(QString::number(tmpresult, 'f', 2));
        ui->tableWidget->setItem(row, 0, resultItem);

        QScrollBar *vScrollBar = ui->tableWidget->verticalScrollBar();
        if (vScrollBar)
        { // 防护滚动条空指针
            vScrollBar->setValue(vScrollBar->maximum());
        }

        // ========== 新增4：将有效数据存入统计列表 + 触发统计更新 ==========
        measureValues.append(static_cast<double>(tmpresult)); // 转double存入统计列表

        updateStatistics(); // 立即更新最大值/最小值/平均值
        btn = 0;
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

#ifdef HAS_QXLSX

// ===================== QXlsx 工具函数 =====================

// 单行指定列合并
bool mergeSingleRowColumns(QXlsx::Document *doc, const QString &sheetName, int targetRow, int col1, int col2, const QVariant &value)
{
    if (!doc || targetRow < 1 || col1 >= col2 || col1 < 1 || col2 < 1)
    {
        qDebug() << "合并参数非法：targetRow=" << targetRow << " col1=" << col1 << " col2=" << col2;
        return false;
    }

    try
    {
        QXlsx::Format format;
        format.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
        format.setVerticalAlignment(QXlsx::Format::AlignVCenter);
        format.setBorderStyle(QXlsx::Format::BorderThin);

        if (!doc->selectSheet(sheetName)) {
            qWarning() << "无法选中工作表:" << sheetName;
            return false;
        }

        doc->mergeCells(QXlsx::CellRange(targetRow, col1, targetRow, col2));

        doc->write(targetRow, col1, value, format);

        return true;
    }
    catch (...)
    {
        qDebug() << "QXlsx合并异常";
        return false;
    }
}

// 取消指定行的所有合并
void unmergeRowAllColumns(QXlsx::Document *doc, const QString &sheetName, int targetRow)
{
    if (!doc || targetRow < 1)
        return;
    try
    {

        // 1. 先选中目标工作表
        if (!doc->selectSheet(sheetName)) {
            qWarning() << "无法选中工作表:" << sheetName;
            return;
        }

        // 2. 获取当前工作表对象（注意：需包含 QXlsx/Worksheet.h 头文件）
        QXlsx::Worksheet* worksheet = doc->currentWorksheet();
        if (!worksheet) {
            qWarning() << "无法获取工作表对象";
            return;
        }

        // 3. 从 Worksheet 对象获取合并单元格列表
        QList<QXlsx::CellRange> merges = worksheet->mergedCells();

        // 4. 遍历并解除合并
        for (const QXlsx::CellRange &range : merges)
        {
            if (range.firstRow() <= targetRow && range.lastRow() >= targetRow)
            {
                // unmergeCells 可以直接用 Document 调用，也可以用 Worksheet 调用
                doc->unmergeCells(range);
            }
        }

        for (const QXlsx::CellRange &range : merges)
        {
            if (range.firstRow() <= targetRow && range.lastRow() >= targetRow)
            {
                doc->unmergeCells(range);
            }
        }
    }
    catch (...)
    {
        qDebug() << "取消行合并异常：row=" << targetRow;
    }
}

#endif

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

#ifdef HAS_QXLSX

// 【核心修复】精准查找最后一条业务数据行（排除检测行）
int MainWindow::findLastDataRow(QXlsx::Document *doc, const QString &sheetName, int col)
{
    if (!doc)
        return START_ROW - 1;
    int lastRow = START_ROW - 1;

    for (int row = MAX_SEARCH_ROW; row >= START_ROW; row--)
    {
        unmergeRowAllColumns(doc, sheetName, row);

        if (!doc->selectSheet(sheetName)) {
            qWarning() << "无法选中工作表:" << sheetName;
        }

        // 2. 读取数据（移除所有 read 函数最后的 sheetName 参数）
        QString valSerial = doc->read(row, COL_SERIAL).toString().trimmed();
        QString valData = doc->read(row, COL_DATA_START).toString().trimmed();
        QString valDetect = doc->read(row, DETECT_COL_START).toString().trimmed();

        if (!valSerial.isEmpty() && !valData.isEmpty() && !valDetect.contains("检测："))
        {
            lastRow = row;
            break;
        }
    }

    return qMax(lastRow, START_ROW - 1);
}

// 辅助：设置单元格值
bool MainWindow::setCellValue(QXlsx::Document *doc, const QString &sheetName, int row, int col, const QVariant &val)
{
    if (!doc || row < 1 || col < 1)
        return false;

    unmergeRowAllColumns(doc, sheetName, row);

    try
    {
        QXlsx::Format format;
        format.setBorderStyle(QXlsx::Format::BorderThin);

        bool isSingleConclusion = (col == COL_SINGLE_CONCL);
        bool isUnPass = (val.toString() == "不合格");
        if (isSingleConclusion && isUnPass)
        {
            format.setFontColor(Qt::red);
        }
        else
        {
            format.setFontColor(Qt::black);
        }

        // 1. 先选中目标工作表
        if (!doc->selectSheet(sheetName)) {
            qWarning() << "无法选中工作表:" << sheetName;
        }

        // 2. 写入数据（移除最后的 sheetName 参数）
        doc->write(row, col, val, format);
        return true;
    }
    catch (...)
    {
        qDebug() << "设置单元格异常：row=" << row << " col=" << col;
        return false;
    }
}

// 辅助：多行单列合并
bool MainWindow::mergeMultiRowSingleCol(QXlsx::Document *doc, const QString &sheetName, int sRow, int eRow, int col, const QVariant &val)
{
    if (!doc || sRow < 1 || eRow < 1 || sRow > eRow || col < 1)
        return false;
    try
    {
        QXlsx::Format format;
        format.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
        format.setVerticalAlignment(QXlsx::Format::AlignVCenter);
        format.setBorderStyle(QXlsx::Format::BorderThin);

        bool isGroupConclusion = (col == COL_GROUP_CONCL);
        bool isGroupUnPass = (val.toString() == "组不合格");
        if (isGroupConclusion && isGroupUnPass)
        {
            format.setFontColor(Qt::red);
        }
        else
        {
            format.setFontColor(Qt::black);
        }

        // 1. 先选中目标工作表
        if (!doc->selectSheet(sheetName)) {
            qWarning() << "无法选中工作表:" << sheetName;

        }

        // 2. 合并单元格（仅传范围，无需 sheetName）
        doc->mergeCells(QXlsx::CellRange(sRow, col, eRow, col));

        // 3. 写入数据（仅传行、列、值、格式，无需 sheetName）
        doc->write(sRow, col, val, format);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

// 辅助：清空指定行内容
void MainWindow::clearRowContent(QXlsx::Document *doc, const QString &sheetName, int targetRow)
{
    if (!doc || targetRow < 1)
        return;
    try
    {
        for (int col = 1; col <= 20; col++)
        {
            // 1. 先选中目标工作表
            if (!doc->selectSheet(sheetName)) {
                qWarning() << "无法选中工作表:" << sheetName;
                return; // 或根据函数逻辑返回错误值
            }

            // 2. 写入空值（移除最后的 sheetName 参数）
            doc->write(targetRow, col, QVariant());
        }
    }
    catch (...)
    {
        qDebug() << "清空行内容异常：row=" << targetRow;
    }
}

// 辅助：清空起始行后所有数据（重填用）
void MainWindow::clearStartRowData(QXlsx::Document *doc, const QString &sheetName)
{
    if (!doc)
        return;
    try
    {
        for (int row = START_ROW; row <= MAX_SEARCH_ROW; row++)
        {
            clearRowContent(doc, sheetName, row);
        }
    }
    catch (...)
    {
    }
}

void MainWindow::setFontColor(QXlsx::Format &format, const QColor &color)
{
    format.setFontColor(color);
}

void MainWindow::setvalue(QXlsx::Document *doc, const QString &sheetName, int row, int col, const QVariant &val)
{
    if (!doc)
        return;
    // 1. 先选中目标工作表
    if (!doc->selectSheet(sheetName)) {
        qWarning() << "无法选中工作表:" << sheetName;
        return; // 或根据函数逻辑返回错误值
    }

    // 2. 写入数据（移除最后的 sheetName 参数）
    doc->write(row, col, val);
}

#endif

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

void MainWindow::saveToFile()
{
#ifndef HAS_QXLSX
    QMessageBox::warning(this, "提示", "QXlsx 库未集成，Excel 导出功能不可用");
    return;
#else

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
    // for (int i = 0; i < 23; i++){
    //     dataList.append(QString::number(48.0 + i * 0.1, 'f', 1));
    // }
    if (dataList.isEmpty())
    {
        QMessageBox::warning(this, "提示", "无数据");
        return;
    }

    // 6. Excel初始化
    QXlsx::Document doc(savePath);
    if (!doc.load())
    {
        QMessageBox::critical(this, "错误", "打开Excel文件失败");
        return;
    }

    if (!doc.selectSheet(sheetName))
    {
        QMessageBox::critical(this, "错误", QString("Sheet%1不存在").arg(sheetName));
        return;
    }

    // 7. 重填/续填逻辑（核心：覆盖检测行）
    bool isNewFill = ui->radioButton_3->isChecked();
    int currRow;
    int serialNum;
    int codeNum = 1001;

    if (isNewFill)
    {
        clearStartRowData(&doc, sheetName);
        currRow = START_ROW;
        serialNum = 1;
        codeNum = 1001;
    }
    else
    {
        int lastDataRow = findLastDataRow(&doc, sheetName, COL_SERIAL);
        currRow = lastDataRow + 1;
        currRow = qMax(currRow, 1);

        if (lastDataRow >= START_ROW)
        {
            // 1. 先选中目标工作表
            if (!doc.selectSheet(sheetName)) {
                qWarning() << "无法选中工作表:" << sheetName;
                return; // 或根据函数逻辑返回错误值
            }

            // 2. 读取数据（移除所有 read 函数最后的 sheetName 参数）
            serialNum = doc.read(lastDataRow, COL_SERIAL).toInt() + 1;
            QString lastCodeStr = doc.read(lastDataRow, COL_CODE).toString().trimmed();
            QString prefix;
            splitCode(lastCodeStr, prefix, codeNum);
            codeNum += 1;
        }
        else
        {
            currRow = START_ROW;
            serialNum = 1;
            codeNum = 1001;
        }

        clearRowContent(&doc, sheetName, currRow);
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

        clearRowContent(&doc, sheetName, currRow);

        if (COL_SERIAL)
            setCellValue(&doc, sheetName, currRow, COL_SERIAL, serialNum);
        if (COL_CODE && !content2.isEmpty())
            setCellValue(&doc, sheetName, currRow, COL_CODE, QString("%1-%2").arg(content2).arg(codeNum));
        if (COL_SPEC)
            setCellValue(&doc, sheetName, currRow, COL_SPEC, content);

        QList<double> rowVals;
        int dataCount = 0;
        for (int i = 0; i < DATA_PER_ROW && dataIdx < dataList.size(); i++)
        {
            int col = COL_DATA_START + i;
            double val = dataList[dataIdx].toDouble();
            setCellValue(&doc, sheetName, currRow, col, val);
            rowVals.append(val);
            dataIdx++;
            dataCount++;
        }

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

        if (COL_ROW_AVG > 0 && rowVals.size() >= 1)
            setCellValue(&doc, sheetName, currRow, COL_ROW_AVG, QString::number(rowAvg, 'f', 1));
        if (COL_ROW_MIN > 0 && rowVals.size() >= 1)
            setCellValue(&doc, sheetName, currRow, COL_ROW_MIN, QString::number(rowMin, 'f', 1));

        if (COL_SINGLE_CONCL > 0 && isDataEnough && rowVals.size() >= 1)
        {
            QString singleConcl = getSingleConclusion(rowAvg, rowMin);
            if (!singleConcl.isEmpty())
            {
                setCellValue(&doc, sheetName, currRow, COL_SINGLE_CONCL, singleConcl);
            }
        }

        if (COL_MERGE_L && COL_MERGE_M && !rowVals.isEmpty())
            mergeSingleRowColumns(&doc, sheetName, currRow, COL_MERGE_L, COL_MERGE_M, QString::number(rowAvg, 'f', 1));

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

        if (COL_GROUP_AVG && cnt > 0)
            mergeMultiRowSingleCol(&doc, sheetName, groupS, groupE, COL_GROUP_AVG, QString::number(groupAvg, 'f', 1));

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
                mergeMultiRowSingleCol(&doc, sheetName, groupS, groupE, COL_GROUP_CONCL, groupConcl);
            }
        }
    }

    // 10. 检测行（覆盖原有检测行，不跳过）
    if (currRow < 1)
        currRow = 1;
    clearRowContent(&doc, sheetName, currRow);
    unmergeRowAllColumns(&doc, sheetName, currRow);

    QString detectText = ui->lineEdit->text().trimmed();
    QDate selectedDate = ui->dateEdit->date();
    QString dateStr = selectedDate.toString("yyyy年MM月dd日");
    QString detectRowText = QString("检测：%1  日期：%2").arg(detectText).arg(dateStr);

    mergeSingleRowColumns(&doc, sheetName, currRow, DETECT_COL_START, DETECT_COL_END, detectRowText);

    QString ManText = ui->lineEdit_4->text().trimmed();
    switch (selIdx)
    {
    case 1:
    {
        setvalue(&doc, sheetName, 2, 8, ManText);
        setvalue(&doc, sheetName, 3, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(&doc, sheetName, 4, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 2:
    {
        setvalue(&doc, sheetName, 2, 10, ManText);
        setvalue(&doc, sheetName, 4, 9, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(&doc, sheetName, 4, 10, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 3:
    {
        setvalue(&doc, sheetName, 2, 16, ManText);
        setvalue(&doc, sheetName, 4, 16, QString("δ＜5mm:≥%1").arg(revalue(selIdx, 0)));
        setvalue(&doc, sheetName, 4, 17, QString("δ＜5mm :≥%1").arg(revalue(selIdx, 0)));
        setvalue(&doc, sheetName, 5, 16, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        setvalue(&doc, sheetName, 5, 17, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        break;
    }
    case 4:
    {
        setvalue(&doc, sheetName, 2, 8, ManText);
        setvalue(&doc, sheetName, 3, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(&doc, sheetName, 4, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 5:
    {
        setvalue(&doc, sheetName, 2, 10, ManText);
        setvalue(&doc, sheetName, 4, 9, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(&doc, sheetName, 4, 10, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 6:
    {
        setvalue(&doc, sheetName, 2, 16, ManText);
        setvalue(&doc, sheetName, 4, 16, QString("δ＜5mm:≥%1").arg(revalue(selIdx, 0)));
        setvalue(&doc, sheetName, 4, 17, QString("δ＜5mm :≥%1").arg(revalue(selIdx, 0)));
        setvalue(&doc, sheetName, 5, 16, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        setvalue(&doc, sheetName, 5, 17, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        break;
    }
    case 7:
    {
        setvalue(&doc, sheetName, 2, 8, ManText);
        setvalue(&doc, sheetName, 3, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(&doc, sheetName, 4, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 8:
    {
        setvalue(&doc, sheetName, 2, 10, ManText);
        setvalue(&doc, sheetName, 4, 9, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(&doc, sheetName, 4, 10, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 9:
    {
        setvalue(&doc, sheetName, 2, 16, ManText);
        setvalue(&doc, sheetName, 4, 16, QString("δ＜5mm:≥%1").arg(revalue(selIdx, 0)));
        setvalue(&doc, sheetName, 4, 17, QString("δ＜5mm :≥%1").arg(revalue(selIdx, 0)));
        setvalue(&doc, sheetName, 5, 16, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        setvalue(&doc, sheetName, 5, 17, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        break;
    }
    case 10:
    {
        setvalue(&doc, sheetName, 2, 8, ManText);
        setvalue(&doc, sheetName, 3, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(&doc, sheetName, 4, 9, QString("单体锌厚：≥%1μm平均锌厚：≥%2μm").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 11:
    {
        setvalue(&doc, sheetName, 2, 10, ManText);
        setvalue(&doc, sheetName, 4, 9, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        setvalue(&doc, sheetName, 4, 10, QString("δ≥6mm：≥%1（10kV及以下）；δ≥6mm：≥%2（35kV-500kV）").arg(revalue(selIdx, 0)).arg(revalue(selIdx, 1)));
        break;
    }
    case 12:
    {
        setvalue(&doc, sheetName, 2, 16, ManText);
        setvalue(&doc, sheetName, 4, 16, QString("δ＜5mm:≥%1").arg(revalue(selIdx, 0)));
        setvalue(&doc, sheetName, 4, 17, QString("δ＜5mm:≥%1").arg(revalue(selIdx, 0)));
        setvalue(&doc, sheetName, 5, 16, QString("δ≥5mm:≥%1").arg(revalue(selIdx, 1)));
        setvalue(&doc, sheetName, 5, 17, QString("δ≥5mm:≥%1").arg(revalue(selIdx, 1)));
    }
    case 13:
    {
        setvalue(&doc, sheetName, 2, 16, ManText);
        setvalue(&doc, sheetName, 4, 16, QString("δ＜5mm:≥%1").arg(revalue(selIdx, 0)));
        setvalue(&doc, sheetName, 4, 17, QString("δ＜5mm :≥%1").arg(revalue(selIdx, 0)));
        setvalue(&doc, sheetName, 5, 16, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        setvalue(&doc, sheetName, 5, 17, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        break;
    }
    case 14:
    {
        setvalue(&doc, sheetName, 2, 16, ManText);
        setvalue(&doc, sheetName, 4, 16, QString("δ＜5mm:≥%1").arg(revalue(selIdx, 0)));
        setvalue(&doc, sheetName, 4, 17, QString("δ＜5mm :≥%1").arg(revalue(selIdx, 0)));
        setvalue(&doc, sheetName, 5, 16, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        setvalue(&doc, sheetName, 5, 17, QString("δ≥5mm :≥%1").arg(revalue(selIdx, 1)));
        break;
    }
    }

    if (doc.save())
    {
        QMessageBox::information(this, "成功", QString("保存完成！"));
    }
    else
    {
        QMessageBox::critical(this, "错误", "保存Excel文件失败");
    }

#endif
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












