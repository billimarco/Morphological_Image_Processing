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
const json CONFIG = json::parse(std::ifstream("settings/config.json"));
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
    void initializeBinary(int w, int h, int color=CONFIG["background_color"]) {
        width = w;
        height = h;
        channels = 1; // Immagine binaria con 1 canale
        image_data = (uint8_t*)malloc(width * height * channels);

        // Inizializza l'immagine a nera (tutti i pixel sono 0)
        for (int i = 0; i < width * height * channels; i++) {
            image_data[i] = color; // 0 = nero, 255 = bianco
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
std::vector<STBImage> loadImagesFromDirectory(const std::string& directory) {
    std::vector<STBImage> images;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().string();
            STBImage img;
            if (img.loadImage(filename)) {
                images.push_back(img);
            }
        }
    }
    return images;
}

// Disegna un rettangolo pieno
void drawRectangle(STBImage &img, int x, int y, int w, int h, int color) {
    for (int i = y; i < y + h; i++)
        for (int j = x; j < x + w; j++)
            if (i >= 0 && i < img.height && j >= 0 && j < img.width)
                img.image_data[i * img.width + j] = color;
}

// Disegna una cornice rettangolare (rettangolo con buco)
void drawHollowRectangle(STBImage &img, int x, int y, int w, int h, int thickness, int color) {
    drawRectangle(img, x, y, w, thickness, color);
    drawRectangle(img, x, y + h - thickness, w, thickness, color);
    drawRectangle(img, x, y, thickness, h, color);
    drawRectangle(img, x + w - thickness, y, thickness, h, color);
}

// Disegna un cerchio pieno
void drawCircle(STBImage &img, int cx, int cy, int radius, int color) {
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++)
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= radius * radius)
                img.image_data[y * img.width + x] = color;
}

// Disegna un anello (cerchio con buco)
void drawHollowCircle(STBImage &img, int cx, int cy, int outerRadius, int innerRadius, int color) {
    for (int y = 0; y < img.height; y++)
        for (int x = 0; x < img.width; x++) {
            int distSq = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (distSq <= outerRadius * outerRadius && distSq >= innerRadius * innerRadius)
                img.image_data[y * img.width + x] = color;
        }
}

