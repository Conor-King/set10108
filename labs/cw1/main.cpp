// This is a chopped Pong example from SFML examples

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////


/**
 * TODO:
 * 
 * 
 * Load all images at the start of the program with and without threads?
 * 
 * Sort loaded images depending on the hue.
 * Thread for image loading, image sorting, median hue calculations. (Futures???)
 * Measure performance for startup time, app execution, main loop. (See CW printout).
 *
 * 
 * Use a pipline model for the thread distribution thread load, hue, sort!
 */
#define STB_IMAGE_IMPLEMENTATION

#include <SFML/Graphics.hpp>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include "stb_image.h"
#include <thread>
#include <mutex>

namespace fs = std::filesystem;

struct threadCount {
    // return if we managed to pop a load from the pile (we can pop only if the number is greater than 0)
    bool Pop()
    {
        std::lock_guard<std::mutex> guard(mutex);
        auto ok = num > 0;
        if (num > 0)
            --num;
        return ok;
    }

    // increment the size of the pile
    void Increment()
    {
        std::lock_guard<std::mutex> guard(mutex);
        ++num;
    }

    // get the size of the pile
    int Num() {
        std::lock_guard<std::mutex> guard(mutex);
        return num;
    }

    // set the starting size of the pile
    void Initialize(int startingNumberOfLoads) {
        std::lock_guard<std::mutex> guard(mutex);
        num = startingNumberOfLoads;
    }

private:
    std::mutex mutex;
    int num = 0;
};

typedef struct RgbColour
{
    float r;
    float g;
    float b;
} RgbColour;

typedef struct HsvColour
{
    float h;
    float s;
    float v;
}HsvColour;

const int gameWidth = 800;
const int gameHeight = 600;

std::map<std::shared_ptr<float>, std::shared_ptr<sf::Texture>> loadedImages;
std::vector<std::shared_ptr<sf::Texture>> orderedImages;

std::vector<std::pair<float, std::shared_ptr<sf::Texture>>> images;


threadCount loadingThreadCount;
threadCount hueCalcThreadCount;
threadCount sortThreadCount;
threadCount finishedCount;

int imageCount = 0;
int imageIndex = 0;

void initialize_counts()
{
    // Initialize the loading thread with the number of images loaded in the folder. The rest start at 0.
    loadingThreadCount.Initialize(imageCount);
    hueCalcThreadCount.Initialize(0);
    sortThreadCount.Initialize(0);
    finishedCount.Initialize(0);
}

sf::Vector2f ScaleFromDimensions(const sf::Vector2u& textureSize, int screenWidth, int screenHeight)
{
    float scaleX = screenWidth / float(textureSize.x);
    float scaleY = screenHeight / float(textureSize.y);
    float scale = std::min(scaleX, scaleY);
    return { scale, scale };
}

float frac(float x) {
    return x - floorf(x);
}

//void GetPixel(stbi_uc* image, size_t imageWidth, size_t x, size_t y, std::vector<unsigned char> rgb) {
//    const stbi_uc* p = image + (3 * (y * imageWidth + x));
//    rgb.push_back(p[0]);
//    rgb.push_back(p[1]);
//    rgb.push_back(p[2]);
//}

HsvColour RgbToHsv(RgbColour rgb)
{

    HsvColour hsv;

    // R, G, B values are divided by 255
    // to change the range from 0..255 to 0..1
    rgb.r = rgb.r / 255.0;
    rgb.g = rgb.g / 255.0;
    rgb.b = rgb.b / 255.0;

    // h, s, v = hue, saturation, value
    float cmax = std::max(rgb.r, std::max(rgb.g, rgb.b)); // maximum of r, g, b
    float cmin = std::min(rgb.r, std::min(rgb.g, rgb.b)); // minimum of r, g, b
    float diff = cmax - cmin; // diff of cmax and cmin.
    float h = -1, s = -1;

    // if cmax and cmax are equal then h = 0
    if (cmax == cmin)
        h = 0;

    // if cmax equal r then compute h
    else if (cmax == rgb.r)
        h = fmod(60 * ((rgb.g - rgb.b) / diff) + 360, 360);

    // if cmax equal g then compute h
    else if (cmax == rgb.g)
        h = fmod(60 * ((rgb.b - rgb.r) / diff) + 120, 360);

    // if cmax equal b then compute h
    else if (cmax == rgb.b)
        h = fmod(60 * ((rgb.r - rgb.g) / diff) + 240, 360);

    // if cmax equal zero
    if (cmax == 0)
       float s = 0;
    else
        s = (diff / cmax) * 100;

    // compute v
    float v = cmax * 100;
    
    hsv.h = h;
    hsv.s = s;
    hsv.v = v;

    //printf("Hue: %f , Sat: %f , Val: %f ", h,s,v);

    return hsv;
}

