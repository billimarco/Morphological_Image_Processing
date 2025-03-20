#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <random>
#include <ctime>
#include <iomanip>
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
            // Calcolare la distanza dal centro (0, 0) con Teorema di Pitagora
            if (i * i + j * j <= radius * radius) {
                kernel[i + radius][j + radius] = 1;
            }
        }
    }
    return kernel;
}

// Funzione per generare un elemento strutturante quadrato
std::vector<std::vector<int>> generateSquareKernel(int halfSideLength) {
    int fullSide = 2 * halfSideLength + 1;
    std::vector<std::vector<int>> kernel(fullSide, std::vector<int>(fullSide, 0));

    for (int i = -halfSideLength; i <= halfSideLength; ++i) {
        for (int j = -halfSideLength; j <= halfSideLength; ++j) {
            kernel[i + halfSideLength][j + halfSideLength] = 1;
        }
    }
    return kernel;
}




// FUNZIONI OPERAZIONI MORFOLOGICHE IN MODO SEQUENZIALE

// Funzione per eseguire l'erosione
STBImage erosion_V1(const STBImage& img, const StructuringElement& se) {
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
                            break;
                        }
                    }
                }
                if (erode) break;
            }
            result.image_data[y * img.width + x] = erode ? 0 : 255;
        }
    }

    return result;
}

// Funzione per eseguire la dilatazione
STBImage dilation_V1(const STBImage& img, const StructuringElement& se) {
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
                            break;
                        }
                    }
                }
                if (dilate) break;
            }
            result.image_data[y * img.width + x] = dilate ? 255 : 0;
        }
    }

    return result;
}

// Funzione per eseguire l'apertura (Erosione seguita da Dilatazione)
STBImage opening_V1(const STBImage& img, const StructuringElement& se) {
    return dilation_V1(erosion_V1(img, se), se);
}

// Funzione per eseguire la chiusura (Dilatazione seguita da Erosione)
STBImage closing_V1(const STBImage& img, const StructuringElement& se) {
    return erosion_V1(dilation_V1(img, se), se);
}

// Funzione per eseguire l'erosione per un vettore di immagini
std::unordered_map<std::string, STBImage> erosion_V1_imgvec(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};
    for (auto &img : imgs) { 
        imgs_results[img.filename] = erosion_V1(img, se);
    }
    return imgs_results;
}

// Funzione per eseguire la dilatazione per un vettore di immagini
std::unordered_map<std::string, STBImage> dilation_V1_imgvec(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};
    for (auto &img : imgs) {
        imgs_results[img.filename] = dilation_V1(img, se);
    }
    return imgs_results;
}

// Funzione per eseguire l'apertura per un vettore di immagini (Erosione seguita da Dilatazione)
std::unordered_map<std::string, STBImage> opening_V1_imgvec(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};
    for (auto &img : imgs) {
        imgs_results[img.filename] = dilation_V1(erosion_V1(img, se), se);
    }
    return imgs_results;
}

// Funzione per eseguire la chiusura per un vettore di immagini (Dilatazione seguita da Erosione)
std::unordered_map<std::string, STBImage> closing_V1_imgvec(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};
    for (auto &img : imgs) {
        imgs_results[img.filename] = erosion_V1(dilation_V1(img, se), se);
    }
    return imgs_results;
}

// Funzione per eseguire l'erosione ottimizzata
STBImage erosion_V2(const STBImage& img, const StructuringElement& se) {
    STBImage result;
    result.initialize(img.width, img.height);

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            bool erode = false;
            for (const auto& [dy, dx] : active_pixels) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                    if (img.image_data[ny * img.width + nx] == 0) {
                        erode = true;
                        break;
                    }
                }
            }
            result.image_data[y * img.width + x] = erode ? 0 : 255;
        }
    }
    return result;
}

// Funzione per eseguire la dilatazione ottimizzata
STBImage dilation_V2(const STBImage& img, const StructuringElement& se) {
    STBImage result;
    result.initialize(img.width, img.height);

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            bool dilate = false;
            for (const auto& [dy, dx] : active_pixels) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                    if (img.image_data[ny * img.width + nx] == 255) {
                        dilate = true;
                        break;
                    }
                }
            }
            result.image_data[y * img.width + x] = dilate ? 255 : 0;
        }
    }
    return result;
}

// Funzione per eseguire l'apertura ottimizzata (Erosione seguita da Dilatazione)
STBImage opening_V2(const STBImage& img, const StructuringElement& se) {
    STBImage half_result;
    STBImage result;
    half_result.initialize(img.width, img.height);
    result.initialize(img.width, img.height);

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            bool dilate = false;
            for (const auto& [dy, dx] : active_pixels) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                    if (img.image_data[ny * img.width + nx] == 255) {
                        dilate = true;
                        break;
                    }
                }
            }
            half_result.image_data[y * img.width + x] = dilate ? 255 : 0;
        }
    }

    for (int y = 0; y < half_result.height; y++) {
        for (int x = 0; x < half_result.width; x++) {
            bool erode = false;
            for (const auto& [dy, dx] : active_pixels) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < half_result.width && ny >= 0 && ny < half_result.height) {
                    if (half_result.image_data[ny * half_result.width + nx] == 0) {
                        erode = true;
                        break;
                    }
                }
            }
            result.image_data[y * half_result.width + x] = erode ? 0 : 255;
        }
    }

    return result;
}

// Funzione per eseguire la chiusura ottimizzata (Dilatazione seguita da Erosione)
STBImage closing_V2(const STBImage& img, const StructuringElement& se) {
    STBImage half_result;
    STBImage result;
    half_result.initialize(img.width, img.height);
    result.initialize(img.width, img.height);

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            bool erode = false;
            for (const auto& [dy, dx] : active_pixels) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                    if (img.image_data[ny * img.width + nx] == 0) {
                        erode = true;
                        break;
                    }
                }
            }
            half_result.image_data[y * img.width + x] = erode ? 0 : 255;
        }
    }

    for (int y = 0; y < half_result.height; y++) {
        for (int x = 0; x < half_result.width; x++) {
            bool dilate = false;
            for (const auto& [dy, dx] : active_pixels) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < half_result.width && ny >= 0 && ny < half_result.height) {
                    if (half_result.image_data[ny * half_result.width + nx] == 255) {
                        dilate = true;
                        break;
                    }
                }
            }
            result.image_data[y * half_result.width + x] = dilate ? 255 : 0;
        }
    }

    return result;
}

// Funzione per eseguire l'erosione ottimizzata per un vettore di immagini
std::unordered_map<std::string, STBImage> erosion_V2_imgvec(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    for (auto &img : imgs) { 
        STBImage result;
        result.initialize(img.width, img.height);

        for (int y = 0; y < img.height; y++) {
            for (int x = 0; x < img.width; x++) {
                bool erode = false;
                for (const auto& [dy, dx] : active_pixels) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                        if (img.image_data[ny * img.width + nx] == 0) {
                            erode = true;
                            break;
                        }
                    }
                }
                result.image_data[y * img.width + x] = erode ? 0 : 255;
            }
        }

        imgs_results[img.filename] = result;
    }
    return imgs_results;
}

// Funzione per eseguire la dilatazione ottimizzata per un vettore di immagini
std::unordered_map<std::string, STBImage> dilation_V2_imgvec(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    for (auto &img : imgs) {
        STBImage result;
        result.initialize(img.width, img.height);

        for (int y = 0; y < img.height; y++) {
            for (int x = 0; x < img.width; x++) {
                bool dilate = false;
                for (const auto& [dy, dx] : active_pixels) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                        if (img.image_data[ny * img.width + nx] == 255) {
                            dilate = true;
                            break;
                        }
                    }
                }
                result.image_data[y * img.width + x] = dilate ? 255 : 0;
            }
        }
        imgs_results[img.filename] = result;
    }
    return imgs_results;
}

