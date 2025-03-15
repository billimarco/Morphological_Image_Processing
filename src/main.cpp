#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <random>
#include <ctime>
#include <filesystem>
#include <omp.h>

#include <sys/stat.h>  // Per creare cartelle
#include <sys/types.h>
#ifdef _WIN32
    #include <direct.h>
    #define MKDIR(path) _mkdir(path)
#else
    #include <unistd.h>
    #define MKDIR(path) mkdir(path, 0777)
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <nlohmann/json.hpp>

using json = nlohmann::json;
// change OMP_NUM_THREADS environment variable to run with 1 to X threads...
// check configuration in drop down menu
// XXX check working directory so that ./images and ./output are valid !

struct STBImage {
    int width{0}, height{0}, channels{0};
    uint8_t *image_data{nullptr};
    std::string filename{};

    // Funzione per caricare un'immagine
    bool loadImage(const std::string &name) {
        image_data = stbi_load(name.c_str(), &width, &height, &channels, 1); // Immagine binaria (1 canale)
        if (!image_data)
            return false;
        else {
            filename = name;
            return true;
        }
    }

    // Funzione per salvare l'immagine
    void saveImage(const std::string &newName) const {
        stbi_write_jpg(newName.c_str(), width, height, channels, image_data, width);
    }

    // Funzione per inizializzare un'immagine binaria
    void initialize(int w, int h) {
        width = w;
        height = h;
        channels = 1; // Immagine binaria con 1 canale
        image_data = (uint8_t*)malloc(width * height * channels);

        // Inizializza l'immagine a nera (tutti i pixel sono 0)
        for (int i = 0; i < width * height * channels; i++) {
            image_data[i] = 0;
        }
    }
};

struct StructuringElement {
    std::vector<std::vector<int>> kernel;
    int width, height;
    int anchor_x, anchor_y; 

    StructuringElement(std::vector<std::vector<int>> k): 
        kernel(std::move(k)),
        width(kernel.empty() ? 0 : kernel[0].size()), 
        height(kernel.size()),
        anchor_x(width / 2), 
        anchor_y(height / 2) {}

    StructuringElement() : width(0), height(0), anchor_x(0), anchor_y(0) {}

    // Funzione per cambiare il kernel
    void setKernel(std::vector<std::vector<int>> new_kernel) {
        kernel = std::move(new_kernel);
        width = kernel.empty() ? 0 : kernel[0].size();
        height = kernel.size();
        anchor_x = width / 2;
        anchor_y = height / 2;
    }

    // Funzione per stampare il kernel
    void print() const {
        std::cout << "Elemento Strutturante:" << std::endl;
        for (const auto& row : kernel) {
            for (int val : row) {
                std::cout << val << " ";
            }
            std::cout << std::endl;
        }
    }

    void saveImage(const std::string& filename) const {
        int rows = kernel.size();
        int cols = kernel[0].size();
        std::vector<unsigned char> image(rows * cols, 0); // Inizializza tutta l'immagine a nero (0)

        // Riempie l'immagine con i valori del kernel
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                image[i * cols + j] = kernel[i][j] ? 255 : 0; // 255 = bianco, 0 = nero
            }
        }

        // Salva l'immagine in formato JPG
        stbi_write_jpg(filename.c_str(), cols, rows, 1, image.data(), 100);

        //std::cout << "Immagine salvata come: " << filename << std::endl;
    }
};

// Funzione per creare un cammino di cartelle
void createPath(const std::string &path) {
    std::istringstream ss(path);
    std::string partialPath;
    std::vector<std::string> directories;
    
    // Dividere il percorso nelle singole directory
    while (std::getline(ss, partialPath, '/')) {
        directories.push_back(partialPath);
    }

    std::string currentPath;
    for (const auto &dir : directories) {
        if (!currentPath.empty()) {
            currentPath += "/";
        }
        currentPath += dir;

        struct stat info;
        if (stat(currentPath.c_str(), &info) != 0) { // Se la cartella non esiste
            MKDIR(currentPath.c_str());
        }
    }
}

// Funzione per caricare immagini in un vettore
std::vector<STBImage> loadImages(const std::string &directory, int startIdx, int endIdx) {
    std::vector<STBImage> images;

    for (int i = startIdx; i <= endIdx; i++) {
        std::string filename = directory + "/image_" + std::to_string(i) + ".jpg";
        STBImage img;

        // Prova a caricare l'immagine
        if (img.loadImage(filename)) {
            images.push_back(std::move(img)); // Sposta l'immagine nel vettore per evitare copie inutili
            //std::cout << "Immagine caricata: " << filename << " (" << img.width << "x" << img.height << ")" << std::endl;
        } else {
            std::cerr << "Errore nel caricamento dell'immagine: " << filename << std::endl;
        }
    }

    return images;
}

