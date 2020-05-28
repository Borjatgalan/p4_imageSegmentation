#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "QMessageBox"
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

/**
 * P4 - Image Segmentation
 * Ivan González Domínguez
 * Borja Alberto Tirado Galán
 *
 *
 */

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
                                          ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    cap = new VideoCapture(0);
    winSelected = false;
    selectColorImage = false;

    //Inicializacion de imagenes
    colorImage.create(240, 320, CV_8UC3);
    grayImage.create(240, 320, CV_8UC1);
    destColorImage.create(240, 320, CV_8UC3);
    destGrayImage.create(240, 320, CV_8UC1);
    canny_image.create(240, 320, CV_8UC1);
    detected_edges.create(240, 320, CV_8UC1);
    imgRegiones.create(240,320, CV_32SC1);
    imgMask.create(240, 320, CV_8UC1);

    visorS = new ImgViewer(&grayImage, ui->imageFrameS);
    visorD = new ImgViewer(&destGrayImage, ui->imageFrameD);

    connect(&timer, SIGNAL(timeout()), this, SLOT(compute()));
    connect(ui->captureButton, SIGNAL(clicked(bool)), this, SLOT(start_stop_capture(bool)));
    connect(ui->colorButton, SIGNAL(clicked(bool)), this, SLOT(change_color_gray(bool)));
    connect(visorS, SIGNAL(windowSelected(QPointF, int, int)), this, SLOT(selectWindow(QPointF, int, int)));
    connect(visorS, SIGNAL(pressEvent()), this, SLOT(deselectWindow()));

    connect(ui->loadButton, SIGNAL(pressed()), this, SLOT(loadFromFile()));



    timer.start(30);
}

MainWindow::~MainWindow()
{
    delete ui;
    delete cap;
    delete visorS;
    delete visorD;
}

void MainWindow::compute()
{
    //Captura de imagen

    if (ui->captureButton->isChecked() && cap->isOpened())
    {
        *cap >> colorImage;

        cv::resize(colorImage, colorImage, Size(320, 240));
        cvtColor(colorImage, grayImage, COLOR_BGR2GRAY);
        cvtColor(colorImage, colorImage, COLOR_BGR2RGB);
    }

    // ________________Procesamiento__________________


    if (winSelected)
    {
        visorS->drawSquare(QPointF(imageWindow.x + imageWindow.width / 2, imageWindow.y + imageWindow.height / 2), imageWindow.width, imageWindow.height, Qt::green);
    }
    visorS->update();
    visorD->update();
}

void MainWindow::start_stop_capture(bool start)
{
    if (start)
        ui->captureButton->setText("Stop capture");
    else
        ui->captureButton->setText("Start capture");
}

void MainWindow::change_color_gray(bool color)
{
    if (color)
    {
        ui->colorButton->setText("Gray image");
        visorS->setImage(&colorImage);
        visorD->setImage(&destColorImage);
    }
    else
    {
        ui->colorButton->setText("Color image");
        visorS->setImage(&grayImage);
        visorD->setImage(&destGrayImage);
    }
}

void MainWindow::selectWindow(QPointF p, int w, int h)
{
    QPointF pEnd;
    if (w > 0 && h > 0)
    {
        imageWindow.x = p.x() - w / 2;
        if (imageWindow.x < 0)
            imageWindow.x = 0;
        imageWindow.y = p.y() - h / 2;
        if (imageWindow.y < 0)
            imageWindow.y = 0;
        pEnd.setX(p.x() + w / 2);
        if (pEnd.x() >= 320)
            pEnd.setX(319);
        pEnd.setY(p.y() + h / 2);
        if (pEnd.y() >= 240)
            pEnd.setY(239);
        imageWindow.width = pEnd.x() - imageWindow.x;
        imageWindow.height = pEnd.y() - imageWindow.y;

        winSelected = true;
    }
}

void MainWindow::deselectWindow()
{
    winSelected = false;
}