// Funzione per eseguire l'apertura ottimizzata per un vettore di immagini (Erosione seguita da Dilatazione)
std::unordered_map<std::string, STBImage> opening_V2_imgvec(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    for (auto &img : imgs) {
        STBImage half_result;
        STBImage result;
        half_result.initialize(img.width, img.height);
        result.initialize(img.width, img.height);

        for (int y = 0; y < img.height; y++) {
            for (int x = 0; x < img.width; x++) {
                bool dilate = false;
                for (const auto& [dy, dx] : active_pixels) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                        if (img.image_data[ny * img.width + nx] == 255) {
                            dilate = true;
                            break;
                        }
                    }
                }
                half_result.image_data[y * img.width + x] = dilate ? 255 : 0;
            }
        }
        for (int y = 0; y < half_result.height; y++) {
            for (int x = 0; x < half_result.width; x++) {
                bool erode = false;
                for (const auto& [dy, dx] : active_pixels) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < half_result.width && ny >= 0 && ny < half_result.height) {
                        if (half_result.image_data[ny * half_result.width + nx] == 0) {
                            erode = true;
                            break;
                        }
                    }
                }
                result.image_data[y * half_result.width + x] = erode ? 0 : 255;
            }
        }
        imgs_results[img.filename] = result;
    }
    return imgs_results;
}

// Funzione per eseguire la chiusura ottimizzata per un vettore di immagini (Dilatazione seguita da Erosione)
std::unordered_map<std::string, STBImage> closing_V2_imgvec(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    for (auto &img : imgs) {
        STBImage half_result;
        STBImage result;
        half_result.initialize(img.width, img.height);
        result.initialize(img.width, img.height);

        for (int y = 0; y < img.height; y++) {
            for (int x = 0; x < img.width; x++) {
                bool erode = false;
                for (const auto& [dy, dx] : active_pixels) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                        if (img.image_data[ny * img.width + nx] == 0) {
                            erode = true;
                            break;
                        }
                    }
                }
                half_result.image_data[y * img.width + x] = erode ? 0 : 255;
            }
        }
        for (int y = 0; y < half_result.height; y++) {
            for (int x = 0; x < half_result.width; x++) {
                bool dilate = false;
                for (const auto& [dy, dx] : active_pixels) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < half_result.width && ny >= 0 && ny < half_result.height) {
                        if (half_result.image_data[ny * half_result.width + nx] == 255) {
                            dilate = true;
                            break;
                        }
                    }
                }
                result.image_data[y * half_result.width + x] = dilate ? 255 : 0;
            }
        }
        imgs_results[img.filename] = result;
    }
    return imgs_results;
}




// FUNZIONI OPERAZIONI MORFOLOGICHE IN MODO PARALLELO TODO

// Funzione per eseguire l'erosione in parallelo
STBImage erosion_V1_parallel(const STBImage& img, const StructuringElement& se) {
    STBImage result;
    result.initialize(img.width, img.height);

    #pragma omp parallel for collapse(2) schedule(static) shared(result,img,se) default(none)
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
                            break;
                        }
                    }
                }
                if (erode) break;
            }
            result.image_data[y * img.width + x] = erode ? 0 : 255;
        }
    }

    return result;
}

// Funzione per eseguire la dilatazione in parallelo
STBImage dilation_V1_parallel(const STBImage& img, const StructuringElement& se) {
    STBImage result;
    result.initialize(img.width, img.height);

    #pragma omp parallel for collapse(2) schedule(static) shared(result,img,se) default(none)
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
                            break;
                        }
                    }
                }
                if (dilate) break;
            }
            result.image_data[y * img.width + x] = dilate ? 255 : 0;
        }
    }

    return result;
}

// Funzione per eseguire l'apertura in parallelo (Erosione seguita da Dilatazione)
STBImage opening_V1_parallel(const STBImage& img, const StructuringElement& se) {
    STBImage half_result;
    STBImage result;
    half_result.initialize(img.width, img.height);
    result.initialize(img.width, img.height);

    #pragma omp parallel shared(result,half_result,img,se) default(none)
    {
        #pragma omp for collapse(2) schedule(static)
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
                                break;
                            }
                        }
                    }
                    if (erode) break;
                }
                half_result.image_data[y * img.width + x] = erode ? 0 : 255;
            }
        }

        #pragma omp for collapse(2) schedule(static)
        for (int y = 0; y < half_result.height; y++) {
            for (int x = 0; x < half_result.width; x++) {
                bool dilate = false;
                for (int i = 0; i < se.height; i++) {
                    for (int j = 0; j < se.width; j++) {
                        int nx = x + j - se.anchor_x;
                        int ny = y + i - se.anchor_y;

                        if (nx >= 0 && nx < half_result.width && ny >= 0 && ny < half_result.height) {
                            if (se.kernel[i][j] == 1 && half_result.image_data[ny * half_result.width + nx] == 255) {
                                dilate = true;
                                break;
                            }
                        }
                    }
                    if (dilate) break;
                }
                result.image_data[y * img.width + x] = dilate ? 255 : 0;
            }
        }
    }

    return result;
}

// Funzione per eseguire la chiusura in parallelo (Dilatazione seguita da Erosione)
STBImage closing_V1_parallel(const STBImage& img, const StructuringElement& se) {
    STBImage half_result;
    STBImage result;
    half_result.initialize(img.width, img.height);
    result.initialize(img.width, img.height);

    #pragma omp parallel shared(result,half_result,img,se) default(none)
    {
        #pragma omp for collapse(2) schedule(static)
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
                                break;
                            }
                        }
                    }
                    if (dilate) break;
                }
                half_result.image_data[y * img.width + x] = dilate ? 255 : 0;
            }
        }

        #pragma omp for collapse(2) schedule(static)
        for (int y = 0; y < half_result.height; y++) {
            for (int x = 0; x < half_result.width; x++) {
                bool erode = false;
                for (int i = 0; i < se.height; i++) {
                    for (int j = 0; j < se.width; j++) {
                        int nx = x + j - se.anchor_x;
                        int ny = y + i - se.anchor_y;

                        if (nx >= 0 && nx < half_result.width && ny >= 0 && ny < half_result.height) {
                            if (se.kernel[i][j] == 1 && half_result.image_data[ny * half_result.width + nx] == 0) {
                                erode = true;
                                break;
                            }
                        }
                    }
                    if (erode) break;
                }
                result.image_data[y * half_result.width + x] = erode ? 0 : 255;
            }
        }
    }

    return result;
}

// Funzione per eseguire l'erosione per un vettore di immagini in parallelo
std::unordered_map<std::string, STBImage> erosion_V1_imgvec_parallel(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};
    #pragma omp parallel for schedule(dynamic) shared(imgs_results,imgs,se) default(none)
    for (auto &img : imgs) { 
        STBImage result;
        result.initialize(img.width, img.height);

        #pragma omp parallel for collapse(2) schedule(static) shared(result,img,se) default(none)
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
                                break;
                            }
                        }
                    }
                    if (erode) break;
                }
                result.image_data[y * img.width + x] = erode ? 0 : 255;
            }
        }

        #pragma omp critical
        {
            imgs_results[img.filename] = result;
        }
    }
    return imgs_results;
}

// Funzione per eseguire la dilatazione per un vettore di immagini in parallelo
std::unordered_map<std::string, STBImage> dilation_V1_imgvec_parallel(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};
    for (auto &img : imgs) {
        STBImage result;
        result.initialize(img.width, img.height);

        #pragma omp parallel for collapse(2) schedule(static) shared(result,img,se) default(none)
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
                                break;
                            }
                        }
                    }
                    if (dilate) break;
                }
                result.image_data[y * img.width + x] = dilate ? 255 : 0;
            }
        }

        #pragma omp critical
        {
            imgs_results[img.filename] = result;
        }
    }
    return imgs_results;
}

