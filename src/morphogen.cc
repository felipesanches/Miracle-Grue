/**
   MiracleGrue - Model Generator for toolpathing. <http://www.grue.makerbot.com>
   Copyright (C) 2011 Far McKon <Far@makerbot.com>, Hugo Boyer (hugo@makerbot.com)

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Affero General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

*/

//#define OMPFF // openMP mulitithreading extensions This Fu packs a ompff!


#include <iostream>
#include <string>

#include <stdlib.h>
#include "mgl/abstractable.h"
#include "mgl/configuration.h"
#include "mgl/miracle.h"

#include "mgl/Vector2.h"

using namespace std;
using namespace mgl;


double numberFromCharEqualsStr(const std::string& str)
{
	string nb = str.substr(3, str.length()-3);
	double val = atof(nb.c_str());
	return val;
}

void parseArgs(Configuration &config,
				int argc,
				char *argv[],
				string &modelFile,
				string &configFileName)
{
	modelFile = argv[argc-1];
    for(int i = 1;i < argc - 1;i++){
        string str = argv[i];
        cout << i << " " << str << endl;
        if(str.find("f=") != string::npos)
        {
        	config["slicer"]["firstLayerZ"]  = numberFromCharEqualsStr(str);
        }

        if(str.find("l=") != string::npos)
        {
        	config["slicer"]["layerH"] = numberFromCharEqualsStr(str);
        }

        if(str.find("w=") != string::npos)
        {
        	config["slicer"]["layerW"] = numberFromCharEqualsStr(str);
        }

        if(str.find("t=") != string::npos)
        {
        	config["slicer"]["tubeSpacing"] = numberFromCharEqualsStr(str);
        }

        if(str.find("a=") != string::npos)
        {
        	config["slicer"]["angle"] = numberFromCharEqualsStr(str);
        }

        if(str.find("-d") != string::npos)
        {
        	config["slicer"]["writeDebugScadFiles"] = true;
        }
    }
}

string getVersionStr()
{
	return  "v 0.01 alpha" ;
}

int preConditionsOrShowUsage(int argc, char *argv[])
{
	if (argc < 2)
	{
		cout << endl;
		cout << endl;
		cout << "Miracle-Grue" << endl;
		cout << "Makerbot Industries 2012" << endl;
		cout << endl;
		cout << getVersionStr().c_str() << endl;
		cout << endl;
		cout << "This program translates a 3d model file in STL format to GCODE toolpath for a 3D printer "<< endl;
		cout << "It also generates an OpenScad file for visualization"<< endl;
		cout << endl;
		cout << "usage: miracle-grue [OPTIONS] STL FILE" << endl;
		cout << "options: " << endl;
		cout << "  c=file.config : set the configuration file (default is local miracle.config)" << endl;
		cout << "  f=height : override the first layer height" << endl;
		cout << "  l=height : override the layer height" << endl;
		cout << "  w=height : override layer width" << endl;
		cout << "  t=width : override the infill grid width" << endl;
		cout << "  a=angle : override the infill grid inter slice angle (radians)" << endl;
		cout << "  -d : debug mode (creates scad files for each inset error)" << endl;
		cout << endl;
		cout << "It is pitch black. You are likely to be eaten by a grue." << endl;
		return (-1);
	}
	return 0;
}



int main(int argc, char *argv[], char *envp[])
{

	// design by contract ;-)
	int checks = preConditionsOrShowUsage(argc, argv);
	if(checks != 0)
	{
		return checks;
	}

	string modelFile;
	string configFileName = "miracle.config";

	for(int i = 1;i < argc - 1;i++)
    {
        string str = argv[i];
        if(str.find("c=") != string::npos)
        {
        	configFileName = str.substr(2, str.length()-2);

        }
    }

    Configuration config;
    try
    {
		cout << "Configuration file: " << configFileName << endl;
		config.readFromFile(configFileName.c_str());

		parseArgs(config, argc, argv, modelFile, configFileName);
		cout << config.asJson() << endl;

		MyComputer computer;
		cout << endl;
		cout << endl;
		cout << "behold!" << endl;
		cout << "Materialization of \"" << modelFile << "\" has begun at " << computer.clock.now() << endl;

		std::string scadFile = "."; // outDir
		scadFile += computer.fileSystem.getPathSeparatorCharacter();
		scadFile = computer.fileSystem.ChangeExtension(computer.fileSystem.ExtractFilename(modelFile.c_str()).c_str(), ".scad" );

		std::string gcodeFile = ".";
		gcodeFile += computer.fileSystem.getPathSeparatorCharacter();
		gcodeFile = computer.fileSystem.ChangeExtension(computer.fileSystem.ExtractFilename(modelFile.c_str()).c_str(), ".gcode" );

		cout << endl << endl;
		cout << modelFile << " to \"" << gcodeFile << "\" and \"" << scadFile << "\"" << endl;

		GCoder gcoder;
		loadGCoderData(config, gcoder);

		Slicer slicer;
		loadSlicerData(config, slicer);

		std::vector<mgl::SliceData> slices;
		miracleGrue(gcoder, slicer, modelFile.c_str(), scadFile.c_str(), gcodeFile.c_str(), slices);
	    cout << endl << computer.clock.now() << endl;
	    cout << "Done!" << endl;

    }
    catch(mgl::Exception &mixup)
    {
    	cout << "ERROR: "<< mixup.error << endl;
    	return -1;
    }


}
