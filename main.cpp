#include <iostream>
#include <stdlib.h>
#include "GDConnect.h"

using namespace std;

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cout << "Usage: GDConnect <config.json>" << std::endl;
        return 0;
    }
    try
    {
        GDConnect connection;
        int err = connection.init(argv[1]);
        if (!connection.valid())
        {
            std::cout << "Connection did not open properly" << std::endl;
            return -1;
        }

        if (!err)
        {
            std::string input;
            std::pair<std::string, int> result;
            bool repeat = true;
            do
            {
                std::cout << "Select an option:" << std::endl;
                std::cout << "1 - List files on Google Drive" << std::endl;
                std::cout << "2 - Get a file id using filename" << std::endl;
                std::cout << "3 - Download a file" << std::endl;
                std::cout << "4 - Upload a file" << std::endl;
                std::cout << "5 - Renew token" << std::endl;
                std::cout << "6 - Quit" << std::endl;
                getline(std::cin, input);
                int choice = atoi(input.c_str());
                switch(choice)
                {
                case 1:
                    std::cout << "Listing files" << std::endl;
                    connection.listFiles();
                    break;
                case 2:
                    std::cout << "Enter the filename:" << std::endl;
                    getline(std::cin, input);
                    std::cout << "File id: " << connection.getFileId(input.c_str()) << std::endl;
                    break;
                case 3:
                    std::cout << "Enter the file id:" << std::endl;
                    getline(std::cin, input);
                    connection.getFileById(input.c_str());
                    break;
                case 4:
                    std::cout << "Enter the file name:" << std::endl;
                    getline(std::cin, input);
                    result = connection.putFile(input.c_str());
                    if (!result.second) {
                        std::cout << "New file id: " << result.first << std::endl;
                    }
                    break;
                case 5:
                    std::cout << "Renewing token" << std::endl;
                    connection.renewToken();
                    break;
                case 6:
                    std::cout << "Exiting" << std::endl;
                    repeat = false;
                    break;
                }
            }
            while (repeat);
        }
        else
        {
            std::cout << "Unable to acquire access token" << std::endl;
            return -1;
        }
        return 0;
    }
    catch (const char * e)
    {
        std::cout << e << std::endl;
    }

}