// Disegna un rettangolo pieno
void drawRectangle(STBImage &img, int x, int y, int w, int h) {
    for (int i = y; i < y + h; i++)
        for (int j = x; j < x + w; j++)
            if (i >= 0 && i < img.height && j >= 0 && j < img.width)
                img.image_data[i * img.width + j] = 255;
}

// Disegna una cornice rettangolare (rettangolo con buco)
void drawHollowRectangle(STBImage &img, int x, int y, int w, int h, int thickness) {
    drawRectangle(img, x, y, w, thickness);
    drawRectangle(img, x, y + h - thickness, w, thickness);
    drawRectangle(img, x, y, thickness, h);
    drawRectangle(img, x + w - thickness, y, thickness, h);
}

// Disegna un cerchio pieno
void drawCircle(STBImage &img, int cx, int cy, int radius) {
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++)
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= radius * radius)
                img.image_data[y * img.width + x] = 255;
}

// Disegna un anello (cerchio con buco)
void drawHollowCircle(STBImage &img, int cx, int cy, int outerRadius, int innerRadius) {
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++) {
            int distSq = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (distSq <= outerRadius * outerRadius && distSq >= innerRadius * innerRadius)
                img.image_data[y * img.width + x] = 255;
        }
}

// Disegna una linea
void drawLine(STBImage &img, int x1, int y1, int x2, int y2) {
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1, err = dx - dy;

    while (true) {
        if (x1 >= 0 && x1 < img.width && y1 >= 0 && y1 < img.height)
            img.image_data[y1 * img.width + x1] = 255;
        if (x1 == x2 && y1 == y2) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

// Funzione per generare immagini binarie con forme casuali
void generateBinaryImages(int numImages, int width=256, int height=256) {
    srand(time(0)); // Inizializza il generatore di numeri casuali

    for (int i = 1; i <= numImages; i++) {
        STBImage img;
        img.initialize(width, height);

        int numShapes = rand() % 3 + 1;  // Può generare da 1 a 3 forme

        for (int j = 0; j < numShapes; j++) {
            int shapeType = rand() % 5; // 0: Rettangolo, 1: Cornice rettangolare, 2: Cerchio, 3: Anello, 4: Linea

            if (shapeType == 0) {
                int x = rand() % (img.width - 50);
                int y = rand() % (img.height - 50);
                int w = rand() % (img.width - x);
                int h = rand() % (img.height - y);
                drawRectangle(img, x, y, w, h);
            } 
            else if (shapeType == 1) {
                int x = rand() % (img.width - 100);
                int y = rand() % (img.height - 100);
                int w = rand() % 100 + 40;
                int h = rand() % 100 + 40;
                int thickness = 10;
                drawHollowRectangle(img, x, y, w, h, thickness);
            } 
            else if (shapeType == 2) {
                int cx = rand() % (img.width - 50);
                int cy = rand() % (img.height - 50);
                int radius = rand() % 50 + 10;
                drawCircle(img, cx, cy, radius);
            } 
            else if (shapeType == 3) {
                int cx = rand() % (img.width - 50);
                int cy = rand() % (img.height - 50);
                int outerRadius = rand() % 50 + 30;
                int innerRadius = rand() % (outerRadius - 10) + 10;
                drawHollowCircle(img, cx, cy, outerRadius, innerRadius);
            } 
            else {
                int x1 = rand() % img.width;
                int y1 = rand() % img.height;
                int x2 = rand() % img.width;
                int y2 = rand() % img.height;
                drawLine(img, x1, y1, x2, y2);
            }
        }

        // Salva l'immagine generata
        std::string filename = "images/basis/image_" + std::to_string(i) + ".jpg";
        img.saveImage(filename);
        //std::cout << "Immagine " << i << " salvata" << std::endl;
    }
}

// Funzione per generare un elemento strutturante circolare
std::vector<std::vector<int>> generateCircularKernel(int radius) {
    int size = 2 * radius + 1;  // La dimensione della matrice che rappresenta il kernel
    std::vector<std::vector<int>> kernel(size, std::vector<int>(size, 0));

    for (int i = -radius; i <= radius; ++i) {
        for (int j = -radius; j <= radius; ++j) {
            // Calcolare la distanza dal centro (0, 0)
            if (i * i + j * j <= radius * radius) {
                kernel[i + radius][j + radius] = 1;
            }
        }
    }
    return kernel;
}

// Funzione per generare un elemento strutturante quadrato
std::vector<std::vector<int>> generateSquareKernel(int sideLength) {
    int halfSide = sideLength / 2;
    int size = sideLength;
    std::vector<std::vector<int>> kernel(size, std::vector<int>(size, 0));

    for (int i = -halfSide; i <= halfSide; ++i) {
        for (int j = -halfSide; j <= halfSide; ++j) {
            kernel[i + halfSide][j + halfSide] = 1;
        }
    }
    return kernel;
}

// Funzione per eseguire l'erosione
STBImage erosion(const STBImage& img, const StructuringElement& se) {
    STBImage result;
    result.initialize(img.width, img.height);

    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            bool erode = false;
            for (int i = 0; i < se.height; i++) {
                for (int j = 0; j < se.width; j++) {
                    int nx = x + j - se.anchor_x;
                    int ny = y + i - se.anchor_y;

                    if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                        if (se.kernel[i][j] == 1 && img.image_data[ny * img.width + nx] == 0) {
                            erode = true;
                        }
                    }
                }
            }
            result.image_data[y * img.width + x] = erode ? 0 : 255;
        }
    }

    return result;
}