// Funzione per eseguire l'apertura per un vettore di immagini in parallelo (Erosione seguita da Dilatazione)
std::unordered_map<std::string, STBImage> opening_V1_imgvec_parallel(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};
    for (auto &img : imgs) {
        STBImage half_result;
        STBImage result;
        half_result.initialize(img.width, img.height);
        result.initialize(img.width, img.height);

        #pragma omp parallel shared(result,half_result,img,se) default(none)
        {
            #pragma omp for collapse(2) schedule(static)
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
                                    break;
                                }
                            }
                        }
                        if (erode) break;
                    }
                    half_result.image_data[y * img.width + x] = erode ? 0 : 255;
                }
            }

            #pragma omp for collapse(2) schedule(static)
            for (int y = 0; y < half_result.height; y++) {
                for (int x = 0; x < half_result.width; x++) {
                    bool dilate = false;
                    for (int i = 0; i < se.height; i++) {
                        for (int j = 0; j < se.width; j++) {
                            int nx = x + j - se.anchor_x;
                            int ny = y + i - se.anchor_y;

                            if (nx >= 0 && nx < half_result.width && ny >= 0 && ny < half_result.height) {
                                if (se.kernel[i][j] == 1 && half_result.image_data[ny * half_result.width + nx] == 255) {
                                    dilate = true;
                                    break;
                                }
                            }
                        }
                        if (dilate) break;
                    }
                    result.image_data[y * img.width + x] = dilate ? 255 : 0;
                }
            }
        }

        #pragma omp critical
        {
            imgs_results[img.filename] = result;
        }
    }
    return imgs_results;
}

// Funzione per eseguire la chiusura per un vettore di immagini in parallelo (Dilatazione seguita da Erosione)
std::unordered_map<std::string, STBImage> closing_V1_imgvec_parallel(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};
    for (auto &img : imgs) {
        STBImage half_result;
        STBImage result;
        half_result.initialize(img.width, img.height);
        result.initialize(img.width, img.height);
    
        #pragma omp parallel shared(result,half_result,img,se) default(none)
        {
            #pragma omp for collapse(2) schedule(static)
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
                                    break;
                                }
                            }
                        }
                        if (dilate) break;
                    }
                    half_result.image_data[y * img.width + x] = dilate ? 255 : 0;
                }
            }
    
            #pragma omp for collapse(2) schedule(static)
            for (int y = 0; y < half_result.height; y++) {
                for (int x = 0; x < half_result.width; x++) {
                    bool erode = false;
                    for (int i = 0; i < se.height; i++) {
                        for (int j = 0; j < se.width; j++) {
                            int nx = x + j - se.anchor_x;
                            int ny = y + i - se.anchor_y;
    
                            if (nx >= 0 && nx < half_result.width && ny >= 0 && ny < half_result.height) {
                                if (se.kernel[i][j] == 1 && half_result.image_data[ny * half_result.width + nx] == 0) {
                                    erode = true;
                                    break;
                                }
                            }
                        }
                        if (erode) break;
                    }
                    result.image_data[y * half_result.width + x] = erode ? 0 : 255;
                }
            }
        }

        #pragma omp critical
        {
            imgs_results[img.filename] = result;
        }
    }
    return imgs_results;
}

// Funzione per eseguire l'erosione ottimizzata in parallelo
STBImage erosion_V2_parallel(const STBImage& img, const StructuringElement& se) {
    STBImage result;
    std::vector<std::pair<int, int>> active_pixels;
    result.initialize(img.width, img.height);

    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    #pragma omp parallel for collapse(2) schedule(static) shared(result,active_pixels,img) default(none)
    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            bool erode = false;
            for (const auto& [dy, dx] : active_pixels) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                    if (img.image_data[ny * img.width + nx] == 0) {
                        erode = true;
                        break;
                    }
                }
            }
            result.image_data[y * img.width + x] = erode ? 0 : 255;
        }
    }

    return result;
}

// Funzione per eseguire la dilatazione ottimizzata in parallelo
STBImage dilation_V2_parallel(const STBImage& img, const StructuringElement& se) {
    STBImage result;
    result.initialize(img.width, img.height);

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    #pragma omp parallel for collapse(2) schedule(static) shared(result,active_pixels,img) default(none)
    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            bool dilate = false;
            for (const auto& [dy, dx] : active_pixels) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                    if (img.image_data[ny * img.width + nx] == 255) {
                        dilate = true;
                        break;
                    }
                }
            }
            result.image_data[y * img.width + x] = dilate ? 255 : 0;
        }
    }
    return result;
}

// Funzione per eseguire l'apertura ottimizzata in parallelo (Erosione seguita da Dilatazione)
STBImage opening_V2_parallel(const STBImage& img, const StructuringElement& se) {
    STBImage half_result;
    STBImage result;
    half_result.initialize(img.width, img.height);
    result.initialize(img.width, img.height);

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    #pragma omp parallel shared(result,half_result,active_pixels,img) default(none)
    {
        #pragma omp for collapse(2) schedule(static)
        for (int y = 0; y < img.height; y++) {
            for (int x = 0; x < img.width; x++) {
                bool dilate = false;
                for (const auto& [dy, dx] : active_pixels) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                        if (img.image_data[ny * img.width + nx] == 255) {
                            dilate = true;
                            break;
                        }
                    }
                }
                half_result.image_data[y * img.width + x] = dilate ? 255 : 0;
            }
        }

        #pragma omp for collapse(2) schedule(static)
        for (int y = 0; y < half_result.height; y++) {
            for (int x = 0; x < half_result.width; x++) {
                bool erode = false;
                for (const auto& [dy, dx] : active_pixels) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < half_result.width && ny >= 0 && ny < half_result.height) {
                        if (half_result.image_data[ny * half_result.width + nx] == 0) {
                            erode = true;
                            break;
                        }
                    }
                }
                result.image_data[y * half_result.width + x] = erode ? 0 : 255;
            }
        }
    }

    return result;
}

// Funzione per eseguire la chiusura ottimizzata in parallelo (Dilatazione seguita da Erosione)
STBImage closing_V2_parallel(const STBImage& img, const StructuringElement& se) {
    STBImage half_result;
    STBImage result;
    half_result.initialize(img.width, img.height);
    result.initialize(img.width, img.height);

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    #pragma omp parallel shared(result,half_result,active_pixels,img) default(none)
    {
        #pragma omp for collapse(2) schedule(static)
        for (int y = 0; y < img.height; y++) {
            for (int x = 0; x < img.width; x++) {
                bool erode = false;
                for (const auto& [dy, dx] : active_pixels) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                        if (img.image_data[ny * img.width + nx] == 0) {
                            erode = true;
                            break;
                        }
                    }
                }
                half_result.image_data[y * img.width + x] = erode ? 0 : 255;
            }
        }

        #pragma omp for collapse(2) schedule(static)
        for (int y = 0; y < half_result.height; y++) {
            for (int x = 0; x < half_result.width; x++) {
                bool dilate = false;
                for (const auto& [dy, dx] : active_pixels) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < half_result.width && ny >= 0 && ny < half_result.height) {
                        if (half_result.image_data[ny * half_result.width + nx] == 255) {
                            dilate = true;
                            break;
                        }
                    }
                }
                result.image_data[y * half_result.width + x] = dilate ? 255 : 0;
            }
        }
    }
    return result;
}

// Funzione per eseguire l'erosione ottimizzata per un vettore di immagini in parallelo
std::unordered_map<std::string, STBImage> erosion_V2_imgvec_parallel(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    #pragma omp parallel for schedule(dynamic) shared(imgs_results,imgs,active_pixels) default(none)
    for (auto &img : imgs) { 
        STBImage result;
        result.initialize(img.width, img.height);

        #pragma omp parallel for collapse(2) schedule(static) shared(result,active_pixels,img) default(none)
        for (int y = 0; y < img.height; y++) {
            for (int x = 0; x < img.width; x++) {
                bool erode = false;
                for (const auto& [dy, dx] : active_pixels) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                        if (img.image_data[ny * img.width + nx] == 0) {
                            erode = true;
                            break;
                        }
                    }
                }
                result.image_data[y * img.width + x] = erode ? 0 : 255;
            }
        }

        #pragma omp critical
        {
        imgs_results[img.filename] = result;
        }
    }
    return imgs_results;
}

