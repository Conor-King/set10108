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
#include <chrono>
#include <array>

namespace fs = std::filesystem;
using namespace std;
using namespace std::chrono;

// Window size
const int gameWidth = 800;
const int gameHeight = 600;

// Stores the max size of the image folder.
int MAX_LOADS = 0;

// Shared pointer stores the list of images as a texture in a pair with the hue of the image.
auto loadedImages = std::make_shared<std::vector<std::pair<float, sf::Texture>>>();

// Finished boolean for timing.
bool finished = false;

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

// Struct queue for keeping track of the work to be done.
struct pile_t
{
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

// our piles
pile_t to_load_num;
pile_t to_hue_num;
pile_t to_sort_num;
pile_t done_num;

// initialize all piles
void initialize_piles()
{
	to_load_num.Initialize(MAX_LOADS);
	to_hue_num.Initialize(0);
	to_sort_num.Initialize(0);
	done_num.Initialize(0);
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

//void ImagePlaceholders(std::condition_variable& placeholderCond) {
//	float tempHue = 999;
//
//	// Inputing temp placeholder images into the list.
//	for (int i = 0; i < imageCount; i++) {
//		auto temp = std::make_shared<sf::Texture>();
//		temp->loadFromFile("Images/placeholder.png"); // This will not work on another computer????????????????????
//
//		oldImages.push_back(std::make_pair(tempHue, *temp));
//		loadedImages.push_back(std::make_pair(tempHue, *temp));
//	}
//	//printf("Placeholders Finished!");
//	placeholderCond.notify_all();
//}

void ImagePlaceholdersNoThread() {
	float tempHue = 999;
	sf::Texture temp;
	temp.loadFromFile("Images/placeholder.png");

	// Inputing temp placeholder images into the list.
	for (int i = 0; i < MAX_LOADS; i++) {
		loadedImages->push_back(std::make_pair(tempHue, temp));
	}
}

sf::Texture LoadImagesToTexture(std::string filename) {
	sf::Texture texture;
	float hue = -1.0;

	if (!texture.loadFromFile(filename)) {
		printf("Error loading image to texture - LoadImageToTextures Function");
	}

	return texture;
}

void LoadingFunction(pile_t& pile_in, pile_t& pile_out) {
	// Even if the input pile is empty and the output pile is empty, we might still have work to do if everything is not done
	// We're certain that nothing more is comin, if the "done" pile is full
	while (done_num.Num() != MAX_LOADS)
	{
		const char* image_folder = "Images/unsorted";
		std::vector<std::string> imageFilenames;
		for (auto& p : fs::directory_iterator(image_folder))
			imageFilenames.push_back(p.path().u8string());

		auto index = 0;

		// keep trying to pop while the input pile has things to work on
		while (pile_in.Pop())
		{
			//printf("Loading thread started!\n");
			auto texture = LoadImagesToTexture(imageFilenames[index]);
			loadedImages->at(index).second = texture;
			// ... and add the item (sort of, by incrementing) to the output pile
			pile_out.Increment();
			index++;
		}
		//this_thread::sleep_for(std::chrono::milliseconds(1)); // is this better if we sleep for a tiny bit?
	}
}

void LoadingFunctionNoThread() {
	const char* image_folder = "Images/unsorted";
	std::vector<std::string> imageFilenames;
	for (auto& p : fs::directory_iterator(image_folder))
		imageFilenames.push_back(p.path().u8string());

	for (int i = 0; i < imageFilenames.size(); i++) {
		auto texture = LoadImagesToTexture(imageFilenames[i]);
		loadedImages->at(i).second = texture;
	}
}

void HueFunction(pile_t& pile_in, pile_t& pile_out, vector<string> imageFilenames) {
	while (done_num.Num() != MAX_LOADS)
	{
		auto index = 0;
		// keep trying to pop while the input pile has things to work on
		while (pile_in.Pop())
		{
			auto hue = GetImagePixelValues(imageFilenames[index].c_str());

			hue = hue / 360;
			hue = frac(hue + 1 / 6);

			loadedImages->at(index).first = hue;

			// ... and add the item (sort of, by incrementing) to the output pile
			pile_out.Increment();
			index++;
		}
		//this_thread::sleep_for(std::chrono::milliseconds(1)); // is this better if we sleep for a tiny bit?
	}
}

void HueFunctionNoThread(std::vector<std::string> imageFilenames) {
	for (int i = 0; i < loadedImages->size(); i++)
	{
		auto hue = GetImagePixelValues(imageFilenames[i].c_str());

		hue = hue / 360;
		hue = frac(hue + 1 / 6);

		loadedImages->at(i).first = hue;

		printf("Hue complete : %f\n", hue);
	}
}

void SortFunction(pile_t& pile_in, pile_t& pile_out) {
	while (done_num.Num() != MAX_LOADS)
	{
		// keep trying to pop while the input pile has things to work on
		while (pile_in.Pop())
		{
			for (int i = loadedImages->size(); i >= 2; --i) {
				for (int j = 0; j < i - 1; j++) {
					if (loadedImages->at(j).first > loadedImages->at(j+1).first) {
						auto temp = loadedImages->at(j);
						loadedImages->at(j) = loadedImages->at(j+1);
						loadedImages->at(j+1) = temp;
					}
				}
			}

			// ... and add the item (sort of, by incrementing) to the output pile
			pile_out.Increment();
		}
		//this_thread::sleep_for(std::chrono::milliseconds(1)); // is this better if we sleep for a tiny bit?
	}
}

void SortFunctionNoThread() {
	// Bubble sort.
	for (int i = loadedImages->size(); i >= 2; --i) {
		for (int j = 0; j < i - 1; j++) {
			if (loadedImages->at(j).first > loadedImages->at(j + 1).first) {
				auto temp = loadedImages->at(j);
				loadedImages->at(j) = loadedImages->at(j + 1);
				loadedImages->at(j + 1) = temp;
			}
		}
	}
}

// our 3 functions
void load()
{
	LoadingFunction(to_load_num, to_hue_num);
}

void hue(vector<string> imageFilenames)
{
	HueFunction(to_hue_num, to_sort_num, imageFilenames);
}

void sort()
{
	SortFunction(to_sort_num, done_num);
}

int main()
{
	std::srand(static_cast<unsigned int>(std::time(NULL)));

	clock_t time;
	time = clock();

	// example folder to load images
	const char* image_folder = "Images/unsorted";
	std::vector<std::string> imageFilenames;
	for (auto& p : fs::directory_iterator(image_folder))
		imageFilenames.push_back(p.path().u8string());

	// Get the count for images in the folder.
	MAX_LOADS = imageFilenames.size();

	// Create the window of the application
	sf::RenderWindow window(sf::VideoMode(gameWidth, gameHeight, 32), "Image Fever",
		sf::Style::Titlebar | sf::Style::Close);
	window.setVerticalSyncEnabled(true);

	initialize_piles();
	ImagePlaceholdersNoThread();

	// Create an array of threads. Will loop until there is nothing more to load/covert/sort.
	array< thread, 3> threads = { thread(load),
		thread(hue, imageFilenames),
		thread(sort) };

	sf::Sprite visibleSprite;
	int displayIndex = 0;
	//std::vector<std::pair<float, sf::Texture>> oldImages = loadedImages;

	// Calculate the startup time of the application.
	time = clock() - time;
	printf("\nStartup Time: %f seconds\n", ((float)time) / CLOCKS_PER_SEC);

	// Testing the output from the pipline.
	/*while (done_num.Num() != MAX_LOADS)
	{
		printf("Checking work status: %d loaded, %d hue conversion, %d sorted, %d done!\n", to_load_num.Num(), to_hue_num.Num(), to_sort_num.Num(), done_num.Num());
		this_thread::sleep_for(std::chrono::seconds(1));
	}*/

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
				view.setCenter(gameWidth / 2.f, gameHeight / 2.f);
				window.setView(view);
			}

			// Arrow key handling!
			if (event.type == sf::Event::KeyPressed)
			{
				// adjust the image index
				if (event.key.code == sf::Keyboard::Key::Left) {
					displayIndex = (displayIndex + imageFilenames.size() - 1) % imageFilenames.size();
					
				}
				else if (event.key.code == sf::Keyboard::Key::Right) {
					displayIndex = (displayIndex + 1) % imageFilenames.size();
					
				}

				// set it as the window title
				window.setTitle("Image Fever");
			}
		}

		if (done_num.Num() == MAX_LOADS)
		{
			finished = true;
			for (auto& t : threads) {
				if (t.joinable())
				{
					t.join();
				}
			}
			MAX_LOADS = 0;
		}

		if (finished)
		{
			time = clock() - time;
			printf("All images loaded! Finish time: %f seconds\n", ((float)time) / CLOCKS_PER_SEC);
			finished = false;
		}

		visibleSprite.setTexture(loadedImages->at(displayIndex).second);
		// Clear the window
		window.clear(sf::Color(0, 0, 0));
		// draw the sprite
		window.draw(visibleSprite);
		// Display things on screen
		window.display();
	}

	return EXIT_SUCCESS;
}