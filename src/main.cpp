#include "../third-party/AudioFile.h"
#include "../third-party/lodepng.h"
#include <string>
#include <iostream>
#include <chrono>

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
	return averageDB >= 1.0f;
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
	std::vector<double> averagedDBLeft;
	std::vector<double> averagedDBRight;
	for (int i = 0; i < numSamples; i++)
	{
		double currentSampleLeft = audioFile.demandSamples.getBufferAt(0, i);
		double currentSampleRight = audioFile.demandSamples.getBufferAt(1, i);
		averageDBLeft += currentSampleLeft * currentSampleLeft;
		averageDBRight += currentSampleRight * currentSampleRight;
		if (hzElapsed++ == sampleRate) {
			double calcLeft = (averageDBLeft / sampleRate) * 1000;
			double calcRight = (averageDBRight / sampleRate) * 1000;
			averagedDBLeft.push_back(calcLeft);
			averagedDBRight.push_back(calcRight);
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
	int cameraFocus = 2;
	for (size_t i = 2; i < averagedDBLeft.size(); i++)
	{
		bool isRightSpeaking = isSpeaking(averagedDBLeft[i]);
		bool isLeftSpeaking = isSpeaking(averagedDBRight[i]);

		bool isRightSpeakingNext3Sec = isSpeaking(averagedDBLeft[i - 1]) && isSpeaking(averagedDBLeft[i - 2]);
		bool isLeftSpeakingNext3Sec = isSpeaking(averagedDBRight[i - 1]) && isSpeaking(averagedDBRight[i - 2]);


		std::string filename = "test_" + std::to_string(secondsElapsed++) + ".png";
		std::cout << filename << std::endl;
		switch (cameraFocus) {
		case(0): encodeOneStep(filename.c_str(), redPixels, 64, 64); break;
		case(1): encodeOneStep(filename.c_str(), greenPixels, 64, 64); break;
		case(2): encodeOneStep(filename.c_str(), bluePixels, 64, 64); break;
		}

		if ((isLeftSpeakingNext3Sec && isRightSpeakingNext3Sec) || (!isLeftSpeakingNext3Sec && !isRightSpeakingNext3Sec)) {
			cameraFocus = 2;
			continue;
		}

		if (cameraFocus == 2) {
			if (isLeftSpeakingNext3Sec)
				cameraFocus = 0;
			else if (isRightSpeaking)
				cameraFocus = 1;
			continue;
		}

		if (cameraFocus == 0) {
			if (isRightSpeakingNext3Sec && !isLeftSpeakingNext3Sec)
				cameraFocus = 1;
			else if (isLeftSpeaking)
				cameraFocus = 0;
			else
				cameraFocus = 2;
			continue;
		}

		if (cameraFocus == 1) {
			if (isLeftSpeakingNext3Sec && !isRightSpeakingNext3Sec)
				cameraFocus = 0;
			else if (isRightSpeaking)
				cameraFocus = 1;
			else
				cameraFocus = 2;
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
