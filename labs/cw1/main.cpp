// This is a chopped Pong example from SFML examples

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////


/**
 * TODO:
 * Covert hue value to the colour wheel (frac equation?)
 * Load all images at the start of the program with and without threads?
 * Input temp data for images that have not loaded at the start of the application.
 * Thread for image loading, image sorting, median hue calculations. (Futures???)
 * Measure performance for startup time, app execution, main loop. (See CW printout).
 *
 */
#define STB_IMAGE_IMPLEMENTATION

#include <SFML/Graphics.hpp>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include "stb_image.h"

namespace fs = std::filesystem;


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

std::vector<float> hueValues;

sf::Vector2f ScaleFromDimensions(const sf::Vector2u& textureSize, int screenWidth, int screenHeight)
{
    float scaleX = screenWidth / float(textureSize.x);
    float scaleY = screenHeight / float(textureSize.y);
    float scale = std::min(scaleX, scaleY);
    return { scale, scale };
}

void GetPixel(stbi_uc* image, size_t imageWidth, size_t x, size_t y, std::vector<unsigned char> rgb) {
    const stbi_uc* p = image + (3 * (y * imageWidth + x));
    rgb.push_back(p[0]);
    rgb.push_back(p[1]);
    rgb.push_back(p[2]);
}

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


//HsvColour RgbToHsv(RgbColour rgb)
//{
//    HsvColour hsv;
//    unsigned char rgbMin, rgbMax;
//
//    rgbMin = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
//    rgbMax = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);
//
//    hsv.v = rgbMax;
//    if (hsv.v == 0)
//    {
//        hsv.h = 0;
//        hsv.s = 0;
//        return hsv;
//    }
//
//    hsv.s = 255 * long(rgbMax - rgbMin) / hsv.v;
//    if (hsv.s == 0)
//    {
//        hsv.h = 0;
//        return hsv;
//    }
//
//    if (rgbMax == rgb.r)
//        hsv.h = 0 + 43 * (rgb.g - rgb.b) / (rgbMax - rgbMin);
//    else if (rgbMax == rgb.g)
//        hsv.h = 85 + 43 * (rgb.b - rgb.r) / (rgbMax - rgbMin);
//    else
//        hsv.h = 171 + 43 * (rgb.r - rgb.g) / (rgbMax - rgbMin);
//
//    return hsv;
//}

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

void GetImagePixelValues(char const* imagePath)
{
    int x, y, channels;
    unsigned char* data = stbi_load(imagePath, &x, &y, &channels, 3);
    if (!data)
    {
        printf("Error loading image Stb_image");

    }

    printf("Loaded image width: %dpx, height %dpx, and channels %d", x, y, channels);

    int count = 0;
    

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

            // Don't do push back in the double loop. (Do NOT use std::vector's push_back in double-for loops that process every pixel,
            // because you know how many pixels you have in advance and you don't want to do e.g. a quarter million allocations for an image,
            // or having to protect that call with a mutex. Pre-allocate a vector ( std::vector has a resize method) and access each pixel using an appropriate index. 
            // If a vector has width*height elements, you can calculate an 1D index from 2D coordinates as "x + y*width". Like we saw in the CUDA lecture!)
            
            hueValues.push_back(hsv.h);

            //printf("h: %d, s: %d, v: %d \n", hsv.h, hsv.s, hsv.v);
            count++;
        }
    }

    printf("pixel count: %d", count);

    // Call the median hue which returns a float value with the median hue of all values.
    auto medianHue = GetMedianHue(hueValues);

    printf("Median Hue: %d", static_cast<int>(medianHue));
}



int main()
{
    std::srand(static_cast<unsigned int>(std::time(NULL)));

    // example folder to load images
    const char* image_folder = "C:/Users/Conor/Desktop/Coursework1CPS/unsorted";
    std::vector<std::string> imageFilenames;
    for (auto& p : fs::directory_iterator(image_folder))
        imageFilenames.push_back(p.path().u8string());

    // Define some constants
    const float pi = 3.14159f;
    const int gameWidth = 800;
    const int gameHeight = 600;

    int imageIndex = 0;

    // Create the window of the application
    sf::RenderWindow window(sf::VideoMode(gameWidth, gameHeight, 32), "Image Fever",
                            sf::Style::Titlebar | sf::Style::Close);
    window.setVerticalSyncEnabled(true);

    // Load an image to begin with
    sf::Texture texture;
    if (!texture.loadFromFile(imageFilenames[imageIndex]))
        return EXIT_FAILURE;
    sf::Sprite sprite (texture);
    // Make sure the texture fits the screen
    sprite.setScale(ScaleFromDimensions(texture.getSize(),gameWidth,gameHeight));

    // Image Testing here --------------------------------------------------------------------
    char const* imagePath = imageFilenames[imageIndex].c_str();
    GetImagePixelValues(imagePath);

    // ---------------------------------------------------------------------------------------

    sf::Clock clock;
    while (window.isOpen())
    {
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
                if (event.key.code == sf::Keyboard::Key::Left)
                    imageIndex = (imageIndex + imageFilenames.size() - 1) % imageFilenames.size();

                else if (event.key.code == sf::Keyboard::Key::Right)
                    imageIndex = (imageIndex + 1) % imageFilenames.size();

                // get image filename
                const auto& imageFilename = imageFilenames[imageIndex];

                // set it as the window title 
                window.setTitle(imageFilename);

                // ... and load the appropriate texture, and put it in the sprite
                if (texture.loadFromFile(imageFilename))
                {
                    sprite = sf::Sprite(texture);
                    sprite.setScale(ScaleFromDimensions(texture.getSize(), gameWidth, gameHeight));
                }
            }
        }

        // Clear the window
        window.clear(sf::Color(0, 0, 0));
        // draw the sprite
        window.draw(sprite);
        // Display things on screen
        window.display();
    }

    return EXIT_SUCCESS;
}
