#include <iostream>
#include <filesystem>
#include <algorithm>
#include <string>
#include <vector>
#include <fstream>


#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

namespace fs = std::filesystem;

std::vector<fs::directory_entry> getPaths(fs::path path)
{
	std::vector<fs::directory_entry> paths;
	
	for(const auto &entry : fs::directory_iterator(path))
	{
		paths.push_back(entry);
	}
	
	return paths;
}

int main()
{
	#pragma region Set terminal characteristics
	static struct termios oldt, newt;

	/*tcgetattr gets the parameters of the current terminal
	STDIN_FILENO will tell tcgetattr that it should write the settings
	of stdin to oldt*/
	tcgetattr(STDIN_FILENO, &oldt);
	/*now the settings will be copied*/
	newt = oldt;

	/*ICANON normally takes care that one line at a time will be processed
	that means it will return if it sees a "\n" or an EOF or an EOL*/
	newt.c_lflag &= ~( ICANON | ECHO );

	/*Those new settings will be set to STDIN
	TCSANOW tells tcsetattr to change attributes immediately. */
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	#pragma endregion
	
	
	fs::path currentPath = fs::current_path();
	std::vector<fs::directory_entry> paths;
	
	int lastSelected = 0;
	int selected = 0;
	
	auto regeneratePaths = [&]() -> void
	{
		paths = getPaths(currentPath);
		std::sort(paths.begin(), paths.end());
	};
	
	auto buildConsoleString = [&]() -> std::string
	{
		struct winsize w;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

		int size = paths.size();

		int start = 0;
		int end = std::min(size, static_cast<int>( w.ws_row ) - 2);

		std::string output = "\x01b[1;1H$ ";
		output += currentPath.c_str();
		output += "\n";
		
		if(selected >= w.ws_row - 2)
		{
			output += "\x01[1m...\n";
			start = selected - w.ws_row + 2;
			end = selected + 2;
		}

		output += "\x01b[0m";

		for(int i = start; i < end; i++)
		{
			auto name = paths[i].path().filename();

			if(i == selected) output += "> \x01b[7m";
			else output += "  ";

			if(paths[i].is_directory()) output += "\x01b[33m";

			output += name;
			output += "\n\x01b[0m";
		}

		if(end < size)
		{
			output += "\x01b[1m...\x01b[0m";
		}

		return output;
	};
	
	regeneratePaths();
	
	
	while(true)
	{
		std::cout << "\x01b[2J";
		std::cout << buildConsoleString();
		
		char ch;
		
		ch = getchar();
		
		// std::cout << static_cast<int>(ch) << std::endl;
		
		// break;
		
		switch(static_cast<int>(ch))
		{
			// UP
			case 65:
				selected--;
				break;
			// DOWN
			case 66:
				selected++;
				break;
			// ENTER
			case 10:
			{
				auto selectedPath = paths[selected].path();
			
				if(paths[selected].is_directory())
				{
					currentPath = currentPath / selectedPath.filename();

					int oldSize = paths.size();
					regeneratePaths();
					
					if(oldSize > paths.size())
					{
						lastSelected = selected;
					}
				}
				else
				{
					char cmd[256];
					snprintf(cmd, 256, "file %s > temp.txt", selectedPath.c_str());
					
					system(cmd);
					
					std::ifstream ifs("temp.txt");
					std::string ret{ std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>() };
					ifs.close();
					
					if(std::remove("temp.txt") != 0) {
						perror("Error deleting temporary file");
					}
					
					// Is text file
					if(ret.find("text") != std::string::npos)
					{
						snprintf(cmd, 256, "nano %s", selectedPath.c_str());
						
						system(cmd);
					}
				}
			}
				break;
			// BACKSPACE
			case 127:
			{
				std::string pString = currentPath.string();
			
				currentPath = fs::path( pString.substr(0, pString.find_last_of("\\/")) );
				
				if(currentPath.empty()) currentPath = "/";
				
				int oldSize = paths.size();
				regeneratePaths();
				if(oldSize < paths.size())
				{
					selected = lastSelected;
				}
			}
				break;
			// QUIT
			case 'q':
				std::cout << "\n";
				goto endLoop;
				break;
			// Open current directory
			case 'c':
				fs::current_path(currentPath);
				
				system("gnome-terminal");
				
				break;
		}
		
		selected = std::clamp(selected, 0, static_cast<int>(paths.size())-1);
	}
	
endLoop:
	
	/*restore the old settings*/
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	
	return 0;
}