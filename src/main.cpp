#include "../third-party/AudioFile.h"
#include "../third-party/lodepng.h"
#include "logger.h"
#include "utility.h"
#include <string>
#include <iostream>
#include <chrono>


//TODO: Make it work for single camera and 3 camera setup, cuz now either one is broken.
// Add all ffmpeg commands


void fillAveragedDbVector(std::vector<std::vector<double>>& averagedDB, int& numSamples, int& sampleRate, AudioFile<double>& audioFile) {
	int channel = 0;
	int hzElapsed = 0;
	double averageDBLeft = 0;
	double averageDBRight = 0;
	averagedDB.resize(2);
	Logger::get().processRead(numSamples - 2, "Averaging the Decibels in a *seconds array*. Note: This is onDemand operation, so the file is open for reading.");
	// It is numSamples - 1, just because sometimes it crashes right on the last element, sometimes it doesn't.
	// I guess I should read more sound file format theory to get why that happens.
	// Nontheless, 1 sample isn't a big loss.
	for (int i = 0; i < numSamples - 1; i++)
	{
		double currentSampleLeft = audioFile.demandSamples.getBufferAt(LEFT, i);
		double currentSampleRight = audioFile.demandSamples.getBufferAt(RIGHT, i);

		averageDBLeft += currentSampleLeft * currentSampleLeft;
		averageDBRight += currentSampleRight * currentSampleRight;
		if (hzElapsed++ == sampleRate) {
			// todo: better approach to noise calculation.
			double calcLeft = (averageDBLeft / sampleRate) * 1000;
			double calcRight = (averageDBRight / sampleRate) * 1000;
			averagedDB[LEFT].push_back(calcLeft);
			averagedDB[RIGHT].push_back(calcRight);
			std::string result;

			averageDBLeft = 0;
			averageDBRight = 0;
			hzElapsed = 0;

			Logger::get().updateProcess(sampleRate);
		}
	}
}
void exportVideoTemplate(std::string audioFileName, int cameraCount = 3) {
	/* Keeping this, so I can just copy paste when I need performance check.
	auto start = std::chrono::steady_clock::now();
	auto end = std::chrono::steady_clock::now();
	std::cout << "Elapsed time in milliseconds: "
		<< std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
		<< " ms" << std::endl;
		*/
	if (cameraCount != 1 && cameraCount != 3) {
		Logger::get().error("INVALID NUMBER OF CAMERAS! 1 and 3 cameras are supported.");
	}
	AudioFile<double> audioFile;
	bool successLoad = audioFile.load(audioFileName, true); // For podcasts the load should always be true (note: it's my custom functianality in AudioFile.h)
	if (successLoad)
		Logger::get().read("Loaded " + audioFileName + " successfully!");
	else {
		Logger::get().error("Failed to load " + audioFileName);
		return;
	}


	int sampleRate = audioFile.getSampleRate();
	int bitDepth = audioFile.getBitDepth();
	int numSamples = audioFile.demandSamples.numSamples; // Demand samples is practically the count of all samples, not just the current demand.
	double lengthInSeconds = audioFile.getLengthInSeconds();
	int numChannels = audioFile.getNumChannels();
	bool isMono = audioFile.isMono();
	bool isStereo = audioFile.isStereo();
	
	Logger::get().info("Sample rate: " + std::to_string(sampleRate));
	Logger::get().info("Bit depth: " + std::to_string(bitDepth));
	Logger::get().info("Number of samples: " + std::to_string(numSamples));
	
	std::vector<std::vector<double>> averagedDB;
	fillAveragedDbVector(averagedDB, numSamples, sampleRate, audioFile);
	int secondsElapsed = 0;
	// 0 - left, 1 - right, 2 - central
	int cameraFocus = CENTRAL;
	int skipFrames = 0; // These are used for multicamera
	//todok: refactor this...

	std::wstring exportDirectoryWide = std::wstring(audioFileName.begin(), audioFileName.begin() + audioFileName.find('.'));
	std::string exportDirectory = std::string(audioFileName.begin(), audioFileName.begin() + audioFileName.find('.'));
	std::string fileNameRoot = exportDirectory.substr(exportDirectory.find_last_of('\\') + 1);
	bool createDir = _wmkdir(exportDirectoryWide.c_str());
	
	if (createDir) {
		Logger::get().write("Created directory: " + exportDirectory);
	}
	else {
		Logger::get().error("Failed to create directory: " + exportDirectory);
		return;
	}

	if (cameraCount == 1) {

		for (size_t i = 2; i < averagedDB[0].size(); i++) {
			std::string filename = exportDirectory + "/" + fileNameRoot + "_" + std::to_string(secondsElapsed++) + ".png";
			bool isCurrentlySpeaking = isSpeaking(averagedDB[LEFT][i]);
			if (isCurrentlySpeaking) {
				encodeOneStep(filename.c_str(), greenPixels, 64, 64);
			} else {
				encodeOneStep(filename.c_str(), blackPixels, 64, 64);
			}
		}
	}
	else {
		Logger::get().processWrite(averagedDB[0].size() - 4, " Exporting an image for every frame..");
		for (size_t i = 3; i < averagedDB[0].size(); i++)
		{
			std::string filename = exportDirectory + "/" + fileNameRoot + "_" + std::to_string(secondsElapsed++) + ".png";
			//std::cout << filename << std::endl;
			switch (cameraFocus) {
			case(LEFT): encodeOneStep(filename.c_str(), redPixels, 64, 64); break;
			case(RIGHT): encodeOneStep(filename.c_str(), greenPixels, 64, 64); break;
			case(CENTRAL): encodeOneStep(filename.c_str(), bluePixels, 64, 64); break;
			}
			Logger::get().updateProcess(1, filename);
			if (skipFrames > 0) {
				skipFrames--;
				continue;
			}
			double lDb = averagedDB[LEFT][i];
			double rDb = averagedDB[RIGHT][i];
			bool isLeftSpeaking = isSpeaking(averagedDB[LEFT][i]);
			bool isRightSpeaking = isSpeaking(averagedDB[RIGHT][i]);

			bool isLeftSpeakingNext3Sec = isSpeaking((averagedDB[LEFT][i - 1] + averagedDB[LEFT][i - 2] + averagedDB[LEFT][i - 3]) / 3.0f);
			bool isRightSpeakingNext3Sec = isSpeaking((averagedDB[RIGHT][i - 1] + averagedDB[RIGHT][i - 2] + averagedDB[RIGHT][i - 3]) / 3.0f);




			if ((isLeftSpeakingNext3Sec && isRightSpeakingNext3Sec) || (!isLeftSpeakingNext3Sec && !isRightSpeakingNext3Sec)) {
				cameraFocus = CENTRAL;
				continue;
			}

			if (cameraFocus == CENTRAL) {
				if (isLeftSpeakingNext3Sec) {
					cameraFocus = LEFT;
					skipFrames += 3;
				}
				else if (isRightSpeakingNext3Sec) {
					cameraFocus = RIGHT;
					skipFrames += 3;
				}
				else {
					skipFrames += 1;
				}
				continue;
			}

			if (cameraFocus == LEFT) {
				if (isRightSpeakingNext3Sec && !isLeftSpeakingNext3Sec) {
					cameraFocus = RIGHT;
					skipFrames += 1;
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
					skipFrames += 1;
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
}
	Logger::get().info("Exported all images for " + audioFileName + ".");

	Logger::get().info("Creating video using ffmpeg" + audioFileName + ".");
	std::string program = "C:/Program Files/ffmpeg/ffmpeg.exe";
	std::string commandLineExe;
	// ffmpeg should be a command
	commandLineExe +=  " -r 1 -i \"" + exportDirectory + "\\" + fileNameRoot + "_%d.png" + "\" -c:v libx264 -vf fps=25 -pix_fmt yuv420p -y " + exportDirectory + "\\" + fileNameRoot + "_" + "output.mp4";
	Logger::get().info("Calling program: " + program + " with arguments: " + commandLineExe);
	startupCmdProcess("C:/Program Files/ffmpeg/ffmpeg.exe", (char*)commandLineExe.c_str());
	cmdProcessFinished = false;
	// or, just use this quick shortcut to print a summary to the console
	//audioFile.printSummary();
}

int main(int argc, char* argv[]) {
	//printLogo();
	if (argc < 2) {
		std::cout << " You need to pass a .WAV file(s) as an argument" << std::endl;
	} else if (argc == 2) {
		exportVideoTemplate(argv[1], 3);
	}
	if (argc > 2) {
		if (isdigit(atoi(argv[1]))) {
			int cameraCount = atoi(argv[1]);
			for (int i = 2; i < argc; i++) {
				exportVideoTemplate(argv[i], cameraCount);
			}
		}
	}
	//return 0;
	

	return 0;
}


// TODO:
/*
1. Fix your goddamn github key
2. Add a create folder hierarchy functionality
3. Refactor the main function
4. Create a logger, that you can reuse in other projects.
5. Refactor exportVideoTemplate function
6. Create the template project.


*/