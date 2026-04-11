#include "setting.h"
#include "ui_setting.h"

Setting::Setting(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Setting)
{
    ui->setupUi(this);
}

double savevalue[15][2]={
    {0.0,0.0},
    {60.0,50.0},
    {120.0,100.0},
    {100.0,85.0},
    {75.0,65.0},
    {85.0,70.0},
    {75.0,65.0},
    {55.0,45.0},
    {90.0,75.0},
    {100.0,85.0},
    {55.0,45.0},
    {70.0,55.0},
    {86.0,70.0},
    {65.0,55.0},
    {100.0,85.0},

    };




Setting::~Setting()
{

    delete ui;
}

void Setting::on_pushButton_clicked()
{
    // 1. 定义临时数组存储初始化数据（声明时可直接用大括号）
    double tempData[15][2] = {
        {0.0,0.0},
        {60.0,50.0},
        {120.0,100.0},
        {100.0,85.0},
        {75.0,65.0},
        {85.0,70.0},
        {75.0,65.0},
        {55.0,45.0},
        {90.0,75.0},
        {100.0,85.0},
        {55.0,45.0},
        {70.0,55.0},
        {86.0,70.0},
        {65.0,55.0},
        {100.0,85.0},
    };

    // 2. 将临时数组的数据复制到类成员数组savevalue中
    // 方式1：双层循环（新手友好，易调试）
    for (int i = 0; i < 15; ++i) { // 遍历13行
        for (int j = 0; j < 2; ++j) { // 遍历2列
            savevalue[i][j] = tempData[i][j];
        }
    }
    if(ui->comboBox->currentIndex()>0)
    {
        ui->lineEdit->setText(QString::number(savevalue[ui->comboBox->currentIndex()][0],'f',2));
        ui->lineEdit_2->setText(QString::number(savevalue[ui->comboBox->currentIndex()][1],'f',2));
    }
}


// 假设savevalue是类的成员变量，需在Setting类中声明：
// double savevalue[13][2] = {0.0}; // 初始化所有值为0.0
// int old_index = 0; // 初始化old_index为0，避免首次使用未定义

void Setting::on_comboBox_currentIndexChanged(int index)
{
    // 防止重复触发：如果新索引和旧索引一致，直接返回
    if(index == old_index) return;

    QString avgnum = ui->lineEdit->text().trimmed(); // 去除首尾空格
    QString minnum = ui->lineEdit_2->text().trimmed();
    bool avgOK= false,minOK= false;

    // 统一边界判断：0~12（和新索引一致）
    if(old_index > 0 && old_index < 15)
    {
        double avgVal = avgnum.toDouble(&avgOK);
        double minVal = minnum.toDouble(&minOK);

        // 校验转换结果，且输入框不为空
        if(!avgOK || avgnum.isEmpty())
        {
            QMessageBox::warning(this, "输入错误", "平均值必须是有效数字，且不能为空！");
            ui->comboBox->blockSignals(true); // 临时阻塞信号，避免重复触发槽函数
            ui->comboBox->setCurrentIndex(old_index);
            ui->comboBox->blockSignals(false); // 恢复信号
            return; // 终止后续逻辑
        }
        if(!minOK || minnum.isEmpty())
        {
            QMessageBox::warning(this, "输入错误", "最小值必须是有效数字，且不能为空！");
            ui->comboBox->blockSignals(true);
            ui->comboBox->setCurrentIndex(old_index);
            ui->comboBox->blockSignals(false);
            return;
        }

        // 转换成功，保存数值
        savevalue[old_index][0] = avgVal;
        savevalue[old_index][1] = minVal;
    }

    // 加载新索引对应的数值
    if(index > 0 && index < 15)
    {
        ui->lineEdit->setText(QString::number(savevalue[index][0],'f',2));
        ui->lineEdit_2->setText(QString::number(savevalue[index][1],'f',2));
    }
    else
    {
        ui->lineEdit->setText(" ");
        ui->lineEdit_2->setText(" ");
    }

    old_index = index;
}


void Setting::closeEvent(QCloseEvent *event)
{
    // 关闭前校验最后一次的输入值
    if(old_index >= 0 && old_index < 15)
    {
        QString avgnum = ui->lineEdit->text().trimmed();
        QString minnum = ui->lineEdit_2->text().trimmed();
        bool avgOK= false,minOK= false;

        avgnum.toDouble(&avgOK);
        minnum.toDouble(&minOK);

        if((!avgOK || !minOK) && ui->comboBox->currentIndex())
        {
            // 弹出提示，让用户选择“取消关闭（修正）”或“强制关闭”
            QMessageBox::StandardButton ret = QMessageBox::warning(this,
                                                                   "输入错误",
                                                                   QString("当前选中项（%1）的数值输入无效，请修正后再关闭！\n是否强制关闭？")
                                                                       .arg(old_index),
                                                                   QMessageBox::Cancel | QMessageBox::Ok);

            if(ret == QMessageBox::Cancel)
            {
                event->ignore(); // 取消关闭窗口
                return;
            }
            // 若选Ok，继续执行关闭流程
        }
    }

    // 校验通过/用户强制关闭，正常销毁窗口
    event->accept();
}




double revalue(int sheet ,int two)
{
    return savevalue[sheet][two];
}






void Setting::on_pushButton_2_clicked()
{
    if(old_index >= 0 && old_index < 15)
    {
        QString avgnum = ui->lineEdit->text().trimmed();
        QString minnum = ui->lineEdit_2->text().trimmed();
        bool avgOK= false,minOK= false;

        avgnum.toDouble(&avgOK);
        minnum.toDouble(&minOK);

        if((!avgOK || !minOK) && ui->comboBox->currentIndex())
        {
            // 弹出提示，让用户选择“取消关闭（修正）”或“强制关闭”
            QMessageBox::StandardButton ret = QMessageBox::warning(this,
                                                                   "输入错误",
                                                                   QString("当前选中项（%1）的数值输入无效，请修正后再关闭！\n是否强制关闭？")
                                                                       .arg(old_index),
                                                                   QMessageBox::Cancel | QMessageBox::Ok);

            // 若选Ok，继续执行关闭流程
        }
    }

}

