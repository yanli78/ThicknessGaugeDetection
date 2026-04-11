#ifndef SETTING_H
#define SETTING_H

#include <QDialog>
#include <QMessageBox>
#include <QCloseEvent>

namespace Ui {
class Setting;
}

class Setting : public QDialog
{
    Q_OBJECT

public:
    explicit Setting(QWidget *parent = nullptr);
    ~Setting();
    static double revalue(int sheet ,int two);

private slots:
    void on_pushButton_clicked();

    void on_comboBox_currentIndexChanged(int index);



    void on_pushButton_2_clicked();

private:
    Ui::Setting *ui;
    int old_index=0;




protected:
    void closeEvent(QCloseEvent *event) override;



};
//double savevalue[13][2];


double revalue(int sheet ,int two);

#endif // SETTING_H
