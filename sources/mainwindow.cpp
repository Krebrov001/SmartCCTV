/**
 * File Name:  mainwindow.cpp
 * Created By:  Wei Tao Li <>
 * Created On:  
 *
 * Modified By:  Konstantin Rebrov <krebrov@mail.csuchico.edu>
 * Modified On:  5/18/20
 *
 * Description:
 * This file contains code that runs in the GUI process when the user clicks
 * on various buttons, or when the GUI process itself reacts to internal events.
 * This file contains the definitions of functions of the MainWindow class,
 * as well as verious helper functions and global data structures.
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <string>       /* for std::string */
#include <syslog.h>     /* for openlog(), syslog(), closelog() */
#include <cstdlib>      /* for getenv(), atexit(), exit(), EXIT_FAILURE */
#include <cstring>      /* for memset(), strcmp(), strncmp() */
#include <signal.h>     /* for sigaction(), sigemptyset(), SIGCONT */
#include <mqueue.h>     /* for mq_notify(), mq_receive(), mq_open(), mq_close(), mq_unlink() */
#include <unistd.h>     /* for sleep() */
#include <stdio.h>      /* for sprintf() */
#include<iostream>      /* for is_open(), close(), ifstream */
#include <fstream>      /* for is_open(), close(), ifstream */
#include <QPixmap>
#include <QtDebug>
#include <QLabel>
#include <QCheckBox>

using namespace std;

mqd_t message_handler;  // message queue file descriptor
string message_handler_name = "/SmartCCTV_Message_handler";
QLabel* message_label = nullptr;
QLabel* daemon_label = nullptr;
QCheckBox* checkBox_1 = nullptr;
QCheckBox* checkBox_2 = nullptr;
QCheckBox* checkBox_3 = nullptr;
Daemon_facade* daemon_facade_ptr = nullptr;


void close_message_handler()
{
    syslog(log_facility | LOG_NOTICE, "Closing %s", message_handler_name.c_str());
    mq_close(message_handler);
    mq_unlink(message_handler_name.c_str());
}


void read_message(int)
{
    // Set up the message handler to recieve signal whenever a message comes in.
    // This is being done inside of the handler function because it has to be "reset" each time it is used.
    struct sigevent message_recieved;
    message_recieved.sigev_notify = SIGEV_SIGNAL;
    message_recieved.sigev_signo  = SIGCONT;
    mq_notify(message_handler, &message_recieved);

    // read a message
    int err;
    char message[151];
    memset(message, 0, 151);  // zero out the buffer
    // This is a while loop to handle when there are multiple messages queued up,
    // waiting to be read.
    while ( (err = mq_receive(message_handler, message, 150, nullptr)) != -1) {
        syslog(log_facility | LOG_NOTICE, "%s", message);
        // set the message into the label.
        if (message_label != nullptr) {
            message_label->setText(message);
        }

        if (strcmp("SmartCCTV encountered an error.", message) == 0) {
            if (daemon_label != nullptr) {
                daemon_label->setText("SmartCCTV have stopped running.");
            }
            //outline checkbox
            checkBox_1->setEnabled(true);
            //human detection checkbox
            checkBox_2->setEnabled(true);
            //motion detection checkbox
            checkBox_3->setEnabled(true);
        } else if (strcmp("SmartCCTV unexpected failure.", message) == 0) {
            if (daemon_label != nullptr) {
                daemon_label->setText("SmartCCTV have stopped running.");
            }
            //outline checkbox
            checkBox_1->setEnabled(true);
            //human detection checkbox
            checkBox_2->setEnabled(true);
            //motion detection checkbox
            checkBox_3->setEnabled(true);
        } else if (strncmp("SmartCCTV failed to open ", message, 25) == 0) {
            if (daemon_label != nullptr) {
                daemon_label->setText("SmartCCTV have stopped running.");
            }
            //outline checkbox
            checkBox_1->setEnabled(true);
            //human detection checkbox
            checkBox_2->setEnabled(true);
            //motion detection checkbox
            checkBox_3->setEnabled(true);
        } else if (strncmp("SmartCCTV could not create ", message, 27) == 0) {
            if (daemon_label != nullptr) {
                daemon_label->setText("Can not run SmartCCTV due to permission error.");
            }
            //outline checkbox
            checkBox_1->setEnabled(true);
            //human detection checkbox
            checkBox_2->setEnabled(true);
            //motion detection checkbox
            checkBox_3->setEnabled(true);
        } else if (strcmp("LiveStream Viewer unexpected failure.", message) == 0) {
            if (daemon_label != nullptr) {
                daemon_label->setText("LiveStream Viewer have stopped running.");
            }
        } else if (strcmp("Can't open LiveStream Viewer: SmartCCTV has been tampered.", message) == 0) {
            if (daemon_facade_ptr != nullptr) {
                bool daemon = daemon_facade_ptr->kill_daemon();
                if(daemon == false){
                    if (daemon_label != nullptr) {
                        daemon_label->setText("SmartCCTV is currently not running.");
                    }
                } else {
                    if (daemon_label != nullptr) {
                        daemon_label->setText("SmartCCTV have stopped running.");
                    }
                }

                //outline checkbox
                checkBox_1->setEnabled(true);
                //human detection checkbox
                checkBox_2->setEnabled(true);
                //motion detection checkbox
                checkBox_3->setEnabled(true);
            }
        } else if (strcmp("LiveStream Viewer window was closed.", message) == 0) {
            if (daemon_label != nullptr) {
                daemon_label->setText("LiveStream Viewer have stopped running.");
            }
        } else if (strcmp("Cannot find project configuration files.", message) == 0) {
            if (daemon_label != nullptr) {
                daemon_label->setText("SmartCCTV have stopped running.");
            }
            //outline checkbox
            checkBox_1->setEnabled(true);
            //human detection checkbox
            checkBox_2->setEnabled(true);
            //motion detection checkbox
            checkBox_3->setEnabled(true);
        }

        memset(message, 0, 151);  // zero out the buffer
    }
}


