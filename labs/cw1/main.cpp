// This is a chopped Pong example from SFML examples

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////

/**
 * TODO:
 * Stop the image from changing until a button is pressed.
 * Measure performance for startup time, app execution, main loop. (See CW printout).
 * Add data gathering code for analysis of the threading.
 * Testing with threads and without.
 * Clean up and comment code.
 * Finish report.
 * 
 *
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
#include <condition_variable>

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

const int gameWidth = 800;
const int gameHeight = 600;

std::mutex mut;

std::vector<std::pair<float, sf::Texture>> loadedImages;

bool finished = false;
int imageCount = 0;
int imageIndex = 0;

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

	return medianHue;
}

float GetImagePixelValues(char const* imagePath)
{
	int x, y, channels;
	std::vector<float> hueValues;

	unsigned char* data = stbi_load(imagePath, &x, &y, &channels, 3);
	if (!data)
	{
		printf("Error loading image Stb_image");
	}

	//printf("Loaded image width: %dpx, height %dpx, and channels %d\n", x, y, channels);

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

	//printf("pixel count: %d", count);
	//printf("Median Hue: %d", static_cast<int>(medianHue));

	// Call the median hue which returns a float value with the median hue of all values.
	return GetMedianHue(hueValues);
}


void ImagePlaceholders() {
	float tempHue = 999;

	// Inputing temp placeholder images into the list.    
	for (int i = 0; i < imageCount; i++) {
		auto temp = std::make_shared<sf::Texture>();
		temp->loadFromFile("C:/Users/Conor/Desktop/Coursework1CPS\\placeholder.png"); // This will not work on another computer????????????????????

		loadedImages.push_back(std::make_pair(tempHue, *temp));
	}
}


std::shared_ptr<sf::Texture> LoadImagesToTexture(std::string filename) {
	auto texture = std::make_shared<sf::Texture>();
	float hue = -1.0;

	if (!texture->loadFromFile(filename)) {
		printf("Error loading image to texture - LoadImageToTextures Function");
	}

	return texture;
}

void LoadingFunction(std::condition_variable &imageCond, std::condition_variable &hueCond) {

	const char* image_folder = "C:/Users/Conor/Desktop/Coursework1CPS/unsorted";
	std::vector<std::string> imageFilenames;
	for (auto& p : fs::directory_iterator(image_folder))
		imageFilenames.push_back(p.path().u8string());

	for (int i = 0; i < imageFilenames.size(); i++) {
		printf("Loading thread started!\n");
		auto texture = LoadImagesToTexture(imageFilenames[i]);

		auto lock = std::unique_lock<std::mutex>(mut);
		loadedImages[i].second = *texture;

		// Temp Sleep to show the images loading in from the background thread.
		//std::this_thread::sleep_for(std::chrono::seconds(1));
		
		printf("Image loaded - notify hue thread.\n");
		hueCond.notify_one();
		printf("Loading thread waiting.\n");
		imageCond.wait(lock);
	}
	
	//finished = true;
}

void HueFunction(std::condition_variable& hueCond, std::condition_variable& sortCond, std::vector<std::string> imageFilenames) {

	for (int i = 0; i < loadedImages.size(); i++)
	{
		auto lock = std::unique_lock<std::mutex>(mut);
		
		printf("Hue Function waiting for condition\n");
		hueCond.wait(lock);
		printf("Hue Started!\n");

		auto hue = GetImagePixelValues(imageFilenames[i].c_str());

		hue = hue / 360;
		hue = frac(hue + 1 / 6);

		loadedImages[i].first = hue;

		printf("Hue complete : %f\n", hue);

		// Sleep for one sec.
		//std::this_thread::sleep_for(std::chrono::seconds(1));

		// Notify sort to sort the images.
		printf("Hue finished - Notify sort thread.\n");
		sortCond.notify_one();
	}
	finished = true;
}

void SortFunction(std::condition_variable& sortCond, std::condition_variable &imageCond) {

	while (!finished) {
		auto lock = std::unique_lock<std::mutex>(mut);

		printf("Sort Function waiting for condition.\n");
		sortCond.wait(lock);
		printf("Sorting Started! \n");

		// Bubble sort.
		for (int i = loadedImages.size(); i >= 2; --i) {
			for (int j = 0; j < i - 1; j++) {
				if (loadedImages[j].first > loadedImages[j + 1].first) {
					auto temp = loadedImages[j];
					loadedImages[j] = loadedImages[j + 1];
					loadedImages[j + 1] = temp;
				}
			}
		}
		// Sleep for one sec.
		//std::this_thread::sleep_for(std::chrono::seconds(1));

		printf("Sort finished! - Notify image thread.\n");
		imageCond.notify_one();
	}
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


	// Create condition variable.
	std::condition_variable imageCond;
	std::condition_variable hueCond;
	std::condition_variable sortCond;



	// Start threads for loading, and sorting

	// Inputing temp data into the image vector to store temp hue and temp placeholder image.
	ImagePlaceholders();

	// Starting the load thread loading each image in the folder one by one and inserting them into the image vector.
	//LoadingFunction();
	std::thread loadingThread(LoadingFunction, ref(imageCond), ref(hueCond));

	std::thread hueThread(HueFunction, ref(hueCond), ref(sortCond), imageFilenames);

	std::thread sortThread(SortFunction, ref(sortCond), ref(imageCond));



	sf::Clock clock;
	sf::Sprite tempsprite;
	sf::Sprite visibleSprite;
	int index = 0;
	tempsprite.setTexture(loadedImages[index].second);
	visibleSprite = tempsprite;
	
	while (window.isOpen())
	{
		//sf::Sprite sprite;
		//finished = false;

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
				view.setCenter(gameWidth / 2.f, gameHeight / 2.f);
				window.setView(view);
			}

			// Arrow key handling!
			if (event.type == sf::Event::KeyPressed)
			{
				// adjust the image index
				if (event.key.code == sf::Keyboard::Key::Left) {
					index = (index + imageFilenames.size() - 1) % imageFilenames.size();
					// Set the texture of the spirte used for the image.
					visibleSprite.setTexture(loadedImages[index].second);
					//sortCond.notify_one();
				}
				else if (event.key.code == sf::Keyboard::Key::Right) {
					index = (index + 1) % imageFilenames.size();
					// Set the texture of the spirte used for the image.
					visibleSprite.setTexture(loadedImages[index].second);
					//sortCond.notify_one();
				}

				// set it as the window title
				window.setTitle(imageFilenames[index]);
			}
		}

		// Clear the window
		window.clear(sf::Color(0, 0, 0));
		// draw the sprite
		window.draw(visibleSprite);
		// Display things on screen
		window.display();
	}

	// Not sure if this is needed. Stops an abort error.
	loadingThread.join();
	hueThread.join();
	sortThread.join();

	return EXIT_SUCCESS;
}