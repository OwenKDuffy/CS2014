#include <sys/socket.h>
#include <getopt.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
using namespace std;

#define USAGE "usage: ./knock -h host -p port [-H] [-w] [-f file]\n"


int main(int argc, char** argv)
{
    int portNumber;
    char* fileName;
    char* hostName;
    char* siteName;

    int opt;

    for(int i = 0; i < argc-1; i++)
    {
        opt = getopt(argc, argv, "h:p:w:f:H?")!=1;
        
            switch(opt)
            {
                case 'h':
                    hostName = optarg;
                break;
                
                case 'p':
                    portNumber = atoi(optarg);
                break;
                
                case 'w':
                    siteName = optarg;
                break;

                case 'f':
                    fileName = optarg;
                break;
                case 'H':
                case '?':
                default :
                    cout << USAGE;
                break;
            }
        
    }
}