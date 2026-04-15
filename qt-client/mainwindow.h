#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QApplication>
#include <QDebug>
#include <QTableWidget>
#include <QDesktopServices>
#include <QScrollBar>
#include <windows.h>
#include <QThread>
#include <QKeyEvent>
#include "about.h"
#include "setting.h"
#include <QPainter>
#include <QColor>
#include <QVector>
#include <QComboBox>

#ifdef HAS_QXLSX
#include "xlsxdocument.h"
#include "xlsxformat.h"
#include "xlsxcellrange.h"
#endif

struct SampleConfig {
    QString sheetName;
    int colSerial;
    int colCode;
    int colSpec;
    int colDataStart;
    int startRow;
    int dataPerRow;
    int colRowAvg;
    int colRowMin;
    int colMergeL;
    int colMergeM;
    int colGroupAvg;
    int colGroupConcl;
    int colSingleConcl;
    int detectColStart;
    int detectColEnd;
    QString customText;
    QString imagePath;
};

const int NumMax = 5000;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE



class BluetoothProtocolParser : public QObject {
    Q_OBJECT

public:
    BluetoothProtocolParser(QObject *parent = nullptr) : QObject(parent) {}

    // 接收串口或蓝牙发来的原始数据
    void onDataReceived(const QByteArray &newData);
    double thick = 0;

private:
    QByteArray m_buffer; // 用于解决粘包和半包的底层缓存

    // 根据文档提供的 C 语言算法翻译的 CRC8 计算函数 [cite: 19-37]
    uint8_t crc8(const uint8_t *data, uint16_t length);

    // 解析 0x02 实时测量数据
    void parseRealTimeData(const QByteArray &data);
};



class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void serialSet();
    void dowork();

    float SaveData[NumMax];

protected:
    // 重写键盘按下事件处理函数（核心）
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void updateStatistics();

    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

    void on_pushButton_3_clicked();

    void dataReceive();
    void btReceive();

    void processPacket(const QByteArray &packet);

    void on_pushButton_5_clicked();

    void on_pushButton_finish_clicked();

    void on_pushButton_clear_clicked();

    void saveToFile();

    void deleteSelectedRow();

    QStringList getTableThirdColumn(QTableWidget *tableWidget);

    void on_pushButton_4_clicked();

    void splitCode(const QString &code, QString &prefix, int &num);
#ifdef HAS_QXLSX
    int findLastDataRow(QXlsx::Document *doc, const QString &sheetName, int col);
    bool setCellValue(QXlsx::Document *doc, const QString &sheetName, int row, int col, const QVariant &val);
    bool mergeMultiRowSingleCol(QXlsx::Document *doc, const QString &sheetName, int sRow, int eRow, int col, const QVariant &val);
    void clearRowContent(QXlsx::Document *doc, const QString &sheetName, int targetRow);
    void clearStartRowData(QXlsx::Document *doc, const QString &sheetName);
    void setFontColor(QXlsx::Format &format, const QColor &color);
    void setvalue(QXlsx::Document *doc, const QString &sheetName, int row, int col, const QVariant &val);
    void fillSampleTemplate(QXlsx::Document *doc, int selIdx, const QString &manText);
#endif
    QString getSingleConclusion(double avg, double min);
    QString getGroupConclusion(QMap<int, double> &avgMap, int sRow, int eRow);
    QString findValidBgImage();

    void on_comboBox_sampleName_currentIndexChanged(int index);

private:
    Ui::MainWindow *ui;
    void populateSerialPortComboBox(QComboBox *comboBox, const QStringList &keywords, int &autoSelectIndex);
    void configureSerialPort(QSerialPort *port);
    QVector<SampleConfig> getSampleConfigs();
    void applySampleConfig(const SampleConfig &config);
    double tmpresult = 0.0;
    QList<double> measureValues;
    QSerialPort *serial;
    QSerialPort *btserial;
    QLabel *statusLabel;               // 状态显示标签（若.ui中已添加，可改为ui->statusLabel）
    int take_flag;                     // 控制标志位
    int btn = 0;                      // 蓝牙按钮标志位
    QPixmap m_bgPixmap;
    BluetoothProtocolParser bluetoothprotocolparser;



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

    // 在MainWindow类中定义成员变量（缓存不完整数据）
    QByteArray m_dataCache;
    QByteArray m_buffer;
};





#endif // MAINWINDOW_H