// Funzione per eseguire la dilatazione
STBImage dilation(const STBImage& img, const StructuringElement& se) {
    STBImage result;
    result.initialize(img.width, img.height);

    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            bool dilate = false;
            for (int i = 0; i < se.height; i++) {
                for (int j = 0; j < se.width; j++) {
                    int nx = x + j - se.anchor_x;
                    int ny = y + i - se.anchor_y;

                    if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                        if (se.kernel[i][j] == 1 && img.image_data[ny * img.width + nx] == 255) {
                            dilate = true;
                        }
                    }
                }
            }
            result.image_data[y * img.width + x] = dilate ? 255 : 0;
        }
    }

    return result;
}

// Funzione per eseguire l'apertura (Erosione seguita da Dilatazione)
STBImage opening(const STBImage& img, const StructuringElement& se) {
    return dilation(erosion(img, se), se);
}

// Funzione per eseguire la chiusura (Dilatazione seguita da Erosione)
STBImage closing(const STBImage& img, const StructuringElement& se) {
    return erosion(dilation(img, se), se);
}

int main(){
    createPath("images/basis");
    createPath("images/erosion");
    createPath("images/dilation");
    createPath("images/opening");
    createPath("images/closing");

    std::ifstream conf_file("settings/config.json");
    json config = json::parse(conf_file);

    int width = config["image_size"]["width"], height = config["image_size"]["height"], num_images = config["num_images"];
    
    generateBinaryImages(num_images, width, height);
    std::cout << num_images <<" immagini " << width << "x" << height << " generate con successo!" << std::endl;

    std::vector<STBImage> loadedImages = loadImages("images/basis", 1, num_images);
    std::cout << "Totale immagini caricate: " << loadedImages.size() << std::endl;

    StructuringElement se;
    if(config["structuring_element"]["shape"] == "square"){
        se.setKernel(generateSquareKernel(config["structuring_element"]["size"]));
        se.print();
        se.saveImage("se.jpg");
    } else if(config["structuring_element"]["shape"] == "circle"){
        se.setKernel(generateCircularKernel(config["structuring_element"]["size"]));
        se.print();
        se.saveImage("se.jpg");
    } else {
        std::cerr << "Forma dell'elemento strutturante non valida!" << std::endl;
        return 1;
    }
    /*
    StructuringElement se({{0, 1, 0}, 
                           {1, 1, 1}, 
                           {0, 1, 0}});
    */
    /*
    StructuringElement se({{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 
                           {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 
                           {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                           {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                           {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                           {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                           {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                           {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                           {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                           {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                           {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}});
    se.print();
    se.saveImage("se.jpg");
    */

    // Elaborazione di ogni immagine
    for (auto &img : loadedImages) {
        // Estrazione del nome del file senza percorso
        std::string filename = std::filesystem::path(img.filename).filename().string();

        // Applicazione delle trasformazioni e salvataggio
        STBImage eroded  = erosion(img, se);
        eroded.saveImage("images/erosion/" + filename);

        STBImage dilated = dilation(img, se);
        dilated.saveImage("images/dilation/" + filename);

        STBImage opened  = opening(img, se);
        opened.saveImage("images/opening/" + filename);

        STBImage closed  = closing(img, se);
        closed.saveImage("images/closing/" + filename);
    }

    std::cout << "Tutte le immagini sono state processate e salvate!" << std::endl;

    // Libera memoria delle immagini caricate
    for (auto &img : loadedImages) {
        stbi_image_free(img.image_data);
    }
}
