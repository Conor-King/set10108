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
#include <shared_mutex>
#include <condition_variable>
#include <chrono>
#include <array>

namespace fs = std::filesystem;
using namespace std;
using namespace std::chrono;

// Window size
const int gameWidth = 800;
const int gameHeight = 600;

//
std::shared_mutex mutex_;

// Stores the max size of the image folder.
int MAX_LOADS = 0;

// Shared pointer stores the list of images as a texture in a pair with the hue of the image.
auto loadedImages = std::make_shared<std::vector<std::pair<float, sf::Texture>>>();
auto displayImages = std::make_shared<std::vector<std::pair<float, sf::Texture>>>();

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
pile_t to_placeholder_num;
pile_t to_load_num;
pile_t to_hue_num;
pile_t to_sort_num;
pile_t done_num;

// initialize all piles
void initialize_piles()
{
	to_placeholder_num.Initialize(MAX_LOADS);
	to_load_num.Initialize(0);
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

// Converting RGB values to HSV counterpart.
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

// Sorts the list of hue values and returns the median hue.
float GetMedianHue(std::vector<float> hueVector)
{
	std::sort(hueVector.begin(), hueVector.end());

	auto medianHue = hueVector[hueVector.size() / 2];

	return medianHue;
}

// Gets all the pixel information from the image. Used to get the median hue.
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

	// Set the size of the vector.
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

			// Set the RGB values of one pixel
			RgbColour rgb;
			rgb.r = r;
			rgb.g = g;
			rgb.b = b;

			// Covert one pixel of RGB to HSV
			HsvColour hsv;
			hsv = RgbToHsv(rgb);

			hueValues[i + j * x] = hsv.h;

			//printf("h: %d, s: %d, v: %d \n", hsv.h, hsv.s, hsv.v);

			
		}
	}

	//printf("pixel count: %d", count);
	//printf("Median Hue: %d", static_cast<int>(medianHue));

	// Call the median hue which returns a float value with the median hue of all values.
	return GetMedianHue(hueValues);
}

// Threaded function for inputing placeholder information into the 
void ImagePlaceholders(pile_t& pile_in, pile_t& pile_out) {
	
	while (done_num.Num() != MAX_LOADS)
	{
		float tempHue = 999;
		sf::Texture temp;
		temp.loadFromFile("Images/placeholder.png");

		// keep trying to pop while the input pile has things to work on
		while (pile_in.Pop())
		{
			std::shared_lock lock(mutex_);
			displayImages->push_back(std::make_pair(tempHue, temp));
			loadedImages->push_back(std::make_pair(tempHue, temp));
			
			// Increment image task
			pile_out.Increment();
		}
		this_thread::sleep_for(std::chrono::milliseconds(1)); 
	}
}

void ImagePlaceholdersNoThread() {
	float tempHue = 999;
	sf::Texture temp;
	temp.loadFromFile("Images/placeholder.png");

	// Inputing temp placeholder images into the list.
	for (int i = 0; i < MAX_LOADS; i++) {
		displayImages->push_back(std::make_pair(tempHue, temp));
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

// Loading function thread
void LoadingFunction(pile_t& pile_in, pile_t& pile_out) {
	
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
			std::shared_lock lock(mutex_);
			//printf("Loading thread started!\n");
			auto texture = LoadImagesToTexture(imageFilenames[index]);
			loadedImages->at(index).second = texture;

			// Increment hue task
			pile_out.Increment();
			index++;
		}
		this_thread::sleep_for(std::chrono::milliseconds(1)); 
	}
}

// Loading function used to test without thread.
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

// Hue function thread
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

			std::shared_lock lock(mutex_);
			loadedImages->at(index).first = hue;

			// Increment sort task
			pile_out.Increment();
			index++;
		}
		this_thread::sleep_for(std::chrono::milliseconds(1)); 
	}
}

// Hue function used to test without threads
void HueFunctionNoThread(std::vector<std::string> imageFilenames) {
	for (int i = 0; i < loadedImages->size(); i++)
	{
		auto hue = GetImagePixelValues(imageFilenames[i].c_str());

		hue = hue / 360;
		hue = frac(hue + 1 / 6);

		loadedImages->at(i).first = hue;

		//printf("Hue complete : %f\n", hue);
	}
}

// Sort images using bubble sort thread.
void SortFunction(pile_t& pile_in, pile_t& pile_out) {
	while (done_num.Num() != MAX_LOADS)
	{
		// keep trying to pop while the input pile has things to work on
		while (pile_in.Pop())
		{
			std::shared_lock lock(mutex_);
			for (int i = loadedImages->size(); i >= 2; --i) {
				for (int j = 0; j < i - 1; j++) {
					if (loadedImages->at(j).first > loadedImages->at(j + 1).first) {
						auto temp = loadedImages->at(j);
						loadedImages->at(j) = loadedImages->at(j + 1);
						loadedImages->at(j + 1) = temp;
					}
				}
			}

			// Increment done pile.
			pile_out.Increment();
		}
		this_thread::sleep_for(std::chrono::milliseconds(1)); // is this better if we sleep for a tiny bit?
	}
}

// Sort function used to test without thread.
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

void placeholder() {
	ImagePlaceholders(to_placeholder_num, to_load_num);
}

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

	// Start timer.
	clock_t time;
	time = clock();

	// Load image filenames.
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

	// Without Thread Run ------------------------------------------------------------------
	/*ImagePlaceholdersNoThread();
	LoadingFunctionNoThread();
	HueFunctionNoThread(imageFilenames);
	SortFunctionNoThread();*/
	//--------------------------------------------------------------------------------------

	initialize_piles();

	// Create an array of threads. Will loop until there is nothing more to placeholder/load/covert/sort.
	array< thread, 4> threads = { 
		thread(placeholder),
		thread(load),
		thread(hue, imageFilenames),
		thread(sort) };

	// Create sprite to display to user
	sf::Sprite visibleSprite;
	int displayIndex = 0;
	//std::vector<std::pair<float, sf::Texture>> oldImages = loadedImages;

	// Calculate the startup time of the application.
	time = clock() - time;
	printf("\nStartup Time: %f seconds\n", ((float)time) / CLOCKS_PER_SEC);

	// Testing the output from the pipline.
	/*while (done_num.Num() != MAX_LOADS)
	{
		printf("Checking work status: %d placeholder, %d loaded, %d hue conversion, %d sorted, %d done!\n",to_placeholder_num.Num(), to_load_num.Num(), to_hue_num.Num(), to_sort_num.Num(), done_num.Num());
		this_thread::sleep_for(std::chrono::milliseconds(10));
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
					displayImages = loadedImages;
				}
				else if (event.key.code == sf::Keyboard::Key::Right) {
					displayIndex = (displayIndex + 1) % imageFilenames.size();
					displayImages = loadedImages;
				}

				// set it as the window title
				window.setTitle("Image Fever Index: " + std::to_string(displayIndex) + "Median Hue: " + std::to_string(displayImages->at(displayIndex).first));
			}
		}

		// If done join all threads.
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

		// If finished display time.
		if (finished)
		{
			time = clock() - time;
			printf("All images loaded! Finish time: %f seconds\n", ((float)time) / CLOCKS_PER_SEC);
			finished = false;
		}

		if (displayImages->size() > 0) {
			visibleSprite.setTexture(displayImages->at(displayIndex).second);
		}
		
		// Clear the window
		window.clear(sf::Color(0, 0, 0));
		// draw the sprite
		window.draw(visibleSprite);
		// Display things on screen
		window.display();
	}

	return EXIT_SUCCESS;
}