bool chkList(string str, int dayAmt) 
{
	// Append the user's input date
	str += ".out";
	
	// Get correct starting day for iteration
	int startDay = stoi((str.substr(0, 1)[0] == '0' ? str.substr(1, 1):str.substr(0, 2)));
	for(int i = startDay; i <= startDay + dayAmt; i++) 
	{
		// Get current day
		char buf[3];
		sprintf (buf, "%02d", i);
		
		// Get complete filename using buf and user input
		string cFName = string(buf) + str.substr(2, 12);
		
		// Check that file is reachable and readable before closing
		ifstream curFile(cFName);
		if(!curFile.is_open()) 
		{
			return false;
		}
		curFile.close();
	}
	return true;
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow), home_directory(nullptr)
{
    ui->setupUi(this);
    // The SmartCCTv GUI also writes messages to the syslog, so we need to open that as well.
    openlog("SmartCCTV_GUI", LOG_PID, log_facility);
    atexit(closelog);

    home_directory = getenv("HOME");
    if (home_directory == nullptr) {
        syslog(log_facility | LOG_ERR, "Error: $HOME environmental varaible not set : failed to identify home directory");
        syslog(log_facility | LOG_CRIT, "Failure starting the SmartCCTV application.");
        exit(EXIT_FAILURE);
    }

    // Set the QLabel to display the message that the daemon sends it.
    message_label = ui->label_3;
    daemon_label = ui->daemon_label;

    // Set the QCheckBox pointers.
    checkBox_1 = ui->checkBox;
    checkBox_2 = ui->checkBox_2;
    checkBox_3 = ui->checkBox_3;

    // Set the daemon_facade_ptr.
    daemon_facade_ptr = &daemon_facade;

    // Setup the message handler to recieve error messages from the daemon and dispaly them onto the GUI.
    // Make sure we can handle the SIGCONT message when the message queue notification sens the signal.
    struct sigaction action_to_take;
    // handle with this function
    action_to_take.sa_handler = read_message;
    // zero out the mask (allow any signal to interrupt)
    sigemptyset(&action_to_take.sa_mask);
    action_to_take.sa_flags = 0;
    // Setup the handler for SIGCONT.
    sigaction(SIGCONT, &action_to_take, nullptr);

    struct mq_attr mq_attributes;
    mq_attributes.mq_flags = 0;
    mq_attributes.mq_maxmsg = 5;
    mq_attributes.mq_msgsize = 120;

    if ( (message_handler = mq_open(message_handler_name.c_str(), O_RDWR | O_CREAT | O_EXCL | O_NONBLOCK, S_IRUSR | S_IWUSR, &mq_attributes)) == -1)
    {
        syslog(log_facility | LOG_ERR, "Error: Could not create the message handler %s %m", message_handler_name.c_str());
        syslog(log_facility | LOG_CRIT, "Failure starting the SmartCCTV application.");
        exit(EXIT_FAILURE);
    }

    // Set up the message handler to recieve signal whenever a message comes in.
    struct sigevent message_recieved;
    message_recieved.sigev_notify = SIGEV_SIGNAL;
    message_recieved.sigev_signo  = SIGCONT;
    mq_notify(message_handler, &message_recieved);

    syslog(log_facility | LOG_NOTICE, "Creating %s", message_handler_name.c_str());

    daemon_facade.set_daemon_info(home_directory);
    QDate date = QDate::currentDate();
    ui->dateEdit->setDate(date);
    ui->dateEdit->setMaximumDate(date);
    ui->dateEdit->setMinimumDate(date.addMonths(-6));

    syslog(log_facility | LOG_NOTICE, "SmartCCTV GUI was started.");
}