float GetMedianHue(std::vector<float> hueVector)
{
    std::sort(hueVector.begin(), hueVector.end());

    auto medianHue = hueVector[hueVector.size() / 2];

    /*for (auto value : hueVector)
    {
        printf("hue: %f ", value);
    }*/

    return medianHue;
}

int GetImagePixelValues(char const* imagePath)
{
    int x, y, channels;
    std::vector<float> hueValues;

    unsigned char* data = stbi_load(imagePath, &x, &y, &channels, 3);
    if (!data)
    {
        printf("Error loading image Stb_image");

    }

    printf("Loaded image width: %dpx, height %dpx, and channels %d", x, y, channels);

    int count = 0;
    hueValues.resize(x * y);


    for (int i = 0; i < x; i++)
    {
        for (int j = 0; j < y; j++)
        {
            unsigned bytePerPixel = channels;
            unsigned char* pixelOffset = data + (i + x * j) * bytePerPixel;
            unsigned char r = pixelOffset[0];
            unsigned char g = pixelOffset[1];
            unsigned char b = pixelOffset[2];
            unsigned char a = channels >= 4 ? pixelOffset[3] : 0xff;

            RgbColour rgb;
            rgb.r = r;
            rgb.g = g;
            rgb.b = b;

            HsvColour hsv;
            hsv = RgbToHsv(rgb);
            
            hueValues[i + j * x] = hsv.h;

            //printf("h: %d, s: %d, v: %d \n", hsv.h, hsv.s, hsv.v);

            // Pixel count increment
            count++;
        }
    }

    printf("pixel count: %d", count);

    // Call the median hue which returns a float value with the median hue of all values.
    auto medianHue = GetMedianHue(hueValues);

    printf("Median Hue: %d", static_cast<int>(medianHue));

    return medianHue;
}

// Implemented in main for now........................................................................................................................................
void ImagePlaceholders() {
    // Inputing temp placeholder images into the ordered list.     Thread this?????????????????
    for (int i = 0; i < orderedImages.size(); i++) {
        auto temp = std::make_shared<sf::Texture>();
        temp->loadFromFile("C:/Users/Conor/Desktop/Coursework1CPS\\placeholder.png"); // This will not work on another computer????????????????????

        orderedImages[i] = temp;
    }
}

// Load all images into textures.                           THIS SHOULD BE USING THREADS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
void LoadImagesToTexture(std::vector<std::string> imageFilenames) {

    for (int i = 0; i < imageFilenames.size(); i++) {

        auto texture = std::make_shared<sf::Texture>();
        float hue = -1.0;
        
        if (!texture->loadFromFile(imageFilenames[i])) {
            printf("Error loading image to texture - LoadImageToTextures Function");
        }

        //hue = GetImagePixelValues(imageFilenames[i].c_str());

        //loadedImages.emplace(hue, texture);
        images.push_back(std::make_pair(hue, texture));
    }

    printf("\nAll image loaded ------------------------------------------------------------------------------------\n");
}

// Sort the loaded images by their hue values.                          This should run in a thread!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
void SortImagesHotCold(std::vector<std::pair<float, std::shared_ptr<sf::Texture>>> hueImagePair) {

    for (auto i = 0; i < hueImagePair.size(); i++) {

        auto hue = hueImagePair[i].first / 360.0;

        hueImagePair[i].first = frac(hue + 1 / 6);

        printf("hue position: %f", hueImagePair[i].first);
    }

    // Sort float values to sort images 
    std::sort(hueImagePair.begin(), hueImagePair.end());
    printf("\nImages sorted -----------------------------------------------------------------------------------------\n");

    // Print Values
    for (auto i = 0; i < hueImagePair.size(); i++) {

        orderedImages[i] = hueImagePair[i].second;
        printf("hue position: %f \n", hueImagePair[i].first);

    }


    //std::map<std::shared_ptr<sf::Texture>, int>::iterator iter = imageHuePair.begin();

    //while (iter != imageHuePair.end()) {


    //    //printf("first: %c", iter->first);
    //    printf("second: %c", iter->second);
    //}
}

