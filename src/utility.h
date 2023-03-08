#pragma once
#include <Windows.h>
/*void startupCmdProcess(LPCSTR lpApplicationName, char* args)
{
	// additional information
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	// set the size of the structures
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	// start the program up
	CreateProcessA
	(
		lpApplicationName,   // the path
		args,                // Command line
		NULL,                   // Process handle not inheritable
		NULL,                   // Thread handle not inheritable
		FALSE,                  // Set handle inheritance to FALSE
		CREATE_NEW_CONSOLE,     // Opens file in a separate console
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi           // Pointer to PROCESS_INFORMATION structure
	);
	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}
*/
HANDLE m_hChildStd_OUT_Rd = NULL;
HANDLE m_hChildStd_OUT_Wr = NULL;
HANDLE m_hreadDataFromExtProgram = NULL;
const int BUFSIZE = 10000;
static bool cmdProcessFinished = false;

DWORD __stdcall readDataFromExtProgram(void* argh)
{
    DWORD dwRead;
    CHAR chBuf[BUFSIZE];
    BOOL bSuccess = FALSE;

    for (;;)
    {
        bSuccess = ReadFile(m_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL);
        if (!bSuccess || dwRead == 0) continue;

        for (char c : chBuf) {
            if (c >= 0) {
                //std::cout << c << std::flush;
            }
        }
        if (!bSuccess) break;
    }
    cmdProcessFinished = true;
    return 0;
}

bool startupCmdProcess(std::string externalProgram, std::string arguments)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES saAttr;

    ZeroMemory(&saAttr, sizeof(saAttr));
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe for the child process's STDOUT. 

    if (!CreatePipe(&m_hChildStd_OUT_Rd, &m_hChildStd_OUT_Wr, &saAttr, 0))
    {
        // log error
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Ensure the read handle to the pipe for STDOUT is not inherited.

    if (!SetHandleInformation(m_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
    {
        // log error
        return HRESULT_FROM_WIN32(GetLastError());
    }

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = m_hChildStd_OUT_Wr;
    si.hStdOutput = m_hChildStd_OUT_Wr;
    si.dwFlags |= STARTF_USESTDHANDLES;

    ZeroMemory(&pi, sizeof(pi));

    std::string commandLine = externalProgram + " " + arguments;
    // Start the child process. 
    if (!CreateProcessA(NULL,           // No module name (use command line)
        (LPSTR)(commandLine.c_str()),    // Command line
        NULL,                           // Process handle not inheritable
        NULL,                           // Thread handle not inheritable
        TRUE,                           // Set handle inheritance
        0,                              // No creation flags
        NULL,                           // Use parent's environment block
        NULL,                           // Use parent's starting directory 
        &si,                            // Pointer to STARTUPINFO structure
        &pi)                            // Pointer to PROCESS_INFORMATION structure
        )
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    else
    {
        m_hreadDataFromExtProgram = CreateThread(0, 0, readDataFromExtProgram, NULL, 0, NULL);
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    return S_OK;
}

/**/

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
std::vector<unsigned char> blackPixels = generateSolidColorPixels(64, 64, 0, 0, 0);


// Encode an array of pixels into a png image.
void encodeOneStep(const char* filename, std::vector<unsigned char>& image, unsigned width, unsigned height) {
	//Encode the image
	unsigned error = lodepng::encode(filename, image, width, height);

	//if there's an error, display it
	if (error) std::cout << "encoder error " << error << ": " << lodepng_error_text(error) << std::endl;
}

bool isSpeaking(double averageDB) {
	return averageDB >= 0.7f;
}

void printLogo() {
	std::string logoAscii =
		"\n \n \n\
          #%%%%%%%%%%%%%%%# .#%%%%%%%* #%%%%%%%%%%%%%%%%#.\n\
          *%%%%%%%##**+==-...=*%%%%%%%.+%%%%%%%%%%%%%%%%%#\n\
          *%%%*....:-=++*#%%#* :%%%%%%-:%%%%%%%%%##*%%%%%#\n\
          +%%%###%%%%%%%%%%%%*  #%%%%%* #%%%%%+.   -%%%%#=              Podcast automation script\n\
          +%%%%%%%%%%%%%%%%%#- :#%%%%%# +%%%%%*:::+#%%#=..                written by: Sasho Prostoff\n\
          =%%%%-::-+%%%%%%%+.:*%%%%%%%%::%%%%%%%%%%%#- -**\n\
          =%%%%.  =%%%%%%*::*%%%#+#%%%%+ #%%%%%%%%*- -#%%*                                Barlog 2023\n\
          -%%%%..*%%%%%*:.+%%%*--#%%%%%# +%%%%%#+: =#%%%%*\n\
          -%%%%%%%%%%#-.+#%#+:.+%%%%%%%%.-%%%%%..=#%%%%%%*\n\
          .#%%%%%%%#=.+#%%%=.=#%%%#*%%%%-.%%%%%-=%%%%%%%%*\n\
            :=+++*= :#%%%%%%%%%%*: -%%%%+ #%%%%=:%%%%%%%%*\n\
          =*++===-: -#%%%%%%%#=..  .=+*## *%%%%+ %%%%%%%#=\n\
          -%%%%%%%%*- -#%%%*::=#%%##*+=-:.:=*#%# #%%%%%*  \n\
          :%%%%%%%%%%#- -: :*%%%%%%%%%%%%%%#+:.- +%%%%%*  \n\
          .%%%%%=*%%%%%#: -%%%%%%%%%%%%%%%%%%%#+.=%%%%%*  \n\
           %%%%%+ #%%%%%%= +%%%%%*=++*#%%%%%%%%%-=%%%%%*                Automates the proccess of\n\
           #%%%%%-.#%%%%%%+ =%%%%#=:.    .-#%%%%=-%%%%%*                editing a three-camera set up.\n\
           #%%%%%#.:#%%%%%%*.:#%%%%%%#*=+#%%%%%%=:%%%%%*  \n\
           %%%%%%%* =%%%%%%%#:.*%%%%%%%%%%%%%%%%=:%%%%%*  \n\
          :%%%%%%%%= +%%%%%%%#- =%%%%%%%%%%%%%%#-.%%%%%*  \n\
          =%%%%%%%%%: *%%%%%%%%+ :#%%%%%#####**.  *****+\n\n\n\n\n";
	for (char a : logoAscii) {
		if (a == '%') {
			std::cout << "\033[1;31m%\033[0m";
		}
		else {
			std::cout << a;
		}
	}
}