// Disegna una linea
void drawLine(STBImage &img, int x1, int y1, int x2, int y2, int color) {
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1, err = dx - dy;

    while (true) {
        if (x1 >= 0 && x1 < img.width && y1 >= 0 && y1 < img.height)
            img.image_data[y1 * img.width + x1] = color;
        if (x1 == x2 && y1 == y2) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

// Funzione per generare immagini binarie con forme casuali
void generateBinaryImages(int numImages, int width=256, int height=256) {
    srand(time(0)); // Inizializza il generatore di numeri casuali
    int color = CONFIG["foreground_color"]; // 255 = bianco, 0 = nero
    int shape_per_image = CONFIG["shape_per_image"];
    int numShapes = rand() % shape_per_image + 1;  // Può generare da 1 a 3 forme

    for (int i = 1; i <= numImages; i++) {
        STBImage img;
        img.initializeBinary(width, height);
        for (int j = 0; j < numShapes; j++) {
            int shapeType = rand() % 5; // 0: Rettangolo, 1: Cornice rettangolare, 2: Cerchio, 3: Anello, 4: Linea

            if (shapeType == 0) {
                int x = rand() % (img.width - 50);
                int y = rand() % (img.height - 50);
                int w = rand() % (img.width - x);
                int h = rand() % (img.height - y);
                drawRectangle(img, x, y, w, h, color);
            } 
            else if (shapeType == 1) {
                int x = rand() % (img.width - 100);
                int y = rand() % (img.height - 100);
                int w = rand() % 100 + 40;
                int h = rand() % 100 + 40;
                int thickness = 10;
                drawHollowRectangle(img, x, y, w, h, thickness, color);
            } 
            else if (shapeType == 2) {
                int cx = rand() % (img.width - 50);
                int cy = rand() % (img.height - 50);
                int radius = rand() % 50 + 10;
                drawCircle(img, cx, cy, radius, color);
            } 
            else if (shapeType == 3) {
                int cx = rand() % (img.width - 50);
                int cy = rand() % (img.height - 50);
                int outerRadius = rand() % 50 + 30;
                int innerRadius = rand() % (outerRadius - 10) + 10;
                drawHollowCircle(img, cx, cy, outerRadius, innerRadius, color);
            } 
            else {
                int x1 = rand() % img.width;
                int y1 = rand() % img.height;
                int x2 = rand() % img.width;
                int y2 = rand() % img.height;
                drawLine(img, x1, y1, x2, y2, color);
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
    result.initializeBinary(img.width, img.height);

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
    result.initializeBinary(img.width, img.height);

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
    result.initializeBinary(img.width, img.height);

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
    result.initializeBinary(img.width, img.height);

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
    half_result.initializeBinary(img.width, img.height);
    result.initializeBinary(img.width, img.height);

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

// Funzione per eseguire la chiusura ottimizzata (Dilatazione seguita da Erosione)
STBImage closing_V2(const STBImage& img, const StructuringElement& se) {
    STBImage half_result;
    STBImage result;
    half_result.initializeBinary(img.width, img.height);
    result.initializeBinary(img.width, img.height);

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
        result.initializeBinary(img.width, img.height);

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
        result.initializeBinary(img.width, img.height);

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
        half_result.initializeBinary(img.width, img.height);
        result.initializeBinary(img.width, img.height);

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
        half_result.initializeBinary(img.width, img.height);
        result.initializeBinary(img.width, img.height);

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






// FUNZIONI OPERAZIONI MORFOLOGICHE IN MODO PARALLELO

// Funzione per eseguire l'erosione in parallelo
STBImage erosion_V1_parallel(const STBImage& img, const StructuringElement& se) {
    STBImage result;
    result.initializeBinary(img.width, img.height);

    #pragma omp parallel for collapse(2) schedule(dynamic) shared(result,img,se,CONFIG) default(none)
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
    result.initializeBinary(img.width, img.height);

    #pragma omp parallel for collapse(2) schedule(dynamic) shared(result,img,se,CONFIG) default(none)
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
    half_result.initializeBinary(img.width, img.height);
    result.initializeBinary(img.width, img.height);

    #pragma omp parallel shared(result,half_result,img,se,CONFIG) default(none)
    {
        #pragma omp for collapse(2) schedule(dynamic)
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

        #pragma omp for collapse(2) schedule(dynamic)
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
    half_result.initializeBinary(img.width, img.height);
    result.initializeBinary(img.width, img.height);

    #pragma omp parallel shared(result,half_result,img,se,CONFIG) default(none)
    {
        #pragma omp for collapse(2) schedule(dynamic)
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

        #pragma omp for collapse(2) schedule(dynamic)
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
    #pragma omp parallel for schedule(dynamic) shared(imgs_results,imgs,se,CONFIG) default(none)
    for (auto &img : imgs) { 
        STBImage result;
        result.initializeBinary(img.width, img.height);

        #pragma omp parallel for collapse(2) schedule(dynamic) shared(result,img,se,CONFIG) default(none)
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
    #pragma omp parallel for schedule(dynamic) shared(imgs_results,imgs,se,CONFIG) default(none)
    for (auto &img : imgs) {
        STBImage result;
        result.initializeBinary(img.width, img.height);

        #pragma omp parallel for collapse(2) schedule(dynamic) shared(result,img,se,CONFIG) default(none)
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
    #pragma omp parallel for schedule(dynamic) shared(imgs_results,imgs,se,CONFIG) default(none)
    for (auto &img : imgs) {
        STBImage half_result;
        STBImage result;
        half_result.initializeBinary(img.width, img.height);
        result.initializeBinary(img.width, img.height);

        #pragma omp parallel shared(result,half_result,img,se,CONFIG) default(none)
        {
            #pragma omp for collapse(2) schedule(dynamic)
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
    #pragma omp parallel for schedule(dynamic) shared(imgs_results,imgs,se,CONFIG) default(none)
    for (auto &img : imgs) {
        STBImage half_result;
        STBImage result;
        half_result.initializeBinary(img.width, img.height);
        result.initializeBinary(img.width, img.height);
    
        #pragma omp parallel shared(result,half_result,img,se,CONFIG) default(none)
        {
            #pragma omp for collapse(2) schedule(dynamic)
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
    result.initializeBinary(img.width, img.height);

    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    #pragma omp parallel for collapse(2) schedule(dynamic) shared(result,active_pixels,img) default(none)
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
    result.initializeBinary(img.width, img.height);

    std::vector<std::pair<int, int>> active_pixels;
    for (int i = 0; i < se.height; i++) {
        for (int j = 0; j < se.width; j++) {
            if (se.kernel[i][j] == 1) {
                active_pixels.emplace_back(i - se.anchor_y, j - se.anchor_x);
            }
        }
    }

    #pragma omp parallel for collapse(2) schedule(dynamic) shared(result,active_pixels,img) default(none)
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
    half_result.initializeBinary(img.width, img.height);
    result.initializeBinary(img.width, img.height);

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
        #pragma omp for collapse(2) schedule(dynamic)
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

// Funzione per eseguire la chiusura ottimizzata in parallelo (Dilatazione seguita da Erosione)
STBImage closing_V2_parallel(const STBImage& img, const StructuringElement& se) {
    STBImage half_result;
    STBImage result;
    half_result.initializeBinary(img.width, img.height);
    result.initializeBinary(img.width, img.height);

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
        #pragma omp for collapse(2) schedule(dynamic)
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

    #pragma omp parallel for schedule(dynamic) shared(imgs_results,imgs,active_pixels,CONFIG) default(none)
    for (auto &img : imgs) { 
        STBImage result;
        result.initializeBinary(img.width, img.height);

        #pragma omp parallel for collapse(2) schedule(dynamic) shared(result,active_pixels,img,CONFIG) default(none)
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

    #pragma omp parallel for schedule(dynamic) shared(imgs_results,imgs,active_pixels,CONFIG) default(none)
    for (auto &img : imgs) {
        STBImage result;
        result.initializeBinary(img.width, img.height);

        #pragma omp parallel for collapse(2) schedule(dynamic) shared(result,active_pixels,img,CONFIG) default(none)
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

    #pragma omp parallel for schedule(dynamic) shared(imgs_results,imgs,active_pixels,CONFIG) default(none)
    for (auto &img : imgs) {
        STBImage half_result;
        STBImage result;
        half_result.initializeBinary(img.width, img.height);
        result.initializeBinary(img.width, img.height);

        #pragma omp parallel shared(result,half_result,active_pixels,img,CONFIG) default(none)
        {
            #pragma omp for collapse(2) schedule(dynamic)
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

            #pragma omp for collapse(2) schedule(dynamic)
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

    #pragma omp parallel for schedule(dynamic) shared(imgs_results,imgs,active_pixels,CONFIG) default(none)
    for (auto &img : imgs) {
        STBImage half_result;
        STBImage result;
        half_result.initializeBinary(img.width, img.height);
        result.initializeBinary(img.width, img.height);

        #pragma omp parallel shared(result,half_result,active_pixels,img,CONFIG) default(none)
        {   
            #pragma omp for collapse(2) schedule(dynamic)
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

            #pragma omp for collapse(2) schedule(dynamic)
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




// Funzione per testare le funzioni di morfologia matematica ed ottenere i tempi di esecuzione
void testProcessImages(const std::vector<STBImage>& loadedImages, 
    const StructuringElement& se, 
    const std::string& operation, 
    const std::string& mode, 
    double& mean_time, 
    double& total_time) {
    auto operationFunc = [&](const STBImage& img) -> STBImage {
        if (operation == "erosion" && mode == "V1") return erosion_V1(img, se);
        if (operation == "dilation" && mode == "V1") return dilation_V1(img, se);
        if (operation == "opening" && mode == "V1") return opening_V1(img, se);
        if (operation == "closing" && mode == "V1") return closing_V1(img, se);
        if (operation == "erosion" && mode == "V2") return erosion_V2(img, se);
        if (operation == "dilation" && mode == "V2") return dilation_V2(img, se);
        if (operation == "opening" && mode == "V2") return opening_V2(img, se);
        if (operation == "closing" && mode == "V2") return closing_V2(img, se);
        if (operation == "erosion" && mode == "V1_parallel") return erosion_V1_parallel(img, se);
        if (operation == "dilation" && mode == "V1_parallel") return dilation_V1_parallel(img, se);
        if (operation == "opening" && mode == "V1_parallel") return opening_V1_parallel(img, se);
        if (operation == "closing" && mode == "V1_parallel") return closing_V1_parallel(img, se);
        if (operation == "erosion" && mode == "V2_parallel") return erosion_V2_parallel(img, se);
        if (operation == "dilation" && mode == "V2_parallel") return dilation_V2_parallel(img, se);
        if (operation == "opening" && mode == "V2_parallel") return opening_V2_parallel(img, se);
        if (operation == "closing" && mode == "V2_parallel") return closing_V2_parallel(img, se);
        throw std::invalid_argument("Invalid operation or mode");
    };

    auto operationImgVecFunc = [&]() -> std::unordered_map<std::string, STBImage> {
        if (operation == "erosion" && mode == "V1") return erosion_V1_imgvec(loadedImages, se);
        if (operation == "dilation" && mode == "V1") return dilation_V1_imgvec(loadedImages, se);
        if (operation == "opening" && mode == "V1") return opening_V1_imgvec(loadedImages, se);
        if (operation == "closing" && mode == "V1") return closing_V1_imgvec(loadedImages, se);
        if (operation == "erosion" && mode == "V2") return erosion_V2_imgvec(loadedImages, se);
        if (operation == "dilation" && mode == "V2") return dilation_V2_imgvec(loadedImages, se);
        if (operation == "opening" && mode == "V2") return opening_V2_imgvec(loadedImages, se);
        if (operation == "closing" && mode == "V2") return closing_V2_imgvec(loadedImages, se);
        if (operation == "erosion" && mode == "V1_parallel") return erosion_V1_imgvec_parallel(loadedImages, se);
        if (operation == "dilation" && mode == "V1_parallel") return dilation_V1_imgvec_parallel(loadedImages, se);
        if (operation == "opening" && mode == "V1_parallel") return opening_V1_imgvec_parallel(loadedImages, se);
        if (operation == "closing" && mode == "V1_parallel") return closing_V1_imgvec_parallel(loadedImages, se);
        if (operation == "erosion" && mode == "V2_parallel") return erosion_V2_imgvec_parallel(loadedImages, se);
        if (operation == "dilation" && mode == "V2_parallel") return dilation_V2_imgvec_parallel(loadedImages, se);
        if (operation == "opening" && mode == "V2_parallel") return opening_V2_imgvec_parallel(loadedImages, se);
        if (operation == "closing" && mode == "V2_parallel") return closing_V2_imgvec_parallel(loadedImages, se);
        throw std::invalid_argument("Invalid operation or mode");
    };

    auto calculateMeanTime = [](const std::vector<double> &test_times, double &mean_time) {
        double sum = std::accumulate(test_times.begin(), test_times.end(), 0.0);
        mean_time = sum / test_times.size();
    };

    std::string outputDir = "images/" + operation + mode +"/";
    double start_time_one_image, end_time_one_image;
    std::vector<double> test_times;

    for (auto& img : loadedImages) {
        std::string filename = std::filesystem::path(img.filename).filename().string();
        start_time_one_image = omp_get_wtime();
        STBImage result = operationFunc(img);
        end_time_one_image = omp_get_wtime();
        test_times.push_back(end_time_one_image - start_time_one_image);
        result.saveImage(outputDir + filename);
    }

    double start_time_all_images = omp_get_wtime();
    operationImgVecFunc();
    double end_time_all_images = omp_get_wtime();

    total_time = end_time_all_images - start_time_all_images;
    calculateMeanTime(test_times, mean_time);

    std::cout << "Mean " << mode << " " << operation << " execution time: " << mean_time << " sec" << std::endl;
    std::cout << "Total " << mode << " " << operation << " execution time: " << total_time << " sec" << std::endl;
}

std::string format_double(double value, int precision = 3) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

int main(){
    #ifdef _OPENMP
        std::cout << "_OPENMP defined" << std::endl;
    #endif
    createPath("images/basis");
    createPath("images/erosionV1");
    createPath("images/dilationV1");
    createPath("images/openingV1");
    createPath("images/closingV1");
    createPath("images/erosionV2");
    createPath("images/dilationV2");
    createPath("images/openingV2");
    createPath("images/closingV2");

    int width = CONFIG["image_size"]["width"], height = CONFIG["image_size"]["height"], num_images = CONFIG["num_images"];
    
    generateBinaryImages(num_images, width, height);
    std::cout << num_images <<" immagini " << width << "x" << height << " generate con successo!" << std::endl;

    std::vector<STBImage> loadedImages = loadImagesFromDirectory("images/basis");
    std::cout << "Totale immagini caricate: " << loadedImages.size() << std::endl;

    StructuringElement se;
    if(CONFIG["structuring_element"]["shape"] == "square"){
        se.setKernel(generateSquareKernel(CONFIG["structuring_element"]["radius"]));
        se.print();
        se.saveImage("se.jpg");
    } else if(CONFIG["structuring_element"]["shape"] == "circle"){
        se.setKernel(generateCircularKernel(CONFIG["structuring_element"]["radius"]));
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
    //sequential variables
    double erosion_V1_seq_mean;
    double dilation_V1_seq_mean;
    double opening_V1_seq_mean;
    double closing_V1_seq_mean;
    double erosion_V1_seq_total;
    double dilation_V1_seq_total;
    double opening_V1_seq_total;
    double closing_V1_seq_total;

    double erosion_V2_seq_mean;
    double dilation_V2_seq_mean;
    double opening_V2_seq_mean;
    double closing_V2_seq_mean;
    double erosion_V2_seq_total;
    double dilation_V2_seq_total;
    double opening_V2_seq_total;
    double closing_V2_seq_total;

    std::cout << "\nSEQUENTIAL PART V1\n" << std::endl;
    testProcessImages(loadedImages, se, "erosion", "V1", erosion_V1_seq_mean, erosion_V1_seq_total);
    testProcessImages(loadedImages, se, "dilation", "V1", dilation_V1_seq_mean, dilation_V1_seq_total);
    testProcessImages(loadedImages, se, "opening", "V1", opening_V1_seq_mean, opening_V1_seq_total);
    testProcessImages(loadedImages, se, "closing", "V1", closing_V1_seq_mean, closing_V1_seq_total);

    // Sequential V2
    std::cout << "\nSEQUENTIAL PART V2\n" << std::endl;
    testProcessImages(loadedImages, se, "erosion", "V2", erosion_V2_seq_mean, erosion_V2_seq_total);
    testProcessImages(loadedImages, se, "dilation", "V2", dilation_V2_seq_mean, dilation_V2_seq_total);
    testProcessImages(loadedImages, se, "opening", "V2", opening_V2_seq_mean, opening_V2_seq_total);
    testProcessImages(loadedImages, se, "closing", "V2", closing_V2_seq_mean, closing_V2_seq_total);

    //parallel variables
    std::vector<int> test_thread = {1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};
    std::vector<double> erosion_V1_par_mean_vector;
    std::vector<double> dilation_V1_par_mean_vector;
    std::vector<double> opening_V1_par_mean_vector;
    std::vector<double> closing_V1_par_mean_vector;
    std::vector<double> erosion_V1_par_total_vector;
    std::vector<double> dilation_V1_par_total_vector;
    std::vector<double> opening_V1_par_total_vector;
    std::vector<double> closing_V1_par_total_vector;

    std::vector<double> erosion_V2_par_mean_vector;
    std::vector<double> dilation_V2_par_mean_vector;
    std::vector<double> opening_V2_par_mean_vector;
    std::vector<double> closing_V2_par_mean_vector;
    std::vector<double> erosion_V2_par_total_vector;
    std::vector<double> dilation_V2_par_total_vector;
    std::vector<double> opening_V2_par_total_vector;
    std::vector<double> closing_V2_par_total_vector;

    double erosion_V1_par_mean;
    double dilation_V1_par_mean;
    double opening_V1_par_mean;
    double closing_V1_par_mean;
    double erosion_V1_par_total;
    double dilation_V1_par_total;
    double opening_V1_par_total;
    double closing_V1_par_total;

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
        // Parallel V1
        std::cout << "\nPARALLEL PART V1\n" << std::endl;
        testProcessImages(loadedImages, se, "erosion", "V1_parallel", erosion_V1_par_mean, erosion_V1_par_total);
        testProcessImages(loadedImages, se, "dilation", "V1_parallel", dilation_V1_par_mean, dilation_V1_par_total);
        testProcessImages(loadedImages, se, "opening", "V1_parallel", opening_V1_par_mean, opening_V1_par_total);
        testProcessImages(loadedImages, se, "closing", "V1_parallel", closing_V1_par_mean, closing_V1_par_total);

        erosion_V1_par_mean_vector.push_back(erosion_V1_par_mean);
        erosion_V1_par_total_vector.push_back(erosion_V1_par_total);
        dilation_V1_par_mean_vector.push_back(dilation_V1_par_mean);
        dilation_V1_par_total_vector.push_back(dilation_V1_par_total);
        opening_V1_par_mean_vector.push_back(opening_V1_par_mean);
        opening_V1_par_total_vector.push_back(opening_V1_par_total);
        closing_V1_par_mean_vector.push_back(closing_V1_par_mean);
        closing_V1_par_total_vector.push_back(closing_V1_par_total);

        // Parallel V2
        std::cout << "\nPARALLEL PART V2\n" << std::endl;
        testProcessImages(loadedImages, se, "erosion", "V2_parallel", erosion_V2_par_mean, erosion_V2_par_total);
        testProcessImages(loadedImages, se, "dilation", "V2_parallel", dilation_V2_par_mean, dilation_V2_par_total);
        testProcessImages(loadedImages, se, "opening", "V2_parallel", opening_V2_par_mean, opening_V2_par_total);
        testProcessImages(loadedImages, se, "closing", "V2_parallel", closing_V2_par_mean, closing_V2_par_total);

        erosion_V2_par_mean_vector.push_back(erosion_V2_par_mean);
        erosion_V2_par_total_vector.push_back(erosion_V2_par_total);
        dilation_V2_par_mean_vector.push_back(dilation_V2_par_mean);
        dilation_V2_par_total_vector.push_back(dilation_V2_par_total);
        opening_V2_par_mean_vector.push_back(opening_V2_par_mean);
        opening_V2_par_total_vector.push_back(opening_V2_par_total);
        closing_V2_par_mean_vector.push_back(closing_V2_par_mean);
        closing_V2_par_total_vector.push_back(closing_V2_par_total);
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
            << std::setw(25) << "E_Mean"
            << std::setw(25) << "D_Mean"
            << std::setw(25) << "O_Mean"
            << std::setw(25) << "C_Mean"
            << std::setw(25) << "E_Total"
            << std::setw(25) << "D_Total"
            << std::setw(25) << "O_Total"
            << std::setw(25) << "C_Total" << std::endl;

    logfile << std::left << std::setw(10) << "Threads"
            << std::setw(25) << "E_Mean"
            << std::setw(25) << "D_Mean"
            << std::setw(25) << "O_Mean"
            << std::setw(25) << "C_Mean"
            << std::setw(25) << "E_Total"
            << std::setw(25) << "D_Total"
            << std::setw(25) << "O_Total"
            << std::setw(25) << "C_Total" << std::endl;

    for (int i = 0; i < test_thread.size(); i++) {
        std::cout << std::fixed << std::setprecision(3)
                << std::left << std::setw(10) << test_thread[i]
                << std::setw(25) << (format_double(erosion_V1_mean_speedup[i]) + " [" + format_double(erosion_V1_seq_mean) + "/" + format_double(erosion_V1_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(dilation_V1_mean_speedup[i]) + " [" + format_double(dilation_V1_seq_mean) + "/" + format_double(dilation_V1_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(opening_V1_mean_speedup[i]) + " [" + format_double(opening_V1_seq_mean) + "/" + format_double(opening_V1_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(closing_V1_mean_speedup[i]) + " [" + format_double(closing_V1_seq_mean) + "/" + format_double(closing_V1_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(erosion_V1_total_speedup[i]) + " [" + format_double(erosion_V1_seq_total) + "/" + format_double(erosion_V1_par_total_vector[i]) + "]s")
                << std::setw(25) << (format_double(dilation_V1_total_speedup[i]) + " [" + format_double(dilation_V1_seq_total) + "/" + format_double(dilation_V1_par_total_vector[i]) + "]s")
                << std::setw(25) << (format_double(opening_V1_total_speedup[i]) + " [" + format_double(opening_V1_seq_total) + "/" + format_double(opening_V1_par_total_vector[i]) + "]s")
                << std::setw(25) << (format_double(closing_V1_total_speedup[i]) + " [" + format_double(closing_V1_seq_total) + "/" + format_double(closing_V1_par_total_vector[i]) + "]s")
                << std::endl;
    
        logfile << std::fixed << std::setprecision(3)
                << std::left << std::setw(10) << test_thread[i]
                << std::setw(25) << (format_double(erosion_V1_mean_speedup[i]) + " [" + format_double(erosion_V1_seq_mean) + "/" + format_double(erosion_V1_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(dilation_V1_mean_speedup[i]) + " [" + format_double(dilation_V1_seq_mean) + "/" + format_double(dilation_V1_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(opening_V1_mean_speedup[i]) + " [" + format_double(opening_V1_seq_mean) + "/" + format_double(opening_V1_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(closing_V1_mean_speedup[i]) + " [" + format_double(closing_V1_seq_mean) + "/" + format_double(closing_V1_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(erosion_V1_total_speedup[i]) + " [" + format_double(erosion_V1_seq_total) + "/" + format_double(erosion_V1_par_total_vector[i]) + "]s")
                << std::setw(25) << (format_double(dilation_V1_total_speedup[i]) + " [" + format_double(dilation_V1_seq_total) + "/" + format_double(dilation_V1_par_total_vector[i]) + "]s")
                << std::setw(25) << (format_double(opening_V1_total_speedup[i]) + " [" + format_double(opening_V1_seq_total) + "/" + format_double(opening_V1_par_total_vector[i]) + "]s")
                << std::setw(25) << (format_double(closing_V1_total_speedup[i]) + " [" + format_double(closing_V1_seq_total) + "/" + format_double(closing_V1_par_total_vector[i]) + "]s")
                << std::endl;
    }
            

    // Tabella per V2
    std::cout << "\n=== Speedup Table V2 ===\n" << std::endl;
    logfile << "\n=== Speedup Table V2 ===\n" << std::endl;

    std::cout << std::left << std::setw(10) << "Threads"
            << std::setw(25) << "E_Mean"
            << std::setw(25) << "D_Mean"
            << std::setw(25) << "O_Mean"
            << std::setw(25) << "C_Mean"
            << std::setw(25) << "E_Total"
            << std::setw(25) << "D_Total"
            << std::setw(25) << "O_Total"
            << std::setw(25) << "C_Total" << std::endl;

    logfile << std::left << std::setw(10) << "Threads"
            << std::setw(25) << "E_Mean"
            << std::setw(25) << "D_Mean"
            << std::setw(25) << "O_Mean"
            << std::setw(25) << "C_Mean"
            << std::setw(25) << "E_Total"
            << std::setw(25) << "D_Total"
            << std::setw(25) << "O_Total"
            << std::setw(25) << "C_Total" << std::endl;

    for (int i = 0; i < test_thread.size(); i++) {
        std::cout << std::fixed << std::setprecision(3)
                << std::left << std::setw(10) << test_thread[i]
                << std::setw(25) << (format_double(erosion_V2_mean_speedup[i]) + " [" + format_double(erosion_V2_seq_mean) + "/" + format_double(erosion_V2_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(dilation_V2_mean_speedup[i]) + " [" + format_double(dilation_V2_seq_mean) + "/" + format_double(dilation_V2_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(opening_V2_mean_speedup[i]) + " [" + format_double(opening_V2_seq_mean) + "/" + format_double(opening_V2_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(closing_V2_mean_speedup[i]) + " [" + format_double(closing_V2_seq_mean) + "/" + format_double(closing_V2_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(erosion_V2_total_speedup[i]) + " [" + format_double(erosion_V2_seq_total) + "/" + format_double(erosion_V2_par_total_vector[i]) + "]s")
                << std::setw(25) << (format_double(dilation_V2_total_speedup[i]) + " [" + format_double(dilation_V2_seq_total) + "/" + format_double(dilation_V2_par_total_vector[i]) + "]s")
                << std::setw(25) << (format_double(opening_V2_total_speedup[i]) + " [" + format_double(opening_V2_seq_total) + "/" + format_double(opening_V2_par_total_vector[i]) + "]s")
                << std::setw(25) << (format_double(closing_V2_total_speedup[i]) + " [" + format_double(closing_V2_seq_total) + "/" + format_double(closing_V2_par_total_vector[i]) + "]s")
                << std::endl;
    
        logfile << std::fixed << std::setprecision(3)
                << std::left << std::setw(10) << test_thread[i]
                << std::setw(25) << (format_double(erosion_V2_mean_speedup[i]) + " [" + format_double(erosion_V2_seq_mean) + "/" + format_double(erosion_V2_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(dilation_V2_mean_speedup[i]) + " [" + format_double(dilation_V2_seq_mean) + "/" + format_double(dilation_V2_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(opening_V2_mean_speedup[i]) + " [" + format_double(opening_V2_seq_mean) + "/" + format_double(opening_V2_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(closing_V2_mean_speedup[i]) + " [" + format_double(closing_V2_seq_mean) + "/" + format_double(closing_V2_par_mean_vector[i]) + "]s")
                << std::setw(25) << (format_double(erosion_V2_total_speedup[i]) + " [" + format_double(erosion_V2_seq_total) + "/" + format_double(erosion_V2_par_total_vector[i]) + "]s")
                << std::setw(25) << (format_double(dilation_V2_total_speedup[i]) + " [" + format_double(dilation_V2_seq_total) + "/" + format_double(dilation_V2_par_total_vector[i]) + "]s")
                << std::setw(25) << (format_double(opening_V2_total_speedup[i]) + " [" + format_double(opening_V2_seq_total) + "/" + format_double(opening_V2_par_total_vector[i]) + "]s")
                << std::setw(25) << (format_double(closing_V2_total_speedup[i]) + " [" + format_double(closing_V2_seq_total) + "/" + format_double(closing_V2_par_total_vector[i]) + "]s")
                << std::endl;
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