//void SortImagesHotCold(std::map<std::shared_ptr<float>, std::shared_ptr<sf::Texture>> imageHuePair) {
//
//    
//    
//    for (auto iter = imageHuePair.begin(); iter != imageHuePair.end(); iter++) {
//
//        
//
//        auto hue = *iter->first / 360.0;
//
//        *iter->first = frac(hue+1/6);
//
//        printf("hue position: %f", *iter->first);
//
//    }
//    
//    std::sort(imageHuePair.begin(), imageHuePair.end());
//    
//    for (auto iter = imageHuePair.begin(); iter != imageHuePair.end(); iter++) {
//
//        printf("hue position: %f \n", *iter->first);
//
//    }
//
//    
//    //std::map<std::shared_ptr<sf::Texture>, int>::iterator iter = imageHuePair.begin();
//
//    //while (iter != imageHuePair.end()) {
//
//
//    //    //printf("first: %c", iter->first);
//    //    printf("second: %c", iter->second);
//    //}
//
//
//}

void load() {

}

void hue() {

}

void sortHue() {

}

int main()
{
    std::srand(static_cast<unsigned int>(std::time(NULL)));

    // example folder to load images
    const char* image_folder = "C:/Users/Conor/Desktop/Coursework1CPS/unsorted";
    std::vector<std::string> imageFilenames;
    for (auto& p : fs::directory_iterator(image_folder))
        imageFilenames.push_back(p.path().u8string());

    // Get the count for images in the folder.
    imageCount = imageFilenames.size();

    // Define some constants
    const float pi = 3.14159f;

    // Create the window of the application
    sf::RenderWindow window(sf::VideoMode(gameWidth, gameHeight, 32), "Image Fever",
                            sf::Style::Titlebar | sf::Style::Close);
    window.setVerticalSyncEnabled(true);


    // Start threads for loading, and sorting
    
    // Load all images into sprites for use in the app.        THIS SHOULD BE IN A THREAD!!!!!!!!!!!!!!!!!
    std::thread loadingThread(LoadImagesToTexture, imageFilenames);
    //LoadImagesToTexture(imageFilenames);
    loadingThread.join();

    // Inputing temp placeholder images into the ordered list.    
    orderedImages.resize(imageFilenames.size());
    ImagePlaceholders();

    // Sort the images from hot to cold
    //std::thread sortingThread(SortImagesHotCold, images);
    SortImagesHotCold(images);
    
    
    

    // Ordering the images based on hue                          Thread??????????????????????

    //First image to display to the user.


    //// Load an image to begin with
    //sf::Texture texture;
    //if (!texture.loadFromFile(imageFilenames[imageIndex]))
    //    return EXIT_FAILURE;
    //sf::Sprite sprite (texture);
    //// Make sure the texture fits the screen
    //sprite.setScale(ScaleFromDimensions(texture.getSize(),gameWidth,gameHeight));
    

    sf::Clock clock;
    int index = 0;
    while (window.isOpen())
    {
        sf::Sprite sprite;

        // Handle events
        sf::Event event;
        while (window.pollEvent(event))
        {
            // Window closed or escape key pressed: exit
            if ((event.type == sf::Event::Closed) ||
               ((event.type == sf::Event::KeyPressed) && (event.key.code == sf::Keyboard::Escape)))
            {
                window.close();
                break;
            }
            
            // Window size changed, adjust view appropriately
            if (event.type == sf::Event::Resized)
            {
                sf::View view;
                view.setSize(gameWidth, gameHeight);
                view.setCenter(gameWidth/2.f, gameHeight/2.f);
                window.setView(view);
            }

            // Arrow key handling!
            if (event.type == sf::Event::KeyPressed)
            {
                // adjust the image index
                if (event.key.code == sf::Keyboard::Key::Left) {
                    index = (index + imageFilenames.size() - 1) % imageFilenames.size();
                }
                else if (event.key.code == sf::Keyboard::Key::Right) {
                    index = (index + 1) % imageFilenames.size();
                }

                // set it as the window title 
                window.setTitle(imageFilenames[index]);
            }
        }

        // Set the texture of the spirte used for the image.
        sprite.setTexture(*images[index].second);

        // Clear the window
        window.clear(sf::Color(0, 0, 0));
        // draw the sprite
        window.draw(sprite);
        // Display things on screen
        window.display();
    }

    return EXIT_SUCCESS;
}
