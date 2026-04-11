#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QApplication>
#include <QAxObject>
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
#include <QUdpSocket>
#include <QNetworkInterface>

const int NumMax = 5000;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

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

    void on_pushButton_5_clicked();

    void on_pushButton_finish_clicked();

    void on_pushButton_clear_clicked();

    void saveToFile();

    void deleteSelectedRow();

    void onNewConnection();      // 新客户端连接
    void onReadyRead();          // 接收数据
    void onClientDisconnected(); // 客户端断开

    QStringList getTableThirdColumn(QTableWidget *tableWidget);

    void onUdpBroadcastReceived();

    void on_pushButton_4_clicked();

    void splitCode(const QString &code, QString &prefix, int &num);
    int findLastDataRow(QAxObject *ws, int col);
    bool setCellValue(QAxObject *ws, int row, int col, const QVariant &val);
    bool mergeMultiRowSingleCol(QAxObject *ws, int sRow, int eRow, int col, const QVariant &val);
    void clearRowContent(QAxObject *ws, int targetRow);
    void clearStartRowData(QAxObject *ws);
    QString getSingleConclusion(double avg, double min);
    QString getGroupConclusion(QMap<int, double> &avgMap, int sRow, int eRow);

    void setFontColor(QAxObject *range, int color);

    void setvalue(QAxObject *ws, int row, int col, const QVariant &val);
    QString findValidBgImage();

    void on_comboBox_sampleName_currentIndexChanged(int index);

private:
    Ui::MainWindow *ui;
    float tempResult = 0.0;
    QList<double> measureValues;
    QSerialPort *serial;
    QTcpServer *tcpServer;             // TCP服务器（改为成员，避免局部变量销毁）
    QList<QTcpSocket *> clientSockets; // TCP客户端连接列表
    QLabel *statusLabel;               // 状态显示标签（若.ui中已添加，可改为ui->statusLabel）
    int take_flag;                     // 控制标志位
    QPixmap m_bgPixmap;
    QUdpSocket *m_udpListener;
};

#endif // MAINWINDOW_H