MainWindow::~MainWindow()
{
    syslog(log_facility | LOG_NOTICE, "The GUI window was closed.");
    close_message_handler();

    delete ui;
}

//Display Chart
void MainWindow::on_pushButton_clicked()
{
//    qDebug() << ui->dateEdit->date().addDays(-1).toString("ddMMyyyy");
    int position = ui->horizontalSlider->value();
    qDebug() << position;
    QString command = "Rscript document.R ";
    for(int i = position; i >= 0; i--){
        command.append(ui->dateEdit->date().addDays(-i).toString("dd.MM.yyyy") + ".out ");
    }
    qDebug() << command;
    std::system(qPrintable(command));
    QString imgPath = home_directory;
    imgPath.append("/Desktop/SmartCCTV/Charts/Overview.pdf");

    QPixmap img(imgPath);
    ui->statChartLabel->setPixmap(img.scaled(ui->statChartLabel->width(),ui->statChartLabel->height(),Qt::KeepAspectRatio));
}


void MainWindow::on_pushButton_Run_clicked()
{
    // Making a command to run or kill the daemon should reset the dispalyed error message.
    if (message_label != nullptr) {
        message_label->setText("");
    }

    //This returns the value of the selected camera.
    int cameraNumber = ui->cameraSpinBox->value() - 1;

    //This will return boolean value which option is selected.
    bool outline = ui->checkBox->isChecked();
    bool human_det = ui->checkBox_2->isChecked();
    bool motion_det = ui->checkBox_3->isChecked();

    int daemon = daemon_facade.run_daemon(human_det, motion_det, outline, cameraNumber);
    if(daemon == 0){
        ui->daemon_label->setText("SmartCCTV is now running.");
        //outline checkbox
        ui->checkBox->setEnabled(false);
        //human detection checkbox
        ui->checkBox_2->setEnabled(false);
        //motion detection checkbox
        ui->checkBox_3->setEnabled(false);
    }
    else if(daemon == 1){
        ui->daemon_label->setText("SmartCCTV is already running.");
    }
    else if(daemon == 2){
        ui->daemon_label->setText("Can not run SmartCCTV due to permission error.");
    }
}


void MainWindow::on_pushButton_Kill_clicked()
{
    // Making a command to run or kill the daemon should reset the dispalyed error message.
    if (message_label != nullptr) {
        message_label->setText("");
    }

    bool daemon = daemon_facade.kill_daemon();
    if(daemon == false){
        ui->daemon_label->setText("SmartCCTV is currently not running.");
    }
    else{
        ui->daemon_label->setText("SmartCCTV have stopped running.");
    }

    //outline checkbox
    ui->checkBox->setEnabled(true);
    //human detection checkbox
    ui->checkBox_2->setEnabled(true);
    //motion detection checkbox
    ui->checkBox_3->setEnabled(true);
}


void MainWindow::on_horizontalSlider_sliderMoved(int position)
{
    if (position > 1){
        ui->day_label->setText("Additional Days");
    }
    else{
        ui->day_label->setText("Additional Day");
    }
    ui->range_counter->setText(QString::number(position));
}


void MainWindow::on_pushButton_2_clicked()
{
    int livestream = liveStream_facade.run_livestream_viewer(home_directory);
    if(livestream == 0){
        ui->daemon_label->setText("LiveStream Viewer is now running.");
    }
    else if(livestream == 1){
        ui->daemon_label->setText("LiveStream Viewer is already running.");
    }
    else if(livestream == 2){
        ui->daemon_label->setText("Can not run LiveStream Viewer due to permission error.");
    }
}

