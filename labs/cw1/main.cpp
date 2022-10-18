// This is a chopped Pong example from SFML examples

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////

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
    unsigned char r;
    unsigned char g;
    unsigned char b;
} RgbColour;

typedef struct HsvColour
{
    unsigned char h;
    unsigned char s;
    unsigned char v;
};

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
    unsigned char rgbMin, rgbMax;

    rgbMin = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
    rgbMax = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);

    hsv.v = rgbMax;
    if (hsv.v == 0)
    {
        hsv.h = 0;
        hsv.s = 0;
        return hsv;
    }

    hsv.s = 255 * long(rgbMax - rgbMin) / hsv.v;
    if (hsv.s == 0)
    {
        hsv.h = 0;
        return hsv;
    }

    if (rgbMax == rgb.r)
        hsv.h = 0 + 43 * (rgb.g - rgb.b) / (rgbMax - rgbMin);
    else if (rgbMax == rgb.g)
        hsv.h = 85 + 43 * (rgb.b - rgb.r) / (rgbMax - rgbMin);
    else
        hsv.h = 171 + 43 * (rgb.r - rgb.g) / (rgbMax - rgbMin);

    return hsv;
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

            printf("h: %d, s: %d, v: %d \n", hsv.h, hsv.s, hsv.v);

        }
    }
   /* unsigned bytePerPixel = channels;
    unsigned char* pixelOffset = data + (i + x * j) * bytePerPixel;
    unsigned char r = pixelOffset[0];
    unsigned char g = pixelOffset[1];
    unsigned char b = pixelOffset[2];
    unsigned char a = channels >= 4 ? pixelOffset[3] : 0xff;

    printf("r: %d, g: %d, b: %d", r, g, b);*/


    //unsigned char r;
    //unsigned char g;
    //unsigned char b;

    //std::vector<unsigned char> rgb;

    //const stbi_uc* p = data + (3 * (1 * x + 1));
    //rgb.push_back(static_cast<unsigned int>(p[0]));
    //rgb.push_back(static_cast<unsigned int>(p[1]));
    //rgb.push_back(static_cast<unsigned int>(p[2]));
    //
	//printf("r: %c, g: %c, b: %c" ,rgb[0], rgb[1], rgb[2]);
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
