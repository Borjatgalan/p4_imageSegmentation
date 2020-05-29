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
    initVecinos();

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

    connect(ui->showBottomUp_checkbox, SIGNAL(clicked()), this, SLOT(segmentation()));



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

    if(ui->showBottomUp_checkbox->isChecked()){
        segmentation();
    }


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

    //Initialize regions img  and region list
    imgRegiones.setTo(-1);
    listRegiones.clear();
    //initialize mask image
    cv::copyMakeBorder(canny_image,imgMask,1,1,1,1,1, BORDER_DEFAULT);

}

/** SE ENCARGA DEL PROCESAMIENTO DE LA IMAGEN
 * @brief MainWindow::segmentation
 */
void MainWindow::segmentation(){
    initialize();
    idReg = 0;
    Point seedPoint;
    int grisAcum;

    for(int i = 0; i<imgRegiones.rows; i++){
        for(int j = 0; j<imgRegiones.cols; j++){
            if(imgRegiones.at<int>(i,j) == -1 && detected_edges.at<uchar>(i,j) != 255){
                seedPoint.x = j;
                seedPoint.y = i;
                cv::floodFill(grayImage, imgMask, seedPoint,idReg, &minRect,Scalar(ui->max_box->value()),Scalar(ui->max_box->value()),4|(1 << 8)| FLOODFILL_MASK_ONLY);

                grisAcum = 0;
                r.nPuntos = 0;
                for(int k = minRect.x; k < minRect.x+minRect.width; k++){ 		//columnas
                    for(int z = minRect.y; z < minRect.y+minRect.height; z++){ 	//filas
                        if(imgMask.at<uchar>(z+1, k+1) == 1 && imgRegiones.at<int>(z, k) == -1){
                            r.id = idReg;
                            r.nPuntos++;
                            r.pIni = Point(k,z); //Point(columa, fila)
                            grisAcum += grayImage.at<uchar>(z, k);
                            r.gMedio = grisAcum / r.nPuntos;
                            imgRegiones.at<int>(z, k) = idReg;
                            idReg++;
                        }
                    }
                }
                listRegiones.push_back(r);
            }
        }
        // ######### POST-PROCESAMIENTO #########
        //Asignar puntos de bordes a alguna region
        //Le asignamos el idReg del vecino que mas se parezca
        int idVecino;
        for(int i = 0; i<imgRegiones.rows; i++){
            for(int j = 0; j<imgRegiones.cols; j++){
                if(imgRegiones.at<int>(i,j) == - 1){
                    idVecino = vecinoMasSimilar(i, j);
                    imgRegiones.at<int>(i,j) = idVecino;
                    listRegiones[idVecino].nPuntos++;

                }
            }
        }

        //Lista fronteras
        vecinosFrontera();
        //Asigna los valores de gris de la imagen de regiones
        bottomUp();


    }

}
/** Metodo que agrega a la lista los puntos frontera de la imagen
 * @brief MainWindow::vecinosFrontera
 */
void MainWindow::vecinosFrontera()
{
    int vx = 0, vy = 0, id = 0;
    for(int x = 0; x < imgRegiones.rows; x++){
        for(int y = 0; y <imgRegiones.cols; y++){
            for(size_t i = 0; i < vecinos.size(); i++){
                vx = vecinos[i].x;
                vy = vecinos[i].y;
                if(((x + vx) < imgRegiones.rows) && ((y + vy) < imgRegiones.cols)){
                    if(imgRegiones.at<int>(y, x) != imgRegiones.at<int>(y+vy, x+vx)){
                        id=imgRegiones.at<int>(y, x);
                        listRegiones[id].frontera.push_back(Point(x,y));
                        break;
                    }
                }
            }
        }
    }
}

/** Metodo que visita los 8 vecinos para elegir el más similar al punto central y devuelve el identificador de region.
 * @brief MainWindow::vecinoMasSimilar
 * @param x
 * @param y
 * @return
 */
int MainWindow::vecinoMasSimilar(int x, int y)
{
    int vx = 0, vy = 0;
    int masSimilar = 255;
    int resta;
    int idReg = -1;
    for(size_t i = 0; i < vecinos.size(); i++){
        vx = vecinos[i].x;
        vy = vecinos[i].y;
        //Comprobamos dentro del rango de la imagen
        if((x + vx) >= 0 && (y + vy) >= 0 && (x + vx) < imgRegiones.rows && (y + vy) < imgRegiones.cols){
            if(imgRegiones.at<int>(y+vy, x+vx) != -1){
	            resta = abs(grayImage.at<uchar>(y, x) - grayImage.at<uchar>(y+vy, x+vx));
	            if(resta == 0){
	                return idReg = imgRegiones.at<int>(y+vy, x+vx);

	            }else if(resta < masSimilar){
	                masSimilar = resta;
	                idReg = imgRegiones.at<int>(y+vy, x+vx);
	            }
            }
        }
    }
    return idReg;
}

/** Inicializa la estructura de visitado de vecinos, el punto (0,0) no se inserta
 * @brief MainWindow::initVecinos
 */
void MainWindow::initVecinos()
{
    /*
     * a  |  b  |   c
     * d  |  p  |   e
     * f  |  g  |   h
     */

//  Point(columna, fila)
    vecinos.push_back(Point(-1,-1)); //a   //Etiquetas corregidas
    vecinos.push_back(Point( 0,-1)); //b
    vecinos.push_back(Point(+1,-1)); //c
    vecinos.push_back(Point(-1, 0)); //d
    vecinos.push_back(Point(+1, 0)); //e
    vecinos.push_back(Point(-1,+1)); //f
    vecinos.push_back(Point( 0,+1)); //g
    vecinos.push_back(Point(+1,+1)); //h


}

/** Asigna en destGrayImage los valores de gris medio que se encuentran en la imagen y en la lista de regiones
 * @brief MainWindow::bottomUp
 */
void MainWindow::bottomUp()
{
    int id = 0;
    uchar valor = 0;
    Mat imgGris;
    imgGris.create(240, 320, CV_8UC1);
    for(int x = 0; x < imgRegiones.rows; x++){
        for(int y = 0; y <imgRegiones.cols; y++){
            id = imgRegiones.at<int>(x, y);
            valor = listRegiones[id].gMedio;
            imgGris.at<uchar>(x, y) = valor;
        }
    }

    imgGris.copyTo(destGrayImage);
}

void MainWindow::mostrarListaRegiones()
{
    Region r;
    qDebug()<<"Size lista regiones: "<<listRegiones.size();
    for(size_t i = 0; i < listRegiones.size(); i++){
        qDebug()<<"ID: "<< listRegiones[i].id;
        qDebug()<<"Punto ini: columna:"<< listRegiones[i].pIni.x << "fila: " << listRegiones[i].pIni.y;
        qDebug()<<"Gris medio: "<< listRegiones[i].gMedio;
        qDebug()<<"Numero de puntos de la region: "<< listRegiones[i].nPuntos;

    }
}



