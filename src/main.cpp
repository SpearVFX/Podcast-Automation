#include "../third-party/AudioFile.h"
#include "../third-party/lodepng.h"
#include "logger.h"
#include "utility.h"
#include <string>
#include <iostream>
#include <chrono>
#include <filesystem>
#include <codecvt>
#include <io.h>
#include <regex>

//TODO: Make it work for single camera and 3 camera setup, cuz now either one is broken.
// Add all ffmpeg commands

bool createFolderHierarchy(const std::wstring& directory) {
	Logger::get().info("Creating the podcast folder hierarchy!");
	std::wstring assets_dir = directory + L"\\Assets";
	std::wstring audio_dir = directory + L"\\Audio";
	std::wstring cameras_dir = directory + L"\\Cameras";
	std::vector<std::wstring> folders;
	folders.push_back(assets_dir);
	folders.push_back(audio_dir);
	folders.push_back(cameras_dir);
	folders.push_back(audio_dir + L"\\Mixdown");
	folders.push_back(audio_dir + L"\\Raw Audio");
	folders.push_back(cameras_dir + L"\\Left");
	folders.push_back(cameras_dir + L"\\Right");
	folders.push_back(cameras_dir + L"\\Center");

	for (auto folder : folders) {
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		std::string str = converter.to_bytes(folder);
		if (_wmkdir(folder.c_str())) {
			Logger::get().write("Successfully created the directory: " + str);
		} else {
			Logger::get().error("An error occurred creating the directory" + str);
			return false;
		}
	}
	return true;
}

bool renameCameraFiles(const std::string& folder_path) {
	int file_counter = 1;
	std::regex camera_file_pattern("^MVI_\\d{4}\\.mp4$");

	struct _finddata_t file_info;
	intptr_t handle = _findfirst((folder_path + "\\*").c_str(), &file_info);
	if (handle == -1) {
		Logger::get().error("Error opening directory: " + folder_path);
		return false;
	}

	do {
		if (file_info.attrib & _A_SUBDIR) {
			continue;
		}

		std::string filename = file_info.name;
		if (std::regex_match(filename, camera_file_pattern)) {
			std::string new_filename = std::to_string(file_counter) + ".mp4";
			std::string old_path = folder_path + "\\" + filename;
			std::string new_path = folder_path + "\\" + new_filename;
			std::rename(old_path.c_str(), new_path.c_str());
			++file_counter;
		}
	} while (_findnext(handle, &file_info) == 0);

	_findclose(handle);
	return true;
}


bool fillAveragedDbVectorMultipleFiles(std::vector<std::vector<double>>& averagedDB, int& numSamples, int& sampleRate, std::vector<AudioFile<double>>& audioFiles) {
	int hzElapsed = 0;
	double decibels = 0;
	double currentSample;
	averagedDB.resize(2);
	Logger::get().processInfo(numSamples - 2, "Averaging the Decibels in a vector.");
	for (int i = 0; i < numSamples - 1; i++)
	{
		for(int cFile = 0; cFile < audioFiles.size(); cFile++) {
			currentSample = audioFiles[cFile].demandSamples.getBufferAt(LEFT, i);
			decibels += currentSample * currentSample;
			if (hzElapsed++ == sampleRate) {
				// todo: better approach to noise calculation.
				double calculated = (decibels / sampleRate) * 1000;
				int channel = LEFT;
				if (cFile > 2)
					channel = RIGHT;
				if (i < averagedDB[channel].size()) {
					if (averagedDB[channel][i] < calculated) {
						averagedDB[channel][i] = calculated;
					} else {
						continue;
					}
				} else {
					averagedDB[channel].push_back(calculated);
				}
				decibels = 0;
				hzElapsed = 0;
				Logger::get().updateProcess(sampleRate);
			}
		}
	}
	return true;
}

bool fillAveragedDbVector(std::vector<std::vector<double>>& averagedDB, int& numSamples, int& sampleRate, AudioFile<double>& audioFile) {
	int hzElapsed = 0;
	double decibels = 0;
	double currentSample[2];
	averagedDB.resize(2);
	Logger::get().processInfo(numSamples - 2, "Averaging the Decibels in a vector.");
	// It is numSamples - 1, just because sometimes it crashes right on the last element, sometimes it doesn't.
	// I guess I should read more sound file format theory to get why that happens.
	// Nontheless, 1 sample isn't a big loss.
	for (int i = 0; i < numSamples - 1; i++)
	{
		for (int channel = 0; channel < 2; channel++) {
			currentSample[channel] = audioFile.demandSamples.getBufferAt(channel, i);
			decibels += currentSample[channel] * currentSample[channel];
			if (hzElapsed++ == sampleRate) {
				// todo: better approach to noise calculation.
				double calculated = (decibels / sampleRate) * 1000;
				averagedDB[channel].push_back(calculated);
				decibels = 0;
				hzElapsed = 0;
				Logger::get().updateProcess(sampleRate);
			}
		}
	}
	return true;
}

void exportVideoTemplateSingleStereoFile(std::string audioFileName) {
	/* Keeping this, so I can just copy paste when I need performance check.
	auto start = std::chrono::steady_clock::now();
	auto end = std::chrono::steady_clock::now();
	std::cout << "Elapsed time in milliseconds: "
		<< std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
		<< " ms" << std::endl;
		*/
	// Note: removed single camera functionality. If someone wants to use it, go back in the commits
	AudioFile<double> audioFile;
	Logger::get().info("Loading " + audioFileName + "...");
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

	std::wstring exportDirectoryWide = std::wstring(audioFileName.begin(), audioFileName.begin() + audioFileName.find('.'));
	std::string exportDirectory = std::string(audioFileName.begin(), audioFileName.begin() + audioFileName.find('.'));
	std::string fileNameRoot = exportDirectory.substr(exportDirectory.find_last_of('\\') + 1);
	bool createDir = _wmkdir(exportDirectoryWide.c_str());
	//todok: actually make it temporary
	if (createDir) {
		Logger::get().write("Created temp directory: " + exportDirectory);
	}
	else {
		Logger::get().error("Failed to create temp directory: " + exportDirectory);
		return;
	}

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
	if (argc != 2) {
		std::cout << " You need to pass a .WAV file(s) as an argument" << std::endl;
	} else {
		if (!strcmp(argv[1], "project")) {
			// auto find
		}
		if (!strcmp(argv[1], "auto")) {
			// auto find
		}
		else if (!strcmp(argv[1], "hierarchy")) {
			createFolderHierarchy(L".");
		}
		else {
			exportVideoTemplateSingleStereoFile(argv[1]);
		}
	}

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