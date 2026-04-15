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

#ifdef HAS_QXLSX
#include "QXlsx/QXlsx/header/xlsxdocument.h"
#include "QXlsx/QXlsx/header/xlsxformat.h"
#include "QXlsx/QXlsx/header/xlsxcellrange.h"
#endif

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
#endif
    QString getSingleConclusion(double avg, double min);
    QString getGroupConclusion(QMap<int, double> &avgMap, int sRow, int eRow);
    QString findValidBgImage();

    void on_comboBox_sampleName_currentIndexChanged(int index);

private:
    Ui::MainWindow *ui;
    double tmpresult = 0.0;
    QList<double> measureValues;
    QSerialPort *serial;
    QSerialPort *btserial;
    QLabel *statusLabel;               // 状态显示标签（若.ui中已添加，可改为ui->statusLabel）
    int take_flag;                     // 控制标志位
    int btn = 0;                      // 蓝牙按钮标志位
    QPixmap m_bgPixmap;
    BluetoothProtocolParser bluetoothprotocolparser;
};





#endif // MAINWINDOW_H