// Funzione per eseguire la dilatazione ottimizzata per un vettore di immagini in parallelo
std::unordered_map<std::string, STBImage> dilation_V2_imgvec_parallel(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    #pragma omp parallel for schedule(dynamic) shared(imgs_results,imgs,active_pixels) default(none)
    for (auto &img : imgs) {
        STBImage result;
        result.initialize(img.width, img.height);

        #pragma omp parallel for collapse(2) schedule(static) shared(result,active_pixels,img) default(none)
        for (int y = 0; y < img.height; y++) {
            for (int x = 0; x < img.width; x++) {
                bool dilate = false;
                for (const auto& [dy, dx] : active_pixels) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                        if (img.image_data[ny * img.width + nx] == 255) {
                            dilate = true;
                            break;
                        }
                    }
                }
                result.image_data[y * img.width + x] = dilate ? 255 : 0;
            }
        }
        #pragma omp critical
        {
        imgs_results[img.filename] = result;
        }
    }
    return imgs_results;
}

// Funzione per eseguire l'apertura ottimizzata per un vettore di immagini in parallelo (Erosione seguita da Dilatazione)
std::unordered_map<std::string, STBImage> opening_V2_imgvec_parallel(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    #pragma omp parallel for schedule(dynamic) shared(imgs_results,imgs,active_pixels) default(none)
    for (auto &img : imgs) {
        STBImage half_result;
        STBImage result;
        half_result.initialize(img.width, img.height);
        result.initialize(img.width, img.height);

        #pragma omp parallel shared(result,half_result,active_pixels,img) default(none)
        {   
            #pragma omp for collapse(2) schedule(static)
            for (int y = 0; y < img.height; y++) {
                for (int x = 0; x < img.width; x++) {
                    bool dilate = false;
                    for (const auto& [dy, dx] : active_pixels) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                            if (img.image_data[ny * img.width + nx] == 255) {
                                dilate = true;
                                break;
                            }
                        }
                    }
                    half_result.image_data[y * img.width + x] = dilate ? 255 : 0;
                }
            }

            #pragma omp for collapse(2) schedule(static)
            for (int y = 0; y < half_result.height; y++) {
                for (int x = 0; x < half_result.width; x++) {
                    bool erode = false;
                    for (const auto& [dy, dx] : active_pixels) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < half_result.width && ny >= 0 && ny < half_result.height) {
                            if (half_result.image_data[ny * half_result.width + nx] == 0) {
                                erode = true;
                                break;
                            }
                        }
                    }
                    result.image_data[y * half_result.width + x] = erode ? 0 : 255;
                }
            }
        }

        #pragma omp critical
        {
            imgs_results[img.filename] = result;
        }
    }
    return imgs_results;
}

// Funzione per eseguire la chiusura ottimizzata per un vettore di immagini in parallelo (Dilatazione seguita da Erosione)
std::unordered_map<std::string, STBImage> closing_V2_imgvec_parallel(const std::vector<STBImage>& imgs, const StructuringElement& se) {
    std::unordered_map<std::string, STBImage> imgs_results = {};

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    #pragma omp parallel for schedule(dynamic) shared(imgs_results,imgs,active_pixels) default(none)
    for (auto &img : imgs) {
        STBImage half_result;
        STBImage result;
        half_result.initialize(img.width, img.height);
        result.initialize(img.width, img.height);

        #pragma omp parallel shared(result,half_result,active_pixels,img) default(none)
        {
            #pragma omp for collapse(2) schedule(static)
            for (int y = 0; y < img.height; y++) {
                for (int x = 0; x < img.width; x++) {
                    bool erode = false;
                    for (const auto& [dy, dx] : active_pixels) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < img.width && ny >= 0 && ny < img.height) {
                            if (img.image_data[ny * img.width + nx] == 0) {
                                erode = true;
                                break;
                            }
                        }
                    }
                    half_result.image_data[y * img.width + x] = erode ? 0 : 255;
                }
            }

            #pragma omp for collapse(2) schedule(static)
            for (int y = 0; y < half_result.height; y++) {
                for (int x = 0; x < half_result.width; x++) {
                    bool dilate = false;
                    for (const auto& [dy, dx] : active_pixels) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < half_result.width && ny >= 0 && ny < half_result.height) {
                            if (half_result.image_data[ny * half_result.width + nx] == 255) {
                                dilate = true;
                                break;
                            }
                        }
                    }
                    result.image_data[y * half_result.width + x] = dilate ? 255 : 0;
                }
            }
        }

        #pragma omp critical
        {
            imgs_results[img.filename] = result;
        }
    }
    return imgs_results;
}