void MainWindow::loadFromFile()
{
    disconnect(&timer, SIGNAL(timeout()), this, SLOT(compute()));

    Mat image;
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open"), "/home", tr("Images (*.jpg *.png "
                                                                                  "*.jpeg *.gif);;All Files(*)"));
    image = cv::imread(fileName.toStdString());

    if (fileName.isEmpty())
        return;
    else
    {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly))
        {
            QMessageBox::information(this, tr("Unable to open file"), file.errorString());
            return;
        }
        ui->captureButton->setChecked(false);
        ui->captureButton->setText("Start capture");
        cv::resize(image, colorImage, Size(320, 240));
        cvtColor(colorImage, colorImage, COLOR_BGR2RGB);
        cvtColor(colorImage, grayImage, COLOR_RGB2GRAY);

        if (ui->colorButton->isChecked())
            colorImage.copyTo(destColorImage);
        else
            grayImage.copyTo(destGrayImage);
        connect(&timer, SIGNAL(timeout()), this, SLOT(compute()));
    }
}

void MainWindow::saveToFile()
{
    disconnect(&timer, SIGNAL(timeout()), this, SLOT(compute()));
    Mat save_image;
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Image File"),
                                                    QString(),
                                                    tr("JPG (*.JPG) ; jpg (*.jpg); png (*.png); jpeg(*.jpeg); gif(*.gif); All Files (*)"));
    if (ui->colorButton->isChecked())
        cvtColor(destColorImage, save_image, COLOR_RGB2BGR);

    else
        cvtColor(destGrayImage, save_image, COLOR_GRAY2BGR);

    if (fileName.isEmpty())
        return;
    else
    {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly))
        {
            QMessageBox::information(this, tr("Unable to open file"),
                                     file.errorString());
            return;
        }
    }
    cv::imwrite(fileName.toStdString(), save_image);

    connect(&timer, SIGNAL(timeout()), this, SLOT(compute()));
}

void MainWindow::initialize(){
    //INICIALIZA PARÁMETROS COMO IMAGEN DE MÁSCARA Y HACE EL GUARDADO DE LA IMAGEN CANNY
    int lowThreshold = 40;
    int const maxThreshold = 120;

    grayImage.copySize(destGrayImage);
    // Reduce noise with a kernel 3x3
    blur(destGrayImage, detected_edges, Size(3, 3));

    // Canny detector
    cv::Canny(detected_edges, canny_image, lowThreshold, maxThreshold, 3);
    canny_image.copyTo(detected_edges);

    // Using Canny's output as a mask, we display our result
    destGrayImage = Scalar::all(0);

    grayImage.copyTo(destGrayImage, canny_image);
}

/** SE ENCARGA DEL PROCESAMIENTO DE LA IMAGEN
 * @brief MainWindow::segmentation
 */
void MainWindow::segmentation(){
    imgRegiones.setTo(-1);
    listRegiones.clear();
    initialize();
    idReg = 0;
    int idx;
    Point seedPoint;
    //inicializar mask
    cv::copyMakeBorder(canny_image,imgMask,1,1,1,1,1, BORDER_DEFAULT);

    for(int i = 0; i<imgRegiones.rows; i++){
        for(int j = 0; j<imgRegiones.cols; j++){
            //Mientras existan puntos que no sean bordes con valor -1 en imgRegiones
            if(imgRegiones.at<int>(j,i) == -1 && detected_edges.at<uchar>(j,i) != 255){
                //seleccionar el primer punto p que no sea borde con valor -1 en imgRegiones
                listRegiones[idx].p = Point(i,j);
                //Lanzar el crecimiento para grayImage y colorImage
                //Devuelve en minRect, el rectangulo minimo donde se marcan los puntos de la region con un 1
                //Falta definir correctamente minRect
                cv::floodFill(grayImage, imgMask, seedPoint, minRect,4|(1 << 8)| FLOODFILL_MASK_ONLY);
                //Recorriendo minRect: Actualizar imgRegiones asignando el valor de idReg a los puntos
                //no etiquetados con valor igual a 1 en imgMask
                for(int k = 0; k < minRect.width; k++){
                    for(int z = 0; z < minRect.height; z++)
                        if(imgMask.at<int>(z, k) != 1)
                            imgRegiones.at<int>(z, k) = idReg;
                            listRegiones[idx].nPuntos++;

                }

            }else
                idReg++;

            idx++;
       }
    }

}

