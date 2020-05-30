#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/video.hpp>
#include <opencv2/videoio/videoio.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/calib3d/calib3d.hpp>

#include <imgviewer.h>

#include <QtWidgets/QFileDialog>


/**
 * P4 - Image Segmentation
 * Ivan González Domínguez
 * Borja Alberto Tirado Galán
 *
 *
 */

using namespace cv;

namespace Ui {
    class MainWindow;
}


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:

    typedef struct{
        Point point;
        float valor;
    } punto;

    struct puntoCompare {
        bool operator()(const punto a, const punto b) const {
            return a.valor > b.valor;
        }
    };

    typedef struct{
        int id;
        Point pIni;
        int nPuntos;
        uchar gMedio; //valor gris medio
        Vec3b rgbMedio; //valor color medio
        std::vector<Point> frontera;
    }Region;

    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    //Interfaz principal de usuario
    Ui::MainWindow *ui;

    QTimer timer;

    VideoCapture *cap;
    ImgViewer *visorS, *visorD, *visorHistoS, *visorHistoD;
    Mat colorImage, grayImage, destColorImage, destGrayImage;
    bool winSelected, selectColorImage;
    Rect imageWindow;
    int idReg;

    Mat imgRegiones;
    Mat imgMask;
    Mat detected_edges;
    Mat canny_image; //Mat de canny    
    Rect minRect; //Minima ventana de los puntos modificados (añadidos a la region)
    Region r;

    std::vector<Region> listRegiones;
    std::vector<Point> vecinos;
    /*
    * cornerList[0] = Point
    * cornerList[1] = Valor de Point */
   std::vector<punto> cornerList;
   // Vector de lineas
   std::vector<QLine> lineList;
   // Vector de puntos validos
   std::vector<Point> pCorte;
   //Vector de segmentos
   std::vector<QLine> segmentList;

public slots:
    void compute();
    void start_stop_capture(bool start);
    void change_color_gray(bool color);
    void selectWindow(QPointF p, int w, int h);
    void deselectWindow();
    void loadFromFile();
    void saveToFile();
    void initialize();
    void segmentation();
    void initVecinos();
    int vecinoMasSimilar(int x, int y);
    void vecinosFrontera();
    void bottomUp();
    void asignarBordesARegion();
    void mostrarListaRegiones();
};


#endif // MAINWINDOW_H