int main(){
    #ifdef _OPENMP
        std::cout << "_OPENMP defined" << std::endl;
    #endif
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
        se.setKernel(generateSquareKernel(config["structuring_element"]["radius"]));
        se.print();
        se.saveImage("se.jpg");
    } else if(config["structuring_element"]["shape"] == "circle"){
        se.setKernel(generateCircularKernel(config["structuring_element"]["radius"]));
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

    // lambda function to calculate the mean time
    auto calculateMeanTime = [](const std::vector<double> &test_times, double &mean_time) {
        double sum = std::accumulate(test_times.begin(), test_times.end(), 0.0);
        mean_time = sum / test_times.size();
    };

    double start_time_all_images, end_time_all_images;
    double start_time_one_image, end_time_one_image;

    // SEQUENTIAL PART V1
    std::vector<double> erosion_V1_test_times;
    std::vector<double> dilation_V1_test_times;
    std::vector<double> opening_V1_test_times;
    std::vector<double> closing_V1_test_times;

    std::cout << "\nSEQUENTIAL PART V1\n" << std::endl;
    for (auto &img : loadedImages) {
        // Estrazione del nome del file senza percorso
        std::string filename = std::filesystem::path(img.filename).filename().string();

        // Applicazione delle trasformazioni e salvataggio
        start_time_one_image = omp_get_wtime();
        STBImage eroded  = erosion_V1(img, se);
        end_time_one_image = omp_get_wtime();
        //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
        erosion_V1_test_times.push_back(end_time_one_image - start_time_one_image);
        eroded.saveImage("images/erosion/" + filename);
    }

    start_time_all_images = omp_get_wtime();
    erosion_V1_imgvec(loadedImages, se);
    end_time_all_images = omp_get_wtime();

    double erosion_V1_seq_mean;
    double erosion_V1_seq_total=end_time_all_images - start_time_all_images;
    calculateMeanTime(erosion_V1_test_times, erosion_V1_seq_mean);
    std::cout << "Mean sequential erosion_V1 execution time : " << erosion_V1_seq_mean << " sec" << std::endl;
    std::cout << "Total sequential erosion_V1 execution time : " << erosion_V1_seq_total << " sec" << std::endl;

    for (auto &img : loadedImages) { 
        // Estrazione del nome del file senza percorso
        std::string filename = std::filesystem::path(img.filename).filename().string();
        
        // Applicazione delle trasformazioni e salvataggio
        start_time_one_image = omp_get_wtime();
        STBImage dilated = dilation_V1(img, se);
        end_time_one_image = omp_get_wtime();
        //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
        dilation_V1_test_times.push_back(end_time_one_image - start_time_one_image);
        dilated.saveImage("images/dilation/" + filename);
    }

    start_time_all_images = omp_get_wtime();
    dilation_V1_imgvec(loadedImages, se);
    end_time_all_images = omp_get_wtime();

    double dilation_V1_seq_mean;
    double dilation_V1_seq_total=end_time_all_images - start_time_all_images;
    calculateMeanTime(dilation_V1_test_times, dilation_V1_seq_mean);
    std::cout << "Mean sequential dilation_V1 execution time : " << dilation_V1_seq_mean << " sec" << std::endl;
    std::cout << "Total sequential dilation_V1 execution time : " << dilation_V1_seq_total << " sec" << std::endl;
    
    for (auto &img : loadedImages) {
        // Estrazione del nome del file senza percorso
        std::string filename = std::filesystem::path(img.filename).filename().string();
        
        // Applicazione delle trasformazioni e salvataggio
        start_time_one_image = omp_get_wtime();
        STBImage opened  = opening_V1(img, se);
        end_time_one_image = omp_get_wtime();
        //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
        opening_V1_test_times.push_back(end_time_one_image - start_time_one_image);
        opened.saveImage("images/opening/" + filename);
    }

    start_time_all_images = omp_get_wtime();
    opening_V1_imgvec(loadedImages, se);
    end_time_all_images = omp_get_wtime();

    double opening_V1_seq_mean;
    double opening_V1_seq_total=end_time_all_images - start_time_all_images;
    calculateMeanTime(opening_V1_test_times, opening_V1_seq_mean);
    std::cout << "Mean sequential opening_V1 execution time : " << opening_V1_seq_mean << " sec" << std::endl;
    std::cout << "Total sequential opening_V1 execution time : " << opening_V1_seq_total << " sec" << std::endl;
    
    for (auto &img : loadedImages) {
        // Estrazione del nome del file senza percorso
        std::string filename = std::filesystem::path(img.filename).filename().string();
        
        // Applicazione delle trasformazioni e salvataggio
        start_time_one_image = omp_get_wtime();
        STBImage closed  = closing_V1(img, se);
        end_time_one_image = omp_get_wtime();
        //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
        closing_V1_test_times.push_back(end_time_one_image - start_time_one_image);
        closed.saveImage("images/closing/" + filename);
    }

    start_time_all_images = omp_get_wtime();
    closing_V1_imgvec(loadedImages, se);
    end_time_all_images = omp_get_wtime();

    double closing_V1_seq_mean;
    double closing_V1_seq_total=end_time_all_images - start_time_all_images;
    calculateMeanTime(closing_V1_test_times, closing_V1_seq_mean);
    std::cout << "Mean sequential closing_V1 execution time : " << closing_V1_seq_mean << " sec" << std::endl;
    std::cout << "Total sequential closing_V1 execution time : " << closing_V1_seq_total << " sec" << std::endl;

    // SEQUENTIAL PART V2
    std::vector<double> erosion_V2_test_times;
    std::vector<double> dilation_V2_test_times;
    std::vector<double> opening_V2_test_times;
    std::vector<double> closing_V2_test_times;

    std::cout << "\nSEQUENTIAL PART V2\n" << std::endl;
    for (auto &img : loadedImages) {
        // Estrazione del nome del file senza percorso
        std::string filename = std::filesystem::path(img.filename).filename().string();

        // Applicazione delle trasformazioni e salvataggio
        start_time_one_image = omp_get_wtime();
        STBImage eroded  = erosion_V2(img, se);
        end_time_one_image = omp_get_wtime();
        //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
        erosion_V2_test_times.push_back(end_time_one_image - start_time_one_image);
        eroded.saveImage("images/erosion/" + filename);
    }

    start_time_all_images = omp_get_wtime();
    erosion_V2_imgvec(loadedImages, se);
    end_time_all_images = omp_get_wtime();

    double erosion_V2_seq_mean;
    double erosion_V2_seq_total=end_time_all_images - start_time_all_images;
    calculateMeanTime(erosion_V2_test_times, erosion_V2_seq_mean);
    std::cout << "Mean sequential erosion_V2 execution time : " << erosion_V2_seq_mean << " sec" << std::endl;
    std::cout << "Total sequential erosion_V2 execution time : " << erosion_V2_seq_total << " sec" << std::endl;

    for (auto &img : loadedImages) { 
        // Estrazione del nome del file senza percorso
        std::string filename = std::filesystem::path(img.filename).filename().string();
        
        // Applicazione delle trasformazioni e salvataggio
        start_time_one_image = omp_get_wtime();
        STBImage dilated = dilation_V2(img, se);
        end_time_one_image = omp_get_wtime();
        //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
        dilation_V2_test_times.push_back(end_time_one_image - start_time_one_image);
        dilated.saveImage("images/dilation/" + filename);
    }

    start_time_all_images = omp_get_wtime();
    dilation_V2_imgvec(loadedImages, se);
    end_time_all_images = omp_get_wtime();

    double dilation_V2_seq_mean;
    double dilation_V2_seq_total=end_time_all_images - start_time_all_images;
    calculateMeanTime(dilation_V2_test_times, dilation_V2_seq_mean);
    std::cout << "Mean sequential dilation_V2 execution time : " << dilation_V2_seq_mean << " sec" << std::endl;
    std::cout << "Total sequential dilation_V2 execution time : " << dilation_V2_seq_total << " sec" << std::endl;
    
    for (auto &img : loadedImages) {
        // Estrazione del nome del file senza percorso
        std::string filename = std::filesystem::path(img.filename).filename().string();
        
        // Applicazione delle trasformazioni e salvataggio
        start_time_one_image = omp_get_wtime();
        STBImage opened  = opening_V2(img, se);
        end_time_one_image = omp_get_wtime();
        //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
        opening_V2_test_times.push_back(end_time_one_image - start_time_one_image);
        opened.saveImage("images/opening/" + filename);
    }

    start_time_all_images = omp_get_wtime();
    opening_V2_imgvec(loadedImages, se);
    end_time_all_images = omp_get_wtime();

    double opening_V2_seq_mean;
    double opening_V2_seq_total=end_time_all_images - start_time_all_images;
    calculateMeanTime(opening_V2_test_times, opening_V2_seq_mean);
    std::cout << "Mean sequential opening_V2 execution time : " << opening_V2_seq_mean << " sec" << std::endl;
    std::cout << "Total sequential opening_V2 execution time : " << opening_V2_seq_total << " sec" << std::endl;
    
    for (auto &img : loadedImages) {
        // Estrazione del nome del file senza percorso
        std::string filename = std::filesystem::path(img.filename).filename().string();
        
        // Applicazione delle trasformazioni e salvataggio
        start_time_one_image = omp_get_wtime();
        STBImage closed  = closing_V2(img, se);
        end_time_one_image = omp_get_wtime();
        //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
        closing_V2_test_times.push_back(end_time_one_image - start_time_one_image);
        closed.saveImage("images/closing/" + filename);
    }

    start_time_all_images = omp_get_wtime();
    closing_V2_imgvec(loadedImages, se);
    end_time_all_images = omp_get_wtime();

    double closing_V2_seq_mean;
    double closing_V2_seq_total=end_time_all_images - start_time_all_images;
    calculateMeanTime(closing_V2_test_times, closing_V2_seq_mean);
    std::cout << "Mean sequential closing_V2 execution time : " << closing_V2_seq_mean << " sec" << std::endl;
    std::cout << "Total sequential closing_V2 execution time : " << closing_V2_seq_total << " sec" << std::endl;
    
    
    // PARALLEL PART
    //const int test_thread_array [16] = {1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};
    std::vector<int> test_thread = {1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};
    //std::vector<int> test_thread = {6,12};

    std::vector<double> erosion_V1_par_mean_vector;
    std::vector<double> dilation_V1_par_mean_vector;
    std::vector<double> opening_V1_par_mean_vector;
    std::vector<double> closing_V1_par_mean_vector;
    std::vector<double> erosion_V1_par_total_vector;
    std::vector<double> dilation_V1_par_total_vector;
    std::vector<double> opening_V1_par_total_vector;
    std::vector<double> closing_V1_par_total_vector;

    double erosion_V1_par_mean;
    double dilation_V1_par_mean;
    double opening_V1_par_mean;
    double closing_V1_par_mean;
    double erosion_V1_par_total;
    double dilation_V1_par_total;
    double opening_V1_par_total;
    double closing_V1_par_total;

    std::vector<double> erosion_V2_par_mean_vector;
    std::vector<double> dilation_V2_par_mean_vector;
    std::vector<double> opening_V2_par_mean_vector;
    std::vector<double> closing_V2_par_mean_vector;
    std::vector<double> erosion_V2_par_total_vector;
    std::vector<double> dilation_V2_par_total_vector;
    std::vector<double> opening_V2_par_total_vector;
    std::vector<double> closing_V2_par_total_vector;

    double erosion_V2_par_mean;
    double dilation_V2_par_mean;
    double opening_V2_par_mean;
    double closing_V2_par_mean;
    double erosion_V2_par_total;
    double dilation_V2_par_total;
    double opening_V2_par_total;
    double closing_V2_par_total;

    for(int i=0; i<test_thread.size(); i++) {
        int thread_num = test_thread[i];
        omp_set_num_threads(thread_num);

        std::cout << "\n----------- Num used threads " << omp_get_max_threads() << std::endl;
        //logfile << "NUM THREADS " <<  omp_get_max_threads() << std::endl;
        std::cout << "\nPARALLEL PART V1\n" << std::endl;

        erosion_V1_test_times.clear();
        dilation_V1_test_times.clear();
        opening_V1_test_times.clear();
        closing_V1_test_times.clear();

        for (auto &img : loadedImages) {
            // Estrazione del nome del file senza percorso
            std::string filename = std::filesystem::path(img.filename).filename().string();

            // Applicazione delle trasformazioni e salvataggio
            start_time_one_image = omp_get_wtime();
            STBImage eroded  = erosion_V1_parallel(img, se);
            end_time_one_image = omp_get_wtime();
            //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
            erosion_V1_test_times.push_back(end_time_one_image - start_time_one_image);
            eroded.saveImage("images/erosion/" + filename);
        }

        start_time_all_images = omp_get_wtime();
        erosion_V1_imgvec_parallel(loadedImages, se);
        end_time_all_images = omp_get_wtime();

        erosion_V1_par_total=end_time_all_images - start_time_all_images;
        calculateMeanTime(erosion_V1_test_times, erosion_V1_par_mean);
        erosion_V1_par_mean_vector.push_back(erosion_V1_par_mean);
        erosion_V1_par_total_vector.push_back(erosion_V1_par_total);
        std::cout << "Mean parallel erosion_V1 execution time : " << erosion_V1_par_mean << " sec" << std::endl;
        std::cout << "Total parallel erosion_V1 execution time : " << erosion_V1_par_total << " sec" << std::endl;

        for (auto &img : loadedImages) { 
            // Estrazione del nome del file senza percorso
            std::string filename = std::filesystem::path(img.filename).filename().string();
            
            // Applicazione delle trasformazioni e salvataggio
            start_time_one_image = omp_get_wtime();
            STBImage dilated = dilation_V1_parallel(img, se);
            end_time_one_image = omp_get_wtime();
            //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
            dilation_V1_test_times.push_back(end_time_one_image - start_time_one_image);
            dilated.saveImage("images/dilation/" + filename);
        }

        start_time_all_images = omp_get_wtime();
        dilation_V1_imgvec_parallel(loadedImages, se);
        end_time_all_images = omp_get_wtime();

        dilation_V1_par_total=end_time_all_images - start_time_all_images;
        calculateMeanTime(dilation_V1_test_times, dilation_V1_par_mean);
        dilation_V1_par_mean_vector.push_back(dilation_V1_par_mean);
        dilation_V1_par_total_vector.push_back(dilation_V1_par_total);
        std::cout << "Mean parallel dilation_V1 execution time : " << dilation_V1_par_mean << " sec" << std::endl;
        std::cout << "Total parallel dilation_V1 execution time : " << dilation_V1_par_total << " sec" << std::endl;
        
        for (auto &img : loadedImages) {
            // Estrazione del nome del file senza percorso
            std::string filename = std::filesystem::path(img.filename).filename().string();
            
            // Applicazione delle trasformazioni e salvataggio
            start_time_one_image = omp_get_wtime();
            STBImage opened  = opening_V1_parallel(img, se);
            end_time_one_image = omp_get_wtime();
            //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
            opening_V1_test_times.push_back(end_time_one_image - start_time_one_image);
            opened.saveImage("images/opening/" + filename);
        }

        start_time_all_images = omp_get_wtime();
        opening_V1_imgvec_parallel(loadedImages, se);
        end_time_all_images = omp_get_wtime();

        opening_V1_par_total=end_time_all_images - start_time_all_images;
        calculateMeanTime(opening_V1_test_times, opening_V1_par_mean);
        opening_V1_par_mean_vector.push_back(opening_V1_par_mean);
        opening_V1_par_total_vector.push_back(opening_V1_par_total);
        std::cout << "Mean parallel opening_V1 execution time : " << opening_V1_par_mean << " sec" << std::endl;
        std::cout << "Total parallel opening_V1 execution time : " << opening_V1_par_total << " sec" << std::endl;
        
        for (auto &img : loadedImages) {
            // Estrazione del nome del file senza percorso
            std::string filename = std::filesystem::path(img.filename).filename().string();
            
            // Applicazione delle trasformazioni e salvataggio
            start_time_one_image = omp_get_wtime();
            STBImage closed  = closing_V1_parallel(img, se);
            end_time_one_image = omp_get_wtime();
            //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
            closing_V1_test_times.push_back(end_time_one_image - start_time_one_image);
            closed.saveImage("images/closing/" + filename);
        }

        start_time_all_images = omp_get_wtime();
        closing_V1_imgvec_parallel(loadedImages, se);
        end_time_all_images = omp_get_wtime();

        closing_V1_par_total=end_time_all_images - start_time_all_images;
        calculateMeanTime(closing_V1_test_times, closing_V1_par_mean);
        closing_V1_par_mean_vector.push_back(closing_V1_par_mean);
        closing_V1_par_total_vector.push_back(closing_V1_par_total);
        std::cout << "Mean parallel closing_V1 execution time : " << closing_V1_par_mean << " sec" << std::endl;
        std::cout << "Total parallel closing_V1 execution time : " << closing_V1_par_total << " sec" << std::endl;

        // SEQUENTIAL PART V2
        erosion_V2_test_times.clear();
        dilation_V2_test_times.clear();
        opening_V2_test_times.clear();
        closing_V2_test_times.clear();

        std::cout << "\nPARALLEL PART V2\n" << std::endl;
        for (auto &img : loadedImages) {
            // Estrazione del nome del file senza percorso
            std::string filename = std::filesystem::path(img.filename).filename().string();

            // Applicazione delle trasformazioni e salvataggio
            start_time_one_image = omp_get_wtime();
            STBImage eroded  = erosion_V2_parallel(img, se);
            end_time_one_image = omp_get_wtime();
            //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
            erosion_V2_test_times.push_back(end_time_one_image - start_time_one_image);
            eroded.saveImage("images/erosion/" + filename);
        }

        start_time_all_images = omp_get_wtime();
        erosion_V2_imgvec_parallel(loadedImages, se);
        end_time_all_images = omp_get_wtime();

        erosion_V2_par_total=end_time_all_images - start_time_all_images;
        calculateMeanTime(erosion_V2_test_times, erosion_V2_par_mean);
        erosion_V2_par_mean_vector.push_back(erosion_V2_par_mean);
        erosion_V2_par_total_vector.push_back(erosion_V2_par_total);
        std::cout << "Mean parallel erosion_V2 execution time : " << erosion_V2_par_mean << " sec" << std::endl;
        std::cout << "Total parallel erosion_V2 execution time : " << erosion_V2_par_total << " sec" << std::endl;

        for (auto &img : loadedImages) { 
            // Estrazione del nome del file senza percorso
            std::string filename = std::filesystem::path(img.filename).filename().string();
            
            // Applicazione delle trasformazioni e salvataggio
            start_time_one_image = omp_get_wtime();
            STBImage dilated = dilation_V2_parallel(img, se);
            end_time_one_image = omp_get_wtime();
            //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
            dilation_V2_test_times.push_back(end_time_one_image - start_time_one_image);
            dilated.saveImage("images/dilation/" + filename);
        }

        start_time_all_images = omp_get_wtime();
        dilation_V2_imgvec_parallel(loadedImages, se);
        end_time_all_images = omp_get_wtime();

        dilation_V2_par_total=end_time_all_images - start_time_all_images;
        calculateMeanTime(dilation_V2_test_times, dilation_V2_par_mean);
        dilation_V2_par_mean_vector.push_back(dilation_V2_par_mean);
        dilation_V2_par_total_vector.push_back(dilation_V2_par_total);
        std::cout << "Mean parallel dilation_V2 execution time : " << dilation_V2_par_mean << " sec" << std::endl;
        std::cout << "Total parallel dilation_V2 execution time : " << dilation_V2_par_total << " sec" << std::endl;
        
        for (auto &img : loadedImages) {
            // Estrazione del nome del file senza percorso
            std::string filename = std::filesystem::path(img.filename).filename().string();
            
            // Applicazione delle trasformazioni e salvataggio
            start_time_one_image = omp_get_wtime();
            STBImage opened  = opening_V2_parallel(img, se);
            end_time_one_image = omp_get_wtime();
            //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
            opening_V2_test_times.push_back(end_time_one_image - start_time_one_image);
            opened.saveImage("images/opening/" + filename);
        }

        start_time_all_images = omp_get_wtime();
        opening_V2_imgvec_parallel(loadedImages, se);
        end_time_all_images = omp_get_wtime();

        opening_V2_par_total=end_time_all_images - start_time_all_images;
        calculateMeanTime(opening_V2_test_times, opening_V2_par_mean);
        opening_V2_par_mean_vector.push_back(opening_V2_par_mean);
        opening_V2_par_total_vector.push_back(opening_V2_par_total);
        std::cout << "Mean parallel opening_V2 execution time : " << opening_V2_par_mean << " sec" << std::endl;
        std::cout << "Total parallel opening_V2 execution time : " << opening_V2_par_total << " sec" << std::endl;
        
        for (auto &img : loadedImages) {
            // Estrazione del nome del file senza percorso
            std::string filename = std::filesystem::path(img.filename).filename().string();
            
            // Applicazione delle trasformazioni e salvataggio
            start_time_one_image = omp_get_wtime();
            STBImage closed  = closing_V2_parallel(img, se);
            end_time_one_image = omp_get_wtime();
            //std::cout << "Execution time : " << end_time_one_image - start_time_one_image << " sec" << std::endl;
            closing_V2_test_times.push_back(end_time_one_image - start_time_one_image);
            closed.saveImage("images/closing/" + filename);
        }

        start_time_all_images = omp_get_wtime();
        closing_V2_imgvec_parallel(loadedImages, se);
        end_time_all_images = omp_get_wtime();

        closing_V2_par_total=end_time_all_images - start_time_all_images;
        calculateMeanTime(closing_V2_test_times, closing_V2_par_mean);
        closing_V2_par_mean_vector.push_back(closing_V2_par_mean);
        closing_V2_par_total_vector.push_back(closing_V2_par_total);
        std::cout << "Mean parallel closing_V2 execution time : " << closing_V2_par_mean << " sec" << std::endl;
        std::cout << "Total parallel closing_V2 execution time : " << closing_V2_par_total << " sec" << std::endl;
    }

    std::vector<double> erosion_V1_mean_speedup;
    std::vector<double> dilation_V1_mean_speedup;
    std::vector<double> opening_V1_mean_speedup;
    std::vector<double> closing_V1_mean_speedup;
    std::vector<double> erosion_V2_mean_speedup;
    std::vector<double> dilation_V2_mean_speedup;
    std::vector<double> opening_V2_mean_speedup;
    std::vector<double> closing_V2_mean_speedup;
    for(int i=0; i<test_thread.size(); i++) {
        erosion_V1_mean_speedup.push_back(erosion_V1_seq_mean / erosion_V1_par_mean_vector[i]);
        dilation_V1_mean_speedup.push_back(dilation_V1_seq_mean / dilation_V1_par_mean_vector[i]);
        opening_V1_mean_speedup.push_back(opening_V1_seq_mean / opening_V1_par_mean_vector[i]);
        closing_V1_mean_speedup.push_back(closing_V1_seq_mean / closing_V1_par_mean_vector[i]);
        erosion_V2_mean_speedup.push_back(erosion_V2_seq_mean / erosion_V2_par_mean_vector[i]);
        dilation_V2_mean_speedup.push_back(dilation_V2_seq_mean / dilation_V2_par_mean_vector[i]);
        opening_V2_mean_speedup.push_back(opening_V2_seq_mean / opening_V2_par_mean_vector[i]);
        closing_V2_mean_speedup.push_back(closing_V2_seq_mean / closing_V2_par_mean_vector[i]);
    }

    std::vector<double> erosion_V1_total_speedup;
    std::vector<double> dilation_V1_total_speedup;
    std::vector<double> opening_V1_total_speedup;
    std::vector<double> closing_V1_total_speedup;
    std::vector<double> erosion_V2_total_speedup;
    std::vector<double> dilation_V2_total_speedup;
    std::vector<double> opening_V2_total_speedup;
    std::vector<double> closing_V2_total_speedup;
    for(int i=0; i<test_thread.size(); i++) {
        erosion_V1_total_speedup.push_back(erosion_V1_seq_total / erosion_V1_par_total_vector[i]);
        dilation_V1_total_speedup.push_back(dilation_V1_seq_total / dilation_V1_par_total_vector[i]);
        opening_V1_total_speedup.push_back(opening_V1_seq_total / opening_V1_par_total_vector[i]);
        closing_V1_total_speedup.push_back(closing_V1_seq_total / closing_V1_par_total_vector[i]);
        erosion_V2_total_speedup.push_back(erosion_V2_seq_total / erosion_V2_par_total_vector[i]);
        dilation_V2_total_speedup.push_back(dilation_V2_seq_total / dilation_V2_par_total_vector[i]);
        opening_V2_total_speedup.push_back(opening_V2_seq_total / opening_V2_par_total_vector[i]);
        closing_V2_total_speedup.push_back(closing_V2_seq_total / closing_V2_par_total_vector[i]);
    }
    
    std::ofstream logfile("speedup_log.txt", std::ofstream::trunc);
    //std::ofstream csvfile("speedup_results.csv", std::ofstream::trunc);

    // Tabella per V1
    std::cout << "\n=== Speedup Table V1 ===\n" << std::endl;
    logfile << "\n=== Speedup Table V1 ===\n" << std::endl;

    std::cout << std::left << std::setw(10) << "Threads"
            << std::setw(10) << "E_Mean"
            << std::setw(10) << "D_Mean"
            << std::setw(10) << "O_Mean"
            << std::setw(10) << "C_Mean"
            << std::setw(10) << "E_Total"
            << std::setw(10) << "D_Total"
            << std::setw(10) << "O_Total"
            << std::setw(10) << "C_Total" << std::endl;

    logfile << std::left << std::setw(10) << "Threads"
            << std::setw(10) << "E_Mean"
            << std::setw(10) << "D_Mean"
            << std::setw(10) << "O_Mean"
            << std::setw(10) << "C_Mean"
            << std::setw(10) << "E_Total"
            << std::setw(10) << "D_Total"
            << std::setw(10) << "O_Total"
            << std::setw(10) << "C_Total" << std::endl;

    for (int i = 0; i < test_thread.size(); i++) {
        std::cout << std::fixed << std::setprecision(2)
                << std::left << std::setw(10) << test_thread[i]
                << std::setw(10) << erosion_V1_mean_speedup[i]
                << std::setw(10) << dilation_V1_mean_speedup[i]
                << std::setw(10) << opening_V1_mean_speedup[i]
                << std::setw(10) << closing_V1_mean_speedup[i]
                << std::setw(10) << erosion_V1_total_speedup[i]
                << std::setw(10) << dilation_V1_total_speedup[i]
                << std::setw(10) << opening_V1_total_speedup[i]
                << std::setw(10) << closing_V1_total_speedup[i] << std::endl;

        logfile << std::fixed << std::setprecision(2)
                << std::left << std::setw(10) << test_thread[i]
                << std::setw(10) << erosion_V1_mean_speedup[i]
                << std::setw(10) << dilation_V1_mean_speedup[i]
                << std::setw(10) << opening_V1_mean_speedup[i]
                << std::setw(10) << closing_V1_mean_speedup[i]
                << std::setw(10) << erosion_V1_total_speedup[i]
                << std::setw(10) << dilation_V1_total_speedup[i]
                << std::setw(10) << opening_V1_total_speedup[i]
                << std::setw(10) << closing_V1_total_speedup[i] << std::endl;
    }

    // Tabella per V2
    std::cout << "\n=== Speedup Table V2 ===\n" << std::endl;
    logfile << "\n=== Speedup Table V2 ===\n" << std::endl;

    std::cout << std::left << std::setw(10) << "Threads"
            << std::setw(10) << "E_Mean"
            << std::setw(10) << "D_Mean"
            << std::setw(10) << "O_Mean"
            << std::setw(10) << "C_Mean"
            << std::setw(10) << "E_Total"
            << std::setw(10) << "D_Total"
            << std::setw(10) << "O_Total"
            << std::setw(10) << "C_Total" << std::endl;

    logfile << std::left << std::setw(10) << "Threads"
            << std::setw(10) << "E_Mean"
            << std::setw(10) << "D_Mean"
            << std::setw(10) << "O_Mean"
            << std::setw(10) << "C_Mean"
            << std::setw(10) << "E_Total"
            << std::setw(10) << "D_Total"
            << std::setw(10) << "O_Total"
            << std::setw(10) << "C_Total" << std::endl;

    for (int i = 0; i < test_thread.size(); i++) {
        std::cout << std::fixed << std::setprecision(2)
                << std::left << std::setw(10) << test_thread[i]
                << std::setw(10) << erosion_V2_mean_speedup[i]
                << std::setw(10) << dilation_V2_mean_speedup[i]
                << std::setw(10) << opening_V2_mean_speedup[i]
                << std::setw(10) << closing_V2_mean_speedup[i]
                << std::setw(10) << erosion_V2_total_speedup[i]
                << std::setw(10) << dilation_V2_total_speedup[i]
                << std::setw(10) << opening_V2_total_speedup[i]
                << std::setw(10) << closing_V2_total_speedup[i] << std::endl;

        logfile << std::fixed << std::setprecision(2)
                << std::left << std::setw(10) << test_thread[i]
                << std::setw(10) << erosion_V2_mean_speedup[i]
                << std::setw(10) << dilation_V2_mean_speedup[i]
                << std::setw(10) << opening_V2_mean_speedup[i]
                << std::setw(10) << closing_V2_mean_speedup[i]
                << std::setw(10) << erosion_V2_total_speedup[i]
                << std::setw(10) << dilation_V2_total_speedup[i]
                << std::setw(10) << opening_V2_total_speedup[i]
                << std::setw(10) << closing_V2_total_speedup[i] << std::endl;
    }
    /*
    std::cout << "\n=== Speedup Table ===\n" << std::endl;
    logfile << "\n=== Speedup Table ===\n" << std::endl;

    std::cout << std::left << std::setw(10) << "Threads"
            << std::setw(10) << "E_Mean"
            << std::setw(10) << "D_Mean"
            << std::setw(10) << "O_Mean"
            << std::setw(10) << "C_Mean"
            << std::setw(10) << "E_Total"
            << std::setw(10) << "D_Total"
            << std::setw(10) << "O_Total"
            << std::setw(10) << "C_Total" << std::endl;

    if (logfile.is_open()) {
        logfile << std::left << std::setw(10) << "Threads"
                << std::setw(10) << "E_Mean"
                << std::setw(10) << "D_Mean"
                << std::setw(10) << "O_Mean"
                << std::setw(10) << "C_Mean"
                << std::setw(10) << "E_Total"
                << std::setw(10) << "D_Total"
                << std::setw(10) << "O_Total"
                << std::setw(10) << "C_Total" << std::endl;
    } else {
        std::cerr << "Error opening the log file!" << std::endl;
    }

    if (csvfile.is_open()) {
        csvfile << "Threads,E_Mean,D_Mean,O_Mean,C_Mean,E_Total,D_Total,O_Total,C_Total\n";
    } else {
        std::cerr << "Error opening the CSV file!" << std::endl;
    }

    for (int i = 0; i < test_thread.size(); i++) {
        std::cout << std::fixed << std::setprecision(2)
                << std::left << std::setw(10) << test_thread[i]
                << std::setw(10) << erosion_mean_speedup[i]
                << std::setw(10) << dilation_mean_speedup[i]
                << std::setw(10) << opening_mean_speedup[i]
                << std::setw(10) << closing_mean_speedup[i]
                << std::setw(10) << erosion_total_speedup[i]
                << std::setw(10) << dilation_total_speedup[i]
                << std::setw(10) << opening_total_speedup[i]
                << std::setw(10) << closing_total_speedup[i] << std::endl;

        if (logfile.is_open()) {
            logfile << std::fixed << std::setprecision(2)
                    << std::left << std::setw(10) << test_thread[i]
                    << std::setw(10) << erosion_mean_speedup[i]
                    << std::setw(10) << dilation_mean_speedup[i]
                    << std::setw(10) << opening_mean_speedup[i]
                    << std::setw(10) << closing_mean_speedup[i]
                    << std::setw(10) << erosion_total_speedup[i]
                    << std::setw(10) << dilation_total_speedup[i]
                    << std::setw(10) << opening_total_speedup[i]
                    << std::setw(10) << closing_total_speedup[i] << std::endl;
        }

        if (csvfile.is_open()) {
            csvfile << test_thread[i] << ","
                    << std::fixed << std::setprecision(2) << erosion_mean_speedup[i] << ","
                    << dilation_mean_speedup[i] << ","
                    << opening_mean_speedup[i] << ","
                    << closing_mean_speedup[i] << ","
                    << erosion_total_speedup[i] << ","
                    << dilation_total_speedup[i] << ","
                    << opening_total_speedup[i] << ","
                    << closing_total_speedup[i] << "\n";
        }
    }

    std::cout << "\nLegend:\n"
            << "E_Mean  - Erosion Mean Speedup\n"
            << "D_Mean  - Dilation Mean Speedup\n"
            << "O_Mean  - Opening Mean Speedup\n"
            << "C_Mean  - Closing Mean Speedup\n"
            << "E_Total - Erosion Total Speedup\n"
            << "D_Total - Dilation Total Speedup\n"
            << "O_Total - Opening Total Speedup\n"
            << "C_Total - Closing Total Speedup\n";

    if (logfile.is_open()) {
        logfile << "\nLegend:\n"
                << "E_Mean  - Erosion Mean Speedup\n"
                << "D_Mean  - Dilation Mean Speedup\n"
                << "O_Mean  - Opening Mean Speedup\n"
                << "C_Mean  - Closing Mean Speedup\n"
                << "E_Total - Erosion Total Speedup\n"
                << "D_Total - Dilation Total Speedup\n"
                << "O_Total - Opening Total Speedup\n"
                << "C_Total - Closing Total Speedup\n";
    }

    logfile.close();
    csvfile.close();
    */
    logfile.close();
    // Libera memoria delle immagini caricate
    for (auto &img : loadedImages) {
        stbi_image_free(img.image_data);
    }
    return 0;
}
