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

    //PILAR (29/05): la gestión del evento de activación del "checkbox" no debería hacerse así porque entonces sólo se ejecuta el SLOT en el instante de la pulsación
    //PILAR (29/05): en su lugar, en el método compute, debéis comprobar si "ui->showBottomUp_checkbox" se encuentra activo y, en tal caso, llamar a "segmentacion"
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

    if(ui->showBottomUp_checkbox->isChecked())
        segmentation();

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
    //PILAR (29/05): PROBLEMA GENERAL: los accesos a Mat con el método at deben indicar primer la posición de fila y después la de columna
    //PILAR (29/05): tenéis los accesos con las posiciones cambiadas en algunos sitios. Por ejemplo, imgRegiones.at<int>(j,i), debería ser imgRegiones.at<int>(i,j)
    for(int i = 0; i<imgRegiones.rows; i++){
        for(int j = 0; j<imgRegiones.cols; j++){
            if(imgRegiones.at<int>(i,j) == -1 && detected_edges.at<uchar>(i,j) != 255){
                seedPoint.x = j; //DONE PILAR (29/05): i indica fila. Se corresponde con seedPoint.y
                seedPoint.y = i; //DONE PILAR (29/05): j indica columna. Se corresponde con seedPoint.x
                cv::floodFill(grayImage, imgMask, seedPoint,idReg, &minRect,Scalar(ui->max_box->value()),Scalar(ui->max_box->value()),4|(1 << 8)| FLOODFILL_MASK_ONLY);
                //DONE PILAR (29/05): para recorrer la ventana minRect debéis partir, en el primer bucle, de minRect.x y terminar en minRect.x+minRect.width
                //DONE PILAR (29/05): en el segundo bucle, comenzáis en minRect.y y termináis en minRect.y+minRect.height
                grisAcum = 0;
                r.nPuntos = 0;
                for(int k = minRect.x; k < minRect.x+minRect.width; k++){ 		//columnas
                    for(int z = minRect.y; z < minRect.y+minRect.height; z++){ 	//filas
                        //DONE PILAR (29/05): imgMask es de tipo CV_8UC1 (uchar)
                        //DONE PILAR (29/05): imgMask tiene una fila más arriba y una columna más a la izquierda que imgRegiones.
                        //DONE PILAR (29/05): para acceder a la posición correcta, el acceso tiene que ser a la posición (z+1, k+1): imgMask.at<uchar>(z+1, k+1)
                        if(imgMask.at<uchar>(z+1, k+1) == 1 && imgRegiones.at<int>(z, k) == -1){
                            r.id = idReg;
                            r.nPuntos++; //DONE PILAR (29/05): tenéis que inicializarlo a 0 antes del bucle (antes de la línea 249)
                            r.pIni = Point(k,z); //Point(columa, fila)
                            //DONE PILAR (29/05): para calcular el nivel de gris medio hay que acumular el nivel de gris del punto actual sobre una variable de tipo int (grisAcum).
                            //DONE PILAR (29/05): Debéis inicializar grisAcum a 0 antes del bucle de la línea 249 y acumular aquí el nivel de gris del punto que habéis añadido a la región.
                            //DONE PILAR (29/05): Para ello, en lugar de la llamada  valorMedio, haríais lo siguiente: grisAcum += grayImage.at<uchar>(z, k)
                            //DONE PILAR (29/05): Una vez que termina este bucle (antes de la línea 267, calculáis el nivel de gris medio de la región como grisAcum/r.nPuntos
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
    Point puntoSimilar;
    for(size_t i = 0; i < vecinos.size(); i++){
        //PILAR (29/05): el propio punto no debería entrar en la comprobación porque siempre va a ser el más similar
        //PILAR (29/05): solo debeís comparar con los que tienen región asignada (imgRegiones.at<int>(y+vy, x+vx)!=-1)
        // Comprobamos los vecinos, el propio punto no se comprueba puesto que no se encuentra en la lista de vecinos.
        vx = vecinos[i].x;
        vy = vecinos[i].y;
        if(((x + vx) < imgRegiones.rows) && ((y + vy) < imgRegiones.cols)){ //Si esta dentro de la imagen
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

    vecinos.push_back(Point(-1,-1)); //a
    vecinos.push_back(Point(-1, 0)); //b
    vecinos.push_back(Point(-1,+1)); //c
    vecinos.push_back(Point( 0,-1)); //d
    vecinos.push_back(Point( 0,+1)); //e
    vecinos.push_back(Point(+1,-1)); //f
    vecinos.push_back(Point(+1, 0)); //g
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
    //DONE PILAR (29/05): el índice de fila debería ser y y el de columna x para que los accesos a las imágenes sean correctos
    for(int x = 0; x < imgRegiones.rows; x++){
        for(int y = 0; y <imgRegiones.cols; y++){
            id = imgRegiones.at<int>(x, y);
            valor = listRegiones[id].gMedio;
            imgGris.at<uchar>(x, y) = valor;
        }
    }

    imgGris.copyTo(destGrayImage);
}

