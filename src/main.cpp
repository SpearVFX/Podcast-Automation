#include "../third-party/AudioFile.h"
#include "../third-party/lodepng.h"
#include <string>
#include <iostream>
#include <chrono>

// Macro defining left focus
#define LEFT 0
// Macro defining right focus
#define RIGHT 1
// Macro defining central focus
#define CENTRAL 2

std::vector<unsigned char> generateSolidColorPixels(unsigned width, unsigned height, unsigned r = 0, unsigned g = 0, unsigned b = 0, unsigned a = 255) {
	std::vector<unsigned char> image;
	image.resize(width * height * 4);
	for (unsigned y = 0; y < height; y++) {
		for (unsigned x = 0; x < width; x++) {
			image[4 * width * y + 4 * x + 0] = r;
			image[4 * width * y + 4 * x + 1] = g;
			image[4 * width * y + 4 * x + 2] = b;
			image[4 * width * y + 4 * x + 3] = a;
		}
	}
	return image;
}

// These operations would be done for every frame, so we pre-generate all the solid color images
std::vector<unsigned char> redPixels = generateSolidColorPixels(64, 64, 255);
std::vector<unsigned char> greenPixels = generateSolidColorPixels(64, 64, 0, 255);
std::vector<unsigned char> bluePixels = generateSolidColorPixels(64, 64, 0, 0, 255);


// Encode an array of pixels into a png image.
void encodeOneStep(const char* filename, std::vector<unsigned char>& image, unsigned width, unsigned height) {
	//Encode the image
	unsigned error = lodepng::encode(filename, image, width, height);
	
	//if there's an error, display it
	if (error) std::cout << "encoder error " << error << ": " << lodepng_error_text(error) << std::endl;
}

bool isSpeaking(double averageDB) {
	return averageDB >= 2.0f;
}

int main() {
	auto start = std::chrono::steady_clock::now();
	auto end = std::chrono::steady_clock::now();
	std::cout << "Elapsed time in milliseconds: "
		<< std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
		<< " ms" << std::endl;

	AudioFile<double> audioFile;
	//AudioFile<double> audioFile2;
	//audioFile.load("../res/left-right.wav", false);
	audioFile.load("../res/actual-podcast.wav", true);
	int sampleRate = audioFile.getSampleRate();
	int bitDepth = audioFile.getBitDepth();

	int numSamples = audioFile.demandSamples.numSamples;
	double lengthInSeconds = audioFile.getLengthInSeconds();

	int numChannels = audioFile.getNumChannels();
	bool isMono = audioFile.isMono();
	bool isStereo = audioFile.isStereo();
	int secondsElapsed = 0;

	int channel = 0;
	int hzElapsed = 0;
	//int secondsElapsed = 0;
	double averageDBLeft = 0;
	double averageDBRight = 0;
	std::vector<std::vector<double>> averagedDB;
	averagedDB.resize(2);
	for (int i = 0; i < numSamples; i++)
	{
		double currentSampleLeft = audioFile.demandSamples.getBufferAt(LEFT, i);
		double currentSampleRight = audioFile.demandSamples.getBufferAt(RIGHT, i);
		averageDBLeft += currentSampleLeft * currentSampleLeft;
		averageDBRight += currentSampleRight * currentSampleRight;
		if (hzElapsed++ == sampleRate) {
			double calcLeft = (averageDBLeft / sampleRate) * 1000;
			double calcRight = (averageDBRight / sampleRate) * 1000;
			averagedDB[LEFT].push_back(calcLeft);
			averagedDB[RIGHT].push_back(calcRight);
			std::string result;
			//result += " Average DbS: ";
			//result += std::to_string(calc);
			result += " Seconds elapsed: ";
			result += std::to_string(secondsElapsed++);

			averageDBLeft = 0;
			averageDBRight = 0;
			hzElapsed = 0;

		}
	}
	secondsElapsed = 0;
	// 0 - left, 1 - right, 2 - central
	int cameraFocus = CENTRAL;

	int skipFrames = 0;

	for (size_t i = 3; i < averagedDB[0].size(); i++)
	{
		std::string filename = "test_" + std::to_string(secondsElapsed++) + ".png";
		std::cout << filename << std::endl;
		switch (cameraFocus) {
		case(LEFT): encodeOneStep(filename.c_str(), redPixels, 64, 64); break;
		case(RIGHT): encodeOneStep(filename.c_str(), greenPixels, 64, 64); break;
		case(CENTRAL): encodeOneStep(filename.c_str(), bluePixels, 64, 64); break;
		}
		if (skipFrames > 0) {
			skipFrames--;
			continue;
		}
		double lDb = averagedDB[LEFT][i];
		double rDb = averagedDB[RIGHT][i];
		bool isLeftSpeaking = isSpeaking(averagedDB[LEFT][i]);
		bool isRightSpeaking = isSpeaking(averagedDB[RIGHT][i]);

		bool isLeftSpeakingNext3Sec = isSpeaking((averagedDB[LEFT][i - 1] + averagedDB[LEFT][i - 2] + averagedDB[LEFT][i - 3])/3.0f);
		bool isRightSpeakingNext3Sec = isSpeaking((averagedDB[RIGHT][i - 1] + averagedDB[RIGHT][i - 2] + averagedDB[RIGHT][i - 3])/3.0f);




		if ((isLeftSpeakingNext3Sec && isRightSpeakingNext3Sec) || (!isLeftSpeakingNext3Sec && !isRightSpeakingNext3Sec)) {
			cameraFocus = CENTRAL;
			continue;
		}

		if (cameraFocus == CENTRAL) {
			if (isLeftSpeakingNext3Sec) {
				cameraFocus = LEFT;
				skipFrames += 4;
			}
			else if (isRightSpeakingNext3Sec) {
				cameraFocus = RIGHT;
				skipFrames += 4;
			}
			else {
				skipFrames += 1;
			}
			continue;
		}

		if (cameraFocus == LEFT) {
			if (isRightSpeakingNext3Sec && !isLeftSpeakingNext3Sec) {
				cameraFocus = RIGHT;
				skipFrames += 4;
			}
			else if (isLeftSpeaking)
				cameraFocus = LEFT;
			else {
				cameraFocus = CENTRAL;
				skipFrames += 2;
			}
			continue;
		}

		if (cameraFocus == RIGHT) {
			if (isLeftSpeakingNext3Sec && !isRightSpeakingNext3Sec) {
				cameraFocus = LEFT;
				skipFrames += 4;
			}
			else if (isRightSpeaking)
				cameraFocus = RIGHT;
			else {
				cameraFocus = CENTRAL;
				skipFrames += 2;
			}
			continue;
		}





		// if speaking not on cameraFocus AND will speak for more than 2 seconds AND 
		// the current speaker stops speaking for the following 2 seconds 
		// focus on new target.
		// ELSE focus on central.

		// if both sides speak in 2 second interval 
		// focus on central

		// if no one is speaking for 2 seconds =/= (combine with above)

		
		//if (isLeftSpeaking && isRightSpeaking)
		//	encodeOneStep(filename.c_str(), bluePixels, 64, 64);
		//else if (isLeftSpeaking)
		//	encodeOneStep(filename.c_str(), redPixels, 64, 64);
		//else if (isRightSpeaking)
		//	encodeOneStep(filename.c_str(), greenPixels, 64, 64);
		//else
		//	encodeOneStep(filename.c_str(), bluePixels, 64, 64);
	}
	std::cout << "DONE!!!!" << std::endl;

	// or, just use this quick shortcut to print a summary to the console
	audioFile.printSummary();

	return 0;
}
